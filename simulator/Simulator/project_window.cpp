
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "resource.h"

using namespace edge;
using namespace pg;

static constexpr wchar_t RegValueNameShowCmd[] = L"WindowShowCmd";
static constexpr wchar_t RegValueNameWindowLeft[] = L"WindowLeft";
static constexpr wchar_t RegValueNameWindowTop[] = L"WindowTop";
static constexpr wchar_t RegValueNameWindowRight[] = L"WindowRight";
static constexpr wchar_t RegValueNameWindowBottom[] = L"WindowBottom";
static constexpr wchar_t RegValueNamePropertiesWindowWidth[] = L"PropertiesWindowWidth";
static constexpr wchar_t RegValueNameLogWindowWidth[] = L"LogWindowWidth";

static COMDLG_FILTERSPEC const ProjectFileDialogFileTypes[] =
{
	{ L"Drawing Files", L"*.stp" },
	{ L"All Files",     L"*.*" },
};
static const wchar_t ProjectFileExtensionWithoutDot[] = L"stp";

class ProjectWindowImpl : public IProjectWindow, IConnectionPointContainer, IProjectEventsSink, IProjectWindowCollectionEventsSink, IVlanSelectionEvents
{
	ULONG _refCount = 0;
	ULONG _sig = 0xAA550005;
	WeakRefToThis _weakRefToThis;
	AdviseSinkToken _projectEventsToken;
	AdviseSinkToken _projectWindowCollectionEventsToken;
	AdviseSinkToken _vlanSelectionToken;
	com_ptr<ConnectionPointImpl<IProjectWindowEventsSink>> _projectWindowEventsCP;

	ISimulatorApp*        _app;
	com_ptr<IStpProject>  _project;
	com_ptr<ISelection>   _selection;
	HWND                  _hwnd;

	com_ptr<IEditWindow>         _edit_window;
	com_ptr<IPropertiesWindow>   _pw;
	com_ptr<ILogWindow>          _log_window;
	com_ptr<IVlanWindow>         _vlanWindow;
	RECT _restore_bounds;
	float _pw_desired_width_dips;
	float _log_desired_width_dips;
	bool _restoring_size_from_registry = false;

	enum class tool_window { none, props, vlan, log };
	tool_window _window_being_resized = tool_window::none;
	LONG _resize_offset;

