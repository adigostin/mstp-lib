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
	HWND _comboTreeCount = nullptr;
	HWND _mstConfigNameEdit = nullptr;
	HWND _mstConfigRevLevelEdit = nullptr;
	HWND _buttonEditMstConfigId = nullptr;
	HWND _controlBeingValidated = nullptr;
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

		::SetWindowLong (hwnd, DWL_MSGRESULT, result.messageResult);
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
			ComboBox_AddString (_comboStpVersion, Bridge::GetStpVersionString(STP_VERSION_LEGACY_STP).c_str());
			ComboBox_AddString (_comboStpVersion, Bridge::GetStpVersionString(STP_VERSION_RSTP).c_str());
			ComboBox_AddString (_comboStpVersion, Bridge::GetStpVersionString(STP_VERSION_MSTP).c_str());
			_comboPortCount = GetDlgItem (_hwnd, IDC_COMBO_PORT_COUNT);
			for (size_t i = FirstPortCount; i <= 16; i++)
				ComboBox_AddString(_comboPortCount, std::to_wstring(i).c_str());
			_comboTreeCount = GetDlgItem (_hwnd, IDC_COMBO_TREE_COUNT);
			for (size_t i = 1; i <= 8; i++)
				ComboBox_AddString(_comboTreeCount, std::to_wstring(i).c_str());

			_mstConfigNameEdit = GetDlgItem (_hwnd, IDC_EDIT_MST_CONFIG_NAME);
			bRes = SetWindowSubclass (_mstConfigNameEdit, EditSubclassProc, EditSubClassId, (DWORD_PTR) this); assert (bRes);

			_mstConfigRevLevelEdit = GetDlgItem (_hwnd, IDC_EDIT_MST_CONFIG_REV_LEVEL);
			bRes = SetWindowSubclass (_mstConfigRevLevelEdit, EditSubclassProc, EditSubClassId, (DWORD_PTR) this); assert (bRes);

			_buttonEditMstConfigId = GetDlgItem (_hwnd, IDC_BUTTON_EDIT_MST_CONFIG_TABLE);
			return { FALSE, 0 };
		}

		if (msg == WM_DESTROY)
		{
			BOOL bRes = RemoveWindowSubclass (_bridgeAddressEdit, EditSubclassProc, EditSubClassId); assert (bRes);
			return { FALSE, 0 };
		}

		if (msg == WM_CTLCOLORDLG)
			return { reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW)), 0 };

		if (msg == WM_CTLCOLORSTATIC)
			return { reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW)), 0 };

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
					ProcessStpEnabledClicked();
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

	void ProcessStpEnabledClicked()
	{
		auto timestamp = GetTimestampMilliseconds();

		if (Button_GetCheck(_checkStpEnabled) == BST_UNCHECKED)
		{
			// enable stp for all
			for (auto& o : _selection->GetObjects())
			{
				auto b = dynamic_cast<Bridge*>(o.Get());
				if (!b->IsStpEnabled())
					b->EnableStp(timestamp);
			}
		}
		else
		{
			// disable stp for all
			for (auto& o : _selection->GetObjects())
			{
				auto b = dynamic_cast<Bridge*>(o.Get());
				if (b->IsStpEnabled())
					b->DisableStp(timestamp);
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

			if (b->IsStpEnabled())
			{
				b->DisableStp(timestamp);
				b->SetStpVersion(newVersion, timestamp);
				b->EnableStp(timestamp);
			}
			else
				b->SetStpVersion(newVersion, timestamp);
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
				GetWindowText (hWnd, str.data(), str.size());

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
			GetWindowText (hWnd, str.data(), str.size());

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
		{
			bridge->GetBridgeAddressChangedEvent().AddHandler (&OnSelectedBridgeAddressChanged, window);
			bridge->GetStpEnabledChangedEvent().AddHandler (&OnSelectedBridgeStpEnabledChanged, window);
			bridge->GetStpVersionChangedEvent().AddHandler (&OnSelectedBridgeStpVersionChanged, window);
			bridge->GetPortCountChangedEvent().AddHandler (&OnSelectedBridgePortCountChanged, window);
			bridge->GetTreeCountChangedEvent().AddHandler (&OnSelectedBridgeTreeCountChanged, window);
			bridge->GetMstConfigNameChangedEvent().AddHandler (&OnSelectedBridgeMstConfigNameChanged, window);
			bridge->GetMstConfigRevLevelChangedEvent().AddHandler (&OnSelectedBridgeMstConfigRevLevelChanged, window);
		}
	}

	static void OnObjectRemovingFromSelection (void* callbackArg, ISelection* selection, Object* o)
	{
		auto window = static_cast<BridgePropsWindow*>(callbackArg);

		auto bridge = dynamic_cast<Bridge*>(o);
		if (bridge != nullptr)
		{
			bridge->GetMstConfigRevLevelChangedEvent().RemoveHandler (&OnSelectedBridgeMstConfigRevLevelChanged, window);
			bridge->GetMstConfigNameChangedEvent().RemoveHandler (&OnSelectedBridgeMstConfigNameChanged, window);
			bridge->GetTreeCountChangedEvent().RemoveHandler (&OnSelectedBridgeTreeCountChanged, window);
			bridge->GetPortCountChangedEvent().RemoveHandler (&OnSelectedBridgePortCountChanged, window);
			bridge->GetStpVersionChangedEvent().RemoveHandler (&OnSelectedBridgeStpVersionChanged, window);
			bridge->GetStpEnabledChangedEvent().RemoveHandler (&OnSelectedBridgeStpEnabledChanged, window);
			bridge->GetBridgeAddressChangedEvent().RemoveHandler (&OnSelectedBridgeAddressChanged, window);
		}
	}

	static void OnSelectedBridgeAddressChanged (void* callbackArg, Bridge* b)
	{
		auto window = static_cast<BridgePropsWindow*>(callbackArg);
		window->LoadBridgeAddressTextBox();
	}

	static void OnSelectedBridgeStpEnabledChanged (void* callbackArg, Bridge* b)
	{
		auto window = static_cast<BridgePropsWindow*>(callbackArg);
		window->LoadStpEnabledCheckBox();
	}

	static void OnSelectedBridgeStpVersionChanged (void* callbackArg, Bridge* b)
	{
		static_cast<BridgePropsWindow*>(callbackArg)->LoadStpVersionComboBox();
	}

	static void OnSelectedBridgePortCountChanged (void* callbackArg, Bridge* b)
	{
		static_cast<BridgePropsWindow*>(callbackArg)->LoadPortCountComboBox();
	}

	static void OnSelectedBridgeTreeCountChanged (void* callbackArg, Bridge* b)
	{
		static_cast<BridgePropsWindow*>(callbackArg)->LoadTreeCountComboBox();
	}

	static void OnSelectedBridgeMstConfigNameChanged (void* callbackArg, Bridge* b)
	{
		static_cast<BridgePropsWindow*>(callbackArg)->LoadMstConfigNameTextBox();
	}

	static void OnSelectedBridgeMstConfigRevLevelChanged (void* callbackArg, Bridge* b)
	{
		static_cast<BridgePropsWindow*>(callbackArg)->LoadMstConfigRevLevelTextBox();
	}

	void LoadBridgeAddressTextBox()
	{
		assert (BridgesSelected());

		if (_selection->GetObjects().size() == 1)
		{
			auto bridge = dynamic_cast<Bridge*>(_selection->GetObjects()[0].Get());
			::SetWindowText (_bridgeAddressEdit, bridge->GetMacAddressAsString().c_str());
			::EnableWindow (_bridgeAddressEdit, TRUE);
		}
		else
		{
			::SetWindowText (_bridgeAddressEdit, L"(multiple selection)");
			::EnableWindow (_bridgeAddressEdit, FALSE);
		}
	}

	void LoadStpEnabledCheckBox()
	{
		assert (BridgesSelected());

		auto isStpEnabled = [](const ComPtr<Object>& o) { return dynamic_cast<Bridge*>(o.Get())->IsStpEnabled(); };

		if (none_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), isStpEnabled))
			::SendMessage (_checkStpEnabled, BM_SETCHECK, BST_UNCHECKED, 0);
		else if (all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), isStpEnabled))
			::SendMessage (_checkStpEnabled, BM_SETCHECK, BST_CHECKED, 0);
		else
			::SendMessage (_checkStpEnabled, BM_SETCHECK, BST_INDETERMINATE, 0);
	}

	void LoadStpVersionComboBox()
	{
		assert (BridgesSelected());

		auto getStpVersion = [](const ComPtr<Object>& o) { return dynamic_cast<Bridge*>(o.Get())->GetStpVersion(); };

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
			index = portCount - FirstPortCount;

		ComboBox_SetCurSel (_comboPortCount, index);
	}

	void LoadTreeCountComboBox()
	{
		assert (BridgesSelected());

		auto getTreeCount = [](const ComPtr<Object>& o) { return dynamic_cast<Bridge*>(o.Get())->GetTreeCount(); };

		int index = -1;
		auto treeCount = getTreeCount(_selection->GetObjects()[0]);
		if (all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), [&](const ComPtr<Object>& o) { return getTreeCount(o) == treeCount; }))
			index = treeCount - 1;

		ComboBox_SetCurSel (_comboTreeCount, index);
	}

	void LoadMstConfigNameTextBox()
	{
		auto name = dynamic_cast<Bridge*>(_selection->GetObjects()[0].Get())->GetMstConfigName();

		bool allSame = all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(),
							   [&name](const ComPtr<Object>& o) { return dynamic_cast<Bridge*>(o.Get())->GetMstConfigName() == name; });

		if (allSame)
			::SetWindowTextA (_mstConfigNameEdit, name.c_str());
		else
			::SetWindowTextA (_mstConfigNameEdit, "(multiple selection)");
	}

	void LoadMstConfigRevLevelTextBox()
	{
		uint16_t level = dynamic_cast<Bridge*>(_selection->GetObjects()[0].Get())->GetMstConfigRevLevel();

		bool allSame = all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(),
							   [level](const ComPtr<Object>& o) { return dynamic_cast<Bridge*>(o.Get())->GetMstConfigRevLevel() == level; });

		if (allSame)
			::SetWindowTextA (_mstConfigRevLevelEdit, to_string(level).c_str());
		else
			::SetWindowTextA (_mstConfigRevLevelEdit, "(multiple selection)");
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
			window->LoadBridgeAddressTextBox();
			window->LoadStpEnabledCheckBox();
			window->LoadStpVersionComboBox();
			window->LoadPortCountComboBox();
			window->LoadTreeCountComboBox();
			window->LoadMstConfigNameTextBox();
			window->LoadMstConfigRevLevelTextBox();

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
				dynamic_cast<Bridge*>(o.Get())->SetMstConfigName(ascii.c_str(), timestamp);

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
