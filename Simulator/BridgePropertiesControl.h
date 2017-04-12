
#pragma once
#include "Simulator.h"

class BridgePropertiesControl
{
	HWND _hwnd = nullptr;

public:
	BridgePropertiesControl (HWND hwndParent, const RECT& rect);
	~BridgePropertiesControl();

private:

	struct Result
	{
		INT_PTR dialogProcResult;
		LRESULT messageResult;
	};

	static INT_PTR CALLBACK DialogProcStatic (HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	Result DialogProc (UINT msg, WPARAM wParam , LPARAM lParam);
};