	static inline uint32_t wnd_class_ref_count = 0;
	static const WNDCLASSEX wnd_class;

public:
	HRESULT InitInstance (const project_window_create_params& create_params)
	{
		auto hr = _weakRefToThis.InitInstance(AsUnknown());

		hr = MakeConnectionPoint<IProjectWindowEventsSink>(this, &_projectWindowEventsCP); RETURN_IF_FAILED(hr);

		_app = create_params.app;
		_project = create_params.project;
	
		hr = create_params.app->selection_factory()(create_params.project, &_selection); RETURN_IF_FAILED(hr);

		if (!wnd_class_ref_count)
		{
			ATOM wnd_class_atom = RegisterClassEx(&wnd_class);
			_ASSERT(wnd_class_atom);
		}
		wnd_class_ref_count++;
		_hwnd = CreateWindowEx(0, wnd_class.lpszClassName, L"", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, (HINSTANCE)&__ImageBase, nullptr);

		_ASSERT (create_params.selectedVlan >= 1);

		SetWindowLongPtr (_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

		int nCmdShow = create_params.nCmdShow;
		bool read = TryGetSavedWindowLocation (&_restore_bounds, &nCmdShow);
		if (!read)
			::GetWindowRect(_hwnd, &_restore_bounds);
		else
		{
			_restoring_size_from_registry = true;
			::MoveWindow(_hwnd, _restore_bounds.left, _restore_bounds.top, 
				_restore_bounds.right - _restore_bounds.left, _restore_bounds.bottom - _restore_bounds.top, FALSE);
			_restoring_size_from_registry = false;
		}
		::ShowWindow (_hwnd, nCmdShow);

		RECT client_rect_pixels;
		::GetClientRect(_hwnd, &client_rect_pixels);
		uint32_t dpi = edge::dpi(_hwnd);
		float client_width = client_rect_pixels.right * 96.0f / dpi;
		_pw_desired_width_dips = client_width * 20 / 100;
		TryReadRegFloat (RegValueNamePropertiesWindowWidth, _pw_desired_width_dips);
		_log_desired_width_dips = client_width * 30 / 100;
		TryReadRegFloat (RegValueNameLogWindowWidth, _log_desired_width_dips);

		hr = vlan_window_factory (_app, this, _project, _selection.get(), create_params.selectedVlan, _hwnd, { GetVlanWindowLeft(), 0 }, &_vlanWindow); RETURN_IF_FAILED(hr);
		SetMainMenuItemCheck (ID_VIEW_VLANS, true);
		hr = AdviseSink<IVlanSelectionEvents>(_vlanWindow, _weakRefToThis, &_vlanSelectionToken); RETURN_IF_FAILED(hr);

		if (create_params.show_property_grid) {
			hr = CreatePropertiesWindow(); RETURN_IF_FAILED(hr);
			MoveWindow (_vlanWindow->hwnd(), { GetVlanWindowLeft(), 0, GetVlanWindowRight(), _vlanWindow->preferred_size().cy });
		}

		if (create_params.showLogWindow) {
			hr = CreateLogWindow(); RETURN_IF_FAILED(hr);
		}

		EditWindowCreateParams cps = { _app, this, _project.get(), _selection.get(), _hwnd, edit_window_rect() };
		hr = _app->edit_window_factory()(cps, &_edit_window); LOG_IF_FAILED(hr);
		_edit_window->zoom_all();

		if (auto recentFiles = GetRecentFileList(); !recentFiles.empty())
			AddRecentFileMenuItems(recentFiles);

		hr = AdviseSink<IProjectEventsSink>(_project, _weakRefToThis, &_projectEventsToken); RETURN_IF_FAILED(hr);
		hr = AdviseSink<IProjectWindowCollectionEventsSink>(_app, _weakRefToThis, &_projectWindowCollectionEventsToken); RETURN_IF_FAILED(hr);

		return S_OK;
	}

	~ProjectWindowImpl()
	{
		// Destroy things explicitly, and in this order, because they keep raw pointers to each other.
		// This needs refactoring!
		if (_pw)
			DestroyPropertiesWindow();
		_log_window = nullptr;
		_vlanWindow = nullptr;
		_edit_window = nullptr;
		_selection = nullptr;

		_ASSERT (reinterpret_cast<ProjectWindowImpl*>(GetWindowLongPtr(_hwnd, GWLP_USERDATA)) == this);
		::SetWindowLongPtr (_hwnd, GWLP_USERDATA, 0);
		::DestroyWindow(_hwnd);
		_ASSERT(wnd_class_ref_count);
		wnd_class_ref_count--;
		if (!wnd_class_ref_count)
		{
			BOOL bres = UnregisterClass(wnd_class.lpszClassName, (HINSTANCE)&__ImageBase);
			_ASSERT(bres);
		}
	}

	IUnknown* AsUnknown() { return static_cast<IProjectWindow*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IProjectWindow>(this, riid, ppvObject)
			|| TryQI<IConnectionPointContainer>(this, riid, ppvObject)
			|| TryQI<IProjectEventsSink>(this, riid, ppvObject)
			|| TryQI<IProjectWindowCollectionEventsSink>(this, riid, ppvObject)
			|| TryQI<IVlanSelectionEvents>(this, riid, ppvObject)
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
		if (riid == __uuidof(IProjectWindowEventsSink))
			return wil::com_query_to_nothrow(_projectWindowEventsCP, ppCP);
		RETURN_HR(E_NOTIMPL);
	}
	#pragma endregion

	static void MoveWindow (HWND hwnd, RECT rect)
	{
		BOOL bres = ::MoveWindow (hwnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
		_ASSERT(bres);
	}

	LONG splitter_width_pixels() const
	{
		static constexpr float splitter_width_dips = 5;
		uint32_t dpi = edge::dpi(_hwnd);
		return lengthd_to_lengthp(splitter_width_dips, dpi, 0);
	}

	RECT pg_restricted_rect() const
	{
		uint32_t dpi = edge::dpi(_hwnd);
		SIZE client_size_pixels = edge::client_size_pixels(_hwnd);
		LONG pg_desired_width_pixels = lengthd_to_lengthp(_pw_desired_width_dips, dpi, 0);
		LONG w = std::min (pg_desired_width_pixels, client_size_pixels.cx * 40 / 100);
		w = std::max(w, 100l);
		return RECT{ 0, 0, w, client_size_pixels.cy };
	}

	HRESULT CreatePropertiesWindow()
	{
		HRESULT hr;
		com_ptr<IVlanSelection> vlanSel;
		hr = _vlanWindow->GetVlanSelection(&vlanSel); RETURN_IF_FAILED(hr);
		properties_window_create_params cps = { _app, _hwnd, pg_restricted_rect(), vlanSel, _selection };
		hr = _app->properties_window_factory()(cps, &_pw); RETURN_IF_FAILED(hr);
		SetMainMenuItemCheck (ID_VIEW_PROPERTIES, true);
		return S_OK;
	}

	void DestroyPropertiesWindow()
	{
		_pw = nullptr;
		SetMainMenuItemCheck (ID_VIEW_PROPERTIES, false);
	}

	RECT log_restricted_rect() const
	{
		uint32_t dpi = edge::dpi(_hwnd);
		LONG log_desired_width_pixels = lengthd_to_lengthp(_log_desired_width_dips, dpi, 0);
		SIZE client_size_pixels = edge::client_size_pixels(_hwnd);
		LONG w = std::min (log_desired_width_pixels, client_size_pixels.cx * 40 / 100);
		w = std::max(w, 100l);
		return RECT{ client_size_pixels.cx - w, 0, client_size_pixels.cx, client_size_pixels.cy };
	}

	HRESULT CreateLogWindow()
	{
		HRESULT hr;
		hr = MakeLogWindow(_app, _hwnd, log_restricted_rect(), _selection.get(), _project, &_log_window); RETURN_IF_FAILED(hr);
		SetMainMenuItemCheck (ID_VIEW_STPLOG, true);
		return S_OK;
	}

	void destroy_log_window()
	{
		_log_window = nullptr;
		SetMainMenuItemCheck (ID_VIEW_STPLOG, false);
	}

	#pragma region IProjectEventsSink
	virtual HRESULT STDMETHODCALLTYPE OnProjectLoaded (IStpProject* project) override
	{
		SetWindowTitle();
		_edit_window->zoom_all();
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnProjectSaved (IStpProject*) override
	{
		SetWindowTitle();
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnProjectChangedFlagChanged (IStpProject*) override
	{
		SetWindowTitle();
		return S_OK;
	}
	#pragma endregion

	#pragma region IProjectWindowCollectionEventsSink
	virtual HRESULT STDMETHODCALLTYPE OnProjectWindowInserting (IProjectWindow*) override { return S_OK; }

	virtual HRESULT STDMETHODCALLTYPE OnProjectWindowInserted (IProjectWindow* pw) override
	{
		if (pw->project() == _project)
			SetWindowTitle();
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnProjectWindowRemoving (IProjectWindow*) override { return S_OK; }

	virtual HRESULT STDMETHODCALLTYPE OnProjectWindowRemoved (IProjectWindow* pw) override
	{
		if (pw->project() == _project)
			SetWindowTitle();
		return S_OK;
	}
	#pragma endregion

	LONG GetVlanWindowLeft() const
	{
		if (_pw)
		{
			RECT rect;
			::GetWindowRect (_pw->hWnd(), &rect);
			return rect.right - rect.left + splitter_width_pixels();
		}
		else
			return 0;
	}

	LONG GetVlanWindowRight() const
	{
		SIZE cs = edge::client_size_pixels(_hwnd);
		if (_log_window != nullptr)
		{
			RECT rect;
			::GetWindowRect (_log_window->hwnd(), &rect);
			return cs.cx - (rect.right - rect.left) - splitter_width_pixels();
		}
		else
			return cs.cx;
	}

	RECT edit_window_rect() const
	{
		auto rect = edge::client_rect_pixels(_hwnd);

		if (_pw)
		{
			RECT pwRect;
			::GetWindowRect (_pw->hWnd(), &pwRect);
			rect.left += pwRect.right - pwRect.left + splitter_width_pixels();
		}

		if (_log_window != nullptr)
		{
			RECT logRect;
			::GetWindowRect (_log_window->hwnd(), &logRect);
			rect.right -= logRect.right - logRect.left + splitter_width_pixels();
		}

		if (_vlanWindow != nullptr)
		{
			RECT vlanRect;
			::GetWindowRect(_vlanWindow->hwnd(), &vlanRect);
			rect.top += vlanRect.bottom - vlanRect.top;
		}

		return rect;
	}

	HRESULT SetWindowTitle()
	{
		HRESULT hr;

		wil::unique_process_heap_string windowTitle;

		wil::unique_bstr filePath;
		hr = _project->GetFilePath(&filePath); RETURN_IF_FAILED(hr);
		if (filePath && filePath.get()[0])
		{
			const wchar_t* fileName = PathFindFileName (filePath.get());
			const wchar_t* fileExt = PathFindExtension (filePath.get());
			windowTitle = wil::make_process_heap_string_nothrow(fileName, fileExt - fileName); RETURN_IF_NULL_ALLOC(windowTitle);
		}
		else
		{
			windowTitle = wil::make_process_heap_string_nothrow(L"Untitled"); RETURN_IF_NULL_ALLOC(windowTitle);
		}

		if (_project->GetChangedFlag())
		{
			hr = wil::str_concat_nothrow(windowTitle, L"*"); RETURN_IF_FAILED(hr);
		}

		for (ULONG i = 0; i < _app->ProjectWindowCount(); i++)
		{
			IProjectWindow* pw = _app->ProjectWindowAt(i);
			if (pw != this && pw->project() == _project)
			{
				com_ptr<IVlanSelection> vlanSel;
				hr = _vlanWindow->GetVlanSelection(&vlanSel); RETURN_IF_FAILED(hr);
				DWORD vlan;
				hr = vlanSel->GetSelectedVlan(&vlan); RETURN_IF_FAILED(hr);
				wil::unique_process_heap_string temp;
				hr = wil::str_printf_nothrow(temp, L" - VLAN %u", vlan); RETURN_IF_FAILED(hr);
				hr = wil::str_concat_nothrow(windowTitle, temp); RETURN_IF_FAILED(hr);
				break;
			}
		}

		::SetWindowText (_hwnd, windowTitle.get());
		return S_OK;
	}

	void SetMainMenuItemCheck (UINT item, bool checked)
	{
		auto menu = ::GetMenu(_hwnd);
		MENUITEMINFO mii = { sizeof(mii) };
		mii.fMask = MIIM_STATE;
		mii.fState = checked ? MFS_CHECKED : MFS_UNCHECKED;
		::SetMenuItemInfo (menu, item, FALSE, &mii);
	}

	static LRESULT CALLBACK window_proc_static (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
//		if (!assert_function_running)
		{
			if (auto w = reinterpret_cast<ProjectWindowImpl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA)))
			{
				std::optional<LRESULT> result = w->on_window_proc(hwnd, msg, wparam, lparam);
				if (result)
					return result.value();
			}
		}

		return DefWindowProc (hwnd, msg, wparam, lparam);
	}

	std::optional<LRESULT> on_window_proc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (msg == WM_DPICHANGED)
		{
			if (!_restoring_size_from_registry)
			{
				auto r = (RECT*) lParam;
				::SetWindowPos (hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
			}
			return std::nullopt;
		}

		if (msg == WM_CLOSE)
		{
			if (try_close_window())
			{
				SaveWindowLocation();
				_projectWindowEventsCP->Notify([this](IProjectWindowEventsSink* sink) { return sink->OnProjectWindowClosed(this); });
			}

			return 0;
		}

		if (msg == WM_SIZE)
		{
			process_wm_size (hwnd, wParam, { LOWORD(lParam), HIWORD(lParam) });
			return std::nullopt;
		}

		if (msg == WM_MOVE)
		{
			WINDOWPLACEMENT wp = { sizeof(wp) };
			::GetWindowPlacement (hwnd, &wp);
			if (wp.showCmd == SW_NORMAL)
				::GetWindowRect (hwnd, &_restore_bounds);
			return std::nullopt;
		}

		if (msg == WM_PAINT)
		{
			ProcessWmPaint();
			return 0;
		}

		if (msg == WM_COMMAND)
			return ProcessWmCommand (wParam, lParam);

		if (msg == WM_SETCURSOR)
		{
			if (((HWND) wParam == hwnd) && (LOWORD (lParam) == HTCLIENT))
			{
				POINT pt;
				BOOL bRes = ::GetCursorPos (&pt);
				if (bRes)
				{
					bRes = ::ScreenToClient (hwnd, &pt); _ASSERT(bRes);
					::SetCursor(cursor_at(pt));
					return 0;
				}
			}

			return std::nullopt;
		}

		if (msg == WM_LBUTTONDOWN)
		{
			auto handled = ProcessWmLButtonDown (POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }, (UINT) wParam);
			return handled ? std::optional<LRESULT>(0) : std::nullopt;
		}

		if (msg == WM_MOUSEMOVE)
		{
			ProcessWmMouseMove (POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }, (UINT) wParam);
			return std::nullopt;
		}

		if (msg == WM_LBUTTONUP)
		{
			auto handled = ProcessWmLButtonUp (POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }, (UINT) wParam);
			return handled ? std::optional<LRESULT>(0) : std::nullopt;
		}

		return std::nullopt;
	}

