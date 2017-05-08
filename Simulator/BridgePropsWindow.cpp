#include "pch.h"
#include "Simulator.h"
#include "Resource.h"
#include "Bridge.h"

using namespace std;

static constexpr UINT WM_WORK = WM_APP + 1;
static constexpr size_t FirstPortCount = 2;

class BridgePropsWindow : public IBridgePropsWindow
{
	ISimulatorApp* const _app;
	IProject* const _project;
	IProjectWindow* const _projectWindow;
	ISelection* const _selection;
	HWND _hwnd = nullptr;
	HWND _bridgeAddressEdit = nullptr;
	WNDPROC _bridgeAddressEditOriginalProc;
	HWND _checkStpEnabled = nullptr;
	HWND _comboStpVersion = nullptr;
	HWND _comboPortCount = nullptr;
	HWND _comboMstiCount = nullptr;
	HWND _mstConfigNameEdit = nullptr;
	HWND _mstConfigRevLevelEdit = nullptr;
	HWND _mstConfigDigestEdit = nullptr;
	HWND _buttonEditMstConfigId = nullptr;
	HWND _staticTreeProps = nullptr;
	HWND _controlBeingValidated = nullptr;
	HWND _configTableDigestToolTip = nullptr;
	bool _editChangedByUser = false;
	std::queue<std::function<void()>> _workQueue;

public:
	BridgePropsWindow (HWND hwndParent, POINT location, ISimulatorApp* app, IProject* project, IProjectWindow* projectWindow, ISelection* selection)
		: _app(app), _project(project), _projectWindow(projectWindow), _selection(selection)
	{
		HINSTANCE hInstance;
		BOOL bRes = GetModuleHandleEx (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR) &DialogProcStatic, &hInstance);
		if (!bRes)
			throw win32_exception(GetLastError());

		_hwnd = CreateDialogParam (hInstance, MAKEINTRESOURCE(IDD_PROPPAGE_BRIDGE), hwndParent, &DialogProcStatic, reinterpret_cast<LPARAM>(this));

		RECT rc;
		::GetWindowRect(_hwnd, &rc);
		::MoveWindow (_hwnd, location.x, location.y, rc.right - rc.left, rc.bottom - rc.top, TRUE);

		_selection->GetSelectionChangedEvent().AddHandler (&OnSelectionChanged, this);
		_selection->GetAddedToSelectionEvent().AddHandler (&OnObjectAddedToSelection, this);
		_selection->GetRemovingFromSelectionEvent().AddHandler (&OnObjectRemovingFromSelection, this);
	}

	~BridgePropsWindow()
	{
		_selection->GetRemovingFromSelectionEvent().RemoveHandler (&OnObjectRemovingFromSelection, this);
		_selection->GetAddedToSelectionEvent().RemoveHandler (&OnObjectAddedToSelection, this);
		_selection->GetSelectionChangedEvent().RemoveHandler (&OnSelectionChanged, this);

		if (_hwnd != nullptr)
			::DestroyWindow (_hwnd);
	}

	virtual HWND GetHWnd() const override final { return _hwnd; }

