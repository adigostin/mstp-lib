
#pragma once
#include "Simulator.h"

class BridgePropertiesControl
{
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

	ISelection* const _selection;
	HWND _hwnd = nullptr;
	HWND _bridgeAddressEdit = nullptr;
	WNDPROC _bridgeAddressEditOriginalProc;
	HWND _checkStpEnabled = nullptr;
	HWND _comboStpVersion = nullptr;
	HWND _controlBeingValidated = nullptr;
	std::queue<std::function<void()>> _workQueue;

	static INT_PTR CALLBACK DialogProcStatic (HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	Result DialogProc (UINT msg, WPARAM wParam , LPARAM lParam);
	static void OnSelectionChanged (void* callbackArg, ISelection* selection);
	static void OnAddedToSelection (void* callbackArg, ISelection* selection, Object* o);
	static void OnRemovingFromSelection (void* callbackArg, ISelection* selection, Object* o);
	static void OnSelectedBridgeAddressChanged (void* callbackArg, Bridge* b);
	static void OnSelectedBridgeStpEnabledChanged (void* callbackArg, Bridge* b);
	static void OnSelectedBridgeStpVersionChanged (void* callbackArg, Bridge* b);
	static LRESULT CALLBACK EditSubclassProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	void PostWork (std::function<void()>&& work);
	std::wstring GetEditPropertyText (HWND hwnd) const;
	bool ValidateAndSetProperty (HWND hwnd, const std::wstring& newText, std::wstring& errorMessageOut);
	void LoadBridgeAddressTextBox();
	void LoadStpEnabledCheckBox();
	void LoadStpVersionCheckBox();
	void ProcessStpEnabledClicked();
	void ProcessStpVersionSelChanged();
	bool BridgesSelected() const;
};

