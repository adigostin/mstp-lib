
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "resource.h"
#include "bridge.h"
#include "port.h"
#include "wire.h"
#include "win32/property_grid.h"

using namespace edge;

static constexpr wchar_t ProjectWindowWndClassName[] = L"project_window-{24B42526-2970-4B3C-A753-2DABD22C4BB0}";
static constexpr wchar_t RegValueNameShowCmd[] = L"WindowShowCmd";
static constexpr wchar_t RegValueNameWindowLeft[] = L"WindowLeft";
static constexpr wchar_t RegValueNameWindowTop[] = L"WindowTop";
static constexpr wchar_t RegValueNameWindowRight[] = L"WindowRight";
static constexpr wchar_t RegValueNameWindowBottom[] = L"WindowBottom";
static constexpr wchar_t RegValueNamePropertiesWindowWidth[] = L"PropertiesWindowWidth";
static constexpr wchar_t RegValueNameLogWindowWidth[] = L"LogWindowWidth";
static constexpr wchar_t RegValueNamePGDescHeight[] = L"PropertyGridDescriptionHeight";

static COMDLG_FILTERSPEC const ProjectFileDialogFileTypes[] =
{
	{ L"Drawing Files", L"*.stp" },
	{ L"All Files",     L"*.*" },
};
static const wchar_t ProjectFileExtensionWithoutDot[] = L"stp";

static const wnd_class_params class_params =
{
	ProjectWindowWndClassName,      // lpszClassName
	CS_DBLCLKS,                     // style
	MAKEINTRESOURCE(IDR_MAIN_MENU), // lpszMenuName
	MAKEINTRESOURCE(IDI_DESIGNER),  // lpIconName
	MAKEINTRESOURCE(IDI_DESIGNER),  // lpIconSmName
};

#pragma warning (disable: 4250)

class project_window : public window, public virtual project_window_i
{
	using base = window;

	simulator_app_i*              const _app;
	com_ptr<ID3D11DeviceContext1> const _d3d_dc;
	com_ptr<IDWriteFactory>       const _dwrite_factory;
	std::shared_ptr<project_i>    const _project;
	std::unique_ptr<selection_i>  const _selection;
	std::unique_ptr<edit_window_i>      _edit_window;
	std::unique_ptr<property_grid_i>    _pg;
	std::unique_ptr<log_window_i>       _log_window;
	std::unique_ptr<vlan_window_i>      _vlanWindow;
	RECT _restore_bounds;
	uint32_t _selectedVlanNumber = 1;
	uint32_t _dpi;
	float _pg_desired_width_dips;
	float _log_desired_width_dips;
	bool _restoring_size_from_registry = false;

	enum class tool_window { none, props, vlan, log };
	tool_window _windowBeingResized = tool_window::none;
	LONG _resize_offset;

public:
	project_window (const project_window_create_params& create_params)
		: window(create_params.app->GetHInstance(), class_params, 0, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
				 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr)
		, _app(create_params.app)
		, _project(create_params.project)
		, _selection(create_params.app->selection_factory()(create_params.project.get()))
		, _selectedVlanNumber(create_params.selectedVlan)
		, _d3d_dc(create_params.d3d_dc)
		, _dwrite_factory(create_params.dwrite_factory)
	{
		assert (create_params.selectedVlan >= 1);

		int nCmdShow = create_params.nCmdShow;
		bool read = TryGetSavedWindowLocation (&_restore_bounds, &nCmdShow);
		if (!read)
			::GetWindowRect(hwnd(), &_restore_bounds);
		else
		{
			_restoring_size_from_registry = true;
			auto width = _restore_bounds.right - _restore_bounds.left;
			auto height = _restore_bounds.bottom - _restore_bounds.top;
			::MoveWindow(hwnd(), _restore_bounds.left, _restore_bounds.top, width, height, TRUE);
			_restoring_size_from_registry = false;
		}
		::ShowWindow (hwnd(), nCmdShow);

		if (auto proc_addr = GetProcAddress(GetModuleHandleA("User32.dll"), "GetDpiForWindow"))
		{
			auto proc = reinterpret_cast<UINT(WINAPI*)(HWND)>(proc_addr);
			_dpi = proc(hwnd());
		}
		else
		{
			HDC tempDC = GetDC(hwnd());
			_dpi = GetDeviceCaps (tempDC, LOGPIXELSX);
			ReleaseDC (hwnd(), tempDC);
		}

		_pg_desired_width_dips = (client_width_pixels() * 96.0f / _dpi) * 20 / 100;
		TryReadRegFloat (RegValueNamePropertiesWindowWidth, _pg_desired_width_dips);
		_log_desired_width_dips = (client_width_pixels() * 96.0f / _dpi) * 30 / 100;
		TryReadRegFloat (RegValueNameLogWindowWidth, _log_desired_width_dips);

		if (create_params.show_property_grid)
			create_property_grid();

		if (create_params.showLogWindow)
			create_log_window();

		_vlanWindow = vlan_window_factory (_app, this, _project, _selection.get(), hwnd(), { GetVlanWindowLeft(), 0 }, _d3d_dc, _dwrite_factory);
		SetMainMenuItemCheck (ID_VIEW_VLANS, true);

		edit_window_create_params cps = { _app, this, _project.get(), _selection.get(), hwnd(), edit_window_rect(), create_params.d3d_dc, create_params.dwrite_factory };
		_edit_window = _app->edit_window_factory()(cps);
		_edit_window->zoom_all();

		if (auto recentFiles = GetRecentFileList(); !recentFiles.empty())
			AddRecentFileMenuItems(recentFiles);

		_project->GetChangedFlagChangedEvent().add_handler (&OnProjectChangedFlagChanged, this);
		_app->project_window_added().add_handler (&OnProjectWindowAdded, this);
		_app->project_window_removed().add_handler (&OnProjectWindowRemoved, this);
		_project->GetLoadedEvent().add_handler (&OnProjectLoaded, this);
		_selection->changed().add_handler (&on_selection_changed, this);
	}