	void process_wm_size (HWND hwnd, WPARAM wParam, SIZE newClientSize)
	{
		if (wParam == SIZE_RESTORED)
			::GetWindowRect(hwnd, &_restore_bounds);

		ResizeChildWindows();
	}

	void ResizeChildWindows()
	{
		if (_pw)
			MoveWindow (_pw->hWnd(), pg_restricted_rect());

		if (_log_window != nullptr)
			MoveWindow (_log_window->hwnd(), log_restricted_rect());

		if (_vlanWindow != nullptr)
			MoveWindow (_vlanWindow->hwnd(), { GetVlanWindowLeft(), 0, GetVlanWindowRight(), _vlanWindow->preferred_size().cy });

		if (_edit_window != nullptr)
			MoveWindow (_edit_window->hWnd(), edit_window_rect());
	}

	handled ProcessWmLButtonDown (POINT pp, UINT modifierKeysDown)
	{
		RECT rc;
		if (_pw && GetWindowRect(_pw->hWnd(), &rc) && (pp.x >= rc.right - rc.left) && (pp.x < rc.right - rc.left + splitter_width_pixels()))
		{
			_window_being_resized = tool_window::props;
			_resize_offset = pp.x - (rc.right - rc.left);
			::SetCapture(_hwnd);
			return handled(true);
		}

		auto ewr = edit_window_rect();
		if (_log_window && (pp.x >= ewr.right) && (pp.x < ewr.right + splitter_width_pixels()))
		{
			_window_being_resized = tool_window::log;
			_resize_offset = ewr.right + splitter_width_pixels() - pp.x;
			::SetCapture(_hwnd);
			return handled(true);
		}

		return handled(false);
	}

