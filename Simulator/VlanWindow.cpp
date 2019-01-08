
#include "pch.h"
#include "Simulator.h"
#include "Resource.h"
#include "Bridge.h"
#include "Port.h"

class VlanWindow : public virtual IVlanWindow
{
	ISimulatorApp*  const _app;
	IProjectWindow* const _pw;
	std::shared_ptr<IProject> const _project;
	ISelection*    const _selection;
	HWND _hwnd = nullptr;

public:
	VlanWindow (ISimulatorApp* app,
				IProjectWindow* pw,
				const std::shared_ptr<IProject>& project,
				ISelection* selection,
				HWND hWndParent,
				POINT location)
		: _app(app)
		, _pw(pw)
		, _project(project)
		, _selection(selection)
	{
		HINSTANCE hInstance;
		BOOL bRes = GetModuleHandleEx (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR) &DialogProcStatic, &hInstance); assert(bRes);

		_hwnd = CreateDialogParam (hInstance, MAKEINTRESOURCE(IDD_DIALOG_VLAN), hWndParent, &DialogProcStatic, reinterpret_cast<LPARAM>(this));

		RECT rc;
		::GetWindowRect(_hwnd, &rc);
		::MoveWindow (_hwnd, location.x, location.y, rc.right - rc.left, rc.bottom - rc.top, TRUE);

		_selection->GetAddedToSelectionEvent().add_handler (&OnAddedToSelection, this);
		_selection->GetRemovingFromSelectionEvent().add_handler (&OnRemovingFromSelection, this);
		_selection->GetChangedEvent().add_handler (&OnSelectionChanged, this);
		_pw->GetSelectedVlanNumerChangedEvent().add_handler (&OnSelectedVlanChanged, this);