	~project_window()
	{
		_selection->changed().remove_handler (&on_selection_changed, this);
		_project->GetLoadedEvent().remove_handler (&OnProjectLoaded, this);
		_app->project_window_removed().remove_handler (&OnProjectWindowRemoved, this);
		_app->project_window_added().remove_handler (&OnProjectWindowAdded, this);
		_project->GetChangedFlagChangedEvent().remove_handler (&OnProjectChangedFlagChanged, this);
		if (_pg)
			destroy_property_grid();
	}

	virtual HWND hwnd() const override { return base::hwnd(); }

	using base::client_rect_pixels;

	LONG splitter_width_pixels() const
	{
		static constexpr float splitter_width_dips = 4;
		return (LONG) round (splitter_width_dips * _dpi / 96.0f);
	}

	RECT pg_restricted_rect() const
	{
		LONG pg_desired_width_pixels = (LONG)round(_pg_desired_width_dips * _dpi / 96);
		LONG w = std::min (pg_desired_width_pixels, client_width_pixels() * 40 / 100);
		w = std::max(w, 100l);
		return RECT{ 0, 0, w, client_height_pixels() };
	}

	void create_property_grid()
	{
		_pg = property_grid_factory (_app->GetHInstance(), WS_EX_CLIENTEDGE, pg_restricted_rect(), hwnd(), _d3d_dc, _dwrite_factory);
		float desc_height;
		if (TryReadRegFloat(RegValueNamePGDescHeight, desc_height))
			_pg->set_description_height(desc_height);
		set_selection_to_pg();
		SetMainMenuItemCheck (ID_VIEW_PROPERTIES, true);
		_pg->description_height_changed().add_handler(&on_pg_desc_height_changed, this);
	}

	void destroy_property_grid()
	{
		_pg->description_height_changed().remove_handler(&on_pg_desc_height_changed, this);
		_pg = nullptr;
		SetMainMenuItemCheck (ID_VIEW_PROPERTIES, false);
	}

	static void on_pg_desc_height_changed (void* arg, float height)
	{
		auto pw = static_cast<project_window*>(arg);
		pw->WriteRegFloat(RegValueNamePGDescHeight, height);
	}

	RECT log_restricted_rect() const
	{
		LONG log_desired_width_pixels = (LONG)round(_log_desired_width_dips * _dpi / 96);
		LONG w = std::min (log_desired_width_pixels, client_width_pixels() * 40 / 100);
		w = std::max(w, 100l);
		return RECT{ client_width_pixels() - w, 0, client_width_pixels(), client_height_pixels() };
	}

	void create_log_window()
	{
		_log_window = log_window_factory (_app->GetHInstance(), hwnd(), log_restricted_rect(), _d3d_dc, _dwrite_factory, _selection.get(), _project);
		SetMainMenuItemCheck (ID_VIEW_STPLOG, true);
	}

	void destroy_log_window()
	{
		_log_window = nullptr;
		SetMainMenuItemCheck (ID_VIEW_STPLOG, false);
	}

	static void OnProjectLoaded (void* callbackArg, project_i* project)
	{
		auto pw = static_cast<project_window*>(callbackArg);
		pw->SetWindowTitle();
		pw->_edit_window->zoom_all();
	}

	static void OnProjectWindowAdded (void* callbackArg, project_window_i* pw)
	{
		auto thispw = static_cast<project_window*>(callbackArg);
		if (pw->project() == thispw->project())
			thispw->SetWindowTitle();
	}

	static void OnProjectWindowRemoved (void* callbackArg, project_window_i* pw)
	{
		auto thispw = static_cast<project_window*>(callbackArg);
		if (pw->project() == thispw->project())
			thispw->SetWindowTitle();
	}

	static void OnProjectChangedFlagChanged (void* callbackArg, project_i* project)
	{
		auto pw = static_cast<project_window*>(callbackArg);
		pw->SetWindowTitle();
	}

