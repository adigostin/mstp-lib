
#include "pch.h"
#include "Simulator.h"
#include "Resource.h"

class MSTConfigIdDialog : public IMSTConfigIdDialog
{
	ISimulatorApp* const _app;
	IProject* const _project;
	IProjectWindow* const _projectWindow;
	ISelection* const _selection;
	HWND _hwnd = nullptr;

public:
	MSTConfigIdDialog (ISimulatorApp* app, IProject* project, IProjectWindow* projectWindow, ISelection* selection)
		: _app(app), _project(project), _projectWindow(projectWindow), _selection(selection)
	{ }

	virtual UINT ShowModal (HWND hWndParent) override final
	{
		INT_PTR dr = DialogBoxParam (GetModuleHandle(nullptr), MAKEINTRESOURCE(IDD_DIALOG_MST_CONFIG_ID), hWndParent, &DialogProcStatic, (LPARAM) this);
		return (UINT) dr;
	}

	static INT_PTR CALLBACK DialogProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		MSTConfigIdDialog* window;
		if (uMsg == WM_INITDIALOG)
		{
			window = reinterpret_cast<MSTConfigIdDialog*>(lParam);
			window->_hwnd = hwnd;
			assert (GetWindowLongPtr(hwnd, GWLP_USERDATA) == 0);
			SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
		}
		else
			window = reinterpret_cast<MSTConfigIdDialog*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

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
		if (msg == WM_COMMAND)
		{
			if ((wParam == IDOK) || (wParam == IDCANCEL))
			{
				::EndDialog (_hwnd, wParam);
				return { TRUE, 0 };
			}

			return { FALSE, 0 };
		}

		return { FALSE, 0 };
	}
};

const MSTConfigIdDialogFactory mstConfigIdDialogFactory = [](auto... params) { return std::unique_ptr<IMSTConfigIdDialog>(new MSTConfigIdDialog(params...)); };