		for (auto o : _selection->GetObjects())
		{
			if (auto b = dynamic_cast<Bridge*>(o); b != nullptr)
				b->property_changed().add_handler (&OnBridgePropertyChanged, this);
		}
	}

	~VlanWindow()
	{
		for (auto o : _selection->GetObjects())
		{
			if (auto b = dynamic_cast<Bridge*>(o); b != nullptr)
				b->property_changed().remove_handler (&OnBridgePropertyChanged, this);
		}

		_pw->GetSelectedVlanNumerChangedEvent().remove_handler (&OnSelectedVlanChanged, this);
		_selection->GetChangedEvent().remove_handler (&OnSelectionChanged, this);
		_selection->GetRemovingFromSelectionEvent().remove_handler (&OnRemovingFromSelection, this);
		_selection->GetAddedToSelectionEvent().remove_handler (&OnAddedToSelection, this);

		if (_hwnd != nullptr)
			::DestroyWindow(_hwnd);
	}

	virtual destroying_event::subscriber destroying() final
	{
		assert(false); return nullptr;
	}

	virtual HWND hwnd() const override final { return _hwnd; }

	static INT_PTR CALLBACK DialogProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		VlanWindow* window;
		if (uMsg == WM_INITDIALOG)
		{
			window = reinterpret_cast<VlanWindow*>(lParam);
			//window->AddRef();
			window->_hwnd = hwnd;
			assert (GetWindowLongPtr(hwnd, GWLP_USERDATA) == 0);
			SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
		}
		else
			window = reinterpret_cast<VlanWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

		if (window == nullptr)
		{
			// this must be one of those messages sent before WM_NCCREATE or after WM_NCDESTROY.
			return DefWindowProc (hwnd, uMsg, wParam, lParam);
		}

		DialogProcResult result = window->DialogProc (uMsg, wParam, lParam);

		if (uMsg == WM_NCDESTROY)
		{
			window->_hwnd = nullptr;
			SetWindowLongPtr (hwnd, GWLP_USERDATA, 0);
			//window->Release(); // this one last cause it might call Release() which would try to destroy the hwnd again.
		}

		::SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result.messageResult);
		return result.dialogProcResult;
	}

	DialogProcResult DialogProc (UINT msg, WPARAM wParam , LPARAM lParam)
	{
		if (msg == WM_INITDIALOG)
		{
			auto comboSelectedVlan  = GetDlgItem (_hwnd, IDC_COMBO_SELECTED_VLAN);
			auto comboNewWindowVlan = GetDlgItem (_hwnd, IDC_COMBO_NEW_WINDOW_VLAN);
			for (size_t i = 1; i <= MaxVlanNumber; i++)
			{
				auto str = std::to_wstring(i);
				ComboBox_AddString(comboSelectedVlan, str.c_str());
				ComboBox_AddString(comboNewWindowVlan, str.c_str());
			}
			LoadSelectedVlanCombo();
			LoadSelectedTreeEdit();
			return { FALSE, 0 };
		}

		if (msg == WM_CTLCOLORDLG)
			return { reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW)), 0 };

		if (msg == WM_CTLCOLORSTATIC)
		{
			wchar_t className[32];
			GetClassName ((HWND) lParam, className, _countof(className));
			if ((_wcsicmp(className, L"EDIT") == 0) && (GetWindowLongPtr((HWND) lParam, GWL_STYLE) & ES_READONLY))
			{
				SetBkMode ((HDC) wParam, TRANSPARENT);
				return { reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_3DFACE)), 0 };
			}

			return { reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW)), 0 };
		}

		if (msg == WM_COMMAND)
		{
			if ((HIWORD(wParam) == CBN_SELCHANGE) && (LOWORD(wParam) == IDC_COMBO_SELECTED_VLAN))
			{
				ProcessVlanSelChange ((HWND) lParam);
				return { TRUE, 0 };
			}

			if ((HIWORD(wParam) == CBN_SELCHANGE) && (LOWORD(wParam) == IDC_COMBO_NEW_WINDOW_VLAN))
			{
				ProcessNewWindowVlanSelChange ((HWND) lParam);
				return { TRUE, 0 };
			}

			if ((HIWORD(wParam) == BN_CLICKED) && (LOWORD(wParam) == IDC_BUTTON_EDIT_MST_CONFIG_TABLE))
			{
				assert(false);
				//auto dialog = mstConfigIdDialogFactory(_selection->GetObjects());
				//dialog->ShowModal(_pw->GetHWnd());
				return { TRUE, 0 };
			}

			return { FALSE, 0 };
		}

		return { FALSE, 0 };
	}

	static void OnAddedToSelection (void* callbackArg, ISelection* selection, edge::object* obj)
	{
		auto b = dynamic_cast<Bridge*>(obj);
		if (b != nullptr)
			b->property_changed().add_handler (&OnBridgePropertyChanged, callbackArg);
	}

	static void OnRemovingFromSelection (void* callbackArg, ISelection* selection, edge::object* obj)
	{
		auto b = dynamic_cast<Bridge*>(obj);
		if (b != nullptr)
			b->property_changed().remove_handler (&OnBridgePropertyChanged, callbackArg);
	}

	static void OnBridgePropertyChanged (void* callbackArg, edge::object* o, const edge::property* property)
	{
		static_cast<VlanWindow*>(callbackArg)->LoadSelectedTreeEdit();
	}

	static void OnSelectionChanged (void* callbackArg, ISelection* selection)
	{
		static_cast<VlanWindow*>(callbackArg)->LoadSelectedTreeEdit();
	}

	static void OnSelectedVlanChanged (void* callbackArg, IProjectWindow* pw, unsigned int vlanNumber)
	{
		static_cast<VlanWindow*>(callbackArg)->LoadSelectedTreeEdit();
	}

	void ProcessVlanSelChange (HWND hwnd)
	{
		int index = ComboBox_GetCurSel(hwnd);
		_pw->SelectVlan(index + 1);
	}

	void ProcessNewWindowVlanSelChange (HWND hwnd)
	{
		auto index = ComboBox_GetCurSel(hwnd);
		auto vlanNumber = (unsigned int) (index + 1);
		auto& pws = _app->GetProjectWindows();
		auto it = find_if (pws.begin(), pws.end(), [this, vlanNumber](auto& pw)
			{ return (pw->GetProject() == _pw->GetProject()) && (pw->selected_vlan_number() == vlanNumber); });
		if (it != pws.end())
		{
			::BringWindowToTop (it->get()->hwnd());
			::FlashWindow (it->get()->hwnd(), FALSE);
		}
		else
		{
			assert(false);
			//project_window_create_params create_params = 
			//	{ _app, _project, selectionFactory, edit_area_factory, false, false, vlanNumber, SW_SHOW, _d3d_dc, _dwrite_factory };
			//auto pw = projectWindowFactory (create_params);
			//_app->AddProjectWindow(move(pw));
		}

		ComboBox_SetCurSel (hwnd, -1);
	}

	void LoadSelectedVlanCombo()
	{
		ComboBox_SetCurSel (GetDlgItem (_hwnd, IDC_COMBO_SELECTED_VLAN), _pw->selected_vlan_number() - 1);
	}

	void LoadSelectedTreeEdit()
	{
		auto edit = GetDlgItem (_hwnd, IDC_EDIT_SELECTED_TREE); assert (edit != nullptr);
		auto tableButton = GetDlgItem (_hwnd, IDC_BUTTON_EDIT_MST_CONFIG_TABLE); assert (tableButton != nullptr);
		auto& objects = _selection->GetObjects();

		if (objects.empty())
		{
			::SetWindowText (edit, L"(no selection)");
			::EnableWindow (edit, FALSE);
			::EnableWindow (tableButton, FALSE);
			return;
		}

		if (objects.size() > 1)
		{
			::SetWindowText (edit, L"(multiple selection)");
			::EnableWindow (edit, FALSE);
			::EnableWindow (tableButton, FALSE);
			return;
		}

		if (objects[0]->is<Bridge>() || objects[0]->is<Port>())
		{
			auto obj = objects[0];
			auto bridge = obj->is<Bridge>() ? dynamic_cast<Bridge*>(obj) : dynamic_cast<Port*>(obj)->bridge();
			auto treeIndex = STP_GetTreeIndexFromVlanNumber (bridge->stp_bridge(), _pw->selected_vlan_number());
			if (treeIndex == 0)
				::SetWindowText (edit, L"CIST (0)");
			else
				::SetWindowText (edit, (std::wstring(L"MSTI ") + std::to_wstring(treeIndex)).c_str());
			::EnableWindow (edit, TRUE);
			::EnableWindow (tableButton, TRUE);
			return;
		}

		::SetWindowText (edit, L"(no selection)");
		::EnableWindow (edit, FALSE);
		::EnableWindow (tableButton, FALSE);
	}
};

template<typename... Args>
static std::unique_ptr<IVlanWindow> Create (Args... args)
{
	return std::make_unique<VlanWindow> (std::forward<Args>(args)...);
}

const VlanWindowFactory vlanWindowFactory = &Create;
