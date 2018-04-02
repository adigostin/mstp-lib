
#include "pch.h"
#include "Simulator.h"
#include "PropertyGrid.h"
#include "Bridge.h"
#include "Wire.h"

using namespace std;

static const wchar_t WndClassName[] = L"PropertiesWindow-{6ED5A45A-9BF5-4EA2-9F43-4EFEDC11994E}";

class PropertiesWindow : public Window, public IPropertiesWindow
{
	using base = Window;

	com_ptr<ISelection> const _selection;
	IProjectWindow* const _projectWindow;
	com_ptr<IProject> const _project;
	unique_ptr<PropertyGrid> _pg;

public:
	PropertiesWindow (ISimulatorApp* app,
					  IProjectWindow* projectWindow,
					  IProject* project,
					  ISelection* selection,
					  const RECT& rect,
					  HWND hWndParent)
		: base (app->GetHInstance(), WndClassName, WS_EX_CLIENTEDGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, rect, hWndParent, nullptr)
		, _projectWindow(projectWindow)
		, _project(project)
		, _selection(selection)
	{
		_pg.reset (new PropertyGrid(app->GetHInstance(), this->base::GetClientRectPixels(), GetHWnd(), app->GetDWriteFactory()));
		_pg->GetPropertyChangedByUserEvent().AddHandler (&OnPropertyChangedInPG, this);
		this->SetSelectionToPGs();

		_projectWindow->GetSelectedVlanNumerChangedEvent().AddHandler (&OnSelectedVlanChanged, this);
		_selection->GetChangedEvent().AddHandler (&OnSelectionChanged, this);
		_project->GetChangedEvent().AddHandler (&OnProjectChanged, this);
	}

	virtual ~PropertiesWindow()
	{
		_project->GetChangedEvent().RemoveHandler (&OnProjectChanged, this);
		_selection->GetChangedEvent().RemoveHandler (&OnSelectionChanged, this);
		_projectWindow->GetSelectedVlanNumerChangedEvent().RemoveHandler (&OnSelectedVlanChanged, this);

		_pg->GetPropertyChangedByUserEvent().RemoveHandler (&OnPropertyChangedInPG, this);
	}

	template<typename... Args>
	static com_ptr<IPropertiesWindow> Create (Args... args)
	{
		return com_ptr<IPropertiesWindow> (new PropertiesWindow(std::forward<Args>(args)...), false);
	}

	void SetSelectionToPGs()
	{
		_pg->ClearProperties();

		if (_selection->GetObjects().empty())
			return;

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

	static void OnPropertyChangedInPG (void* callbackArg, const Property* property)
	{
		auto pw = static_cast<PropertiesWindow*>(callbackArg);
		pw->_project->SetChangedFlag(true);
	}

	virtual optional<LRESULT> WindowProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override
	{
		auto resultBaseClass = base::WindowProc (hwnd, msg, wParam, lParam);

		if (msg == WM_SIZE)
		{
			if (_pg != nullptr)
				_pg->SetRect(this->base::GetClientRectPixels());
			return 0;
		}

		return resultBaseClass;
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override { return base::QueryInterface(riid, ppvObject); }
	virtual ULONG STDMETHODCALLTYPE AddRef() override final { return base::AddRef(); }
	virtual ULONG STDMETHODCALLTYPE Release() override final { return base::Release(); }
	virtual HWND GetHWnd() const override final { return base::GetHWnd(); }
};

const PropertiesWindowFactory propertiesWindowFactory = &PropertiesWindow::Create;