	void ProcessWmMouseMove (POINT pp, UINT modifierKeysDown)
	{
		uint32_t dpi = edge::dpi(_hwnd);
		LONG vlanHeight = 0;
		if (_vlanWindow)
		{
			RECT vlanRect;
			::GetWindowRect(_vlanWindow->hwnd(), &vlanRect);
			vlanHeight = vlanRect.bottom - vlanRect.top;
		}
		SIZE cs = edge::client_size_pixels(_hwnd);
		if (_window_being_resized == tool_window::props)
		{
			LONG pg_desired_width_pixels = pp.x - _resize_offset;
			pg_desired_width_pixels = std::max (pg_desired_width_pixels, 0l);
			pg_desired_width_pixels = std::min (pg_desired_width_pixels, cs.cx);
			float new_pg_desired_width_dips = edge::lengthp_to_lengthd(pg_desired_width_pixels, dpi);
			if (_pw_desired_width_dips != new_pg_desired_width_dips)
			{
				_pw_desired_width_dips = new_pg_desired_width_dips;
				MoveWindow (_pw->hWnd(), pg_restricted_rect());
				::UpdateWindow (_pw->hWnd());
				MoveWindow (_vlanWindow->hwnd(), { GetVlanWindowLeft(), 0, GetVlanWindowRight(), vlanHeight });
				::UpdateWindow (_vlanWindow->hwnd());
				MoveWindow (_edit_window->hWnd(), edit_window_rect());
				::UpdateWindow (_edit_window->hWnd());
			}
		}
		else if (_window_being_resized == tool_window::log)
		{
			LONG log_desired_width_pixels = cs.cx - pp.x - _resize_offset;
			log_desired_width_pixels = std::max (log_desired_width_pixels, 0l);
			log_desired_width_pixels = std::min (log_desired_width_pixels, cs.cx);
			float new_log_desired_width_dips = edge::lengthp_to_lengthd(log_desired_width_pixels, dpi);
			if (_log_desired_width_dips != new_log_desired_width_dips)
			{
				_log_desired_width_dips = new_log_desired_width_dips;
				MoveWindow (_log_window->hwnd(), log_restricted_rect());
				::UpdateWindow (_log_window->hwnd());
				MoveWindow (_vlanWindow->hwnd(), { GetVlanWindowLeft(), 0, GetVlanWindowRight(), vlanHeight });
				::UpdateWindow (_vlanWindow->hwnd());
				MoveWindow (_edit_window->hWnd(), edit_window_rect());
				::UpdateWindow (_edit_window->hWnd());
			}
		}
	}

