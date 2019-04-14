
#include "pch.h"
#include "simulator.h"
#include "resource.h"
#include "Bridge.h"
#include "Port.h"

class vlan_window : public virtual vlan_window_i, public edge::property_editor_parent_i
{
	simulator_app_i*  const _app;
	project_window_i* const _pw;
	std::shared_ptr<project_i> const _project;
	selection_i*    const _selection;
	ID3D11DeviceContext1* const _d3d_dc;
	IDWriteFactory* const _dwrite_factory;
	HWND _hwnd = nullptr;

public:
	vlan_window (simulator_app_i* app,
				project_window_i* pw,
				const std::shared_ptr<project_i>& project,
				selection_i* selection,
				HWND hWndParent,
				POINT location,
				ID3D11DeviceContext1* d3d_dc,
				IDWriteFactory* dwrite_factory)
		: _app(app)
		, _pw(pw)
		, _project(project)
		, _selection(selection)
		, _d3d_dc(d3d_dc)
		, _dwrite_factory(dwrite_factory)
	{
		HINSTANCE hInstance;
		BOOL bRes = GetModuleHandleEx (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR) &DialogProcStatic, &hInstance); assert(bRes);

		_hwnd = CreateDialogParam (hInstance, MAKEINTRESOURCE(IDD_DIALOG_VLAN), hWndParent, &DialogProcStatic, reinterpret_cast<LPARAM>(this));

		RECT rc;
		::GetWindowRect(_hwnd, &rc);
		::MoveWindow (_hwnd, location.x, location.y, rc.right - rc.left, rc.bottom - rc.top, TRUE);

		_selection->added().add_handler (&OnAddedToSelection, this);
		_selection->removing().add_handler (&OnRemovingFromSelection, this);
		_selection->changed().add_handler (&OnSelectionChanged, this);
		_pw->selected_vlan_number_changed().add_handler (&on_selected_vlan_changed, this);

