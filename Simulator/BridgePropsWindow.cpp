#include "pch.h"
#include "Simulator.h"
#include "Resource.h"
#include "Bridge.h"

using namespace std;

static constexpr size_t FirstPortCount = 2;

class BridgePropsWindow : public IBridgePropsWindow
{
	ISimulatorApp*  const _app;
	IProjectWindow* const _projectWindow;
	IProjectPtr     const _project;
	ISelectionPtr   const _selection;
	IActionListPtr  const _actionList;
	ULONG _refCount = 1;
	HWND _hwnd = nullptr;
	HWND _bridgeAddressEdit = nullptr;
	WNDPROC _bridgeAddressEditOriginalProc;
	HWND _mstConfigNameEdit = nullptr;
	HWND _mstConfigRevLevelEdit = nullptr;
	HWND _controlBeingValidated = nullptr;
	HWND _configTableDigestToolTip = nullptr;
	bool _editChangedByUser = false;
	vector<Bridge*> _bridges;

public:
	BridgePropsWindow (ISimulatorApp* app,
					   IProjectWindow* projectWindow,
					   IProject* project,
					   ISelection* selection,
					   IActionList* actionList,
					   HWND hwndParent,
					   POINT location)
		: _app(app)
		, _projectWindow(projectWindow)
		, _project(project)
		, _selection(selection)
		, _actionList(actionList)
	{
		HINSTANCE hInstance;
		BOOL bRes = GetModuleHandleEx (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR) &DialogProcStatic, &hInstance);
		if (!bRes)
			throw win32_exception(GetLastError());

		_hwnd = CreateDialogParam (hInstance, MAKEINTRESOURCE(IDD_PROPPAGE_BRIDGE), hwndParent, &DialogProcStatic, reinterpret_cast<LPARAM>(this));

		RECT rc;
		::GetWindowRect(_hwnd, &rc);
		::MoveWindow (_hwnd, location.x, location.y, rc.right - rc.left, rc.bottom - rc.top, TRUE);

		for (auto o : _selection->GetObjects())
		{
			auto b = dynamic_cast<Bridge*>(o);
			if (b != nullptr)
				_bridges.push_back(b);
		}

		_selection->GetChangedEvent().AddHandler (&OnSelectionChanged, this);
		_selection->GetAddedToSelectionEvent().AddHandler (&OnObjectAddedToSelection, this);
		_selection->GetRemovingFromSelectionEvent().AddHandler (&OnObjectRemovingFromSelection, this);
		_projectWindow->GetSelectedVlanNumerChangedEvent().AddHandler (&OnSelectedVlanChanged, this);
	}

