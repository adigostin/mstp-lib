
#include "pch.h"
#include "Simulator.h"
#include "Resource.h"
#include "Bridge.h"
#include "Win32/UtilityFunctions.h"

using namespace std;

class MSTConfigIdDialog : public IPropertyEditor
{
	vector<Bridge*> _bridges;
	HWND _hwnd = nullptr;

public:
	MSTConfigIdDialog (const std::vector<Object*>& objects)
	{
		assert (!objects.empty());
		for (auto o : objects)
		{
			auto bridge = dynamic_cast<Bridge*>(o);
			if (bridge == nullptr)
				throw invalid_argument("");
			_bridges.push_back(bridge);
		}
	}

	~MSTConfigIdDialog()
	{ }

	static void OnBridgeRemoving (void* callbackArg, IProject* project, size_t index, Bridge* bridge)
	{
		auto dialog = static_cast<MSTConfigIdDialog*>(callbackArg);
		if (find (dialog->_bridges.begin(), dialog->_bridges.end(), bridge) != dialog->_bridges.end())
			::EndDialog (dialog->_hwnd, IDCANCEL);
	}

	virtual UINT ShowModal (HWND hWndParent) override final
	{
		INT_PTR dr = DialogBoxParam (GetModuleHandle(nullptr), MAKEINTRESOURCE(IDD_DIALOG_MST_CONFIG_ID), hWndParent, &DialogProcStatic, (LPARAM) this);
		return (UINT) dr;
	}

	virtual void Cancel() override final
	{
		::EndDialog (_hwnd, IDCANCEL);
		return;
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

		if (msg == WM_CTLCOLORSTATIC)
			return { (INT_PTR) GetSysColorBrush(COLOR_INFOBK), 0 };

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
		auto hdc = GetDC(_hwnd);
		int dpi = GetDeviceCaps (hdc, LOGPIXELSX);
		ReleaseDC(_hwnd, hdc);

		HWND list = GetDlgItem (_hwnd, IDC_LIST_CONFIG_TABLE);

		ListView_SetExtendedListViewStyle (list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		//ListView_SetBkColor (list, GetSysColor(COLOR_3DFACE));

		auto& configId = *STP_GetMstConfigId(_bridges[0]->GetStpBridge());
		bool allSameConfig = all_of(_bridges.begin(), _bridges.end(), [&](Bridge* b) { return *STP_GetMstConfigId(b->GetStpBridge()) == configId; });

		LVCOLUMN lvc = { 0 };
		lvc.mask = LVCF_TEXT | LVCF_WIDTH;
		lvc.pszText = L"VLAN";
		lvc.cx = (allSameConfig ? 80 : 120) * dpi / 96;
		ListView_InsertColumn (list, 0, &lvc);
		lvc.pszText = L"Tree";
		lvc.cx = (allSameConfig ? 80 : 40) * dpi / 96;
		ListView_InsertColumn (list, 1, &lvc);

		if (allSameConfig)
			LoadTable (list, _bridges[0]);
		else
		{
			LVITEM lvi = { 0 };
			lvi.mask = LVIF_TEXT;
			lvi.pszText = L"(multiple selection)";
			ListView_InsertItem (list, &lvi);
		}

		HWND hint = GetDlgItem (_hwnd, IDC_STATIC_HINT_NOT_MSTP);
		bool showHint = any_of (_bridges.begin(), _bridges.end(), [](Bridge* b) { return STP_GetStpVersion(b->GetStpBridge()) < STP_VERSION_MSTP; });
		auto style = ::GetWindowLongPtr (hint, GWL_STYLE);
		style = (style & ~WS_VISIBLE) | (showHint ? WS_VISIBLE : 0);
		::SetWindowLongPtr (hint, GWL_STYLE, style);
	}

	bool ValidateAndApply()
	{
		return true;
	}

	void LoadTable (HWND list, Bridge* bridge)
	{
		LVITEM lvi = { 0 };
		lvi.mask = LVIF_TEXT;

		unsigned int entryCount;
		auto entries = STP_GetMstConfigTable (bridge->GetStpBridge(), &entryCount);

		for (unsigned int vlanNumber = 0; vlanNumber <= MaxVlanNumber; vlanNumber++)
		{
			lvi.iItem = vlanNumber;

			wstring text = to_wstring(vlanNumber);
			lvi.iSubItem = 0;
			lvi.pszText = const_cast<wchar_t*>(text.c_str());
			ListView_InsertItem (list, &lvi);

			auto treeIndex = entries[vlanNumber].treeIndex;
			text = to_wstring (treeIndex);
			lvi.iSubItem = 1;
			lvi.pszText = const_cast<wchar_t*>(text.c_str());
			ListView_SetItem (list, &lvi);
		}
	}

	void LoadDefaultConfig()
	{
		auto timestamp = GetTimestampMilliseconds();

		vector<STP_CONFIG_TABLE_ENTRY> entries;
		entries.resize(1 + MaxVlanNumber);

		for (auto b : _bridges)
			b->SetMstConfigTable (entries.data(), (unsigned int) entries.size(), timestamp);

		HWND list = GetDlgItem (_hwnd, IDC_LIST_CONFIG_TABLE);
		ListView_DeleteAllItems(list);
		LoadTable (list, _bridges[0]);
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

			b->SetMstConfigTable (entries.data(), (unsigned int) entries.size(), timestamp);
		}

		HWND list = GetDlgItem (_hwnd, IDC_LIST_CONFIG_TABLE);
		ListView_DeleteAllItems(list);
		LoadTable (list, _bridges[0]);
	}
};

template<typename... Args>
static unique_ptr<IPropertyEditor> Create (Args... args)
{
	return unique_ptr<IPropertyEditor>(new MSTConfigIdDialog (std::forward<Args>(args)...));
}

const PropertyEditorFactory mstConfigIdDialogFactory = &Create;
