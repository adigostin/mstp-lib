
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "resource.h"

using namespace edge;
using namespace pg;

static constexpr UINT WM_SHOWN = WM_APP + 1;

class MSTConfigIdEditor : public ICustomPropertyEditor
{
	ULONG _refCount = 0;
	vector_nothrow<IBridge*> _bridges;
	HWND _hwnd = nullptr;

public:
	HRESULT InitInstance (IObjectList& objects)
	{
		RETURN_HR_IF(E_INVALIDARG, objects.empty());

		for (IDispatch* o : objects)
		{
			if (auto b = wil::try_com_query_nothrow<IBridge>(o))
			{
				if (std::find(_bridges.begin(), _bridges.end(), b) == _bridges.end())
					_bridges.try_push_back(b);
			}
			else if (auto p = wil::try_com_query_nothrow<IPort>(objects[0]))
			{
				auto b = p->bridge();
				if (std::find(_bridges.begin(), _bridges.end(), b) == _bridges.end())
					_bridges.try_push_back(b);
			}
			else
				RETURN_HR(E_INVALIDARG);
		}

//		_project->property_changing().add_handler<&MSTConfigIdEditor::on_project_property_changing>(this);
		return S_OK;
	}

	~MSTConfigIdEditor()
	{
//		_project->property_changing().remove_handler<&MSTConfigIdEditor::on_project_property_changing>(this);
	}

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(this, riid, ppvObject)
			|| TryQI<ICustomPropertyEditor>(this, riid, ppvObject)
			)
			return S_OK;

		RETURN_HR(E_NOINTERFACE);
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

