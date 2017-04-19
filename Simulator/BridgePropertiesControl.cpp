#include "pch.h"
#include "BridgePropertiesControl.h"
#include "resource.h"
#include "Bridge.h"

static const UINT WM_WORK = WM_APP + 1;

BridgePropertiesControl::BridgePropertiesControl (HWND hwndParent, const RECT& rect, ISelection* selection)
	: _selection(selection)
{
	HINSTANCE hInstance;
	BOOL bRes = GetModuleHandleEx (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR) &DialogProcStatic, &hInstance);
	if (!bRes)
		throw win32_exception(GetLastError());

	_hwnd = CreateDialogParam (hInstance, MAKEINTRESOURCE(IDD_PROPPAGE_BRIDGE), hwndParent, &DialogProcStatic, reinterpret_cast<LPARAM>(this));

	::MoveWindow (_hwnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);

	_selection->GetSelectionChangedEvent().AddHandler (&OnSelectionChanged, this);
	_selection->GetAddedToSelectionEvent().AddHandler (&OnAddedToSelection, this);
	_selection->GetRemovingFromSelectionEvent().AddHandler (&OnRemovingFromSelection, this);
}


BridgePropertiesControl::~BridgePropertiesControl()
{
	_selection->GetRemovingFromSelectionEvent().RemoveHandler (&OnRemovingFromSelection, this);
	_selection->GetAddedToSelectionEvent().RemoveHandler (&OnAddedToSelection, this);
	_selection->GetSelectionChangedEvent().RemoveHandler (&OnSelectionChanged, this);

	if (_hwnd != nullptr)
		::DestroyWindow (_hwnd);
}

//static
INT_PTR CALLBACK BridgePropertiesControl::DialogProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	BridgePropertiesControl* window;
	if (uMsg == WM_INITDIALOG)
	{
		window = reinterpret_cast<BridgePropertiesControl*>(lParam);
		window->_hwnd = hwnd;
		assert (GetWindowLongPtr(hwnd, GWLP_USERDATA) == 0);
		SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
	}
	else
		window = reinterpret_cast<BridgePropertiesControl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	if (window == nullptr)
	{
		// this must be one of those messages sent before WM_NCCREATE or after WM_NCDESTROY.
		return DefWindowProc (hwnd, uMsg, wParam, lParam);
	}

	Result result = window->DialogProc (uMsg, wParam, lParam);

	if (uMsg == WM_NCDESTROY)
	{
		window->_hwnd = nullptr;
		SetWindowLongPtr (hwnd, GWLP_USERDATA, 0);
	}

	::SetWindowLong (hwnd, DWL_MSGRESULT, result.messageResult);
	return result.dialogProcResult;
}

static const UINT_PTR EditSubClassId = 1;

BridgePropertiesControl::Result BridgePropertiesControl::DialogProc (UINT msg, WPARAM wParam , LPARAM lParam)
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
		ComboBox_SetMinVisible (_comboStpVersion, 5);
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
		if ((HIWORD(wParam) == BN_CLICKED) && ((HWND) lParam == _checkStpEnabled))
		{
			ProcessStpEnabledClicked();
			return { TRUE, 0 };
		}

		if ((HIWORD(wParam) == CBN_SELCHANGE) && ((HWND) lParam == _comboStpVersion))
		{
			ProcessStpVersionSelChanged();
			return { TRUE, 0 };
		}

		return { FALSE, 0 };
	}

	return { FALSE, 0 };
}

void BridgePropertiesControl::ProcessStpEnabledClicked()
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

void BridgePropertiesControl::ProcessStpVersionSelChanged()
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