private:
	~BridgePropsWindow()
	{
		_projectWindow->GetSelectedVlanNumerChangedEvent().RemoveHandler (&OnSelectedVlanChanged, this);
		_selection->GetRemovingFromSelectionEvent().RemoveHandler (&OnObjectRemovingFromSelection, this);
		_selection->GetAddedToSelectionEvent().RemoveHandler (&OnObjectAddedToSelection, this);
		_selection->GetChangedEvent().RemoveHandler (&OnSelectionChanged, this);

		if (_hwnd != nullptr)
			::DestroyWindow (_hwnd);
	}

	virtual HWND GetHWnd() const override final { return _hwnd; }

	static INT_PTR CALLBACK DialogProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		BridgePropsWindow* window;
		if (uMsg == WM_INITDIALOG)
		{
			window = reinterpret_cast<BridgePropsWindow*>(lParam);
			window->AddRef();
			window->_hwnd = hwnd;
			assert (GetWindowLongPtr(hwnd, GWLP_USERDATA) == 0);
			SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
		}
		else
			window = reinterpret_cast<BridgePropsWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

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
			window->Release();
		}

		::SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result.messageResult);
		return result.dialogProcResult;
	}

	static const UINT_PTR EditSubClassId = 1;

	DialogProcResult DialogProc (UINT msg, WPARAM wParam , LPARAM lParam)
	{
		if (msg == WM_INITDIALOG)
		{
			_bridgeAddressEdit = GetDlgItem (_hwnd, IDC_EDIT_BRIDGE_ADDRESS);
			BOOL bRes = SetWindowSubclass (_bridgeAddressEdit, EditSubclassProc, EditSubClassId, (DWORD_PTR) this); assert (bRes);
			auto comboStpVersion = GetDlgItem (_hwnd, IDC_COMBO_STP_VERSION);
			::SendMessageA (comboStpVersion, CB_ADDSTRING, 0, (LPARAM) STP_GetVersionString(STP_VERSION_LEGACY_STP));
			::SendMessageA (comboStpVersion, CB_ADDSTRING, 0, (LPARAM) STP_GetVersionString(STP_VERSION_RSTP));
			::SendMessageA (comboStpVersion, CB_ADDSTRING, 0, (LPARAM) STP_GetVersionString(STP_VERSION_MSTP));

			auto comboPortCount = GetDlgItem (_hwnd, IDC_COMBO_PORT_COUNT);
			for (size_t i = FirstPortCount; i <= 16; i++)
				ComboBox_AddString(comboPortCount, std::to_wstring(i).c_str());

			auto comboMstiCount = GetDlgItem (_hwnd, IDC_COMBO_MSTI_COUNT);
			for (size_t i = 0; i <= 64; i++)
				ComboBox_AddString(comboMstiCount, std::to_wstring(i).c_str());

			_mstConfigNameEdit = GetDlgItem (_hwnd, IDC_EDIT_MST_CONFIG_NAME);
			bRes = SetWindowSubclass (_mstConfigNameEdit, EditSubclassProc, EditSubClassId, (DWORD_PTR) this); assert (bRes);

			_mstConfigRevLevelEdit = GetDlgItem (_hwnd, IDC_EDIT_MST_CONFIG_REV_LEVEL);
			bRes = SetWindowSubclass (_mstConfigRevLevelEdit, EditSubclassProc, EditSubClassId, (DWORD_PTR) this); assert (bRes);

			DWORD ttStyle = WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON;
			_configTableDigestToolTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL, ttStyle, 0, 0, 0, 0, _hwnd, NULL, _app->GetHInstance(), NULL);

			auto comboBridgePrio = GetDlgItem (_hwnd, IDC_COMBO_BRIDGE_PRIORITY); assert (comboBridgePrio != nullptr);
			for (int i = 0; i < 65536; i += 4096)
			{
				wstringstream ss;
				ss << L"0x" << hex << uppercase << setw(4) << setfill(L'0') << i << L" -- " << dec << i;
				ComboBox_AddString (comboBridgePrio, ss.str().c_str());
			}

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

			auto id = GetWindowLongPtr((HWND)lParam, GWLP_ID);
			if ((id == IDC_STATIC_TREE_PROPS) || (id == IDC_STATIC_MSTP_PROPS) || (id == IDC_STATIC_PRIO_VECTOR))
				return { FALSE, 0 };

			return { reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW)), 0 };
		}

		if (msg == WM_COMMAND)
		{
			if (HIWORD(wParam) == BN_CLICKED)
			{
				if (LOWORD(wParam) == IDC_CHECK_STP_ENABLED)
				{
					ProcessStpStartedClicked();
					return { TRUE, 0 };
				}

				if (LOWORD(wParam) == IDC_BUTTON_EDIT_MST_CONFIG_TABLE)
				{
					auto dialog = mstConfigIdDialogFactory(_app, _projectWindow, _project, _selection);
					dialog->ShowModal(_projectWindow->GetHWnd());
					return { TRUE, 0 };
				}
			}
			else if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				if (LOWORD(wParam) == IDC_COMBO_STP_VERSION)
				{
					ProcessStpVersionSelChanged ((HWND) lParam);
					return { TRUE, 0 };
				}
				else if (LOWORD(wParam) == IDC_COMBO_BRIDGE_PRIORITY)
				{
					ProcessBridgePrioritySelChange ((HWND) lParam);
					return { TRUE, 0 };
				}
				else if (LOWORD(wParam) == IDC_COMBO_PORT_COUNT)
				{
					MessageBox (_hwnd, L"The Simulator does not yet support changing the Port Count for an existing bridge.", _app->GetAppName(), 0);
					LoadPortCountComboBox();
					return { TRUE, 0 };
				}
				else if (LOWORD(wParam) == IDC_COMBO_MSTI_COUNT)
				{
					MessageBox (_hwnd, L"The Simulator does not yet support changing the MSTI Count for an existing bridge.", _app->GetAppName(), 0);
					LoadMstiCountComboBox();
					return { TRUE, 0 };
				}
			}
			else if (HIWORD(wParam) == EN_CHANGE)
			{
				_editChangedByUser = true;
				return { TRUE, 0 };
			}

			return { FALSE, 0 };
		}

		return { FALSE, 0 };
	}

	void ProcessStpStartedClicked()
	{
		auto timestamp = GetTimestampMilliseconds();

		auto checkStpEnabled = GetDlgItem (_hwnd, IDC_CHECK_STP_ENABLED);

		bool enable = Button_GetCheck(checkStpEnabled) == BST_UNCHECKED;
		for (auto b: _bridges)
		{
			if (enable)
			{
				if (!STP_IsBridgeStarted(b->GetStpBridge()))
					STP_StartBridge (b->GetStpBridge(), timestamp);
			}
			else
			{
				if (STP_IsBridgeStarted(b->GetStpBridge()))
					STP_StopBridge (b->GetStpBridge(), timestamp);
			}
		}
	}

	void ProcessStpVersionSelChanged (HWND hwnd)
	{
		int index = ComboBox_GetCurSel(hwnd);
		if (index == -1)
			return;

		assert ((index >= 0) && (index < 3));

		STP_VERSION newVersion;
		if (index == 0)
			newVersion = STP_VERSION_LEGACY_STP;
		else if (index == 1)
			newVersion = STP_VERSION_RSTP;
		else
			newVersion = STP_VERSION_MSTP;

		auto timestamp = GetTimestampMilliseconds();

		for (auto b : _bridges)
			STP_SetStpVersion (b->GetStpBridge(), newVersion, timestamp);
	}

	static LRESULT CALLBACK EditSubclassProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
	{
		auto dialog = reinterpret_cast<BridgePropsWindow*>(dwRefData);

		if (msg == WM_CHAR)
		{
			if ((wParam == VK_RETURN) || (wParam == VK_ESCAPE))
				return 0; // disable the beep on these keys

			return DefSubclassProc (hWnd, msg, wParam, lParam);
		}

		if (msg == WM_KEYDOWN)
		{
			if (wParam == VK_ESCAPE)
			{
				if (hWnd == dialog->_bridgeAddressEdit)
					dialog->LoadBridgeAddressTextBox();
				else if (hWnd == dialog->_mstConfigNameEdit)
					dialog->LoadMstConfigNameTextBox();
				else if (hWnd == dialog->_mstConfigRevLevelEdit)
					dialog->LoadMstConfigRevLevelTextBox();
				else
					throw not_implemented_exception();

				::SendMessage (hWnd, EM_SETSEL, 0, -1);
				dialog->_editChangedByUser = false;
				return 0;
			}
			else if (wParam == VK_RETURN)
			{
				std::wstring str;
				str.resize(GetWindowTextLength (hWnd) + 1);
				GetWindowText (hWnd, str.data(), (int) str.size());
				str.resize(str.size() - 1);

				if (dialog->_editChangedByUser && (dialog->_controlBeingValidated == nullptr))
				{
					dialog->_controlBeingValidated = hWnd;
					try
					{
						dialog->ValidateAndSetProperty(hWnd, str.c_str());
						dialog->_editChangedByUser = false;
					}
					catch (const exception& ex)
					{
						::MessageBoxA (dialog->_hwnd, ex.what(), 0, 0);
						::SetFocus (hWnd);
					}

					::SendMessage (hWnd, EM_SETSEL, 0, -1);
					dialog->_controlBeingValidated = nullptr;
				}

				return 0;
			}

			return DefSubclassProc (hWnd, msg, wParam, lParam);
		}

		if (msg == WM_KILLFOCUS)
		{
			std::wstring str;
			str.resize(GetWindowTextLength (hWnd) + 1);
			GetWindowText (hWnd, str.data(), (int) str.size());
			str.resize(str.size() - 1);

			if (dialog->_editChangedByUser && (dialog->_controlBeingValidated == nullptr))
			{
				dialog->_controlBeingValidated = hWnd;

				try
				{
					dialog->ValidateAndSetProperty(hWnd, str.c_str());
					dialog->_controlBeingValidated = nullptr;
					dialog->_editChangedByUser = false;
				}
				catch (const exception& ex)
				{
					::SetFocus(nullptr);
					dialog->_projectWindow->PostWork ([dialog, hWnd, message=string(ex.what())]
					{
						::MessageBoxA (dialog->_hwnd, message.c_str(), 0, 0);
						::SetFocus (hWnd);
						::SendMessage (hWnd, EM_SETSEL, 0, -1);
						dialog->_controlBeingValidated = nullptr;
					});
				}
			}

			return DefSubclassProc (hWnd, msg, wParam, lParam);
		}

		return DefSubclassProc (hWnd, msg, wParam, lParam);
	}

	static void OnObjectAddedToSelection (void* callbackArg, ISelection* selection, Object* o)
	{
		auto window = static_cast<BridgePropsWindow*>(callbackArg);

		auto bridge = dynamic_cast<Bridge*>(o);
		if (bridge != nullptr)
		{
			window->_bridges.push_back(bridge);
			bridge->GetBridgeConfigChangedEvent().AddHandler (&OnBridgeConfigChanged, window);
		}
	}

	static void OnObjectRemovingFromSelection (void* callbackArg, ISelection* selection, Object* o)
	{
		auto window = static_cast<BridgePropsWindow*>(callbackArg);

		auto bridge = dynamic_cast<Bridge*>(o);
		if (bridge != nullptr)
		{
			bridge->GetBridgeConfigChangedEvent().RemoveHandler (&OnBridgeConfigChanged, window);
			auto it = find (window->_bridges.begin(), window->_bridges.end(), bridge);
			window->_bridges.erase(it);
		}
	}

	static void OnBridgeConfigChanged (void* callbackArg, Bridge* b)
	{
		static_cast<BridgePropsWindow*>(callbackArg)->LoadAll();
	}

	void LoadAll()
	{
		assert (!_bridges.empty());
		LoadBridgeAddressTextBox();
		LoadStpStartedCheckBox();
		LoadStpVersionComboBox();
		LoadRootPriorityVectorControls();
		LoadPortCountComboBox();
		LoadMstiCountComboBox();
		LoadMstConfigNameTextBox();
		LoadMstConfigRevLevelTextBox();
		LoadMstConfigTableHashEdit();
		LoadSelectedTreeEdit();
		LoadBridgePriorityCombo();
	}

	// ========================================================================

	void LoadBridgeAddressTextBox()
	{
		if (_bridges.size() == 1)
		{
			::SetWindowText (_bridgeAddressEdit, _bridges[0]->GetBridgeAddressAsString().c_str());
			::EnableWindow (_bridgeAddressEdit, TRUE);
		}
		else
		{
			::SetWindowText (_bridgeAddressEdit, L"(multiple selection)");
			::EnableWindow (_bridgeAddressEdit, FALSE);
		}
	}

	void SetBridgeAddress (const wchar_t* str)
	{
		static constexpr char FormatErrorMessage[] = u8"Invalid address format. The Bridge Address must have the format XX:XX:XX:XX:XX:XX or XXXXXXXXXXXX (6 hex bytes).";

		int offsetMultiplier;
		if (wcslen(str) == 12)
		{
			offsetMultiplier = 2;
		}
		else if (wcslen(str) == 17)
		{
			if ((str[2] != ':') || (str[5] != ':') || (str[8] != ':') || (str[11] != ':') || (str[14] != ':'))
				throw invalid_argument(FormatErrorMessage);

			offsetMultiplier = 3;
		}
		else
			throw invalid_argument(FormatErrorMessage);

		STP_BRIDGE_ADDRESS newAddress;
		for (size_t i = 0; i < 6; i++)
		{
			wchar_t ch0 = str[i * offsetMultiplier];
			wchar_t ch1 = str[i * offsetMultiplier + 1];

			if (!iswxdigit(ch0) || !iswxdigit(ch1))
				throw invalid_argument(FormatErrorMessage);

			auto hn = (ch0 <= '9') ? (ch0 - '0') : ((ch0 >= 'a') ? (ch0 - 'a' + 10) : (ch0 - 'A' + 10));
			auto ln = (ch1 <= '9') ? (ch1 - '0') : ((ch1 >= 'a') ? (ch1 - 'a' + 10) : (ch1 - 'A' + 10));
			newAddress.bytes[i] = (hn << 4) | ln;
		}

		struct SetBridgeAddressAction : public EditAction
		{
			vector<Bridge*>    const _bridges;
			STP_BRIDGE_ADDRESS const _newAddress;
			vector<STP_BRIDGE_ADDRESS> _oldAddresses;

			SetBridgeAddressAction (const vector<Bridge*>& bridges, STP_BRIDGE_ADDRESS newAddress)
				: _bridges(bridges), _newAddress(newAddress)
			{
				for (auto b : bridges)
					_oldAddresses.push_back (*STP_GetBridgeAddress(b->GetStpBridge()));
			}

			virtual void Redo() override final
			{
				auto timestamp = GetTimestampMilliseconds();
				for (Bridge* b : _bridges)
					STP_SetBridgeAddress (b->GetStpBridge(), _newAddress.bytes, timestamp);
			}
			virtual void Undo() override final
			{
				auto timestamp = GetTimestampMilliseconds();
				for (size_t i = 0; i < _bridges.size(); i++)
					STP_SetBridgeAddress (_bridges[i]->GetStpBridge(), _oldAddresses[i].bytes, timestamp);
			}
		};

		_actionList->PerformAndAddUserAction (L"Change bridge address", unique_ptr<EditAction>(new SetBridgeAddressAction(_bridges, newAddress)));
	}

	// ========================================================================

	void LoadStpStartedCheckBox()
	{
		auto check = GetDlgItem (_hwnd, IDC_CHECK_STP_ENABLED);

		if (none_of (_bridges.begin(), _bridges.end(), [](Bridge* b) { return STP_IsBridgeStarted(b->GetStpBridge()); }))
			::SendMessage (check, BM_SETCHECK, BST_UNCHECKED, 0);
		else if (all_of (_bridges.begin(), _bridges.end(), [](Bridge* b) { return STP_IsBridgeStarted(b->GetStpBridge()); }))
			::SendMessage (check, BM_SETCHECK, BST_CHECKED, 0);
		else
			::SendMessage (check, BM_SETCHECK, BST_INDETERMINATE, 0);
	}

	void LoadStpVersionComboBox()
	{
		int index = -1;
		auto version = STP_GetStpVersion(_bridges[0]->GetStpBridge());
		if (all_of (_bridges.begin(), _bridges.end(), [version](Bridge* b) { return STP_GetStpVersion(b->GetStpBridge()) == version; }))
		{
			if (version == STP_VERSION_LEGACY_STP)
				index = 0;
			else if (version == STP_VERSION_RSTP)
				index = 1;
			else if (version == STP_VERSION_MSTP)
				index = 2;
		}

		ComboBox_SetCurSel (GetDlgItem (_hwnd, IDC_COMBO_STP_VERSION), index);
	}

	void LoadPortCountComboBox()
	{
		int index = -1;
		auto portCount = _bridges[0]->GetPorts().size();
		if (all_of (_bridges.begin(), _bridges.end(), [portCount](Bridge* b) { return b->GetPorts().size() == portCount; }))
			index = (int) (portCount - FirstPortCount);
		ComboBox_SetCurSel (GetDlgItem(_hwnd, IDC_COMBO_PORT_COUNT), index);
	}

	void LoadMstiCountComboBox()
	{
		int index = -1;
		auto mstiCount = STP_GetMstiCount(_bridges[0]->GetStpBridge());
		if (all_of (_bridges.begin(), _bridges.end(), [mstiCount](Bridge* b) { return STP_GetMstiCount(b->GetStpBridge()) == mstiCount; }))
			index = mstiCount;
		ComboBox_SetCurSel (GetDlgItem(_hwnd, IDC_COMBO_MSTI_COUNT), index);
	}

	// ========================================================================

	void LoadRootPriorityVectorControls()
	{
		if (_bridges.size() > 1)
		{
			::SetWindowText (GetDlgItem(_hwnd, IDC_STATIC_PRIO_VECTOR),           L"Priority Vector");
			::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_ROOT_BRIDGE_ID),          L"(multiple selection)");
			::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_EXTERNAL_ROOT_PATH_COST), L"(multiple selection)");
			::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_REGIONAL_ROOT_BRIDGE_ID), L"(multiple selection)");
			::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_INTERNAL_ROOT_PATH_COST), L"(multiple selection)");
			::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_DESIGNATED_BRIDGE_ID),    L"(multiple selection)");
			::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_DESIGNATED_PORT_ID),      L"(multiple selection)");
			::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_RECEIVING_PORT_ID),       L"(multiple selection)");
			return;
		}

		auto stpBridge = _bridges[0]->GetStpBridge();

		if (!STP_IsBridgeStarted(stpBridge))
		{
			::SetWindowText (GetDlgItem(_hwnd, IDC_STATIC_PRIO_VECTOR),           L"Priority Vector");
			::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_ROOT_BRIDGE_ID),          L"(STP disabled)");
			::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_EXTERNAL_ROOT_PATH_COST), L"(STP disabled)");
			::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_REGIONAL_ROOT_BRIDGE_ID), L"(STP disabled)");
			::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_INTERNAL_ROOT_PATH_COST), L"(STP disabled)");
			::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_DESIGNATED_BRIDGE_ID),    L"(STP disabled)");
			::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_DESIGNATED_PORT_ID),      L"(STP disabled)");
			::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_RECEIVING_PORT_ID),       L"(STP disabled)");
			return;
		}

		auto treeIndex = STP_GetTreeIndexFromVlanNumber(stpBridge, _projectWindow->GetSelectedVlanNumber());

		wstringstream ss;
		ss << L"Priority Vector (";
		if (treeIndex == 0)
			ss << "CIST";
		else
			ss << "MSTI " << treeIndex;
		ss << L')';
		::SetWindowText (GetDlgItem(_hwnd, IDC_STATIC_PRIO_VECTOR), ss.str().c_str());

		unsigned char prioVector[36];
		STP_GetRootPriorityVector(stpBridge, 0, prioVector);

		// RootBridgeId and
		ss.str(L"");
		ss << uppercase << setfill(L'0') << hex
			<< setw(2) << prioVector[0] << setw(2) << prioVector[1] << "."
			<< setw(2) << prioVector[2] << setw(2) << prioVector[3] << setw(2) << prioVector[4]
			<< setw(2) << prioVector[5] << setw(2) << prioVector[6] << setw(2) << prioVector[7];
		::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_ROOT_BRIDGE_ID), ss.str().c_str());

		// ExternalRootPathCost
		auto externalCost = ((uint32_t) prioVector[8] << 24) | ((uint32_t) prioVector[9] << 16) | ((uint32_t) prioVector[10] << 8) | prioVector[11];
		::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_EXTERNAL_ROOT_PATH_COST), to_wstring(externalCost).c_str());

		// RegionalRootBridgeId
		ss.str(L"");
		ss << uppercase << setfill(L'0') << hex
			<< setw(2) << prioVector[12] << setw(2) << prioVector[13] << "."
			<< setw(2) << prioVector[14] << setw(2) << prioVector[15] << setw(2) << prioVector[16]
			<< setw(2) << prioVector[17] << setw(2) << prioVector[18] << setw(2) << prioVector[19];
		::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_REGIONAL_ROOT_BRIDGE_ID), ss.str().c_str());

		// InternalRootPathCost
		auto internalCost = ((uint32_t) prioVector[20] << 24) | ((uint32_t) prioVector[21] << 16) | ((uint32_t) prioVector[22] << 8) | prioVector[23];
		::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_INTERNAL_ROOT_PATH_COST), to_wstring(internalCost).c_str());

		// DesignatedBridgeId
		ss.str(L"");
		ss << uppercase << setfill(L'0') << hex
			<< setw(2) << prioVector[24] << setw(2) << prioVector[25] << "."
			<< setw(2) << prioVector[26] << setw(2) << prioVector[27] << setw(2) << prioVector[28]
			<< setw(2) << prioVector[29] << setw(2) << prioVector[30] << setw(2) << prioVector[31];
		::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_DESIGNATED_BRIDGE_ID), ss.str().c_str());

		// DesignatedPortId
		ss.str(L"");
		ss << uppercase << setfill(L'0') << hex << setw(2) << prioVector[32] << setw(2) << prioVector[33];
		::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_DESIGNATED_PORT_ID), ss.str().c_str());

		// ReceivingPortId
		ss.str(L"");
		ss << uppercase << setfill(L'0') << hex << setw(2) << prioVector[34] << setw(2) << prioVector[35];
		::SetWindowText (GetDlgItem(_hwnd, IDC_EDIT_RECEIVING_PORT_ID), ss.str().c_str());
	}

	// ========================================================================

	void LoadMstConfigNameTextBox()
	{
		const char* name = STP_GetMstConfigId(_bridges[0]->GetStpBridge())->ConfigurationName;

		bool allSameName = all_of (_bridges.begin(), _bridges.end(),
								   [name](Bridge* b) { return memcmp (STP_GetMstConfigId(b->GetStpBridge())->ConfigurationName, name, 32) == 0; });
		if (allSameName)
			::SetWindowTextA (_mstConfigNameEdit, string(name, 32).c_str());
		else
			::SetWindowTextA (_mstConfigNameEdit, "(multiple selection)");
	}

	void LoadMstConfigRevLevelTextBox()
	{
		auto getLevel = [](Bridge* b) -> unsigned short
		{
			auto id = STP_GetMstConfigId(b->GetStpBridge());
			return ((unsigned short) id->RevisionLevelHigh << 8) | (unsigned short) id->RevisionLevelLow;
		};

		auto level = getLevel(_bridges[0]);

		bool allSameLevel = all_of (_bridges.begin(), _bridges.end(), [&](Bridge* b) { return getLevel(b) == level; });
		if (allSameLevel)
			::SetWindowTextA (_mstConfigRevLevelEdit, to_string(level).c_str());
		else
			::SetWindowTextA (_mstConfigRevLevelEdit, "(multiple selection)");
	}

	void LoadMstConfigTableHashEdit()
	{
		const unsigned char* digest = STP_GetMstConfigId(_bridges.front()->GetStpBridge())->ConfigurationDigest;

		bool allSame = all_of (_bridges.begin(), _bridges.end(),
							   [digest](Bridge* b) { return memcmp (STP_GetMstConfigId(b->GetStpBridge())->ConfigurationDigest, digest, 16) == 0; });

		auto mstConfigDigestEdit = GetDlgItem (_hwnd, IDC_EDIT_MST_CONFIG_TABLE_HASH);
		const wchar_t* tooltipText;
		if (allSame)
		{
			wstringstream ss;
			ss << uppercase << setfill(L'0') << hex
				<< setw(2) << digest[0]  << setw(2) << digest[1]  << setw(2) << digest[2]  << setw(2) << digest[3]
				<< setw(2) << digest[4]  << setw(2) << digest[5]  << setw(2) << digest[6]  << setw(2) << digest[7]
				<< setw(2) << digest[8]  << setw(2) << digest[9]  << setw(2) << digest[10] << setw(2) << digest[11]
				<< setw(2) << digest[12] << setw(2) << digest[13] << setw(2) << digest[14] << setw(2) << digest[15];
			::SetWindowText (mstConfigDigestEdit, ss.str().c_str());

			if (memcmp (digest, DefaultConfigTableDigest, 16) == 0)
				tooltipText = L"All VLANs assigned to the CIST, no VLAN assigned to any MSTI.";
			else
				tooltipText = L"Customized config table.";
		}
		else
		{
			::SetWindowTextA (mstConfigDigestEdit, "(multiple selection)");
			tooltipText = L"";
		}

		TOOLINFO toolInfo = { sizeof(toolInfo) };
		toolInfo.hwnd = _hwnd;
		toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
		toolInfo.uId = (UINT_PTR) GetDlgItem (_hwnd, IDC_STATIC_CONFIG_TABLE_TIP);
		toolInfo.lpszText = const_cast<wchar_t*>(tooltipText);
		SendMessage (_configTableDigestToolTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
	}

	// ========================================================================

	void LoadSelectedTreeEdit()
	{
		auto edit = GetDlgItem (_hwnd, IDC_EDIT_PROPS_SELECTED_TREE); assert (edit != nullptr);

		auto vlanNumber = _projectWindow->GetSelectedVlanNumber();

		auto treeIndex = STP_GetTreeIndexFromVlanNumber(_bridges[0]->GetStpBridge(), vlanNumber);

		bool allSameTree = all_of (_bridges.begin(), _bridges.end(),
								   [vlanNumber, treeIndex](Bridge* b) { return STP_GetTreeIndexFromVlanNumber(b->GetStpBridge(), vlanNumber) == treeIndex; });
		if (allSameTree)
		{
			if (treeIndex == 0)
				::SetWindowText (edit, L"CIST(0)");
			else
				::SetWindowText (edit, (wstring(L"MSTI") + to_wstring(treeIndex)).c_str());
		}
		else
			::SetWindowText (edit, L"(multiple selection)");
	}

	// ========================================================================

	void LoadBridgePriorityCombo()
	{
		auto vlanNumber = _projectWindow->GetSelectedVlanNumber();
		auto getPrio = [vlanNumber](Bridge* b)
		{
			auto treeIndex = STP_GetTreeIndexFromVlanNumber(b->GetStpBridge(), vlanNumber);
			return STP_GetBridgePriority(b->GetStpBridge(), treeIndex);
		};

		auto prio = getPrio(_bridges[0]);
		bool allSamePrio = all_of (_bridges.begin(), _bridges.end(), [&](Bridge* b) { return getPrio(b) == prio; });

		auto combo = GetDlgItem(_hwnd, IDC_COMBO_BRIDGE_PRIORITY);
		if (allSamePrio)
		{
			ComboBox_SetCurSel (combo, prio / 4096);
			::EnableWindow (combo, TRUE);
		}
		else
		{
			ComboBox_SetCurSel (combo, -1);
			::EnableWindow (combo, FALSE);
		}
	}

	void ProcessBridgePrioritySelChange (HWND hwnd)
	{
		int index = ComboBox_GetCurSel(hwnd);
		if (index == -1)
			return;

		auto newPrio = (unsigned int) index * 4096u;
		auto timestamp = GetTimestampMilliseconds();

		for (Bridge* b : _bridges)
		{
			auto stpb = b->GetStpBridge();
			auto treeIndex = STP_GetTreeIndexFromVlanNumber(stpb, _projectWindow->GetSelectedVlanNumber());
			STP_SetBridgePriority (stpb, treeIndex, newPrio, timestamp);
		}
	}

	// ========================================================================

	static void OnSelectedVlanChanged (void* callbackArg, IProjectWindow* pw, unsigned int vlanNumber)
	{
		auto window = static_cast<BridgePropsWindow*>(callbackArg);
		if (!window->_bridges.empty())
			window->LoadAll();
	}

	static void OnSelectionChanged (void* callbackArg, ISelection* selection)
	{
		auto window = static_cast<BridgePropsWindow*>(callbackArg);
		if (window->_bridges.empty())
		{
			::ShowWindow (window->GetHWnd(), SW_HIDE);
		}
		else
		{
			window->LoadAll();
			::ShowWindow (window->GetHWnd(), SW_SHOW);
		}
	}


	void ValidateAndSetProperty (HWND hwnd, const wchar_t* str)
	{
		if (hwnd == _bridgeAddressEdit)
		{
			SetBridgeAddress(str);
		}
		else if (hwnd == _mstConfigNameEdit)
		{
			if (wcslen(str) > 32)
				throw invalid_argument("Invalid MST Config Name: more than 32 characters.");

			string ascii;
			for (auto p = str; *p != 0; p++)
			{
				if (*p >= 128)
					throw invalid_argument("Invalid MST Config Name: non-ASCII characters.");

				ascii.push_back((char) *p);
			}

			auto timestamp = GetTimestampMilliseconds();
			for (Bridge* b : _bridges)
				STP_SetMstConfigName (b->GetStpBridge(), ascii.c_str(), timestamp);
		}
		else if (hwnd == _mstConfigRevLevelEdit)
		{
			throw not_implemented_exception();
		}
		else
			throw not_implemented_exception();
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override { return E_NOTIMPL; }

	virtual ULONG STDMETHODCALLTYPE AddRef() override final
	{
		return InterlockedIncrement(&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release() override final
	{
		assert (_refCount > 0);
		ULONG newRefCount = InterlockedDecrement(&_refCount);
		if (newRefCount == 0)
			delete this;
		return newRefCount;
	}
};

template<typename... Args>
static IBridgePropsWindowPtr Create (Args... args)
{
	return IBridgePropsWindowPtr(new BridgePropsWindow (std::forward<Args>(args)...), false);
}

const BridgePropsWindowFactory bridgePropertiesControlFactory = Create;