		for (auto o : _selection->objects())
		{
			if (auto b = dynamic_cast<Bridge*>(o); b != nullptr)
				b->property_changed().add_handler (&OnBridgePropertyChanged, this);
		}
	}

	~vlan_window()
	{
		for (auto o : _selection->objects())
		{
			if (auto b = dynamic_cast<Bridge*>(o); b != nullptr)
				b->property_changed().remove_handler (&OnBridgePropertyChanged, this);
		}

		_pw->selected_vlan_number_changed().remove_handler (&on_selected_vlan_changed, this);
		_selection->changed().remove_handler (&OnSelectionChanged, this);
		_selection->removing().remove_handler (&OnRemovingFromSelection, this);
		_selection->added().remove_handler (&OnAddedToSelection, this);

		if (_hwnd != nullptr)
			::DestroyWindow(_hwnd);
	}

	virtual destroying_event::subscriber destroying() final
	{
		assert(false); return nullptr;
	}

	virtual HWND hwnd() const override final { return _hwnd; }

	virtual SIZE preferred_size() const override final
	{
		RECT rect;
		::GetWindowRect(GetDlgItem(_hwnd, IDC_STATIC_EXTENT), &rect);
		if (auto proc_addr = GetProcAddress(GetModuleHandleA("User32.dll"), "AdjustWindowRectExForDpi"))
		{
			auto get_dpi_proc_addr = GetProcAddress(GetModuleHandleA("User32.dll"), "GetDpiForWindow");
			auto get_dpi_proc = reinterpret_cast<UINT(WINAPI*)(HWND)>(get_dpi_proc_addr);
			UINT dpi = get_dpi_proc(_hwnd);

			auto proc = reinterpret_cast<BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT)>(proc_addr);
			BOOL bRes = proc (&rect, GetWindowStyle(_hwnd), FALSE, GetWindowExStyle(_hwnd), dpi); assert(bRes);
			return { rect.right - rect.left, rect.bottom - rect.top };
		}
		else
		{
			HDC tempDC = GetDC(hwnd());
			UINT dpi = GetDeviceCaps (tempDC, LOGPIXELSX);
			ReleaseDC (hwnd(), tempDC);
			BOOL bRes = AdjustWindowRectEx (&rect, GetWindowStyle(_hwnd), FALSE, GetWindowExStyle(_hwnd)); assert(bRes);
			return { rect.right - rect.left, rect.bottom - rect.top };
		}
	}

	static INT_PTR CALLBACK DialogProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		vlan_window* window;
		if (uMsg == WM_INITDIALOG)
		{
			window = reinterpret_cast<vlan_window*>(lParam);
			//window->AddRef();
			window->_hwnd = hwnd;
			assert (GetWindowLongPtr(hwnd, GWLP_USERDATA) == 0);
			SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
		}
		else
			window = reinterpret_cast<vlan_window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

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
			for (size_t i = 1; i <= max_vlan_number; i++)
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
				if (std::all_of (_selection->objects().begin(), _selection->objects().end(), [](edge::object* o) { return o->is<Bridge>(); }))
				{
					auto editor = config_id_editor_factory(_selection->objects());
					editor->show(this);
				}
				else if (std::all_of (_selection->objects().begin(), _selection->objects().end(), [](edge::object* o) { return o->is<Port>(); }))
				{
					std::vector<edge::object*> objects;
					std::transform (_selection->objects().begin(), _selection->objects().end(), std::back_inserter(objects),
									[](edge::object* o) { return (edge::object*) static_cast<Port*>(o)->bridge(); });
					auto editor = config_id_editor_factory(objects);
					editor->show(this);
				}
				else
					MessageBoxA (_hwnd, "Select some bridges or ports first.", _app->app_name(), 0);

				return { TRUE, 0 };
			}

			return { FALSE, 0 };
		}

		return { FALSE, 0 };
	}

	static void OnAddedToSelection (void* callbackArg, selection_i* selection, edge::object* obj)
	{
		auto b = dynamic_cast<Bridge*>(obj);
		if (b != nullptr)
			b->property_changed().add_handler (&OnBridgePropertyChanged, callbackArg);
	}

	static void OnRemovingFromSelection (void* callbackArg, selection_i* selection, edge::object* obj)
	{
		auto b = dynamic_cast<Bridge*>(obj);
		if (b != nullptr)
			b->property_changed().remove_handler (&OnBridgePropertyChanged, callbackArg);
	}

	static void OnBridgePropertyChanged (void* callbackArg, edge::object* o, const edge::property_change_args& args)
	{
		static_cast<vlan_window*>(callbackArg)->LoadSelectedTreeEdit();
	}

	static void OnSelectionChanged (void* callbackArg, selection_i* selection)
	{
		static_cast<vlan_window*>(callbackArg)->LoadSelectedTreeEdit();
	}

	static void on_selected_vlan_changed (void* callbackArg, project_window_i* pw, unsigned int vlanNumber)
	{
		static_cast<vlan_window*>(callbackArg)->LoadSelectedTreeEdit();
	}

	void ProcessVlanSelChange (HWND hwnd)
	{
		int index = ComboBox_GetCurSel(hwnd);
		_pw->select_vlan(index + 1);
	}

	void ProcessNewWindowVlanSelChange (HWND hwnd)
	{
		auto index = ComboBox_GetCurSel(hwnd);
		auto vlanNumber = (unsigned int) (index + 1);
		auto& pws = _app->project_windows();
		auto it = find_if (pws.begin(), pws.end(), [this, vlanNumber](auto& pw)
			{ return (pw->project() == _pw->project()) && (pw->selected_vlan_number() == vlanNumber); });
		if (it != pws.end())
		{
			::BringWindowToTop (it->get()->hwnd());
			::FlashWindow (it->get()->hwnd(), FALSE);
		}
		else
		{
			project_window_create_params create_params = 
				{ _app, _project, selection_factory, edit_window_factory, false, false, vlanNumber, SW_SHOW, _d3d_dc, _dwrite_factory };
			auto pw = projectWindowFactory (create_params);
			_app->add_project_window(move(pw));
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
		auto& objects = _selection->objects();

		if (objects.empty() || std::any_of (objects.begin(), objects.end(), [](edge::object* o) { return !o->is<Bridge>() && !o->is<Port>(); }))
		{
			::SetWindowText (edit, L"(no bridge selected)");
			::EnableWindow (edit, FALSE);
			::EnableWindow (tableButton, FALSE);
			return;
		}

		::EnableWindow (edit, TRUE);
		::EnableWindow (tableButton, TRUE);

		std::unordered_set<Bridge*> bridges;
		for (auto o : _selection->objects())
		{
			if (auto b = dynamic_cast<Bridge*>(o))
				bridges.insert(b);
			else if (auto p = dynamic_cast<Port*>(o))
				bridges.insert(p->bridge());
			else
				assert(false);
		}

		auto tree = STP_GetTreeIndexFromVlanNumber ((*bridges.begin())->stp_bridge(), _pw->selected_vlan_number());
		bool all_same_tree = all_of (bridges.begin(), bridges.end(), [tree, vlan=_pw->selected_vlan_number()](Bridge* b)
			{ return STP_GetTreeIndexFromVlanNumber(b->stp_bridge(), vlan) == tree; });
		if (!all_same_tree)
		{
			::SetWindowTextA (edit, "(multiple selection)");
			return;
		}

		Bridge* bridge = *bridges.begin();
		auto treeIndex = STP_GetTreeIndexFromVlanNumber (bridge->stp_bridge(), _pw->selected_vlan_number());
		if (treeIndex == 0)
			::SetWindowTextA (edit, "CIST (0)");
		else
			::SetWindowTextA (edit, (std::string("MSTI ") + std::to_string(treeIndex)).c_str());
	}
};

template<typename... Args>
static std::unique_ptr<vlan_window_i> create (Args... args)
{
	return std::make_unique<vlan_window> (std::forward<Args>(args)...);
}

const vlan_window_factory_t vlan_window_factory = &create;
