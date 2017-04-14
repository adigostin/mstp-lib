
#pragma once
#include "Simulator.h"

class BridgePropertiesControl
{
	ISelection* const _selection;
	HWND _hwnd = nullptr;
	HWND _bridgeAddressEdit = nullptr;
	WNDPROC _bridgeAddressEditOriginalProc;

public:
	BridgePropertiesControl (HWND hwndParent, const RECT& rect, ISelection* selection);
	~BridgePropertiesControl();

	HWND GetHWnd() const { return _hwnd; }

private:

	struct Result
	{
		INT_PTR dialogProcResult;
		LRESULT messageResult;
	};

	static INT_PTR CALLBACK DialogProcStatic (HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	Result DialogProc (UINT msg, WPARAM wParam , LPARAM lParam);
	static void OnSelectionChanged (void* callbackArg, ISelection* selection);
	static LRESULT CALLBACK BridgeAddressEditSubclassProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	bool ValidateAndSetBridgeAddress();
};