	handled ProcessWmLButtonUp (POINT pt, UINT modifierKeysDown)
	{
		if (_window_being_resized == tool_window::props)
		{
			::ReleaseCapture();
			WriteRegFloat (RegValueNamePropertiesWindowWidth, _pw_desired_width_dips);
			_window_being_resized = tool_window::none;
			return handled(true);
		}
		else if (_window_being_resized == tool_window::log)
		{
			::ReleaseCapture();
			WriteRegFloat (RegValueNameLogWindowWidth, _log_desired_width_dips);
			_window_being_resized = tool_window::none;
			return handled(true);
		}

		return handled(false);
	}

	HCURSOR cursor_at (POINT pp) const
	{
		RECT rc;

		if (_pw && GetWindowRect(_pw->hWnd(), &rc) && (pp.x >= rc.right - rc.left) && (pp.x < rc.right - rc.left + splitter_width_pixels()))
			return LoadCursor(nullptr, IDC_SIZEWE);

		auto ewr = edit_window_rect();
		if ((_log_window != nullptr) && (pp.x >= ewr.right) && (pp.x < ewr.right + splitter_width_pixels()))
			return LoadCursor(nullptr, IDC_SIZEWE);

		return LoadCursor(nullptr, IDC_ARROW);
	}

	void ProcessWmPaint()
	{
		PAINTSTRUCT ps;
		BeginPaint(_hwnd, &ps);

		SIZE cs = edge::client_size_pixels(_hwnd);

		if (_pw)
		{
			RECT rect;
			GetWindowRect(_pw->hWnd(), &rect);
			rect.left = rect.right - rect.left;
			rect.top = 0;
			rect.right = rect.left + splitter_width_pixels();
			rect.bottom = cs.cy;
			FillRect (ps.hdc, &rect, GetSysColorBrush(COLOR_3DFACE));
		}

		if (_log_window)
		{
			RECT rect;
			rect.left = edit_window_rect().right;
			rect.right = rect.left + splitter_width_pixels();
			rect.top = 0;
			rect.bottom = cs.cy;
			FillRect (ps.hdc, &rect, GetSysColorBrush(COLOR_3DFACE));
		}

		EndPaint(_hwnd, &ps);
	}

