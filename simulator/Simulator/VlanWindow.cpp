
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "Simulator_.h"
#include "resource.h"

class VlanWindowImpl
	: public IVlanWindow
	, IPropertyChangeSink
	, IProjectWindowEventsSink
	, IObjectCollectionChangeEvents
	, IVlanSelection
	, IConnectionPointContainer
{
	ULONG _refCount = 0;
	ULONG _sig = 0xAA550007;
	WeakRefToThis _weakRefToThis;
	ISimulatorApp*  _app;
	IProjectWindow* _pw;
	DWORD _vlan;
	com_ptr<IStpProject> _project;
	ISelection*    _selection;
	HWND _hwnd = nullptr;
	std::unordered_map<IBridge*, AdviseSinkToken> _adviseSinkTokens;
	AdviseSinkToken _projectWindowEventsToken;
	AdviseSinkToken _collectionChangeToken;
	com_ptr<ConnectionPointImpl<IVlanSelectionEvents>> _vlanSelectionEventsCP;

public:
	HRESULT InitInstance (const VlanWindowCreateParams* params)
	{
		auto hr = _weakRefToThis.InitInstance(AsUnknown()); RETURN_IF_FAILED(hr);

		_app = params->app;
		_pw = params->pw;
		_project = params->project;
		_selection = params->selection;
		_vlan = params->vlan;
	
		hr = MakeConnectionPoint (this, &_vlanSelectionEventsCP); RETURN_IF_FAILED(hr);

		_hwnd = CreateDialogParam ((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG_VLAN), params->hWndParent, &DialogProcStatic, reinterpret_cast<LPARAM>(this));

		RECT rc;
		::GetWindowRect(_hwnd, &rc);
		::MoveWindow (_hwnd, params->location.x, params->location.y, rc.right - rc.left, rc.bottom - rc.top, TRUE);

		hr = AdviseSink<IObjectCollectionChangeEvents>(_selection, _weakRefToThis, &_collectionChangeToken); RETURN_IF_FAILED(hr);

		hr = AdviseSink<IProjectWindowEventsSink>(_pw, _weakRefToThis, &_projectWindowEventsToken); RETURN_IF_FAILED(hr);

		for (auto o : *_selection)
		{
			if (auto b = wil::try_com_query_nothrow<IBridge>(o))
			{
				//b->property_changed().add_handler<&VlanWindowImpl::on_bridge_property_changed>(this);
				AdviseSinkToken token;
				hr = AdviseSink<IPropertyChangeSink>(b, _weakRefToThis, &token); LOG_IF_FAILED(hr);
				_adviseSinkTokens[b] = std::move(token);
			}
		}

		return S_OK;
	}

	~VlanWindowImpl()
	{
		for (auto o : *_selection)
		{
			if (auto b = wil::try_com_query_nothrow<IBridge>(o))
				//b->property_changed().remove_handler<&VlanWindowImpl::on_bridge_property_changed>(this);
				_adviseSinkTokens.erase(b);
		}

		if (_hwnd != nullptr)
			::DestroyWindow(_hwnd);
	}

	IUnknown* AsUnknown() { return static_cast<IVlanWindow*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IVlanWindow>(this, riid, ppvObject)
			|| TryQI<IPropertyChangeSink>(this, riid, ppvObject)
			|| TryQI<IProjectWindowEventsSink>(this, riid, ppvObject)
			|| TryQI<IObjectCollectionChangeEvents>(this, riid, ppvObject)
			|| TryQI<IVlanSelection>(this, riid, ppvObject)
			|| TryQI<IConnectionPointContainer>(this, riid, ppvObject)
		)
			return S_OK;

		if (riid == __uuidof(IWeakRef))
			return _weakRefToThis.QueryIWeakRef(ppvObject);

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	#pragma region IConnectionPointContainer
	virtual HRESULT STDMETHODCALLTYPE EnumConnectionPoints (IEnumConnectionPoints **ppEnum) override
	{
		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE FindConnectionPoint (REFIID riid, IConnectionPoint **ppCP) override
	{
		if (riid == __uuidof(IVlanSelectionEvents))
			return wil::com_query_to_nothrow(_vlanSelectionEventsCP, ppCP);
		RETURN_HR(E_NOTIMPL);
	}
	#pragma endregion

	#pragma region IVlanSelection
	virtual HRESULT STDMETHODCALLTYPE GetSelectedVlan (DWORD* pdwVlan) override
	{
		*pdwVlan = _vlan;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE SelectVlan (DWORD dwVlan) override
	{
		RETURN_HR_IF(E_INVALIDARG, dwVlan == 0 || dwVlan > max_vlan_number);
		if (_vlan != dwVlan)
		{
			_vlanSelectionEventsCP->Notify([this] (IVlanSelectionEvents* e) { return e->OnVlanSelectionChanging(_vlan); });
			_vlan = dwVlan;
			_vlanSelectionEventsCP->Notify([this] (IVlanSelectionEvents* e) { return e->OnVlanSelectionChanged(_vlan); });
			LoadSelectedTreeEdit();
			auto comboVlan = GetDlgItem(_hwnd, IDC_COMBO_SELECTED_VLAN);
			ComboBox_SetCurSel(comboVlan, static_cast<int>(_vlan - 1));
		}
		return S_OK;
	}
	#pragma endregion

	static constexpr auto is_bridge = [](IDispatch* o) { return wil::try_com_query_nothrow<IBridge>(o); };

	static constexpr auto is_port = [](IDispatch* o) { return wil::try_com_query_nothrow<IPort>(o); };

	static constexpr auto is_bridge_or_port = [](IDispatch* o) { return is_bridge(o) || is_port(o); };

	virtual HWND HWnd() const override final { return _hwnd; }

	virtual SIZE PreferredSize() const override final
	{
		RECT rect;
		::GetWindowRect(GetDlgItem(_hwnd, IDC_STATIC_EXTENT), &rect);
		if (auto proc_addr = GetProcAddress(GetModuleHandleA("User32.dll"), "AdjustWindowRectExForDpi"))
		{
			auto get_dpi_proc_addr = GetProcAddress(GetModuleHandleA("User32.dll"), "GetDpiForWindow");
			auto get_dpi_proc = reinterpret_cast<UINT(WINAPI*)(HWND)>(get_dpi_proc_addr);
			UINT dpi = get_dpi_proc(_hwnd);

			auto proc = reinterpret_cast<BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT)>(proc_addr);
			BOOL bRes = proc (&rect, GetWindowStyle(_hwnd), FALSE, GetWindowExStyle(_hwnd), dpi); WI_ASSERT(bRes);
			return { rect.right - rect.left, rect.bottom - rect.top };
		}
		else
		{
			HDC tempDC = GetDC(_hwnd);
			UINT dpi = GetDeviceCaps (tempDC, LOGPIXELSX);
			ReleaseDC (_hwnd, tempDC);
			BOOL bRes = AdjustWindowRectEx (&rect, GetWindowStyle(_hwnd), FALSE, GetWindowExStyle(_hwnd)); WI_ASSERT(bRes);
			return { rect.right - rect.left, rect.bottom - rect.top };
		}
	}

	virtual HRESULT STDMETHODCALLTYPE GetVlanSelection (IVlanSelection** ppVlanSelection) override
	{
		*ppVlanSelection = this;
		(*ppVlanSelection)->AddRef();
		return S_OK;
	}

	static INT_PTR CALLBACK DialogProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		VlanWindowImpl* window;
		if (uMsg == WM_INITDIALOG)
		{
			window = reinterpret_cast<VlanWindowImpl*>(lParam);
			//window->AddRef();
			window->_hwnd = hwnd;
			WI_ASSERT (GetWindowLongPtr(hwnd, GWLP_USERDATA) == 0);
			SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
		}
		else
			window = reinterpret_cast<VlanWindowImpl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

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
			//window->Release(); // this one last cause it might call Release() which would try to destroy the hwnd again.
		}

		::SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result.messageResult);
		return result.dialogProcResult;
	}

	DialogProcResult DialogProc (UINT msg, WPARAM wParam , LPARAM lParam)
	{
		if (msg == WM_INITDIALOG)
		{
			auto comboSelectedVlan  = GetDlgItem (_hwnd, IDC_COMBO_SELECTED_VLAN);
			auto comboNewWindowVlan = GetDlgItem (_hwnd, IDC_COMBO_NEW_WINDOW_VLAN);
			for (size_t i = 1; i <= max_vlan_number; i++)
			{
				auto str = std::to_wstring(i);
				ComboBox_AddString(comboSelectedVlan, str.c_str());
				ComboBox_AddString(comboNewWindowVlan, str.c_str());
			}
			LoadSelectedVlanCombo();
			LoadSelectedTreeEdit();
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

			return { reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW)), 0 };
		}

		if (msg == WM_COMMAND)
		{
			if ((HIWORD(wParam) == CBN_SELCHANGE) && (LOWORD(wParam) == IDC_COMBO_SELECTED_VLAN))
			{
				ProcessVlanSelChange ((HWND) lParam);
				return { TRUE, 0 };
			}

			if ((HIWORD(wParam) == CBN_SELCHANGE) && (LOWORD(wParam) == IDC_COMBO_NEW_WINDOW_VLAN))
			{
				ProcessNewWindowVlanSelChange ((HWND) lParam);
				return { TRUE, 0 };
			}

			if ((HIWORD(wParam) == BN_CLICKED) && (LOWORD(wParam) == IDC_BUTTON_EDIT_MST_CONFIG_TABLE))
			{
				if (_selection->all(is_bridge) || _selection->all(is_port))
				{
					com_ptr<pg::ICustomPropertyEditor> editor;
					auto hr = CreateMSTConfigIdEditor(_selection, &editor); _ASSERT(SUCCEEDED(hr));
					wil::unique_variant unused;
					hr = editor->ShowModal (_hwnd, &unused, false); _ASSERT(SUCCEEDED(hr));
				}
				else
					MessageBox (_hwnd, L"Select some bridges or ports first.", _app->app_name(), 0);

				return { TRUE, 0 };
			}

			return { FALSE, 0 };
		}

		return { FALSE, 0 };
	}

	#pragma region IPropertyChangeSink
	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanging (IUnknown* obj, DISPID dispID, const PropertyChangeArgs* args) override
	{
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanged  (IUnknown* obj, DISPID dispID, const PropertyChangeArgs* args) override
	{
		LoadSelectedTreeEdit();
		return S_OK;
	}
	#pragma endregion

	#pragma region IObjectCollectionChangeEvents
	virtual HRESULT STDMETHODCALLTYPE OnCollectionChanging (IUnknown* sender, const ObjectCollectionChangeArgs* args) override
	{
		if (args->changeType == Remove)
		{
			// Removing
			auto& a = args->setInsertRemoveArgs;
			for (uint32_t i = a.index; i < a.index + a.count; i++)
			{
				if (auto b = wil::try_com_query_nothrow<IBridge>(_selection->operator[](i)))
					_adviseSinkTokens.erase(b);
			}
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnCollectionChanged (IUnknown* sender, const ObjectCollectionChangeArgs* args) override
	{
		if (args->changeType == Insert)
		{
			// Inserted
			auto& a = args->setInsertRemoveArgs;
			for (uint32_t i = a.index; i < a.index + a.count; i++)
			{
				if (auto b = wil::try_com_query_nothrow<IBridge>(_selection->operator[](i)))
				{
					AdviseSinkToken token;
					auto hr = AdviseSink<IPropertyChangeSink>(b, _weakRefToThis, &token); LOG_IF_FAILED(hr);
					if (SUCCEEDED(hr))
						_adviseSinkTokens[b] = std::move(token);
				}
			}
		}

		LoadSelectedTreeEdit();

		return S_OK;
	}
	#pragma endregion

	#pragma region IProjectWindowEventsSink
	virtual HRESULT STDMETHODCALLTYPE OnProjectWindowClosed (IProjectWindow* pw) override
	{
		return S_OK;
	}
	#pragma endregion

	HRESULT ProcessVlanSelChange (HWND hwnd)
	{
		DWORD newVlanNumber = 1 + ComboBox_GetCurSel(hwnd);
		if (_vlan != newVlanNumber)
		{
			_vlanSelectionEventsCP->Notify([this] (IVlanSelectionEvents* e) { return e->OnVlanSelectionChanging(_vlan); });
			_vlan = newVlanNumber;
			_vlanSelectionEventsCP->Notify([this] (IVlanSelectionEvents* e) { return e->OnVlanSelectionChanged(_vlan); });
			LoadSelectedTreeEdit();
		}

		return S_OK;
	}

	void ProcessNewWindowVlanSelChange (HWND hwnd)
	{
		auto index = ComboBox_GetCurSel(hwnd);
		auto vlanNumber = (unsigned int) (index + 1);
		auto hr = _app->OpenWindowForVlan(_project, vlanNumber, nullptr); LOG_IF_FAILED(hr);
		ComboBox_SetCurSel (hwnd, -1);
	}

	void LoadSelectedVlanCombo()
	{
		ComboBox_SetCurSel (GetDlgItem (_hwnd, IDC_COMBO_SELECTED_VLAN), _vlan - 1);
	}

	void LoadSelectedTreeEdit()
	{
		auto edit = GetDlgItem (_hwnd, IDC_EDIT_SELECTED_TREE); WI_ASSERT (edit != nullptr);
		auto tableButton = GetDlgItem (_hwnd, IDC_BUTTON_EDIT_MST_CONFIG_TABLE); WI_ASSERT (tableButton != nullptr);
		auto& objects = *_selection;

		if (objects.empty() || !_selection->all(is_bridge_or_port))
		{
			::SetWindowText (edit, L"(no bridge selected)");
			::EnableWindow (edit, FALSE);
			::EnableWindow (tableButton, FALSE);
			return;
		}

		::EnableWindow (edit, TRUE);
		::EnableWindow (tableButton, TRUE);

		std::unordered_set<IBridge*> bridges;
		for (auto o : *_selection)
		{
			if (auto b = wil::try_com_query_nothrow<IBridge>(o))
				bridges.insert(b);
			else if (auto p = wil::try_com_query_nothrow<IPort>(o))
				bridges.insert(p->bridge());
			else
				WI_ASSERT(false);
		}

		auto tree = STP_GetTreeIndexFromVlanNumber ((*bridges.begin())->stp_bridge(), _vlan);
		bool all_same_tree = all_of (bridges.begin(), bridges.end(), [tree, vlan=_vlan](IBridge* b)
			{ return STP_GetTreeIndexFromVlanNumber(b->stp_bridge(), vlan) == tree; });
		if (!all_same_tree)
		{
			::SetWindowTextA (edit, "(multiple selection)");
			return;
		}

		IBridge* bridge = *bridges.begin();
		auto treeIndex = STP_GetTreeIndexFromVlanNumber (bridge->stp_bridge(), _vlan);
		if (treeIndex == 0)
			::SetWindowTextA (edit, "CIST (0)");
		else
			::SetWindowTextA (edit, (std::string("MSTI ") + std::to_string(treeIndex)).c_str());
	}
};

HRESULT CreateVlanWindow (const VlanWindowCreateParams* params, IVlanWindow** ppVlanWindow)
{
	auto p = com_ptr(new (std::nothrow) VlanWindowImpl()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(params); RETURN_IF_FAILED(hr);
	*ppVlanWindow = p.detach();
	return S_OK;
}
