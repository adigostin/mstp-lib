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
}


BridgePropertiesControl::~BridgePropertiesControl()
{
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

static const UINT_PTR BridgeAddressSubClassId = 1;

BridgePropertiesControl::Result BridgePropertiesControl::DialogProc (UINT msg, WPARAM wParam , LPARAM lParam)
{
	if (msg == WM_INITDIALOG)
	{
		_bridgeAddressEdit = GetDlgItem (_hwnd, IDC_EDIT_BRIDGE_ADDRESS);
		BOOL bRes = SetWindowSubclass (_bridgeAddressEdit, BridgeAddressEditSubclassProc, BridgeAddressSubClassId, (DWORD_PTR) this); assert (bRes);
		return { FALSE, 0 };
	}
	
	if (msg == WM_DESTROY)
	{
		BOOL bRes = RemoveWindowSubclass (_bridgeAddressEdit, BridgeAddressEditSubclassProc, BridgeAddressSubClassId); assert (bRes);
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

	return { FALSE, 0 };
}

//static
LRESULT CALLBACK BridgePropertiesControl::BridgeAddressEditSubclassProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	auto dialog = (BridgePropertiesControl*) (void*) dwRefData;
	
	if ((msg == WM_CHAR) && (wParam == VK_RETURN))
	{
		Edit_SetSel (dialog->_bridgeAddressEdit, 0, -1);
		return 0;
	}

	if (msg == WM_KEYDOWN)
	{
		if (wParam == VK_RETURN)
		{
			if (!dialog->_validating)
			{
				dialog->_validating = true;
				std::wstring errorMessage;
				bool valid = dialog->ValidateAndSetBridgeAddress(errorMessage);
				if (!valid)
				{
					MessageBox (dialog->_hwnd, errorMessage.c_str(), 0, 0);
					SetFocus (dialog->_bridgeAddressEdit);
					Edit_SetSel (dialog->_bridgeAddressEdit, 0, -1);
				}
				dialog->_validating = false;
			}

			return 0;
		}

		return DefSubclassProc (hWnd, msg, wParam, lParam);
	}

	if (msg == WM_KILLFOCUS)
	{
		if (!dialog->_validating)
		{
			dialog->_validating = true;
			std::wstring errorMessage;
			bool valid = dialog->ValidateAndSetBridgeAddress(errorMessage);
			if (valid)
			{
				dialog->_validating = false;
			}
			else
			{
				::SetFocus(nullptr);
				dialog->PostWork ([dialog, errorMessage]
				{
					MessageBox (dialog->_hwnd, errorMessage.c_str(), 0, 0);
					SetFocus (dialog->_bridgeAddressEdit);
					Edit_SetSel (dialog->_bridgeAddressEdit, 0, -1);
					dialog->_validating = false;
				});
			}
		}

		return DefSubclassProc (hWnd, msg, wParam, lParam);
	}

	return DefSubclassProc (hWnd, msg, wParam, lParam);
}

bool BridgePropertiesControl::ValidateAndSetBridgeAddress (std::wstring& errorMessageOut)
{
	std::wstring str;
	str.resize(GetWindowTextLength (_bridgeAddressEdit) + 1);
	GetWindowText (_bridgeAddressEdit, str.data(), str.size());

	if (!iswxdigit(str[0]) || !iswxdigit(str[1]))
	{
		errorMessageOut = L"Invalid address format. The Bridge Address must have the format XX:XX:XX:XX:XX:XX.";
		return false;
	}

	return true;
}

//static
void BridgePropertiesControl::OnSelectionChanged (void* callbackArg, ISelection* selection)
{
	auto window = static_cast<BridgePropertiesControl*>(callbackArg);

	bool bridgesSelected = !selection->GetObjects().empty()
		&& all_of (selection->GetObjects().begin(), selection->GetObjects().end(), [](const ComPtr<Object>& o) { return dynamic_cast<Bridge*>(o.Get()) != nullptr; });

	if (bridgesSelected)
	{
		if (selection->GetObjects().size() == 1)
		{
			auto bridge = dynamic_cast<Bridge*>(selection->GetObjects()[0].Get());
			auto addr = bridge->GetMacAddress();
			wchar_t str[32];
			swprintf_s (str, L"%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
			::SetWindowText (window->_bridgeAddressEdit, str);
			::EnableWindow (window->_bridgeAddressEdit, TRUE);
		}
		else
		{
			::SetWindowText (window->_bridgeAddressEdit, L"(multiple selection)");
			::EnableWindow (window->_bridgeAddressEdit, FALSE);
		}

		::ShowWindow (window->GetHWnd(), SW_SHOW);
	}
	else
		::ShowWindow (window->GetHWnd(), SW_HIDE);
}

void BridgePropertiesControl::PostWork (std::function<void()>&& work)
{
	_workQueue.push(move(work));
	PostMessage (_hwnd, WM_WORK, 0, 0);
}
