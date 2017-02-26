
#include "pch.h"
#include "Simulator.h"

using namespace std;

static ATOM wndClassAtom;
static constexpr wchar_t DockPanelClassName[] = L"DockPanel-{24B42526-2970-4B3C-A753-2DABD22C4BB0}";

class DockPanel : public IDockPanel
{
	ULONG _refCount = 1;
	EventManager _em;
	HWND _hwnd = nullptr;
	vector<ComPtr<ISidePanel>> _sidePanels;

public:
	DockPanel(HWND hWndParent, DWORD controlId, const RECT& rect)
	{
		HINSTANCE hInstance;
		BOOL bRes = GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)&wndClassAtom, &hInstance);
		if (!bRes)
			throw win32_exception(GetLastError());

		if (wndClassAtom == 0)
		{
			WNDCLASS wndClass =
			{
				CS_DBLCLKS, // style
				&WindowProcStatic, // lpfnWndProc
				0, // cbClsExtra
				0, // cbWndExtra
				hInstance, // hInstance
				nullptr, // hIcon
				nullptr, // hCursor
				nullptr, //(HBRUSH)(COLOR_WINDOW + 1), // hbrBackground
				nullptr, // lpszMenuName
				DockPanelClassName // lpszClassName
			};

			wndClassAtom = RegisterClass(&wndClass);
			if (wndClassAtom == 0)
				throw win32_exception(GetLastError());
		}

		LONG x = rect.left;
		LONG y = rect.top;
		LONG w = rect.right - rect.left;
		LONG h = rect.bottom - rect.top;
		auto hwnd = ::CreateWindowEx(0, DockPanelClassName, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, x, y, w, h, hWndParent, 0, hInstance, this);
		if (hwnd == nullptr)
			throw win32_exception(GetLastError());
		assert(hwnd == _hwnd);
	}

	~DockPanel()
	{
		assert(_refCount == 0);
		if (_hwnd != nullptr)
			::DestroyWindow(_hwnd);
	}

	static LRESULT CALLBACK WindowProcStatic(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		//if (AssertFunctionRunning)
		//{
		//	// Let's try not to run application code while the assertion dialog is shown. We'll probably mess things up even more.
		//	return DefWindowProc(hwnd, uMsg, wParam, lParam);
		//}

		DockPanel* window;
		if (uMsg == WM_NCCREATE)
		{
			LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
			window = reinterpret_cast<DockPanel*>(lpcs->lpCreateParams);
			window->_hwnd = hwnd;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
			// GDI now holds a pointer to this object, so let's call AddRef.
			window->AddRef();
		}
		else
			window = reinterpret_cast<DockPanel*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

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
			auto layOutFunction = [](ISidePanel* sp, const RECT& rect)
				{ ::MoveWindow(sp->GetHWnd(), rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE); };
			auto contentRect = LayOutPanels (nullptr, layOutFunction);
			LayOutContent(contentRect);
			return 0;
		}

		return DefWindowProc(_hwnd, msg, wParam, lParam);
	}

	virtual HWND GetHWnd() const override final { return _hwnd; }

	HWND FindChild (std::function<bool(HWND child)> predicate)
	{
		auto child = ::GetWindow(_hwnd, GW_CHILD);
		if (child == nullptr)
			return nullptr;

		if (predicate(child))
			return child;

		while ((child = ::GetWindow(child, GW_HWNDNEXT)) != nullptr)
		{
			if (predicate(child))
				return child;
		}

		return nullptr;
	}

	RECT LayOutPanel (Side side, SIZE proposedSize, _In_ _Out_ RECT* contentRect) const
	{
		RECT panelRect;
		switch (side)
		{
			case Side::Right:
				panelRect = { contentRect->right - proposedSize.cx, contentRect->top, contentRect->right, contentRect->bottom };
				contentRect->right -= proposedSize.cx;
				return panelRect;

			default:
				throw not_implemented_exception();

		}
	}

	RECT LayOutPanels (function<SIZE(ISidePanel*)> getProposedSize, function<void(ISidePanel* sp, const RECT& panelRect)> fn) const
	{
		RECT contentRect = GetClientRect();

		for (auto& sp : _sidePanels)
		{
			SIZE proposedSize = (getProposedSize != nullptr) ? getProposedSize(sp) : (proposedSize = sp->GetWindowSize());
			RECT panelRect = LayOutPanel (sp->GetSide(), proposedSize, &contentRect);
			if (fn != nullptr)
				fn (sp, panelRect);
		}

		return contentRect;
	}

	void LayOutContent (const RECT& contentRect)
	{
		auto isContentHWnd = [this](HWND c)
		{
			return none_of(_sidePanels.begin(), _sidePanels.end(), [c](const ComPtr<ISidePanel>& sp) { return sp->GetHWnd() == c; });
		};

		auto content = FindChild(isContentHWnd);
		if (content != nullptr)
			::MoveWindow (content, contentRect.left, contentRect.top, contentRect.right - contentRect.left, contentRect.bottom - contentRect.top, TRUE);
	}

	virtual RECT GetContentRect() const override final
	{
		return LayOutPanels(nullptr, nullptr);
	}

	virtual ISidePanel* GetOrCreateSidePanel(Side side) override final
	{
		auto it = find_if(_sidePanels.begin(), _sidePanels.end(), [side](const ComPtr<ISidePanel>& p) { return p->GetSide() == side; });
		if (it != _sidePanels.end())
			return it->Get();

		static constexpr SIZE DefaultPanelSize = { 300, 300 };
		auto contentRect = LayOutPanels(nullptr, nullptr);
		RECT panelRect = LayOutPanel (side, DefaultPanelSize, &contentRect);
		
		LayOutContent (contentRect);

		auto panel = sidePanelFactory(_hwnd, 0xFFFF, panelRect, side);

		panel->GetSidePanelSplitterDraggingEvent().AddHandler (&OnSidePanelSplitterDragging, this);
		_sidePanels.push_back(panel);

		return panel;
	}
	
	static void OnSidePanelSplitterDragging (void* callbackArg, ISidePanel* sidePanel, SIZE proposedSize)
	{
		auto dp = static_cast<DockPanel*>(callbackArg);

		auto getProposedSize = [sidePanel, proposedSize](ISidePanel* sp) { return (sp != sidePanel) ? sp->GetWindowSize() : proposedSize; };
		auto layOutFunction = [](ISidePanel* sp, const RECT& rect)
			{ ::MoveWindow(sp->GetHWnd(), rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE); };

		auto contentRect = dp->LayOutPanels(getProposedSize, layOutFunction);
		dp->LayOutContent(contentRect);
	}

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override final { return E_NOTIMPL; }

	virtual ULONG STDMETHODCALLTYPE AddRef() override final
	{
		return InterlockedIncrement(&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release() override final
	{
		auto newRefCount = InterlockedDecrement(&_refCount);
		if (newRefCount == 0)
			delete this;
		return newRefCount;
	}
	#pragma endregion
};

extern const DockPanelFactory dockPanelFactory = [](auto... params) { return ComPtr<IDockPanel>(new DockPanel(params...), false); };
