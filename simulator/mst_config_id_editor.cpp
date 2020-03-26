
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "resource.h"
#include "bridge.h"
#include "win32/utility_functions.h"

static constexpr UINT WM_SHOWN = WM_APP + 1;

class mst_config_id_editor : public edge::property_editor_i
{
	project_i* _project;
	std::unordered_set<bridge*> _bridges;
	HWND _hwnd = nullptr;

public:
	mst_config_id_editor (std::span<object* const> objects)
	{
		assert (!objects.empty());

		if (auto first = dynamic_cast<bridge*>(objects[0]))
		{
			_project = first->project();
			for (auto o : objects)
			{
				auto b = dynamic_cast<bridge*>(o);
				assert (b != nullptr);
				assert (b->project() == _project);
				_bridges.insert(b);
			}
		}
		else if (auto first = dynamic_cast<port*>(objects[0]))
		{
			_project = first->bridge()->project();
			for (auto o : objects)
			{
				auto p = dynamic_cast<port*>(o);
				assert (p != nullptr);
				assert (p->bridge()->project() == _project);
				_bridges.insert(p->bridge());
			}
		}
		else
			assert(false);

		_project->property_changing().add_handler<&mst_config_id_editor::on_project_property_changing>(this);
	}

	~mst_config_id_editor()
	{
		_project->property_changing().remove_handler<&mst_config_id_editor::on_project_property_changing>(this);
	}

	void on_project_property_changing (object* o, const edge::property_change_args& e)
	{
		if ((e.property == _project->bridges_prop()) && (e.type == collection_property_change_type::remove))
		{
			auto bridge_being_removed = _project->bridges()[e.index].get();
			if (_bridges.count(bridge_being_removed))
				::EndDialog (_hwnd, IDCANCEL);
		}
	}

	virtual bool show (edge::win32_window_i* parent) override
	{
		INT_PTR dr = DialogBoxParam (GetModuleHandle(nullptr), MAKEINTRESOURCE(IDD_DIALOG_MST_CONFIG_ID), parent->hwnd(), &DialogProcStatic, (LPARAM) this);
		return (dr == IDOK);
	}

	virtual void cancel() override
	{
		::EndDialog (_hwnd, IDCANCEL);
	}

	static INT_PTR CALLBACK DialogProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		mst_config_id_editor* window;
		if (uMsg == WM_INITDIALOG)
		{
			window = reinterpret_cast<mst_config_id_editor*>(lParam);
			window->_hwnd = hwnd;
			assert (GetWindowLongPtr(hwnd, GWLP_USERDATA) == 0);
			SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
		}
		else
			window = reinterpret_cast<mst_config_id_editor*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

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
				if (ValidateAndApply())
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
		bool allSameDigest = all_of(_bridges.begin(), _bridges.end(), [&](bridge* b)
			{ return memcmp (digest, STP_GetMstConfigId(b->stp_bridge())->ConfigurationDigest, 16) == 0; });

		LVCOLUMN lvc = { 0 };
		lvc.mask = LVCF_TEXT | LVCF_WIDTH;
		lvc.pszText = L"VLAN";
		lvc.cx = (allSameDigest ? 80 : 120) * dpi / 96;
		ListView_InsertColumn (list, 0, &lvc);
		lvc.pszText = L"Tree";
		lvc.cx = (allSameDigest ? 80 : 40) * dpi / 96;
		ListView_InsertColumn (list, 1, &lvc);

		if (allSameDigest)
			LoadTable (list, *_bridges.begin());
		else
		{
			LVITEM lvi = { 0 };
			lvi.mask = LVIF_TEXT;
			lvi.pszText = L"(multiple selection)";
			ListView_InsertItem (list, &lvi);
		}

		HWND hint = GetDlgItem (_hwnd, IDC_STATIC_HINT_NOT_MSTP);
		bool showHint = any_of (_bridges.begin(), _bridges.end(), [](bridge* b) { return STP_GetStpVersion(b->stp_bridge()) < STP_VERSION_MSTP; });
		auto style = ::GetWindowLongPtr (hint, GWL_STYLE);
		style = (style & ~WS_VISIBLE) | (showHint ? WS_VISIBLE : 0);
		::SetWindowLongPtr (hint, GWL_STYLE, style);
	}

	bool ValidateAndApply()
	{
		return true;
	}

	void LoadTable (HWND list, bridge* bridge)
	{
		LVITEM lvi = { 0 };
		lvi.mask = LVIF_TEXT;

		unsigned int entryCount;
		auto entries = STP_GetMstConfigTable (bridge->stp_bridge(), &entryCount);

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
		std::vector<STP_CONFIG_TABLE_ENTRY> entries;
		entries.resize(1 + max_vlan_number);

		for (auto b : _bridges)
			b->SetMstConfigTable (entries.data(), (unsigned int) entries.size());

		HWND list = GetDlgItem (_hwnd, IDC_LIST_CONFIG_TABLE);
		ListView_DeleteAllItems(list);
		LoadTable (list, *_bridges.begin());
	}

	void LoadTestConfig1()
	{
		for (auto b : _bridges)
		{
			auto treeCount = 1 + STP_GetMstiCount(b->stp_bridge());

			std::vector<STP_CONFIG_TABLE_ENTRY> entries;
			entries.resize(1 + max_vlan_number);

			entries[0] = { 0, 0 }; // VLAN0 does not exist.

			unsigned char treeIndex = 1;
			for (unsigned int vid = 1; vid <= max_vlan_number; vid++)
			{
				entries[vid].unused = 0;
				entries[vid].treeIndex = (vid - 1) % treeCount;
			}

			b->SetMstConfigTable (entries.data(), (unsigned int) entries.size());
		}

		HWND list = GetDlgItem (_hwnd, IDC_LIST_CONFIG_TABLE);
		ListView_DeleteAllItems(list);
		LoadTable (list, *_bridges.begin());
	}
};

std::unique_ptr<edge::property_editor_i> create_config_id_editor (std::span<object* const> objects)
{
	return std::make_unique<mst_config_id_editor>(objects);
}