//static
LRESULT CALLBACK BridgePropertiesControl::EditSubclassProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	auto dialog = reinterpret_cast<BridgePropertiesControl*>(dwRefData);

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
			auto text = dialog->GetEditPropertyText(hWnd);
			::SetWindowText (hWnd, text.c_str());
			::SendMessage (hWnd, EM_SETSEL, 0, -1);
			return 0;
		}
		else if (wParam == VK_RETURN)
		{
			std::wstring str;
			str.resize(GetWindowTextLength (hWnd) + 1);
			GetWindowText (hWnd, str.data(), str.size());

			if ((dialog->GetEditPropertyText(hWnd) != str) && (dialog->_controlBeingValidated == nullptr))
			{
				dialog->_controlBeingValidated = hWnd;
				std::wstring errorMessage;
				bool valid = dialog->ValidateAndSetProperty(hWnd, str, errorMessage);
				if (!valid)
				{
					::MessageBox (dialog->_hwnd, errorMessage.c_str(), 0, 0);
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
		GetWindowText (hWnd, str.data(), str.size());

		if ((dialog->GetEditPropertyText(hWnd) != str) && (dialog->_controlBeingValidated == nullptr))
		{
			dialog->_controlBeingValidated = hWnd;

			std::wstring errorMessage;
			bool valid = dialog->ValidateAndSetProperty(hWnd, str, errorMessage);
			if (valid)
			{
				dialog->_controlBeingValidated = nullptr;
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

//static
void BridgePropertiesControl::OnAddedToSelection (void* callbackArg, ISelection* selection, Object* o)
{
	auto window = static_cast<BridgePropertiesControl*>(callbackArg);

	auto bridge = dynamic_cast<Bridge*>(o);
	if (bridge != nullptr)
	{
		bridge->GetBridgeAddressChangedEvent().AddHandler (&OnSelectedBridgeAddressChanged, window);
		bridge->GetStpEnabledChangedEvent().AddHandler (&OnSelectedBridgeStpEnabledChanged, window);
		bridge->GetStpVersionChangedEvent().AddHandler (&OnSelectedBridgeStpVersionChanged, window);
	}
}

//static
void BridgePropertiesControl::OnRemovingFromSelection (void* callbackArg, ISelection* selection, Object* o)
{
	auto window = static_cast<BridgePropertiesControl*>(callbackArg);

	auto bridge = dynamic_cast<Bridge*>(o);
	if (bridge != nullptr)
	{
		bridge->GetStpEnabledChangedEvent().RemoveHandler (&OnSelectedBridgeStpEnabledChanged, window);
		bridge->GetStpVersionChangedEvent().RemoveHandler (&OnSelectedBridgeStpVersionChanged, window);
		bridge->GetBridgeAddressChangedEvent().RemoveHandler (&OnSelectedBridgeAddressChanged, window);
	}
}

//static
void BridgePropertiesControl::OnSelectedBridgeAddressChanged (void* callbackArg, Bridge* b)
{
	auto window = static_cast<BridgePropertiesControl*>(callbackArg);
	window->LoadBridgeAddressTextBox();
}

//static
void BridgePropertiesControl::OnSelectedBridgeStpEnabledChanged (void* callbackArg, Bridge* b)
{
	auto window = static_cast<BridgePropertiesControl*>(callbackArg);
	window->LoadStpEnabledCheckBox();
}

//static
void BridgePropertiesControl::OnSelectedBridgeStpVersionChanged (void* callbackArg, Bridge* b)
{
	auto window = static_cast<BridgePropertiesControl*>(callbackArg);
	window->LoadStpVersionCheckBox();
}

void BridgePropertiesControl::LoadBridgeAddressTextBox()
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

void BridgePropertiesControl::LoadStpEnabledCheckBox()
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

void BridgePropertiesControl::LoadStpVersionCheckBox()
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

//static
void BridgePropertiesControl::OnSelectionChanged (void* callbackArg, ISelection* selection)
{
	auto window = static_cast<BridgePropertiesControl*>(callbackArg);

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
		window->LoadStpVersionCheckBox();

		::ShowWindow (window->GetHWnd(), SW_SHOW);
	}
}

void BridgePropertiesControl::PostWork (std::function<void()>&& work)
{
	_workQueue.push(move(work));
	PostMessage (_hwnd, WM_WORK, 0, 0);
}

std::wstring BridgePropertiesControl::GetEditPropertyText(HWND hwnd) const
{
	if (hwnd == _bridgeAddressEdit)
	{
		auto bridge = dynamic_cast<Bridge*>(_selection->GetObjects()[0].Get());
		return bridge->GetMacAddressAsString();
	}
	else
		throw not_implemented_exception();
}

bool BridgePropertiesControl::ValidateAndSetProperty (HWND hwnd, const std::wstring& str, std::wstring& errorMessageOut)
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
	else
		throw not_implemented_exception();
}

bool BridgePropertiesControl::BridgesSelected() const
{
	return !_selection->GetObjects().empty()
		&& all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), [](const ComPtr<Object>& o) { return dynamic_cast<Bridge*>(o.Get()) != nullptr; });
}

