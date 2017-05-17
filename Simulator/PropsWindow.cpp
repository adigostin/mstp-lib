
#include "pch.h"
#include "Simulator.h"

using namespace std;

static ATOM wndClassAtom;
static constexpr wchar_t PropertiesWindowWndClassName[] = L"PropertiesWindow-{24B42526-2970-4B3C-A753-2DABD22C4BB0}";

class PropertiesWindow : public IPropertiesWindow
{
	ULONG _refCount = 1;
	ISimulatorApp* const _app;
	IProjectPtr const _project;
	IProjectWindow* const _projectWindow;
	HWND _hwnd = nullptr;
	SIZE _clientSize;
	HFONT _font;
	IBridgePropsWindowPtr _bridgePropsControl;

public:
	PropertiesWindow (ISimulatorApp* app,
					  IProjectWindow* projectWindow,
					  IProject* project,
					  ISelection* selection,
					  HWND hWndParent,
					  POINT location)
		: _app(app)
		, _projectWindow(projectWindow)
		, _project(project)
	{
		HINSTANCE hInstance;
		BOOL bRes = GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)&wndClassAtom, &hInstance);
		if (!bRes)
			throw win32_exception(GetLastError());

		if (wndClassAtom == 0)
		{
			WNDCLASSEX wndClassEx =
			{
				sizeof(wndClassEx),
				CS_DBLCLKS, // style
				&WindowProcStatic, // lpfnWndProc
				0, // cbClsExtra
				0, // cbWndExtra
				hInstance, // hInstance
				nullptr, // hIcon
				LoadCursor(nullptr, IDC_ARROW), // hCursor
				(HBRUSH)(COLOR_WINDOW + 1), // hbrBackground
				nullptr, // lpszMenuName
				PropertiesWindowWndClassName, // lpszClassName
				nullptr
			};

			wndClassAtom = RegisterClassEx(&wndClassEx);
			if (wndClassAtom == 0)
				throw win32_exception(GetLastError());
		}

		auto hwnd = ::CreateWindow(PropertiesWindowWndClassName, L"Properties", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
								   location.x, location.y, 0, 0, hWndParent, 0, hInstance, this);
		if (hwnd == nullptr)
			throw win32_exception(GetLastError());
		assert(hwnd == _hwnd);

		NONCLIENTMETRICS ncm = { sizeof(NONCLIENTMETRICS) };
		SystemParametersInfo (SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
		_font = ::CreateFontIndirect (&ncm.lfMessageFont);

		_bridgePropsControl = bridgePropertiesControlFactory (_app, _projectWindow, _project, selection, _hwnd, { 0, 0 });

		SIZE ws = _bridgePropsControl->GetWindowSize();
		::MoveWindow (_hwnd, 0, 0, ws.cx, ws.cy, TRUE);
	}

	~PropertiesWindow()
	{
		::DeleteObject(_font);
		if (_hwnd)
			::DestroyWindow(_hwnd);
	}

	static LRESULT CALLBACK WindowProcStatic(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		//if (AssertFunctionRunning)
		//{
		//	// Let's try not to run application code while the assertion dialog is shown. We'll probably mess things up even more.
		//	return DefWindowProc(hwnd, uMsg, wParam, lParam);
		//}

		PropertiesWindow* window;
		if (uMsg == WM_NCCREATE)
		{
			LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
			window = reinterpret_cast<PropertiesWindow*>(lpcs->lpCreateParams);
			window->AddRef();
			window->_hwnd = hwnd;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
		}
		else
			window = reinterpret_cast<PropertiesWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

		if (window == nullptr)
		{
			// this must be one of those messages sent before WM_NCCREATE or after WM_NCDESTROY.
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}

		LRESULT result = window->WindowProc(uMsg, wParam, lParam);

		if (uMsg == WM_NCDESTROY)
		{
			window->_hwnd = nullptr;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
			window->Release();
		}

		return result;
	}

	LRESULT WindowProc(UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (msg == WM_CREATE)
		{
			_clientSize.cx = ((LPCREATESTRUCT) lParam)->cx;
			_clientSize.cy = ((LPCREATESTRUCT) lParam)->cy;
			return 0;
		}

		if (msg == WM_SIZE)
		{
			_clientSize.cx = LOWORD(lParam);
			_clientSize.cy = HIWORD(lParam);
			if (_bridgePropsControl != nullptr)
			{
				::MoveWindow (_bridgePropsControl->GetHWnd(), 0, 0, _clientSize.cx, _clientSize.cy, FALSE);
				if (_bridgePropsControl->IsVisible())
					::UpdateWindow(_bridgePropsControl->GetHWnd());
			}
			return 0;
		}

		if (msg == WM_PAINT)
		{
			PAINTSTRUCT ps;
			::BeginPaint (_hwnd, &ps);
			auto oldFont = ::SelectObject (ps.hdc, _font);
			static constexpr wchar_t Text[] = L"(nothing selected)";
			RECT rc = { 0, 0, _clientSize.cx, _clientSize.cy };
			::DrawTextW (ps.hdc, Text, -1, &rc, DT_VCENTER | DT_CENTER | DT_SINGLELINE);
			::SelectObject (ps.hdc, oldFont);
			::EndPaint (_hwnd, &ps);
			return 0;
		}

		return DefWindowProc (_hwnd, msg, wParam, lParam);
	}

	virtual HWND GetHWnd() const override final { return _hwnd; }

	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override { return E_NOTIMPL; }

	virtual ULONG STDMETHODCALLTYPE AddRef() override final
	{
		return InterlockedIncrement(&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release() override final
	{
		assert (_refCount > 0);
		ULONG newRefCount = InterlockedDecrement(&_refCount);
		if (newRefCount == 0)
			delete this;
		return newRefCount;
	}
};

template <typename... Args>
static IPropertiesWindowPtr Create (Args... args)
{
	return IPropertiesWindowPtr(new PropertiesWindow (std::forward<Args>(args)...), false);
};

const PropertiesWindowFactory propertiesWindowFactory = &Create;