	static void on_selection_changed (void* callbackArg, selection_i* selection)
	{
		auto pw = static_cast<project_window*>(callbackArg);
		if (pw->_pg != nullptr)
			pw->set_selection_to_pg();
	}

	LONG GetVlanWindowLeft() const
	{
		if (_pg != nullptr)
			return _pg->width_pixels() + splitter_width_pixels();
		else
			return 0;
	}

	LONG GetVlanWindowRight() const
	{
		if (_log_window != nullptr)
			return client_width_pixels() - _log_window->width_pixels() - splitter_width_pixels();
		else
			return client_width_pixels();
	}

	RECT edit_window_rect() const
	{
		auto rect = client_rect_pixels();

		if (_pg != nullptr)
			rect.left += _pg->width_pixels() + splitter_width_pixels();

		if (_log_window != nullptr)
			rect.right -= _log_window->width_pixels() + splitter_width_pixels();

		if (_vlanWindow != nullptr)
			rect.top += _vlanWindow->height_pixels();

		return rect;
	}

	void SetWindowTitle()
	{
		std::wstringstream windowTitle;

		const auto& filePath = _project->file_path();
		if (!filePath.empty())
		{
			const wchar_t* fileName = PathFindFileName (filePath.c_str());
			const wchar_t* fileExt = PathFindExtension (filePath.c_str());
			windowTitle << std::setw(fileExt - fileName) << fileName;
		}
		else
			windowTitle << L"Untitled";

		if (_project->GetChangedFlag())
			windowTitle << L"*";

		auto& pws = _app->project_windows();
		if (any_of (pws.begin(), pws.end(), [this](const std::unique_ptr<project_window_i>& pw) { return (pw.get() != this) && (pw->project() == _project.get()); }))
			windowTitle << L" - VLAN " << _selectedVlanNumber;

		::SetWindowText (hwnd(), windowTitle.str().c_str());
	}

	void SetMainMenuItemCheck (UINT item, bool checked)
	{
		auto menu = ::GetMenu(hwnd());
		MENUITEMINFO mii = { sizeof(mii) };
		mii.fMask = MIIM_STATE;
		mii.fState = checked ? MFS_CHECKED : MFS_UNCHECKED;
		::SetMenuItemInfo (menu, item, FALSE, &mii);
	}