	std::optional<LRESULT> ProcessWmCommand (WPARAM wParam, LPARAM lParam)
	{
		HRESULT hr;

		if (wParam == ID_VIEW_PROPERTIES)
		{
			if (_pw)
				DestroyPropertiesWindow();
			else
				CreatePropertiesWindow();
			ResizeChildWindows();
			return 0;
		}

		if (wParam == ID_VIEW_STPLOG)
		{
			if (_log_window != nullptr)
				destroy_log_window();
			else {
				auto hr = CreateLogWindow(); LOG_IF_FAILED(hr);
			}
			ResizeChildWindows();
			return 0;
		}

		if (wParam == ID_VIEW_VLANS)
		{
			// TODO: show/hide.
			return 0;
		}

		if (((HIWORD(wParam) == 0) || (HIWORD(wParam) == 1)) && ((LOWORD(wParam) == ID_FILE_SAVE) || (LOWORD(wParam) == ID_FILE_SAVEAS)))
		{
			if ((LOWORD(wParam) == ID_FILE_SAVEAS) || _project->GetFilePath(nullptr) == S_FALSE)
			{
				wil::unique_bstr path;
				hr = PickSavePath (_hwnd, nullptr, ProjectFileDialogFileTypes, ProjectFileExtensionWithoutDot, &path);
				if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
					return 0;
				hr = _project->Save(path.get()); RETURN_IF_FAILED(hr);
			}
			else
			{
				hr = _project->Save(nullptr); RETURN_IF_FAILED(hr);
			}
			//catch (const std::exception& ex)
			//{
			//	TaskDialog (_hwnd, nullptr, _app->app_namew(), L"Can't save", utf8_to_utf16(ex.what()).c_str(), 0, TD_ERROR_ICON, nullptr);
			//}

			return 0;
		}

		if (((HIWORD(wParam) == 0) || (HIWORD(wParam) == 1)) && (LOWORD(wParam) == ID_FILE_OPEN))
		{
			wil::unique_bstr path;
			hr = PickOpenPath (_hwnd, nullptr, ProjectFileDialogFileTypes, ProjectFileExtensionWithoutDot, &path);
			if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
				return 0;
			open(path.get());
			//catch (const std::exception& ex)
			//{
			//	TaskDialog (_hwnd, nullptr, _app->app_namew(), L"Can't open", utf8_to_utf16(ex.what()).c_str(), 0, TD_ERROR_ICON, nullptr);
			//}

			return 0;
		}

		if (((HIWORD(wParam) == 0) || (HIWORD(wParam) == 1)) && (LOWORD(wParam) == ID_FILE_NEW))
		{
			com_ptr<IStpProject> project;
			auto hr = _app->project_factory()(&project); LOG_IF_FAILED(hr);
			if (SUCCEEDED(hr))
			{
				project_window_create_params params = { _app, project, true, true, 1, SW_SHOW };
				com_ptr<IProjectWindow> pw;
				hr = _app->project_window_factory()(params, &pw); LOG_IF_FAILED(hr);
				if (SUCCEEDED(hr)) {
					hr = _app->AddProjectWindow(pw); LOG_IF_FAILED(hr);
				}
			}

			return 0;
		}

		if (wParam == ID_FILE_EXIT)
		{
			PostMessage (_hwnd, WM_CLOSE, 0, 0);
			return 0;
		}

		if ((wParam >= ID_RECENT_FILE_FIRST) && (wParam <= ID_RECENT_FILE_LAST))
		{
			UINT recentFileIndex = (UINT)wParam - ID_RECENT_FILE_FIRST;
			auto mainMenu = ::GetMenu(_hwnd);
			auto fileMenu = ::GetSubMenu (mainMenu, 0);
			int charCount = ::GetMenuString (fileMenu, (UINT)wParam, nullptr, 0, MF_BYCOMMAND);
			if (charCount > 0)
			{
				auto path = std::make_unique<wchar_t[]>(charCount + 1);
				::GetMenuString (fileMenu, (UINT)wParam, path.get(), charCount + 1, MF_BYCOMMAND);
				//try
				//{
					open(path.get());
				//}
				//catch (const std::exception& ex)
				//{
				//	TaskDialog (_hwnd, nullptr, _app->app_namew(), L"Can't open", utf8_to_utf16(ex.what()).c_str(), 0, TD_ERROR_ICON, nullptr);
				//}
			}
		}

		if (wParam == ID_HELP_ABOUT)
		{
			auto text = std::string(_app->app_name()) + " v" + _app->app_version_string();
			MessageBoxA (_hwnd, text.c_str(), _app->app_name(), 0);
			return 0;
		}

		return std::nullopt;
	}

