
#include "pch.h"
#include "Simulator.h"
#include "BridgePropertiesControl.h"

static ATOM wndClassAtom;
static constexpr wchar_t PropertiesWindowWndClassName[] = L"PropertiesWindow-{24B42526-2970-4B3C-A753-2DABD22C4BB0}";

class PropertiesWindow : public IPropertiesWindow
{
	ULONG _refCount = 1;
	ComPtr<ISelection> const _selection;
	HWND _hwnd = nullptr;
	BridgePropertiesControl* _bridgePropsControl = nullptr;

public:
	PropertiesWindow (HWND hWndParent, const RECT& rect, ISelection* selection)
		: _selection(selection)
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
				nullptr,//(HBRUSH)(COLOR_WINDOW + 1), // hbrBackground
				nullptr, // lpszMenuName
				PropertiesWindowWndClassName, // lpszClassName
				nullptr
			};

			wndClassAtom = RegisterClassEx(&wndClassEx);
			if (wndClassAtom == 0)
				throw win32_exception(GetLastError());
		}

		auto hwnd = ::CreateWindow(PropertiesWindowWndClassName, L"Properties", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
								   rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, hWndParent, 0, hInstance, this);
		if (hwnd == nullptr)
			throw win32_exception(GetLastError());
		assert(hwnd == _hwnd);

		_bridgePropsControl = new BridgePropertiesControl (_hwnd, GetClientRectPixels());
	}

	~PropertiesWindow()
	{
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
			window->_hwnd = hwnd;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
			// GDI now holds a pointer to this object, so let's call AddRef.
			window->AddRef();
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
			window->Release(); // Release the reference we added on WM_NCCREATE.
		}

		return result;
	}

	LRESULT WindowProc(UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (msg == WM_SIZE)
		{
			if (_bridgePropsControl != nullptr)
				::MoveWindow (_bridgePropsControl->GetHWnd(), 0, 0, LOWORD(lParam), HIWORD(lParam), FALSE);
			return 0;
		}
		else if (msg == WM_ERASEBKGND)
		{
			return 1;
		}
		else if (msg == WM_PAINT)
		{
			PAINTSTRUCT ps;
			::BeginPaint (_hwnd, &ps);
			::RoundRect (ps.hdc, 10, 10, 100, 100, 5, 5);
			::EndPaint (_hwnd, &ps);
			return 0;
		}

		return DefWindowProc (_hwnd, msg, wParam, lParam);
	}

	virtual HWND GetHWnd() const override final { return _hwnd; }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		if (!ppvObject)
			return E_INVALIDARG;

		*ppvObject = NULL;
		if (riid == __uuidof(IUnknown))
		{
			*ppvObject = static_cast<IUnknown*>((IProjectWindow*) this);
			AddRef();
			return S_OK;
		}

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override
	{
		return InterlockedIncrement(&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release() override
	{
		ULONG newRefCount = InterlockedDecrement(&_refCount);
		if (newRefCount == 0)
			delete this;
		return newRefCount;
	}
	#pragma endregion
};

const PropertiesWindowFactory propertiesWindowFactory = [](auto... params) { return ComPtr<IPropertiesWindow>(new PropertiesWindow(params...), false); };
