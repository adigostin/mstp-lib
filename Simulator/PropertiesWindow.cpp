
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

	ISelection* const _selection;
	IProjectWindow* const _projectWindow;
	IProject* const _project;
	unique_ptr<property_grid_i> _pg;
	unique_ptr<property_grid_i> _pg_tree;

public:
	PropertiesWindow (ISimulatorApp* app,
					  IProjectWindow* projectWindow,
					  IProject* project,
					  ISelection* selection,
					  const RECT& rect,
					  HWND hWndParent,
					  ID3D11DeviceContext1* d3d_dc,
					  IDWriteFactory* dwrite_factory)
		: base (app->GetHInstance(), WS_EX_CLIENTEDGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, rect, hWndParent, 0)
		, _projectWindow(projectWindow)
		, _project(project)
		, _selection(selection)
		, _pg(property_grid_factory(app->GetHInstance(), client_rect_pixels(), hwnd(), d3d_dc, dwrite_factory))
		, _pg_tree(property_grid_factory(app->GetHInstance(), client_rect_pixels(), hwnd(), d3d_dc, dwrite_factory))
	{
		this->SetSelectionToPGs();

		_projectWindow->GetSelectedVlanNumerChangedEvent().add_handler (&OnSelectedVlanChanged, this);
		_selection->GetChangedEvent().add_handler (&OnSelectionChanged, this);
		_project->GetChangedEvent().add_handler (&OnProjectChanged, this);
	}

	virtual ~PropertiesWindow()
	{
		_project->GetChangedEvent().remove_handler (&OnProjectChanged, this);
		_selection->GetChangedEvent().remove_handler (&OnSelectionChanged, this);
		_projectWindow->GetSelectedVlanNumerChangedEvent().remove_handler (&OnSelectedVlanChanged, this);
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

		assert(false);
		/*
		wstringstream ss;
		ss << L"VLAN " << _projectWindow->GetSelectedVlanNumber() << L" Specific Properties";
		auto vlanPropsHeading = ss.str();

		if (all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), [](Object* o) { return o->Is<Bridge>(); }))
		{
			_pg->AddProperties(_selection->GetObjects().data(), _selection->GetObjects().size(), L"Bridge Properties");

			std::vector<BridgeTree*> bridgeTrees;
			for (Object* o : _selection->GetObjects())
			{
				auto b = static_cast<Bridge*>(o);
				auto treeIndex = STP_GetTreeIndexFromVlanNumber(b->GetStpBridge(), _projectWindow->GetSelectedVlanNumber());
				bridgeTrees.push_back (b->GetTrees().at(treeIndex).get());
			}

			_pg->AddProperties ((Object**) &bridgeTrees[0], bridgeTrees.size(), vlanPropsHeading.c_str());
		}
		else if (all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), [](Object* o) { return o->Is<Port>(); }))
		{
			_pg->AddProperties(_selection->GetObjects().data(), _selection->GetObjects().size(), L"Port Properties");

			std::vector<PortTree*> portTrees;
			for (Object* o : _selection->GetObjects())
			{
				auto p = static_cast<Port*>(o);
				auto treeIndex = STP_GetTreeIndexFromVlanNumber(p->GetBridge()->GetStpBridge(), _projectWindow->GetSelectedVlanNumber());
				portTrees.push_back (p->GetTrees().at(treeIndex).get());
			}

			_pg->AddProperties ((Object**) &portTrees[0], portTrees.size(), vlanPropsHeading.c_str());
		}
		else if (all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), [](Object* o) { return o->Is<Wire>(); }))
		{
			_pg->AddProperties(_selection->GetObjects().data(), _selection->GetObjects().size(), L"Wire Properties");
		}
		else
			assert(false); // not implemented
		*/
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

	optional<LRESULT> window_proc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) final
	{
		auto resultBaseClass = base::window_proc (hwnd, msg, wParam, lParam);

		if (msg == WM_SIZE)
		{
			if (_pg != nullptr)
			{
				auto rect = this->client_rect_pixels();
				BOOL bRes = ::MoveWindow (_pg->hwnd(), rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE); assert(bRes);
			}

			return 0;
		}

		return resultBaseClass;
	}

	HWND hwnd() const final { return base::hwnd(); }
};

const PropertiesWindowFactory propertiesWindowFactory = &PropertiesWindow::Create;
