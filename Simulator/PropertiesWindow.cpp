
#include "pch.h"
#include "Simulator.h"
#include "Bridge.h"
#include "Wire.h"
#include "win32/property_grid.h"
#include "win32/window.h"

using namespace std;
using namespace edge;

static const wchar_t WndClassName[] = L"PropertiesWindow-{6ED5A45A-9BF5-4EA2-9F43-4EFEDC11994E}";

#pragma warning (disable: 4250)

class PropertiesWindow : public window, public virtual IPropertiesWindow
{
	using base = window;

	ISelection*     const _selection;
	IProjectWindow* const _project_window;
	IProject*       const _project;
	unique_ptr<property_grid_i> _pg;
	unique_ptr<property_grid_i> _pg_tree;

public:
	PropertiesWindow (simulator_app_i* app,
					  IProjectWindow* projectWindow,
					  IProject* project,
					  ISelection* selection,
					  const RECT& rect,
					  HWND hWndParent,
					  ID3D11DeviceContext1* d3d_dc,
					  IDWriteFactory* dwrite_factory)
		: base (app->GetHInstance(), WS_EX_CLIENTEDGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, rect, hWndParent, 0)
		, _project_window(projectWindow)
		, _project(project)
		, _selection(selection)
		, _pg(property_grid_factory(app->GetHInstance(), pg_rect(), hwnd(), d3d_dc, dwrite_factory))
		, _pg_tree(property_grid_factory(app->GetHInstance(), pg_tree_rect(), hwnd(), d3d_dc, dwrite_factory))
	{
		this->SetSelectionToPGs();

		_project_window->GetSelectedVlanNumerChangedEvent().add_handler (&OnSelectedVlanChanged, this);
		_selection->GetChangedEvent().add_handler (&OnSelectionChanged, this);
		_project->GetChangedEvent().add_handler (&OnProjectChanged, this);
	}

	virtual ~PropertiesWindow()
	{
		_project->GetChangedEvent().remove_handler (&OnProjectChanged, this);
		_selection->GetChangedEvent().remove_handler (&OnSelectionChanged, this);
		_project_window->GetSelectedVlanNumerChangedEvent().remove_handler (&OnSelectedVlanChanged, this);
	}

	template<typename... Args>
	static std::unique_ptr<IPropertiesWindow> Create (Args... args)
	{
		return std::make_unique<PropertiesWindow>(std::forward<Args>(args)...);
	}

	void SetSelectionToPGs()
	{
		const auto& objs = _selection->GetObjects();

		if (objs.empty())
		{
			_pg->select_objects (nullptr, 0);
			_pg_tree->select_objects (nullptr, 0);
			return;
		}

		stringstream ss;
		ss << "VLAN " << _project_window->selected_vlan_number() << " Specific Properties";
		auto pg_tree_title = ss.str();

		if (all_of (objs.begin(), objs.end(), [](object* o) { return o->is<Bridge>(); }))
		{
			_pg->set_title ("Bridge Properties");
			_pg->select_objects (objs.data(), objs.size());

			std::vector<object*> bridge_trees;
			for (object* o : objs)
			{
				auto b = static_cast<Bridge*>(o);
				auto tree_index = STP_GetTreeIndexFromVlanNumber(b->stp_bridge(), _project_window->selected_vlan_number());
				bridge_trees.push_back (b->trees().at(tree_index).get());
			}

			_pg_tree->set_title (pg_tree_title);
			_pg_tree->select_objects (bridge_trees.data(), bridge_trees.size());
		}
		else if (all_of (objs.begin(), objs.end(), [](object* o) { return o->is<Port>(); }))
		{
			_pg->set_title ("Port Properties");
			_pg->select_objects (objs.data(), objs.size());

			std::vector<object*> port_trees;
			for (object* o : objs)
			{
				auto p = static_cast<Port*>(o);
				auto tree_index = STP_GetTreeIndexFromVlanNumber(p->bridge()->stp_bridge(), _project_window->selected_vlan_number());
				port_trees.push_back (p->trees().at(tree_index).get());
			}

			_pg_tree->set_title (pg_tree_title);
			_pg_tree->select_objects (port_trees.data(), port_trees.size());
		}
		else if (all_of (objs.begin(), objs.end(), [](object* o) { return o->is<Wire>(); }))
		{
			_pg->set_title ("Wire Properties");
			_pg->select_objects (objs.data(), objs.size());

			_pg_tree->set_title (pg_tree_title);
			_pg_tree->select_objects (nullptr, 0);
		}
		else
			assert(false); // not implemented
	}

	static void OnProjectChanged (void* callbackArg, IProject* project)
	{
		auto window = static_cast<PropertiesWindow*>(callbackArg);
		window->SetSelectionToPGs();
	}

	static void OnSelectionChanged (void* callbackArg, ISelection* selection)
	{
		auto window = static_cast<PropertiesWindow*>(callbackArg);
		window->SetSelectionToPGs();
	}

	static void OnSelectedVlanChanged (void* callbackArg, IProjectWindow* pw, unsigned int selectedVlan)
	{
		auto window = static_cast<PropertiesWindow*>(callbackArg);
		window->SetSelectionToPGs();
	}

	RECT pg_rect() const
	{
		RECT rect = client_rect_pixels();
		rect.bottom /= 2;
		return rect;
	}

	RECT pg_tree_rect() const
	{
		RECT rect = client_rect_pixels();
		rect.top = rect.bottom / 2;
		return rect;
	}

	optional<LRESULT> window_proc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) final
	{
		auto resultBaseClass = base::window_proc (hwnd, msg, wParam, lParam);

		if (msg == WM_SIZE)
		{
			if (_pg != nullptr)
			{
				auto rect = pg_rect();
				BOOL bRes = ::MoveWindow (_pg->hwnd(), rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE); assert(bRes);
				rect = pg_tree_rect();
				bRes = ::MoveWindow (_pg_tree->hwnd(), rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE); assert(bRes);
			}

			return 0;
		}

		return resultBaseClass;
	}

	HWND hwnd() const final { return base::hwnd(); }
};

const PropertiesWindowFactory propertiesWindowFactory = &PropertiesWindow::Create;