/*
	void on_project_property_changing (IDispatch* o, const edge::property_change_args& args)
	{
		if (args.property == _project->bridges_property())
		{
			auto& cpargs = dynamic_cast<const edge::collection_property_change_args&>(args);
			if (cpargs.type == edge::collection_property_change_type::remove)
			{
				auto bridge_being_removed = _project->bridges()[cpargs.index].get();
				if (_bridges.count(bridge_being_removed))
					::EndDialog (_hwnd, IDCANCEL);
			}
		}
	}
*/
	virtual HRESULT STDMETHODCALLTYPE ShowModal (HWND hWndParent, VARIANT* pvarSelectedValue) override
	{
		INT_PTR dr = DialogBoxParam (GetModuleHandle(nullptr), MAKEINTRESOURCE(IDD_DIALOG_MST_CONFIG_ID), hWndParent, &DialogProcStatic, (LPARAM) this);
		if (dr == IDOK)
			return S_OK;
		else if (dr == IDCANCEL)
			return S_FALSE;

		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE Cancel() override
	{
		::EndDialog (_hwnd, IDCANCEL);
		return S_OK;
	}

	static INT_PTR CALLBACK DialogProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		MSTConfigIdEditor* window;
		if (uMsg == WM_INITDIALOG)
		{
			window = reinterpret_cast<MSTConfigIdEditor*>(lParam);
			window->_hwnd = hwnd;
			WI_ASSERT (GetWindowLongPtr(hwnd, GWLP_USERDATA) == 0);
			SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
		}
		else
			window = reinterpret_cast<MSTConfigIdEditor*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

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

		if (msg == WM_SHOWWINDOW)
		{
			::PostMessage (_hwnd, WM_SHOWN, 0, 0);
			return { TRUE, 0 };
		}

		if (msg == WM_SHOWN)
		{
			auto it = _bridges.begin();
			auto msti_count = (*it)->msti_count();
			while (++it != _bridges.end())
			{
				if ((*it)->msti_count() != msti_count)
				{
					::MessageBoxA (_hwnd, "There are multiple bridges selected and they have different values for MSTI Count.\r\n"
						"The Simulator does not currently support editing their MST Config Tables at the same time. Try editing it one by one.",
						"Multiple Selection", 0);
					::EndDialog (_hwnd, IDCANCEL);
					return { TRUE, 0 };
				}
			}

			return { TRUE, 0 };
		}

		if (msg == WM_CTLCOLORSTATIC)
		{
			HDC hdc = (HDC)wParam;
			::SetBkMode (hdc, TRANSPARENT);
			return { (INT_PTR) GetSysColorBrush(COLOR_INFOBK), 0 };
		}

		if (msg == WM_COMMAND)
		{
			if (wParam == IDOK)
			{
				if (TryApply())
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

		const unsigned char* digest = STP_GetMstConfigId((*_bridges.begin())->stp_bridge())->ConfigurationDigest;
		bool allSameDigest = std::all_of(_bridges.begin(), _bridges.end(), [&](IBridge* b)
			{ return memcmp (digest, STP_GetMstConfigId(b->stp_bridge())->ConfigurationDigest, 16) == 0; });

		LVCOLUMN lvc = { 0 };
		lvc.mask = LVCF_TEXT | LVCF_WIDTH;
		lvc.pszText = const_cast<wchar_t*>(L"VLAN");
		lvc.cx = (allSameDigest ? 80 : 120) * dpi / 96;
		ListView_InsertColumn (list, 0, &lvc);
		lvc.pszText = const_cast<wchar_t*>(L"Tree");
		lvc.cx = (allSameDigest ? 80 : 40) * dpi / 96;
		ListView_InsertColumn (list, 1, &lvc);

		if (allSameDigest)
		{
			unsigned entryCount;
			const STP_CONFIG_TABLE_ENTRY* entries = STP_GetMstConfigTable(_bridges[0]->stp_bridge(), &entryCount);
			LoadUI (entries, entryCount);
		}
		else
		{
			LVITEM lvi = { 0 };
			lvi.mask = LVIF_TEXT;
			lvi.pszText = const_cast<wchar_t*>(L"(multiple selection)");
			ListView_InsertItem (list, &lvi);
		}

		HWND hint = GetDlgItem (_hwnd, IDC_STATIC_HINT_NOT_MSTP);
		bool showHint = std::any_of (_bridges.begin(), _bridges.end(), [](IBridge* b) { return STP_GetStpVersion(b->stp_bridge()) < STP_VERSION_MSTP; });
		auto style = ::GetWindowLongPtr (hint, GWL_STYLE);
		style = (style & ~WS_VISIBLE) | (showHint ? WS_VISIBLE : 0);
		::SetWindowLongPtr (hint, GWL_STYLE, style);
	}

	bool TryApply()
	{
		HWND list = GetDlgItem (_hwnd, IDC_LIST_CONFIG_TABLE);
		if (ListView_GetItemCount(list) != 1 + max_vlan_number)
			return false;

		wchar_t buffer[5];
		LV_ITEM lvi = { .iSubItem = 1, .pszText = buffer, .cchTextMax = _countof(buffer) - 1 };
		STP_CONFIG_TABLE_ENTRY entries [1 + max_vlan_number] = { };
		for (unsigned vlanNumber = 0; vlanNumber <= max_vlan_number; vlanNumber++)
		{
			LRESULT len = ::SendMessage(list, LVM_GETITEMTEXT, vlanNumber, reinterpret_cast<LPARAM>(&lvi));
			buffer[len] = 0;
			entries[vlanNumber].treeIndex = (unsigned char)std::wcstoul(buffer, nullptr, 10);
		}

		for (IBridge* b : _bridges)
			STP_SetMstConfigTable(b->stp_bridge(), entries, _countof(entries), (unsigned)::GetMessageTime());
		return true;
	}

	void LoadUI (const STP_CONFIG_TABLE_ENTRY* entries, unsigned entryCount)
	{
		HWND list = GetDlgItem (_hwnd, IDC_LIST_CONFIG_TABLE);
		ListView_DeleteAllItems(list);

		LVITEM lvi = { 0 };
		lvi.mask = LVIF_TEXT;

		for (unsigned int vlanNumber = 0; vlanNumber <= max_vlan_number; vlanNumber++)
		{
			lvi.iItem = vlanNumber;

			std::wstring text = std::to_wstring(vlanNumber);
			lvi.iSubItem = 0;
			lvi.pszText = const_cast<wchar_t*>(text.c_str());
			ListView_InsertItem (list, &lvi);

			auto treeIndex = entries[vlanNumber].treeIndex;
			text = std::to_wstring (treeIndex);
			lvi.iSubItem = 1;
			lvi.pszText = const_cast<wchar_t*>(text.c_str());
			ListView_SetItem (list, &lvi);
		}
	}

	void LoadDefaultConfig()
	{
		STP_CONFIG_TABLE_ENTRY entries[1 + max_vlan_number];
		memset (entries, 0, sizeof(entries));
		LoadUI (entries, _countof(entries));
	}

	void LoadTestConfig1()
	{
		auto treeCount = 1 + STP_GetMstiCount(_bridges[0]->stp_bridge());

		STP_CONFIG_TABLE_ENTRY entries[1 + max_vlan_number];

		// VLAN0 does not exist.
		entries[0] = { 0, 0 };

		unsigned char treeIndex = 0;
		for (unsigned int vid = 1; vid <= max_vlan_number; vid++)
		{
			entries[vid] = { .unused = 0, .treeIndex = treeIndex };
			treeIndex++;
			if (treeIndex == treeCount)
				treeIndex = 0;
		}

		LoadUI(entries, _countof(entries));
	}
};

HRESULT CreateMSTConfigIdEditor (IObjectList* objs, ICustomPropertyEditor** ppEditor)
{
	auto p = com_ptr(new (std::nothrow) MSTConfigIdEditor()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(*objs); RETURN_IF_FAILED(hr);
	*ppEditor = p.detach();
	return S_OK;
}

class MSTConfigIdEditorFactory : public ICustomPropertyEditorFactory
{
	ULONG _refCount = 0;

public:
	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(this, riid, ppvObject)
			|| TryQI<ICustomPropertyEditorFactory>(this, riid, ppvObject)
		)
			return S_OK;

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	#pragma region ICustomPropertyEditorFactory
	virtual HRESULT STDMETHODCALLTYPE CreateEditor (IObjectList* objs, ICustomPropertyEditor** ppEditor) override
	{
		return CreateMSTConfigIdEditor(objs, ppEditor);
	}
	#pragma endregion
};

HRESULT MakeMSTConfigIdEditorFactory (pg::ICustomPropertyEditorFactory** ppFactory)
{
	auto p = com_ptr(new (std::nothrow) MSTConfigIdEditorFactory()); RETURN_IF_NULL_ALLOC(p);
	*ppFactory = p.detach();
	return S_OK;
}
