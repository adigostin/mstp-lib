
#include "pch.h"
#include "Simulator.h"
#include "Resource.h"

using namespace std;

class VlanWindow : public IVlanWindow
{
	ISimulatorApp* const _app;
	IProject* const _project;
	IProjectWindow* const _projectWindow;
	ISelection* const _selection;
	HWND _hwnd = nullptr;
	HWND _comboSelectedVlan = nullptr;
	HWND _comboNewWindowVlan = nullptr;

public:
	VlanWindow (HWND hWndParent, POINT location, ISimulatorApp* app, IProject* project, IProjectWindow* projectWindow, ISelection* selection)
		: _app(app), _project(project), _projectWindow(projectWindow), _selection(selection)
	{
		HINSTANCE hInstance;
		BOOL bRes = GetModuleHandleEx (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR) &DialogProcStatic, &hInstance);
		if (!bRes)
			throw win32_exception(GetLastError());

		_hwnd = CreateDialogParam (hInstance, MAKEINTRESOURCE(IDD_DIALOG_VLAN), hWndParent, &DialogProcStatic, reinterpret_cast<LPARAM>(this));

		RECT rc;
		::GetWindowRect(_hwnd, &rc);
		::MoveWindow (_hwnd, location.x, location.y, rc.right - rc.left, rc.bottom - rc.top, TRUE);
	}

	virtual ~VlanWindow()
	{
		if (_hwnd != nullptr)
			::DestroyWindow(_hwnd);
	}

	virtual HWND GetHWnd() const override final { return _hwnd; }

private:
	static INT_PTR CALLBACK DialogProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		VlanWindow* window;
		if (uMsg == WM_INITDIALOG)
		{
			window = reinterpret_cast<VlanWindow*>(lParam);
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
		}

		::SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result.messageResult);
		return result.dialogProcResult;
	}

	DialogProcResult DialogProc (UINT msg, WPARAM wParam , LPARAM lParam)
	{
		if (msg == WM_INITDIALOG)
		{
			_comboSelectedVlan  = GetDlgItem (_hwnd, IDC_COMBO_SELECTED_VLAN);
			_comboNewWindowVlan = GetDlgItem (_hwnd, IDC_COMBO_NEW_WINDOW_VLAN);
			for (size_t i = 1; i <= 16; i++)
			{
				auto str = std::to_wstring(i);
				ComboBox_AddString(_comboSelectedVlan, str.c_str());
				ComboBox_AddString(_comboNewWindowVlan, str.c_str());
			}
			LoadSelectedVlanCombo();
			return { FALSE, 0 };
		}

		if (msg == WM_CTLCOLORDLG)
			return { reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW)), 0 };

		if (msg == WM_CTLCOLORSTATIC)
			return { reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW)), 0 };

		if (msg == WM_COMMAND)
		{
			if ((HIWORD(wParam) == CBN_SELCHANGE) && ((HWND) lParam == _comboNewWindowVlan))
			{
				ProcessNewWindowVlanSelChanged();
				return { TRUE, 0 };
			}

			return { FALSE, 0 };
		}

		return { FALSE, 0 };
	}

	void ProcessNewWindowVlanSelChanged()
	{
		auto index = ComboBox_GetCurSel(_comboNewWindowVlan);
		uint16_t vlanNumber = (uint16_t) (index + 1);
		auto& pws = _app->GetProjectWindows();
		auto it = find_if (pws.begin(), pws.end(), [this, vlanNumber](const std::unique_ptr<IProjectWindow>& pw)
			{ return (pw->GetProject() == _project) && (pw->GetSelectedVlanNumber() == vlanNumber); });
		if (it != pws.end())
		{
			// bring to front and flash
			throw not_implemented_exception();
		}
		else
		{
			auto selection = selectionFactory(_project);
			auto pw = projectWindowFactory(_app, _project, selection, editAreaFactory, SW_SHOWNORMAL, vlanNumber);
			_app->AddProjectWindow(move(pw));
		}
	}

	void LoadSelectedVlanCombo()
	{
		ComboBox_SetCurSel (_comboSelectedVlan, _projectWindow->GetSelectedVlanNumber() - 1);
	}
};

const VlanWindowFactory vlanWindowFactory = [](auto... params) { return std::unique_ptr<IVlanWindow>(new VlanWindow(params...)); };
