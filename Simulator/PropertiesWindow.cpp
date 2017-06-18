
#include "pch.h"
#include "Win32/Window.h"
#include "Simulator.h"
#include "PropertyGrid.h"

using namespace std;

static constexpr wchar_t ClassName[] = L"PropertiesWindow-{4AA600D1-E399-4A83-B04B-1B1842A6F738}";

class PropertiesWindow : public Window, public IPropertiesWindow
{
	using base = Window;

	unique_ptr<PropertyGrid> _pg;
	unique_ptr<PropertyGrid> _pgTree;

public:
	PropertiesWindow (ISimulatorApp* app,
					  IProjectWindow* projectWindow,
					  IProject* project,
					  ISelection* selection,
					  const RECT& rect,
					  HWND hWndParent)
		: base (app->GetHInstance(), ClassName, WS_EX_CLIENTEDGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, rect, hWndParent, nullptr)
		, _pg (new PropertyGrid(app->GetHInstance(), { rect.left, rect.top, rect.right, rect.bottom / 2 }, GetHWnd(), app->GetDWriteFactory(), projectWindow))
		, _pgTree (new PropertyGrid(app->GetHInstance(), { rect.left, rect.bottom / 2, rect.right, rect.bottom }, GetHWnd(), app->GetDWriteFactory(), projectWindow))
	{ }

	template<typename... Args>
	static IPropertiesWindowPtr Create (Args... args)
	{
		return IPropertiesWindowPtr (new PropertiesWindow(std::forward<Args>(args)...), false);
	}

	virtual optional<LRESULT> WindowProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override
	{
		auto resultBaseClass = base::WindowProc (hwnd, msg, wParam, lParam);

		if (msg == WM_SIZE)
		{
			int width = LOWORD(lParam);
			int height = HIWORD(lParam);

			if (_pg != nullptr)
			{
				::MoveWindow (_pg->GetHWnd(), 0, 0, width, height / 2, TRUE);
				::MoveWindow (_pgTree->GetHWnd(), 0, height / 2, width, height / 2, TRUE);
			}

			return 0;
		}

		return resultBaseClass;
	}

	virtual PropertyGrid* GetPG() const override final { return _pg.get(); }
	virtual PropertyGrid* GetPGTree() const override final { return _pgTree.get(); }

	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override { return base::QueryInterface(riid, ppvObject); }
	virtual ULONG STDMETHODCALLTYPE AddRef() override final { return base::AddRef(); }
	virtual ULONG STDMETHODCALLTYPE Release() override final { return base::Release(); }
	virtual HWND GetHWnd() const override final { return base::GetHWnd(); }
};

const PropertiesWindowFactory propertiesWindowFactory = &PropertiesWindow::Create;