private:
	static INT_PTR CALLBACK DialogProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		BridgePropsWindow* window;
		if (uMsg == WM_INITDIALOG)
		{
			window = reinterpret_cast<BridgePropsWindow*>(lParam);
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
			_checkStpEnabled = GetDlgItem (_hwnd, IDC_CHECK_STP_ENABLED);
			_comboStpVersion = GetDlgItem (_hwnd, IDC_COMBO_STP_VERSION);
			::SendMessageA (_comboStpVersion, CB_ADDSTRING, 0, (LPARAM) STP_GetVersionString(STP_VERSION_LEGACY_STP));
			::SendMessageA (_comboStpVersion, CB_ADDSTRING, 0, (LPARAM) STP_GetVersionString(STP_VERSION_RSTP));
			::SendMessageA (_comboStpVersion, CB_ADDSTRING, 0, (LPARAM) STP_GetVersionString(STP_VERSION_MSTP));
			_comboPortCount = GetDlgItem (_hwnd, IDC_COMBO_PORT_COUNT);
			for (size_t i = FirstPortCount; i <= 16; i++)
				ComboBox_AddString(_comboPortCount, std::to_wstring(i).c_str());
			_comboMstiCount = GetDlgItem (_hwnd, IDC_COMBO_MSTI_COUNT);
			for (size_t i = 0; i <= 64; i++)
				ComboBox_AddString(_comboMstiCount, std::to_wstring(i).c_str());

			_mstConfigNameEdit = GetDlgItem (_hwnd, IDC_EDIT_MST_CONFIG_NAME);
			bRes = SetWindowSubclass (_mstConfigNameEdit, EditSubclassProc, EditSubClassId, (DWORD_PTR) this); assert (bRes);

			_mstConfigRevLevelEdit = GetDlgItem (_hwnd, IDC_EDIT_MST_CONFIG_REV_LEVEL);
			bRes = SetWindowSubclass (_mstConfigRevLevelEdit, EditSubclassProc, EditSubClassId, (DWORD_PTR) this); assert (bRes);

			_mstConfigDigestEdit = GetDlgItem (_hwnd, IDC_EDIT_MST_CONFIG_TABLE_HASH);

			DWORD ttStyle = WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON;
			_configTableDigestToolTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL, ttStyle, 0, 0, 0, 0, _hwnd, NULL, _app->GetHInstance(), NULL);

			_buttonEditMstConfigId = GetDlgItem (_hwnd, IDC_BUTTON_EDIT_MST_CONFIG_TABLE);

			_staticTreeProps = GetDlgItem(_hwnd, IDC_STATIC_TREE_PROPS);

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
			if (reinterpret_cast<HWND>(lParam) == _staticTreeProps)
				return { FALSE, 0 };
			else
				return { reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW)), 0 };
		}

		if (msg == WM_WORK)
		{
			_workQueue.front()();
			_workQueue.pop();
			return { TRUE, 0 };
		}

		if (msg == WM_COMMAND)
		{
			if (HIWORD(wParam) == BN_CLICKED)
			{
				if ((HWND) lParam == _checkStpEnabled)
				{
					ProcessStpStartedClicked();
					return { TRUE, 0 };
				}

				if ((HWND) lParam == _buttonEditMstConfigId)
				{
					auto dialog = mstConfigIdDialogFactory(_app, _project, _projectWindow, _selection);
					dialog->ShowModal(_projectWindow->GetHWnd());
					return { TRUE, 0 };
				}
			}
			else if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				if ((HWND) lParam == _comboStpVersion)
				{
					ProcessStpVersionSelChanged();
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

		if (Button_GetCheck(_checkStpEnabled) == BST_UNCHECKED)
		{
			// enable stp for all
			for (auto& o : _selection->GetObjects())
			{
				auto b = dynamic_cast<Bridge*>(o.Get());
				if (!STP_IsBridgeStarted(b->GetStpBridge()))
					STP_StartBridge (b->GetStpBridge(), timestamp);
			}
		}
		else
		{
			// disable stp for all
			for (auto& o : _selection->GetObjects())
			{
				auto b = dynamic_cast<Bridge*>(o.Get());
				if (STP_IsBridgeStarted(b->GetStpBridge()))
					STP_StopBridge (b->GetStpBridge(), timestamp);
			}
		}
	}

	void ProcessStpVersionSelChanged()
	{
		int index = ComboBox_GetCurSel(_comboStpVersion);
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

		for (auto& o : _selection->GetObjects())
		{
			auto b = dynamic_cast<Bridge*>(o.Get()); assert (b != nullptr);
			STP_SetStpVersion (b->GetStpBridge(), newVersion, timestamp);
		}
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

				if (dialog->_editChangedByUser && (dialog->_controlBeingValidated == nullptr))
				{
					dialog->_controlBeingValidated = hWnd;
					std::wstring errorMessage;
					bool valid = dialog->ValidateAndSetProperty(hWnd, str, errorMessage);
					if (!valid)
					{
						::MessageBox (dialog->_hwnd, errorMessage.c_str(), 0, 0);
						::SetFocus (hWnd);
					}
					else
						dialog->_editChangedByUser = false;

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

			if (dialog->_editChangedByUser && (dialog->_controlBeingValidated == nullptr))
			{
				dialog->_controlBeingValidated = hWnd;

				std::wstring errorMessage;
				bool valid = dialog->ValidateAndSetProperty(hWnd, str, errorMessage);
				if (valid)
				{
					dialog->_controlBeingValidated = nullptr;
					dialog->_editChangedByUser = false;
				}
				else
				{
					::SetFocus(nullptr);
					dialog->PostWork ([dialog, hWnd, message=move(errorMessage)]
					{
						::MessageBox (dialog->_hwnd, message.c_str(), 0, 0);
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
			bridge->GetBridgeConfigChangedEvent().AddHandler (&OnBridgeConfigChanged, window);
	}

	static void OnObjectRemovingFromSelection (void* callbackArg, ISelection* selection, Object* o)
	{
		auto window = static_cast<BridgePropsWindow*>(callbackArg);

		auto bridge = dynamic_cast<Bridge*>(o);
		if (bridge != nullptr)
			bridge->GetBridgeConfigChangedEvent().RemoveHandler (&OnBridgeConfigChanged, window);
	}

	static void OnBridgeConfigChanged (void* callbackArg, Bridge* b)
	{
		static_cast<BridgePropsWindow*>(callbackArg)->LoadAll();
	}

	void LoadAll()
	{
		LoadBridgeAddressTextBox();
		LoadStpStartedCheckBox();
		LoadStpVersionComboBox();
		LoadPortCountComboBox();
		LoadMstiCountComboBox();
		LoadMstConfigNameTextBox();
		LoadMstConfigRevLevelTextBox();
		LoadMstConfigTableHashEdit();
	}

	void LoadBridgeAddressTextBox()
	{
		assert (BridgesSelected());

		if (_selection->GetObjects().size() == 1)
		{
			auto bridge = dynamic_cast<Bridge*>(_selection->GetObjects()[0].Get());
			std::array<unsigned char, 6> ba;
			STP_GetBridgeAddress(bridge->GetStpBridge(), ba.data());
			::SetWindowText (_bridgeAddressEdit, bridge->GetBridgeAddressAsString().c_str());
			::EnableWindow (_bridgeAddressEdit, TRUE);
		}
		else
		{
			::SetWindowText (_bridgeAddressEdit, L"(multiple selection)");
			::EnableWindow (_bridgeAddressEdit, FALSE);
		}
	}

	void LoadStpStartedCheckBox()
	{
		assert (BridgesSelected());

		auto getStpStarted = [](const ComPtr<Object>& o) { return STP_IsBridgeStarted(dynamic_cast<Bridge*>(o.Get())->GetStpBridge()); };

		if (none_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), getStpStarted))
			::SendMessage (_checkStpEnabled, BM_SETCHECK, BST_UNCHECKED, 0);
		else if (all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), getStpStarted))
			::SendMessage (_checkStpEnabled, BM_SETCHECK, BST_CHECKED, 0);
		else
			::SendMessage (_checkStpEnabled, BM_SETCHECK, BST_INDETERMINATE, 0);
	}

	void LoadStpVersionComboBox()
	{
		assert (BridgesSelected());

		auto getStpVersion = [](const ComPtr<Object>& o) { return STP_GetStpVersion (dynamic_cast<Bridge*>(o.Get())->GetStpBridge()); };

		int index = -1;

		auto version = getStpVersion(_selection->GetObjects()[0]);
		if (all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), [&](const ComPtr<Object>& o) { return getStpVersion(o) == version; }))
		{
			if (version == STP_VERSION_LEGACY_STP)
				index = 0;
			else if (version == STP_VERSION_RSTP)
				index = 1;
			else if (version == STP_VERSION_MSTP)
				index = 2;
		}

		ComboBox_SetCurSel (_comboStpVersion, index);
	}

	void LoadPortCountComboBox()
	{
		assert (BridgesSelected());

		auto getPortCount = [](const ComPtr<Object>& o) { return dynamic_cast<Bridge*>(o.Get())->GetPorts().size(); };

		int index = -1;
		auto portCount = getPortCount(_selection->GetObjects()[0]);
		if (all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), [&](const ComPtr<Object>& o) { return getPortCount(o) == portCount; }))
			index = (int) (portCount - FirstPortCount);

		ComboBox_SetCurSel (_comboPortCount, index);
	}

	void LoadMstiCountComboBox()
	{
		assert (BridgesSelected());

		auto getMstiCount = [](const ComPtr<Object>& o) { return STP_GetMstiCount(dynamic_cast<Bridge*>(o.Get())->GetStpBridge()); };

		int index = -1;
		auto mstiCount = getMstiCount(_selection->GetObjects()[0]);
		if (all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), [&](const ComPtr<Object>& o) { return getMstiCount(o) == mstiCount; }))
			index = mstiCount;

		ComboBox_SetCurSel (_comboMstiCount, index);
	}

	void LoadMstConfigNameTextBox()
	{
		assert (BridgesSelected());

		auto getName = [](const ComPtr<Object>& o)
		{
			char name[33];
			STP_GetMstConfigName(dynamic_cast<Bridge*>(o.Get())->GetStpBridge(), name);
			return std::string(name);
		};

		auto name = getName(_selection->GetObjects()[0]);

		bool allSameName = all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), [&](const ComPtr<Object>& o) { return getName(o) == name; });
		if (allSameName)
			::SetWindowTextA (_mstConfigNameEdit, name.c_str());
		else
			::SetWindowTextA (_mstConfigNameEdit, "(multiple selection)");
	}

	void LoadMstConfigRevLevelTextBox()
	{
		assert (BridgesSelected());

		auto getLevel = [](const ComPtr<Object>& o) { return STP_GetMstConfigRevisionLevel(dynamic_cast<Bridge*>(o.Get())->GetStpBridge()); };

		auto level = getLevel(_selection->GetObjects()[0]);

		bool allSameLevel = all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), [&](const ComPtr<Object>& o) { return getLevel(o) == level; });
		if (allSameLevel)
			::SetWindowTextA (_mstConfigRevLevelEdit, to_string(level).c_str());
		else
			::SetWindowTextA (_mstConfigRevLevelEdit, "(multiple selection)");
	}

	void LoadMstConfigTableHashEdit()
	{
		auto getDigest = [](const ComPtr<Object>& o)
		{
			auto b = dynamic_cast<Bridge*>(o.Get());
			unsigned int digestLength;
			auto digest = STP_GetMstConfigTableDigest (b->GetStpBridge(), &digestLength);
			assert (digestLength == 16);
			array<uint8_t, 16> result;
			memcpy (result.data(), digest, 16);
			return result;
		};

		auto digest = getDigest(_selection->GetObjects()[0].Get());

		bool allSame = all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), [&](const ComPtr<Object>& o) { return getDigest(o) == digest; });

		const wchar_t* tooltipText;
		if (allSame)
		{
			wstringstream ss;
			ss << uppercase << setfill(L'0') << hex
				<< setw(2) << digest[0]  << setw(2) << digest[1]  << setw(2) << digest[2]  << setw(2) << digest[3]
				<< setw(2) << digest[4]  << setw(2) << digest[5]  << setw(2) << digest[6]  << setw(2) << digest[7]
				<< setw(2) << digest[8]  << setw(2) << digest[9]  << setw(2) << digest[10] << setw(2) << digest[11]
				<< setw(2) << digest[12] << setw(2) << digest[13] << setw(2) << digest[14] << setw(2) << digest[15];
			::SetWindowText (_mstConfigDigestEdit, ss.str().c_str());

			if (digest == DefaultConfigTableDigest)
				tooltipText = L"All VLANs assigned to the CIST, no VLAN assigned to any MSTI.";
			else
				tooltipText = L"Customized config table.";
		}
		else
		{
			::SetWindowTextA (_mstConfigRevLevelEdit, "(multiple selection)");
			tooltipText = L"";
		}

		TOOLINFO toolInfo = { sizeof(toolInfo) };
		toolInfo.hwnd = _hwnd;
		toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
		toolInfo.uId = (UINT_PTR) GetDlgItem (_hwnd, IDC_STATIC_CONFIG_TABLE_TIP);
		toolInfo.lpszText = const_cast<wchar_t*>(tooltipText);
		SendMessage (_configTableDigestToolTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
	}

	void LoadStaticTreeProps()
	{

	}

	static void OnSelectionChanged (void* callbackArg, ISelection* selection)
	{
		auto window = static_cast<BridgePropsWindow*>(callbackArg);

		bool bridgesSelected = !selection->GetObjects().empty()
			&& all_of (selection->GetObjects().begin(), selection->GetObjects().end(), [](const ComPtr<Object>& o) { return dynamic_cast<Bridge*>(o.Get()) != nullptr; });

		if (!bridgesSelected)
		{
			::ShowWindow (window->GetHWnd(), SW_HIDE);
		}
		else
		{
			window->LoadAll();
			::ShowWindow (window->GetHWnd(), SW_SHOW);
		}
	}

	void PostWork (std::function<void()>&& work)
	{
		_workQueue.push(move(work));
		PostMessage (_hwnd, WM_WORK, 0, 0);
	}

	bool ValidateAndSetProperty (HWND hwnd, const std::wstring& str, std::wstring& errorMessageOut)
	{
		if (hwnd == _bridgeAddressEdit)
		{
			if (!iswxdigit(str[0]) || !iswxdigit(str[1]))
			{
				errorMessageOut = L"Invalid address format. The Bridge Address must have the format XX:XX:XX:XX:XX:XX.";
				return false;
			}

			return true;
		}

		if (hwnd == _mstConfigNameEdit)
		{
			if (str.length() > 32)
			{
				errorMessageOut = L"Invalid MST Config Name: more than 32 characters.";
				return false;
			}

			string ascii;
			for (wchar_t ch : str)
			{
				if (ch >= 128)
				{
					errorMessageOut = L"Invalid MST Config Name: non-ASCII characters.";
					return false;
				}

				ascii.push_back((char) ch);
			}

			auto timestamp = GetTimestampMilliseconds();
			for (auto& o : _selection->GetObjects())
			{
				auto stpBridge = dynamic_cast<Bridge*>(o.Get())->GetStpBridge();
				STP_SetMstConfigName (stpBridge, ascii.c_str(), timestamp);
			}

			return true;
		}

		if (hwnd == _mstConfigRevLevelEdit)
			throw not_implemented_exception();

		throw not_implemented_exception();
	}

	bool BridgesSelected() const
	{
		return !_selection->GetObjects().empty()
			&& all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), [](const ComPtr<Object>& o) { return dynamic_cast<Bridge*>(o.Get()) != nullptr; });
	}
};

const BridgePropsWindowFactory bridgePropertiesControlFactory =
	[](auto... params) { return unique_ptr<IBridgePropsWindow>(new BridgePropsWindow(params...)); };