	std::optional<LRESULT> window_proc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) final
	{
		auto base_class_result = base::window_proc (hwnd, msg, wParam, lParam);

		if (msg == WM_DPICHANGED)
		{
			_dpi = LOWORD(wParam);
			if (!_restoring_size_from_registry)
			{
				auto r = (RECT*) lParam;
				::SetWindowPos (hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
			}
			return 0;
		}

		if (msg == WM_DESTROY)
		{
			this->event_invoker<destroying_e>()(this);
			return 0;
		}

		if (msg == WM_CLOSE)
		{
			TryClose();
			return 0;
		}

		if (msg == WM_SIZE)
		{
			process_wm_size (hwnd, wParam, { LOWORD(lParam), HIWORD(lParam) });
			return 0;
		}

		if (msg == WM_MOVE)
		{
			WINDOWPLACEMENT wp = { sizeof(wp) };
			::GetWindowPlacement (hwnd, &wp);
			if (wp.showCmd == SW_NORMAL)
				::GetWindowRect (hwnd, &_restore_bounds);
			return 0;
		}

		if (msg == WM_PAINT)
		{
			ProcessWmPaint();
			return 0;
		}

		if (msg == WM_COMMAND)
		{
			auto olr = ProcessWmCommand (wParam, lParam);
			if (olr)
				return olr.value();
			else
				return base_class_result;
		}

		if (msg == WM_SETCURSOR)
		{
			if (((HWND) wParam == hwnd) && (LOWORD (lParam) == HTCLIENT))
			{
				POINT pt;
				BOOL bRes = ::GetCursorPos (&pt);
				if (bRes)
				{
					bRes = ::ScreenToClient (hwnd, &pt); assert(bRes);
					this->SetCursor(pt);
					return 0;
				}
			}

			return base_class_result;
		}

		if (msg == WM_LBUTTONDOWN)
		{
			ProcessWmLButtonDown (POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }, (UINT) wParam);
			return 0;
		}

		if (msg == WM_MOUSEMOVE)
		{
			process_wm_mousemove (POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }, (UINT) wParam);
			return 0;
		}

		if (msg == WM_LBUTTONUP)
		{
			ProcessWmLButtonUp (POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }, (UINT) wParam);
			return 0;
		}

		return base_class_result;
	}

	void process_wm_size (HWND hwnd, WPARAM wParam, SIZE newClientSize)
	{
		if (wParam == SIZE_RESTORED)
			::GetWindowRect(hwnd, &_restore_bounds);

		ResizeChildWindows();
	}

	void ResizeChildWindows()
	{
		if (_pg != nullptr)
			_pg->move_window (pg_restricted_rect());

		if (_log_window != nullptr)
			_log_window->move_window (log_restricted_rect());

		if (_vlanWindow != nullptr)
			_vlanWindow->move_window ({ GetVlanWindowLeft(), 0, GetVlanWindowRight(), _vlanWindow->preferred_size().cy });

		if (_edit_window != nullptr)
			_edit_window->move_window (edit_window_rect());
	}

	void ProcessWmLButtonDown (POINT pt, UINT modifierKeysDown)
	{
		if ((_pg != nullptr) && (pt.x >= _pg->width_pixels()) && (pt.x < _pg->width_pixels() + splitter_width_pixels()))
		{
			_windowBeingResized = tool_window::props;
			_resize_offset = pt.x - _pg->width_pixels();
			::SetCapture(hwnd());
		}
		else if ((_log_window != nullptr) && (pt.x >= _log_window->GetX() - splitter_width_pixels()) && (pt.x < _log_window->GetX()))
		{
			_windowBeingResized = tool_window::log;
			_resize_offset = _log_window->GetX() - pt.x;
			::SetCapture(hwnd());
		}
	}

	void process_wm_mousemove (POINT pt, UINT modifierKeysDown)
	{
		if (_windowBeingResized == tool_window::props)
		{
			LONG pg_desired_width_pixels = pt.x - _resize_offset;
			pg_desired_width_pixels = std::max (pg_desired_width_pixels, 0l);
			pg_desired_width_pixels = std::min (pg_desired_width_pixels, client_width_pixels());
			_pg_desired_width_dips = pg_desired_width_pixels * 96.0f / _dpi;
			_pg->move_window (pg_restricted_rect());
			_vlanWindow->move_window ({ GetVlanWindowLeft(), 0, GetVlanWindowRight(), _vlanWindow->height_pixels() });
			_edit_window->move_window (edit_window_rect());
			::UpdateWindow (_pg->hwnd());
			::UpdateWindow (_edit_window->hwnd());
			::UpdateWindow (_vlanWindow->hwnd());
		}
		else if (_windowBeingResized == tool_window::log)
		{
			LONG log_desired_width_pixels = client_width_pixels() - pt.x - _resize_offset;
			log_desired_width_pixels = std::max (log_desired_width_pixels, 0l);
			log_desired_width_pixels = std::min (log_desired_width_pixels, client_width_pixels());
			_log_desired_width_dips = log_desired_width_pixels * 96.0f / _dpi;
			_log_window->move_window (log_restricted_rect());
			_vlanWindow->move_window ({ GetVlanWindowLeft(), 0, GetVlanWindowRight(), _vlanWindow->height_pixels() });
			_edit_window->move_window (edit_window_rect());
			::UpdateWindow (_log_window->hwnd());
			::UpdateWindow (_edit_window->hwnd());
			::UpdateWindow (_vlanWindow->hwnd());
		}
	}

	void ProcessWmLButtonUp (POINT pt, UINT modifierKeysDown)
	{
		if (_windowBeingResized == tool_window::props)
		{
			WriteRegFloat (RegValueNamePropertiesWindowWidth, _pg_desired_width_dips);
			_windowBeingResized = tool_window::none;
			::ReleaseCapture();
		}
		else if (_windowBeingResized == tool_window::log)
		{
			WriteRegFloat (RegValueNameLogWindowWidth, _log_desired_width_dips);
			_windowBeingResized = tool_window::none;
			::ReleaseCapture();
		}
	}

	void SetCursor (POINT pt)
	{
		if ((_pg != nullptr) && (pt.x >= _pg->width_pixels()) && (pt.x < _pg->width_pixels() + splitter_width_pixels()))
		{
			::SetCursor (LoadCursor(nullptr, IDC_SIZEWE));
		}
		else if ((_log_window != nullptr) && (pt.x >= _log_window->GetX() - splitter_width_pixels()) && (pt.x < _log_window->GetX()))
		{
			::SetCursor (LoadCursor(nullptr, IDC_SIZEWE));
		}
		else
			::SetCursor (LoadCursor(nullptr, IDC_ARROW));
	}

	void ProcessWmPaint()
	{
		PAINTSTRUCT ps;
		BeginPaint(hwnd(), &ps);

		RECT rect;

		if (_pg != nullptr)
		{
			rect.left = _pg->width_pixels();
			rect.top = 0;
			rect.right = rect.left + splitter_width_pixels();
			rect.bottom = client_height_pixels();
			FillRect (ps.hdc, &rect, GetSysColorBrush(COLOR_3DFACE));
		}

		if (_log_window != nullptr)
		{
			rect.right = _log_window->GetX();
			rect.left = rect.right - splitter_width_pixels();
			rect.top = 0;
			rect.bottom = client_height_pixels();
			FillRect (ps.hdc, &rect, GetSysColorBrush(COLOR_3DFACE));
		}

		EndPaint(hwnd(), &ps);
	}

	std::optional<LRESULT> ProcessWmCommand (WPARAM wParam, LPARAM lParam)
	{
		if (wParam == ID_VIEW_PROPERTIES)
		{
			if (_pg != nullptr)
				destroy_property_grid();
			else
				create_property_grid();
			ResizeChildWindows();
			return 0;
		}

		if (wParam == ID_VIEW_STPLOG)
		{
			if (_log_window != nullptr)
				destroy_log_window();
			else
				create_log_window();
			ResizeChildWindows();
			return 0;
		}

		if (wParam == ID_VIEW_VLANS)
		{
			// TODO: show/hide.
			return 0;
		}

		if (((HIWORD(wParam) == 0) || (HIWORD(wParam) == 1)) && (LOWORD(wParam) == ID_FILE_SAVE))
		{
			Save();
			return 0;
		}

		if (((HIWORD(wParam) == 0) || (HIWORD(wParam) == 1)) && (LOWORD(wParam) == ID_FILE_OPEN))
		{
			std::wstring openPath;
			HRESULT hr = TryChooseFilePath (OpenOrSave::Open, hwnd(), nullptr, openPath);
			if (SUCCEEDED(hr))
				Open(openPath.c_str());
			return 0;
		}

		if (((HIWORD(wParam) == 0) || (HIWORD(wParam) == 1)) && (LOWORD(wParam) == ID_FILE_NEW))
		{
			auto project = _app->project_factory()();
			project_window_create_params params =
			{
				_app, project, true, true, 1, SW_SHOW, _d3d_dc, _dwrite_factory
			};

			auto pw = _app->project_window_factory()(params);
			_app->add_project_window(std::move(pw));
			return 0;
		}

		if (wParam == ID_FILE_SAVEAS)
		{
			MessageBoxA (hwnd(), "Not yet implemented.", _app->app_name(), 0);
			return 0;
		}

		if (wParam == ID_FILE_EXIT)
		{
			PostMessage (hwnd(), WM_CLOSE, 0, 0);
			return 0;
		}

		if ((wParam >= ID_RECENT_FILE_FIRST) && (wParam <= ID_RECENT_FILE_LAST))
		{
			UINT recentFileIndex = (UINT)wParam - ID_RECENT_FILE_FIRST;
			auto mainMenu = ::GetMenu(hwnd());
			auto fileMenu = ::GetSubMenu (mainMenu, 0);
			int charCount = ::GetMenuString (fileMenu, (UINT)wParam, nullptr, 0, MF_BYCOMMAND);
			if (charCount > 0)
			{
				auto path = std::make_unique<wchar_t[]>(charCount + 1);
				::GetMenuString (fileMenu, (UINT)wParam, path.get(), charCount + 1, MF_BYCOMMAND);
				Open(path.get());
			}
		}

		if (wParam == ID_HELP_ABOUT)
		{
			auto text = std::string(_app->app_name()) + " v" + _app->app_version_string();
			MessageBoxA (hwnd(), text.c_str(), _app->app_name(), 0);
			return 0;
		}

		return std::nullopt;
	}

	void Open (const wchar_t* openPath)
	{
		for (auto& pw : _app->project_windows())
		{
			if (_wcsicmp (pw->project()->file_path().c_str(), openPath) == 0)
			{
				::BringWindowToTop (pw->hwnd());
				::FlashWindow (pw->hwnd(), FALSE);
				return;
			}
		}

		std::shared_ptr<project_i> projectToLoadTo;
		if (_project->bridges().empty() && _project->wires().empty())
			projectToLoadTo = _project;
		else
			projectToLoadTo = _app->project_factory()();

		auto hr = projectToLoadTo->load(openPath);
		if (FAILED(hr))
		{
			auto werror = std::wstring (_com_error(hr).ErrorMessage());
			auto text = std::wstring(L"Cannot open ") + openPath + L"\r\n";
			text.append (werror.begin(), werror.end());
			TaskDialog (hwnd(), GetWindowInstance(hwnd()), _app->app_namew(), text.c_str(), nullptr, 0, TD_ERROR_ICON, nullptr);
			return;
		}

		if (projectToLoadTo != _project)
		{
			project_window_create_params cps = { _app, projectToLoadTo, true, true, 1, SW_SHOW, _d3d_dc, _dwrite_factory };
			auto newWindow = _app->project_window_factory()(cps);
			_app->add_project_window(std::move(newWindow));
		}
	}

	HRESULT Save()
	{
		HRESULT hr;

		auto savePath = _project->file_path();
		if (savePath.empty())
		{
			hr = TryChooseFilePath (OpenOrSave::Save, hwnd(), L"", savePath);
			if (FAILED(hr))
				return hr;
		}

		hr = _project->save (savePath.c_str());
		if (FAILED(hr))
		{
			auto werror = std::wstring (_com_error(hr).ErrorMessage());
			auto text = std::wstring(L"Could Not Save\r\n");
			text.append (werror.begin(), werror.end());
			MessageBox (hwnd(), text.c_str(), _app->app_namew(), 0);
			return hr;
		}

		_project->SetChangedFlag(false);
		return S_OK;
	}

	HRESULT AskSaveDiscardCancel (const wchar_t* askText, bool* saveChosen)
	{
		static const TASKDIALOG_BUTTON buttons[] =
		{
			{ IDYES, L"Save Changes" },
			{ IDNO, L"Discard Changes" },
			{ IDCANCEL, L"Cancel" },
		};

		auto app_name = std::wstring (_app->app_name(), strchr(_app->app_name(), 0));
		TASKDIALOGCONFIG tdc = { sizeof (tdc) };
		tdc.hwndParent = hwnd();
		tdc.pszWindowTitle = app_name.c_str();
		tdc.pszMainIcon = TD_WARNING_ICON;
		tdc.pszMainInstruction = L"File was changed";
		tdc.pszContent = askText;
		tdc.cButtons = _countof(buttons);
		tdc.pButtons = buttons;
		tdc.nDefaultButton = IDOK;

		int pressedButton;
		auto hr = TaskDialogIndirect (&tdc, &pressedButton, nullptr, nullptr);
		if (FAILED(hr))
			return hr;

		if (pressedButton == IDCANCEL)
			return HRESULT_FROM_WIN32(ERROR_CANCELLED);

		*saveChosen = (pressedButton == IDYES);
		return S_OK;
	}

	enum class OpenOrSave { Open, Save };

	static HRESULT TryChooseFilePath (OpenOrSave which, HWND fileDialogParentHWnd, const wchar_t* pathToInitializeDialogTo, std::wstring& sbOut)
	{
		com_ptr<IFileDialog> dialog;
		HRESULT hr = CoCreateInstance ((which == OpenOrSave::Save) ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, __uuidof(dialog), (void**) &dialog);
		if (FAILED(hr))
			return hr;

		//DWORD options;
		//hr = dialog->GetOptions (&options); rassert_hr(hr);
		//hr = dialog->SetOptions (options | FOS_FORCEFILESYSTEM); rassert_hr(hr);
		hr = dialog->SetFileTypes (_countof(ProjectFileDialogFileTypes), ProjectFileDialogFileTypes);
		if (FAILED(hr))
			return hr;

		hr = dialog->SetDefaultExtension (ProjectFileExtensionWithoutDot);
		if (FAILED(hr))
			return hr;

		if ((pathToInitializeDialogTo != nullptr) && (pathToInitializeDialogTo[0] != 0))
		{
			auto filePtr = PathFindFileName(pathToInitializeDialogTo);
			dialog->SetFileName(filePtr);

			std::wstring dir (pathToInitializeDialogTo, filePtr - pathToInitializeDialogTo);
			com_ptr<IShellItem> si;
			hr = SHCreateItemFromParsingName (dir.c_str(), nullptr, IID_PPV_ARGS(&si));
			if (SUCCEEDED(hr))
				dialog->SetFolder(si);
		}

		hr = dialog->Show(fileDialogParentHWnd);
		if (FAILED(hr))
			return hr;

		com_ptr<IShellItem> item;
		hr = dialog->GetResult (&item);
		if (FAILED(hr))
			return hr;

		{
			wchar_t* filePath;
			hr = item->GetDisplayName (SIGDN_FILESYSPATH, &filePath);
			if (FAILED(hr))
				return hr;
			sbOut = filePath;
			CoTaskMemFree(filePath);
		}

		SHAddToRecentDocs(SHARD_PATHW, sbOut.c_str());
		return S_OK;
	}

	HRESULT TryClose()
	{
		auto count = count_if (_app->project_windows().begin(), _app->project_windows().end(),
							   [this] (const std::unique_ptr<project_window_i>& pw) { return pw->project() == _project.get(); });
		if (count == 1)
		{
			// Closing last window of this project.
			if (_project->GetChangedFlag())
			{
				bool saveChosen;
				auto hr = AskSaveDiscardCancel(L"Save changes?", &saveChosen);
				if (FAILED(hr))
					return hr;

				if (saveChosen)
				{
					hr = this->Save();
					if (FAILED(hr))
						return hr;
				}
			}
		}

		SaveWindowLocation();
		::DestroyWindow (hwnd());
		return S_OK;
	}

	bool TryReadRegFloat (const wchar_t* valueName, float& value)
	{
		DWORD data_size;
		auto lresult = RegGetValue(HKEY_CURRENT_USER, _app->GetRegKeyPath(), valueName, RRF_RT_REG_SZ, nullptr, nullptr, &data_size);
		if (lresult != ERROR_SUCCESS)
			return false;
		std::wstring str;
		str.resize(data_size + 1);
		lresult = RegGetValue(HKEY_CURRENT_USER, _app->GetRegKeyPath(), valueName, RRF_RT_REG_SZ, nullptr, str.data(), &data_size);
		if (lresult != ERROR_SUCCESS)
			return false;
		wchar_t* end_ptr = str.data();
		auto v = wcstof(str.data(), &end_ptr);
		if (end_ptr == str.data())
			return false;
		value = v;
		return true;
	}

	void WriteRegFloat (const wchar_t* valueName, float value)
	{
		HKEY key;
		auto lstatus = RegCreateKeyEx(HKEY_CURRENT_USER, _app->GetRegKeyPath(), 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
		if (lstatus == ERROR_SUCCESS)
		{
			auto str = std::to_wstring(value);
			RegSetValueEx(key, valueName, 0, REG_SZ, (BYTE*)str.data(), (DWORD) str.size());
			RegCloseKey(key);
		}
	}

	bool TryReadRegDword (const wchar_t* valueName, DWORD* valueOut)
	{
		DWORD dataSize = 4;
		auto lresult = RegGetValue(HKEY_CURRENT_USER, _app->GetRegKeyPath(), valueName, RRF_RT_REG_DWORD, nullptr, valueOut, &dataSize);
		return lresult == ERROR_SUCCESS;
	}

	void WriteRegDword (const wchar_t* valueName, DWORD value)
	{
		HKEY key;
		auto lstatus = RegCreateKeyEx(HKEY_CURRENT_USER, _app->GetRegKeyPath(), 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
		if (lstatus == ERROR_SUCCESS)
		{
			RegSetValueEx(key, valueName, 0, REG_DWORD, (BYTE*)&value, 4);
			RegCloseKey(key);
		}
	}

	bool TryGetSavedWindowLocation (_Out_ RECT* restoreBounds, _Out_ int* nCmdShow)
	{
		int cmd;
		RECT rb;
		if (   TryReadRegDword (RegValueNameShowCmd,      (DWORD*)&cmd)
			&& TryReadRegDword (RegValueNameWindowLeft,   (DWORD*)&rb.left)
			&& TryReadRegDword (RegValueNameWindowTop,    (DWORD*)&rb.top)
			&& TryReadRegDword (RegValueNameWindowRight,  (DWORD*)&rb.right)
			&& TryReadRegDword (RegValueNameWindowBottom, (DWORD*)&rb.bottom))
		{
			*restoreBounds = rb;
			*nCmdShow = cmd;
			return true;
		}

		return false;
	}

	void SaveWindowLocation() const
	{
		WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
		BOOL bRes = GetWindowPlacement(hwnd(), &wp);
		if (bRes && ((wp.showCmd == SW_NORMAL) || (wp.showCmd == SW_MAXIMIZE)))
		{
			HKEY key;
			auto lstatus = RegCreateKeyEx(HKEY_CURRENT_USER, _app->GetRegKeyPath(), 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
			if (lstatus == ERROR_SUCCESS)
			{
				RegSetValueEx(key, RegValueNameWindowLeft, 0, REG_DWORD, (BYTE*)&_restore_bounds.left, 4);
				RegSetValueEx(key, RegValueNameWindowTop, 0, REG_DWORD, (BYTE*)&_restore_bounds.top, 4);
				RegSetValueEx(key, RegValueNameWindowRight, 0, REG_DWORD, (BYTE*)&_restore_bounds.right, 4);
				RegSetValueEx(key, RegValueNameWindowBottom, 0, REG_DWORD, (BYTE*)&_restore_bounds.bottom, 4);
				RegSetValueEx(key, RegValueNameShowCmd, 0, REG_DWORD, (BYTE*)&wp.showCmd, 4);
				RegCloseKey(key);
			}
		}
	}

	virtual void select_vlan (uint32_t vlanNumber) override final
	{
		assert ((vlanNumber > 0) && (vlanNumber <= 4094));

		if (_selectedVlanNumber != vlanNumber)
		{
			_selectedVlanNumber = vlanNumber;
			event_invoker<selected_vlan_number_changed_e>()(this, vlanNumber);
			::InvalidateRect (hwnd(), nullptr, FALSE);
			if (_pg != nullptr)
				set_selection_to_pg();
			SetWindowTitle();
		}
	};

	virtual uint32_t selected_vlan_number() const override final { return _selectedVlanNumber; }

	virtual selected_vlan_number_changed_e::subscriber selected_vlan_number_changed() override final { return selected_vlan_number_changed_e::subscriber(this); }

	virtual project_i* project() const override final { return _project.get(); }

	virtual destroying_e::subscriber destroying() override final { return destroying_e::subscriber(this); }

	static std::vector<std::wstring> GetRecentFileList()
	{
		// We ignore errors in this particular function.

		std::vector<std::wstring> fileList;

		com_ptr<IApplicationDocumentLists> docList;
		auto hr = CoCreateInstance (CLSID_ApplicationDocumentLists, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IApplicationDocumentLists), (void**) &docList);
		if (FAILED(hr))
			return fileList;

		// This function retrieves the list created via calls to SHAddToRecentDocs.
		// We use the standard file dialogs througout the application; they call SHAddToRecentDocs for us.
		com_ptr<IObjectArray> objects;
		hr = docList->GetList(APPDOCLISTTYPE::ADLT_RECENT, 16, __uuidof(IObjectArray), (void**) &objects);
		if (FAILED(hr))
			return fileList;

		UINT count;
		hr = objects->GetCount(&count);
		if (FAILED(hr))
			return fileList;

		for (UINT i = 0; i < count; i++)
		{
			com_ptr<IShellItem2> si;
			hr = objects->GetAt(i, __uuidof(IShellItem2), (void**) &si);
			if (SUCCEEDED(hr))
			{
				wchar_t* path = nullptr;
				hr = si->GetDisplayName(SIGDN_FILESYSPATH, &path);
				if (SUCCEEDED(hr))
				{
					fileList.push_back(path);
					CoTaskMemFree(path);
				}
			}
		}

		return fileList;
	}

	int GetMenuPosFromID (HMENU menu, UINT id)
	{
		auto itemCount = ::GetMenuItemCount(menu);
		for (int pos = 0; pos < itemCount; pos++)
		{
			if (::GetMenuItemID (menu, pos) == id)
				return pos;
		}

		return -1;
	}

	void AddRecentFileMenuItems (const std::vector<std::wstring>& recentFiles)
	{
		auto mainMenu = ::GetMenu(hwnd());
		auto fileMenu = ::GetSubMenu (mainMenu, 0);
		auto itemCount = ::GetMenuItemCount(fileMenu);
		int pos = GetMenuPosFromID (fileMenu, ID_FILE_RECENT);
		if (pos != -1)
		{
			::RemoveMenu (fileMenu, pos, MF_BYPOSITION);

			MENUITEMINFO mii = { sizeof(mii) };
			mii.fMask = MIIM_STRING | MIIM_ID;
			mii.wID = ID_RECENT_FILE_FIRST;

			for (auto& file : recentFiles)
			{
				mii.dwTypeData = const_cast<wchar_t*>(file.c_str());
				::InsertMenuItem (fileMenu, pos, TRUE, &mii);
				pos++;
				mii.wID++;
				if (mii.wID > ID_RECENT_FILE_LAST)
					break;
			}
		}
	}

	void set_selection_to_pg()
	{
		const auto& objs = _selection->objects();

		_pg->clear();

		if (objs.empty())
			return;

		if (all_of (objs.begin(), objs.end(), [](object* o) { return o->type() == &bridge::_type; })
			|| all_of (objs.begin(), objs.end(), [](object* o) { return o->type() == &port::_type; }))
		{
			const char* first_section_name;
			std::function<std::pair<object*, unsigned int>(object* o)> tree_selector;

			if (all_of (objs.begin(), objs.end(), [](object* o) { return o->type() == &bridge::_type; }))
			{
				first_section_name = "Bridge Properties";

				tree_selector = [vlan=_selectedVlanNumber](object* o)
				{
					auto b = static_cast<bridge*>(o);
					auto tree_index = STP_GetTreeIndexFromVlanNumber(b->stp_bridge(), vlan);
					auto tree_object = b->trees().at(tree_index).get();
					return std::make_pair((object*)tree_object, tree_index);
				};
			}
			else //if (all_of (objs.begin(), objs.end(), [](object* o) { return o->is<port>(); }))
			{
				first_section_name = "Port Properties";

				tree_selector = [vlan=_selectedVlanNumber](object* o)
				{
					auto p = static_cast<port*>(o);
					auto tree_index = STP_GetTreeIndexFromVlanNumber(p->bridge()->stp_bridge(), vlan);
					auto tree_object = p->trees().at(tree_index).get();
					return std::make_pair((object*)tree_object, tree_index);
				};
			}

			_pg->add_section (first_section_name, objs.data(), objs.size());

			auto first_tree_index = tree_selector(objs.front()).second;
			bool all_same_tree_index = true;

			std::vector<object*> trees;
			for (object* o : objs)
			{
				auto tree_object_and_index = tree_selector(o);
				trees.push_back(tree_object_and_index.first);
				if (tree_object_and_index.second != first_tree_index)
					all_same_tree_index = false;
			}

			std::stringstream ss;
			ss << "VLAN " << _selectedVlanNumber << " Properties";
			if (all_same_tree_index && (first_tree_index == 0))
				ss << " (CIST)";
			else if (all_same_tree_index)
				ss << " (MSTI " << first_tree_index << ")";
			else
				ss << " (multiple trees)";

			_pg->add_section (ss.str().c_str(), trees.data(), trees.size());
		}
		else if (all_of (objs.begin(), objs.end(), [](object* o) { return o->type() == &wire::_type; }))
		{
			_pg->add_section("Wire Properties", objs.data(), objs.size());
		}
		else
			assert(false); // not implemented
	}
};

extern const project_window_factory_t project_window_factory = [](const project_window_create_params& create_params) -> std::unique_ptr<project_window_i>
{
	return std::make_unique<project_window>(create_params);
};
