
#include "pch.h"
#include "Simulator.h"
#include "Resource.h"
#include "Bridge.h"

using namespace std;

class MSTConfigIdDialog : public IMSTConfigIdDialog
{
	ISimulatorApp* const _app;
	IProject* const _project;
	IProjectWindow* const _projectWindow;
	ISelection* const _selection;
	vector<Bridge*> _bridges;
	HWND _hwnd = nullptr;

public:
	MSTConfigIdDialog (ISimulatorApp* app, IProjectWindow* projectWindow, IProject* project, ISelection* selection)
		: _app(app), _project(project), _projectWindow(projectWindow), _selection(selection)
	{
		if (_selection->GetObjects().empty())
			throw invalid_argument ("Selection must not be empty.");

		for (auto& o : _selection->GetObjects())
		{
			auto b = dynamic_cast<Bridge*>(o.Get());
			if (b == nullptr)
				throw invalid_argument ("Selection must consists only of bridges.");
			_bridges.push_back(b);
		}
	}

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

		::SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result.messageResult);
		return result.dialogProcResult;
	}

	DialogProcResult DialogProc (UINT msg, WPARAM wParam , LPARAM lParam)
	{
		if (msg == WM_INITDIALOG)
		{
			ProcessWmInitDialog();
			return { FALSE, 0 };
		}

		if (msg == WM_COMMAND)
		{
			if (wParam == IDOK)
			{
				if (ValidateAndApply())
					::EndDialog (_hwnd, IDOK);
				return { TRUE, 0 };
			}
			else if (wParam == IDCANCEL)
			{
				::EndDialog (_hwnd, IDCANCEL);
				return { TRUE, 0 };
			}
			else if (wParam == IDC_BUTTON_USE_DEFAULT_CONFIG_TABLE)
			{
				LoadDefaultConfig();
				return { TRUE, 0 };
			}
			else if (wParam == IDC_BUTTON_USE_TEST1_CONFIG_TABLE)
			{
				LoadTestConfig1();
				return { TRUE, 0 };
			}

			return { FALSE, 0 };
		}

		return { FALSE, 0 };
	}

	void ProcessWmInitDialog()
	{
	}

	bool ValidateAndApply()
	{
		return true;
	}

	void LoadDefaultConfig()
	{
		auto timestamp = GetTimestampMilliseconds();

		vector<STP_CONFIG_TABLE_ENTRY> entries;
		entries.resize(1 + MaxVlanNumber);

		for (auto b : _bridges)
			STP_SetMstConfigTable (b->GetStpBridge(), &entries[0], (unsigned int) entries.size(), timestamp);
	}

	void LoadTestConfig1()
	{
		auto timestamp = GetTimestampMilliseconds();

		for (auto b : _bridges)
		{
			auto treeCount = 1 + STP_GetMstiCount(b->GetStpBridge());

			vector<STP_CONFIG_TABLE_ENTRY> entries;
			entries.resize(1 + MaxVlanNumber);

			entries[0] = { 0, 0 }; // VLAN0 does not exist.

			unsigned char treeIndex = 1;
			for (unsigned int vid = 1; vid <= MaxVlanNumber; vid++)
			{
				entries[vid].unused = 0;
				entries[vid].treeIndex = (vid - 1) % treeCount;
			}

			STP_SetMstConfigTable (b->GetStpBridge(), &entries[0], (unsigned int) entries.size(), timestamp);
		}
	}
};

template<typename... Args>
static IMSTConfigIdDialog* Create (Args... args)
{
	return new MSTConfigIdDialog (std::forward<Args>(args)...);
}

const MSTConfigIdDialogFactory mstConfigIdDialogFactory = &Create;
