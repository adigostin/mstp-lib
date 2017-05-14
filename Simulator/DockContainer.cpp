
#include "pch.h"
#include "Simulator.h"
#include "Window.h"

using namespace std;

static ATOM wndClassAtom;
static constexpr wchar_t DockContainerClassName[] = L"DockContainer-{24B42526-2970-4B3C-A753-2DABD22C4BB0}";

class DockContainer : public Window, public IDockContainer
{
	using base = Window;

	HINSTANCE const _hInstance;
	vector<IDockablePanelPtr> _dockablePanels;

public:
	DockContainer (HINSTANCE hInstance, HWND hWndParent, const RECT& rect)
		: base (hInstance, DockContainerClassName, 0, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, rect, hWndParent, nullptr)
		, _hInstance(hInstance)
	{ }

	virtual std::optional<LRESULT> WindowProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override
	{
		auto resultBaseClass = base::WindowProc (hwnd, msg, wParam, lParam);

		if (msg == WM_SIZE)
		{
			auto contentRect = LayOutPanels (nullptr, true);
			LayOutContent(contentRect);
			return 0;
		}

		return resultBaseClass;
	}

	HWND FindChild (std::function<bool(HWND child)> predicate)
	{
		auto child = ::GetWindow(GetHWnd(), GW_CHILD);
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
		BOOL bRes = ::GetClientRect(GetHWnd(), &contentRect); ThrowWin32IfFailed(bRes);

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

		auto panel = dockablePanelFactory (_hInstance, panelUniqueName, GetHWnd(), panelRect, side, title);
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

	virtual HWND GetHWnd() const { return base::GetHWnd(); }
	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override { return base::QueryInterface(riid, ppvObject); }
	virtual ULONG STDMETHODCALLTYPE AddRef() override { return base::AddRef(); }
	virtual ULONG STDMETHODCALLTYPE Release() override { return base::Release(); }
};

template<typename... Args>
static IDockContainerPtr Create (Args... args)
{
	return IDockContainerPtr(new DockContainer(std::forward<Args>(args)...), false);
}

extern const DockContainerFactory dockContainerFactory = &Create;