	HRESULT open (const wchar_t* path)
	{
		HRESULT hr;

		for (ULONG i = 0; i < _app->ProjectWindowCount(); i++)
		{
			IProjectWindow* pw = _app->ProjectWindowAt(i);
			wil::unique_bstr pwFilePath;
			if ((pw->project()->GetFilePath(&pwFilePath) == S_OK) && !_wcsicmp (pwFilePath.get(), path))
			{
				::BringWindowToTop (pw->hwnd());
				::FlashWindow (pw->hwnd(), FALSE);
				return S_OK;
			}
		}

		if (_project->BridgeCount() == 0 && _project->WireCount() == 0)
		{
			hr = _project->Load(path); RETURN_IF_FAILED(hr);
			return S_OK;
		}

		com_ptr<IStpProject> new_project;
		hr = _app->project_factory()(&new_project); RETURN_IF_FAILED(hr);
		hr = new_project->Load(path); RETURN_IF_FAILED(hr);
		project_window_create_params cps = { _app, new_project, true, true, 1, SW_SHOW };
		com_ptr<IProjectWindow> projectWindow;
		hr = _app->project_window_factory()(cps, &projectWindow); RETURN_IF_FAILED(hr);
		hr = _app->AddProjectWindow(projectWindow); RETURN_IF_FAILED(hr);
		return S_OK;
	}

