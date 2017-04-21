
#include "pch.h"
#include "Simulator.h"
#include "Resource.h"

class VlanWindow : public IVlanWindow
{
	IProject* const _project;
	IProjectWindow* const _projectWindow;
	ISelection* const _selection;
	HWND _hwnd = nullptr;
	HWND _comboVlan = nullptr;

public:
	VlanWindow (HWND hWndParent, POINT location, IProject* project, IProjectWindow* projectWindow, ISelection* selection)
		: _project(project), _projectWindow(projectWindow), _selection(selection)
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

		::SetWindowLong (hwnd, DWL_MSGRESULT, result.messageResult);
		return result.dialogProcResult;
	}

	DialogProcResult DialogProc (UINT msg, WPARAM wParam , LPARAM lParam)
	{
		if (msg == WM_INITDIALOG)
		{
			_comboVlan = GetDlgItem (_hwnd, IDC_COMBO_SELECTED_VLAN);
			for (size_t i = 1; i <= 4095; i++)
				ComboBox_AddString(_comboVlan, std::to_wstring(i).c_str());
			LoadVlanCombo();
			return { FALSE, 0 };
		}

		if (msg == WM_CTLCOLORDLG)
			return { reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW)), 0 };

		if (msg == WM_CTLCOLORSTATIC)
			return { reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW)), 0 };

		return { FALSE, 0 };
	}

	void LoadVlanCombo()
	{
		ComboBox_SetCurSel (_comboVlan, _projectWindow->GetSelectedVlanNumber() - 1);
	}
};

const VlanWindowFactory vlanWindowFactory = [](auto... params) { return std::unique_ptr<IVlanWindow>(new VlanWindow(params...)); };
