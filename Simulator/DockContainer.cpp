
#include "pch.h"
#include "Simulator.h"

using namespace std;

static ATOM wndClassAtom;
static constexpr wchar_t DockContainerClassName[] = L"DockContainer-{24B42526-2970-4B3C-A753-2DABD22C4BB0}";

class DockContainer : public IDockContainer
{
	ULONG _refCount = 1;
	HINSTANCE const _hInstance;
	EventManager _em;
	HWND _hwnd = nullptr;
	vector<IDockablePanelPtr> _dockablePanels;

public:
	DockContainer (HINSTANCE hInstance, HWND hWndParent, const RECT& rect)
		: _hInstance(hInstance)
	{
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
				DockContainerClassName // lpszClassName
			};

			wndClassAtom = RegisterClass(&wndClass);
			if (wndClassAtom == 0)
				throw win32_exception(GetLastError());
		}

		LONG x = rect.left;
		LONG y = rect.top;
		LONG w = rect.right - rect.left;
		LONG h = rect.bottom - rect.top;
		auto hwnd = ::CreateWindowEx(0, DockContainerClassName, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, x, y, w, h, hWndParent, 0, hInstance, this);
		if (hwnd == nullptr)
			throw win32_exception(GetLastError());
		assert(hwnd == _hwnd);
	}

	~DockContainer()
	{
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

		DockContainer* window;
		if (uMsg == WM_NCCREATE)
		{
			LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
			window = reinterpret_cast<DockContainer*>(lpcs->lpCreateParams);
			window->AddRef();
			window->_hwnd = hwnd;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
		}
		else
			window = reinterpret_cast<DockContainer*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

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
		if (msg == WM_SIZE)
		{
			auto contentRect = LayOutPanels (nullptr, true);
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
			case Side::Left:
				panelRect = { contentRect->left, contentRect->top, contentRect->left + proposedSize.cx, contentRect->bottom };
				contentRect->left += proposedSize.cx;
				return panelRect;

			case Side::Right:
				panelRect = { contentRect->right - proposedSize.cx, contentRect->top, contentRect->right, contentRect->bottom };
				contentRect->right -= proposedSize.cx;
				return panelRect;

			case Side::Top:
				panelRect = { contentRect->left, contentRect->top, contentRect->right, contentRect->top + proposedSize.cy };
				contentRect->top += proposedSize.cy;
				return panelRect;

			default:
				throw not_implemented_exception();

		}
	}

	RECT LayOutPanels (function<SIZE(IDockablePanel*)> getProposedSize, bool movePanelWindows) const
	{
		RECT contentRect;
		BOOL bRes = ::GetClientRect(_hwnd, &contentRect); ThrowWin32IfFailed(bRes);

		for (auto& panel : _dockablePanels)
		{
			if (GetWindowLongPtr (panel->GetHWnd(), GWL_STYLE) & WS_VISIBLE)
			{
				SIZE proposedSize = (getProposedSize != nullptr) ? getProposedSize(panel) : panel->GetWindowSize();
				RECT panelRect = LayOutPanel (panel->GetSide(), proposedSize, &contentRect);
				if (movePanelWindows)
					::MoveWindow(panel->GetHWnd(), panelRect.left, panelRect.top, panelRect.right - panelRect.left, panelRect.bottom - panelRect.top, TRUE);
			}
		}

		return contentRect;
	}

	void LayOutContent (const RECT& contentRect)
	{
		auto isContentHWnd = [this](HWND c)
		{
			return none_of(_dockablePanels.begin(), _dockablePanels.end(), [c](auto& sp) { return sp->GetHWnd() == c; });
		};

		auto content = FindChild(isContentHWnd);
		if (content != nullptr)
			::MoveWindow (content, contentRect.left, contentRect.top, contentRect.right - contentRect.left, contentRect.bottom - contentRect.top, TRUE);
	}

	virtual RECT GetContentRect() const override final
	{
		return LayOutPanels(nullptr, nullptr);
	}

	virtual IDockablePanel* GetPanel (const char* panelUniqueName) const override final
	{
		auto it = find_if(_dockablePanels.begin(), _dockablePanels.end(),
						  [panelUniqueName](auto& p) { return p->GetUniqueName() == panelUniqueName; });
		if (it == _dockablePanels.end())
			throw invalid_argument("not found");

		return *it;
	}

	virtual IDockablePanel* CreatePanel (const char* panelUniqueName, Side side, const wchar_t* title) override final
	{
		auto it = find_if(_dockablePanels.begin(), _dockablePanels.end(),
						  [panelUniqueName](auto& p) { return p->GetUniqueName() == panelUniqueName; });
		if (it != _dockablePanels.end())
			throw invalid_argument ("Panel already exists");

		static constexpr SIZE DefaultPanelSize = { 300, 300 };
		auto contentRect = LayOutPanels(nullptr, nullptr);
		RECT panelRect = LayOutPanel (side, DefaultPanelSize, &contentRect);

		LayOutContent (contentRect);

		auto panel = dockablePanelFactory (_hInstance, panelUniqueName, _hwnd, panelRect, side, title);
		panel->GetVisibleChangedEvent().AddHandler (&OnPanelVisibleChanged, this);
		panel->GetSplitterDraggingEvent().AddHandler (&OnSidePanelSplitterDragging, this);
		_dockablePanels.push_back(panel);
		return panel;
	}

	static void OnPanelVisibleChanged (void* callbackArg, IDockablePanel* panel, bool visible)
	{
		auto container = static_cast<DockContainer*>(callbackArg);
		auto contentRect = container->LayOutPanels (nullptr, true);
		container->LayOutContent (contentRect);
	}

	static void OnSidePanelSplitterDragging (void* callbackArg, IDockablePanel* panel, SIZE proposedSize)
	{
		auto container = static_cast<DockContainer*>(callbackArg);

		auto getProposedSize = [panel, proposedSize](IDockablePanel* p) { return (p != panel) ? p->GetWindowSize() : proposedSize; };
		auto contentRect = container->LayOutPanels(getProposedSize, true);
		container->LayOutContent(contentRect);
	}

	virtual void ResizePanel (IDockablePanel* panel, SIZE size) override final
	{
		auto getProposedSize = [panel, proposedSize=size](IDockablePanel* p) { return (p != panel) ? p->GetWindowSize() : proposedSize; };
		auto contentRect = LayOutPanels(getProposedSize, true);
		LayOutContent(contentRect);
	}

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

template<typename... Args>
static IDockContainerPtr Create (Args... args)
{
	return IDockContainerPtr(new DockContainer(std::forward<Args>(args)...), false);
}

extern const DockContainerFactory dockContainerFactory = &Create;