	bool try_close_window()
	{
		HRESULT hr;

		ULONG count = 0;
		for (ULONG i = 0; i < _app->ProjectWindowCount(); i++)
		{
			if (_app->ProjectWindowAt(i)->project() == _project)
				count++;
		}

		if ((count == 1) && _project->GetChangedFlag())
		{
			// Closing last window of a project with changes not saved.
			static const TASKDIALOG_BUTTON buttons[] =
			{
				{ IDYES, L"Save Changes" },
				{ IDNO, L"Discard Changes" },
				{ IDCANCEL, L"Cancel" },
			};

			TASKDIALOGCONFIG tdc = { sizeof (tdc) };
			tdc.hwndParent = _hwnd;
			tdc.pszWindowTitle = _app->app_namew();
			tdc.pszMainIcon = TD_WARNING_ICON;
			tdc.pszMainInstruction = L"File was changed";
			tdc.pszContent = L"Save changes?";
			tdc.cButtons = _countof(buttons);
			tdc.pButtons = buttons;
			tdc.nDefaultButton = IDOK;

			int pressedButton;
			hr = TaskDialogIndirect (&tdc, &pressedButton, nullptr, nullptr); RETURN_IF_FAILED(hr);

			if (pressedButton == IDCANCEL)
				return false;

			if (pressedButton == IDYES)
			{
				wil::unique_bstr path;
				hr = _project->GetFilePath(&path); RETURN_IF_FAILED(hr);
				if (hr == S_FALSE)
				{
					hr = PickSavePath (_hwnd, nullptr, ProjectFileDialogFileTypes, ProjectFileExtensionWithoutDot, &path);
					if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
						return false;
					RETURN_IF_FAILED(hr);
				}

				hr = _project->Save(path.get()); RETURN_IF_FAILED(hr);
			}
			//catch (const std::exception& ex)
			//{
			//	TaskDialog (hwnd(), nullptr, _app->app_namew(), L"Can't save", utf8_to_utf16(ex.what()).c_str(), 0, TD_ERROR_ICON, nullptr);
			//	return false;
			//}
		}

		return true;
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

	virtual HWND hwnd() const override { return _hwnd; }

	#pragma region IVlanSelectionEvents
	virtual HRESULT STDMETHODCALLTYPE OnVlanSelectionChanging (DWORD dwOld) override
	{
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnVlanSelectionChanged (DWORD vlanNumber) override
	{
		_ASSERT ((vlanNumber > 0) && (vlanNumber <= 4094));
		SetWindowTitle();
		return S_OK;
	}
	#pragma endregion

	virtual IStpProject* project() const override final { return _project; }

	virtual HRESULT STDMETHODCALLTYPE GetVlanSelection (IVlanSelection** ppVlanSelection) override
	{
		return _vlanWindow->GetVlanSelection(ppVlanSelection);
	}

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
};

//static
const WNDCLASSEX ProjectWindowImpl::wnd_class = {
	.cbSize = sizeof(WNDCLASSEX),
	.style = CS_DBLCLKS,
	.lpfnWndProc = &ProjectWindowImpl::window_proc_static,
	.hInstance = (HINSTANCE)&__ImageBase,
	.hIcon = ::LoadIcon((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDI_DESIGNER)),
	.hCursor = ::LoadCursor (nullptr, IDC_ARROW),
	.lpszMenuName = MAKEINTRESOURCE(IDR_MAIN_MENU),
	.lpszClassName = L"ProjectWindow",
	.hIconSm = ::LoadIcon((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDI_DESIGNER)),
};

extern HRESULT MakeProjectWindow (const project_window_create_params& cps, IProjectWindow** ppProjectWindow)
{
	auto p = com_ptr(new (std::nothrow) ProjectWindowImpl()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(cps); RETURN_IF_FAILED(hr);
	*ppProjectWindow = p.detach();
	return S_OK;
};
