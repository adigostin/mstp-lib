#include "pch.h"
#include "Simulator.h"
#include "Resource.h"
#include "Bridge.h"
#include "Port.h"
#include "win32/window.h"

using namespace std;
using namespace edge;

static ATOM wndClassAtom;
static constexpr wchar_t ProjectWindowWndClassName[] = L"ProjectWindow-{24B42526-2970-4B3C-A753-2DABD22C4BB0}";
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

static constexpr LONG SplitterWidthDips = 4;
static constexpr LONG PropertiesWindowDefaultWidthDips = 200;
static constexpr LONG log_window_default_width_dips = 200;

static const wnd_class_params class_params = 
{
	ProjectWindowWndClassName,      // lpszClassName
	CS_DBLCLKS,                     // style
	MAKEINTRESOURCE(IDR_MAIN_MENU), // lpszMenuName
	MAKEINTRESOURCE(IDI_DESIGNER),  // lpIconName
	MAKEINTRESOURCE(IDI_DESIGNER),  // lpIconSmName
};

#pragma warning (disable: 4250)

class ProjectWindow : public window, public virtual IProjectWindow
{
	using base = window;

	simulator_app_i* const _app;
	com_ptr<ID3D11DeviceContext1> const _d3d_dc;
	com_ptr<IDWriteFactory>       const _dwrite_factory;
	std::shared_ptr<IProject>   const _project;
	std::unique_ptr<ISelection> const _selection;
	std::unique_ptr<edit_area_i>          _editWindow;
	std::unique_ptr<properties_window_i>  _propertiesWindow;
	std::unique_ptr<log_window_i>       _log_window;
	std::unique_ptr<IVlanWindow>        _vlanWindow;
	SIZE _clientSize;
	RECT _restoreBounds;
	unsigned int _selectedVlanNumber = 1;
	int _dpiX;
	int _dpiY;
	LONG _splitterWidthPixels;

	enum class ToolWindow { None, Props, Vlan, Log };
	ToolWindow _windowBeingResized = ToolWindow::None;
	LONG _resizeOffset;

public:
	ProjectWindow (const project_window_create_params& create_params)
		: window(create_params.app->GetHInstance(), class_params, 0, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
				 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr)
		, _app(create_params.app)
		, _project(create_params.project)
		, _selection(selectionFactory(create_params.project.get()))
		, _selectedVlanNumber(create_params.selectedVlan)
		, _d3d_dc(create_params.d3d_dc)
		, _dwrite_factory(create_params.dwrite_factory)
	{
		int nCmdShow = create_params.nCmdShow;
		bool read = TryGetSavedWindowLocation (&_restoreBounds, &nCmdShow);
		if (!read)
			::GetWindowRect(hwnd(), &_restoreBounds);
		::ShowWindow (hwnd(), nCmdShow);

		auto hdc = ::GetDC(hwnd());
		_dpiX = GetDeviceCaps (hdc, LOGPIXELSX);
		_dpiY = GetDeviceCaps (hdc, LOGPIXELSY);
		::ReleaseDC (hwnd(), hdc);
		_splitterWidthPixels = (SplitterWidthDips * _dpiX + 96 / 2) / 96;

		if (auto recentFiles = GetRecentFileList(); !recentFiles.empty())
			AddRecentFileMenuItems(recentFiles);

		const RECT clientRect = { 0, 0, _clientSize.cx, _clientSize.cy };

		if (create_params.showPropertiesWindow)
			CreatePropertiesWindow();

		if (create_params.showLogWindow)
			CreateLogWindow();

		_vlanWindow = vlanWindowFactory (_app, this, _project, _selection.get(), hwnd(), { GetVlanWindowLeft(), 0 });
		SetMainMenuItemCheck (ID_VIEW_VLANS, true);

		_editWindow = edit_area_factory (create_params.app, this, _project.get(), _selection.get(), hwnd(), edit_window_rect(), create_params.d3d_dc, create_params.dwrite_factory);

		_project->GetChangedFlagChangedEvent().add_handler (&OnProjectChangedFlagChanged, this);
		_app->project_window_added().add_handler (&OnProjectWindowAdded, this);
		_app->project_window_removed().add_handler (&OnProjectWindowRemoved, this);
		_project->GetLoadedEvent().add_handler (&OnProjectLoaded, this);
	}

	~ProjectWindow()
	{
		_project->GetLoadedEvent().remove_handler (&OnProjectLoaded, this);
		_app->project_window_removed().remove_handler (&OnProjectWindowRemoved, this);
		_app->project_window_added().remove_handler (&OnProjectWindowAdded, this);
		_project->GetChangedFlagChangedEvent().remove_handler (&OnProjectChangedFlagChanged, this);
	}

	void CreatePropertiesWindow()
	{
		LONG w = (PropertiesWindowDefaultWidthDips * _dpiX + 96 / 2) / 96;
		TryReadRegDword (RegValueNamePropertiesWindowWidth, (DWORD*) &w);
		w = RestrictToolWindowWidth(w);
		_propertiesWindow = properties_window_factory (_app, this, _project.get(), _selection.get(), { 0, 0, w, _clientSize.cy }, hwnd(), _d3d_dc, _dwrite_factory);
		SetMainMenuItemCheck (ID_VIEW_PROPERTIES, true);
	}

	void DestroyPropertiesWindow()
	{
		_propertiesWindow = nullptr;
		SetMainMenuItemCheck (ID_VIEW_PROPERTIES, false);
	}

	void CreateLogWindow()
	{
		LONG w = (log_window_default_width_dips * _dpiX + 96 / 2) / 96;
		TryReadRegDword (RegValueNameLogWindowWidth, (DWORD*) &w);
		w = RestrictToolWindowWidth(w);
		_log_window = log_window_factory (_app->GetHInstance(), hwnd(), { _clientSize.cx - w, 0, _clientSize.cx, _clientSize.cy }, _d3d_dc, _dwrite_factory, _selection.get());
		SetMainMenuItemCheck (ID_VIEW_STPLOG, true);
	}

	void DestroyLogWindow()
	{
		_log_window = nullptr;
		SetMainMenuItemCheck (ID_VIEW_STPLOG, false);
	}

	static void OnProjectLoaded (void* callbackArg, IProject* project)
	{
		auto pw = static_cast<ProjectWindow*>(callbackArg);
		pw->SetWindowTitle();
	}

	static void OnProjectWindowAdded (void* callbackArg, IProjectWindow* pw)
	{
		auto thispw = static_cast<ProjectWindow*>(callbackArg);
		if (pw->GetProject() == thispw->GetProject())
			thispw->SetWindowTitle();
	}

	static void OnProjectWindowRemoved (void* callbackArg, IProjectWindow* pw)
	{
		auto thispw = static_cast<ProjectWindow*>(callbackArg);
		if (pw->GetProject() == thispw->GetProject())
			thispw->SetWindowTitle();
	}

	static void OnProjectChangedFlagChanged (void* callbackArg, IProject* project)
	{
		auto pw = static_cast<ProjectWindow*>(callbackArg);
		pw->SetWindowTitle();
	}

	LONG GetVlanWindowLeft() const
	{
		if (_propertiesWindow != nullptr)
		{
			//return _propertiesWindow->GetWidth() + _splitterWidthPixels;
			RECT rect;
			::GetWindowRect (_propertiesWindow->hwnd(), &rect);
			return rect.right - rect.left + _splitterWidthPixels;
		}
		else
			return 0;
	}

	LONG GetVlanWindowRight() const
	{
		if (_log_window != nullptr)
			return _clientSize.cx - _log_window->GetWidth() - _splitterWidthPixels;
		else
			return _clientSize.cx;
	}

	RECT edit_window_rect() const
	{
		auto rect = client_rect_pixels();

		if (_propertiesWindow != nullptr)
			rect.left += _propertiesWindow->GetWidth() + _splitterWidthPixels;

		if (_log_window != nullptr)
			rect.right -= _log_window->GetWidth() + _splitterWidthPixels;

		if (_vlanWindow != nullptr)
			rect.top += _vlanWindow->GetHeight();

		return rect;
	}

	void SetWindowTitle()
	{
		wstringstream windowTitle;

		const auto& filePath = _project->GetFilePath();
		if (!filePath.empty())
		{
			const wchar_t* fileName = PathFindFileName (filePath.c_str());
			const wchar_t* fileExt = PathFindExtension (filePath.c_str());
			windowTitle << setw(fileExt - fileName) << fileName;
		}
		else
			windowTitle << L"Untitled";

		if (_project->GetChangedFlag())
			windowTitle << L"*";

		auto& pws = _app->project_windows();
		if (any_of (pws.begin(), pws.end(), [this](const std::unique_ptr<IProjectWindow>& pw) { return (pw.get() != this) && (pw->GetProject() == _project.get()); }))
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
			auto r = (RECT*) lParam;
			::SetWindowPos (hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
			return 0;
		}

		if (msg == WM_CLOSE)
		{
			TryClose();
			return 0;
		}

		if (msg == WM_SIZE)
		{
			ProcessWmSize (hwnd, wParam, { LOWORD(lParam), HIWORD(lParam) });
			return 0;
		}

		if (msg == WM_MOVE)
		{
			WINDOWPLACEMENT wp = { sizeof(wp) };
			::GetWindowPlacement (hwnd, &wp);
			if (wp.showCmd == SW_NORMAL)
				::GetWindowRect (hwnd, &_restoreBounds);
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
			ProcessWmMouseMove (POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }, (UINT) wParam);
			return 0;
		}

		if (msg == WM_LBUTTONUP)
		{
			ProcessWmLButtonUp (POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }, (UINT) wParam);
			return 0;
		}

		return base_class_result;
	}

	void ProcessWmSize (HWND hwnd, WPARAM wParam, SIZE newClientSize)
	{
		if (wParam == SIZE_RESTORED)
			::GetWindowRect(hwnd, &_restoreBounds);

		if (wParam != SIZE_MINIMIZED)
		{
			_clientSize = newClientSize;
			ResizeChildWindows();
		}
	}

	void ResizeChildWindows()
	{
		if (_propertiesWindow != nullptr)
			_propertiesWindow->SetRect ({ 0, 0, RestrictToolWindowWidth(_propertiesWindow->GetWidth()), _clientSize.cy });

		if (_log_window != nullptr)
			_log_window->SetRect ({ _clientSize.cx - RestrictToolWindowWidth(_log_window->GetWidth()), 0, _clientSize.cx, _clientSize.cy });

		if (_vlanWindow != nullptr)
			_vlanWindow->SetRect ({ GetVlanWindowLeft(), 0, GetVlanWindowRight(), _vlanWindow->GetHeight() });

		if (_editWindow != nullptr)
			_editWindow->SetRect (edit_window_rect());
	}

	void ProcessWmLButtonDown (POINT pt, UINT modifierKeysDown)
	{
		if ((_propertiesWindow != nullptr) && (pt.x >= _propertiesWindow->GetWidth()) && (pt.x < _propertiesWindow->GetWidth() + _splitterWidthPixels))
		{
			_windowBeingResized = ToolWindow::Props;
			_resizeOffset = pt.x - _propertiesWindow->GetWidth();
			::SetCapture(hwnd());
		}
		else if ((_log_window != nullptr) && (pt.x >= _log_window->GetX() - _splitterWidthPixels) && (pt.x < _log_window->GetX()))
		{
			_windowBeingResized = ToolWindow::Log;
			_resizeOffset = _log_window->GetX() - pt.x;
			::SetCapture(hwnd());
		}
	}

	void ProcessWmMouseMove (POINT pt, UINT modifierKeysDown)
	{
		if (_windowBeingResized == ToolWindow::Props)
		{
			_propertiesWindow->SetWidth (RestrictToolWindowWidth(pt.x - _resizeOffset));
			_vlanWindow->SetRect ({ GetVlanWindowLeft(), 0, GetVlanWindowRight(), _vlanWindow->GetHeight() });
			_editWindow->SetRect (edit_window_rect());
			::UpdateWindow (_propertiesWindow->hwnd());
			::UpdateWindow (_editWindow->hwnd());
			::UpdateWindow (_vlanWindow->hwnd());
		}
		else if (_windowBeingResized == ToolWindow::Log)
		{
			_log_window->SetRect ({ _clientSize.cx - RestrictToolWindowWidth(_clientSize.cx - pt.x - _resizeOffset), 0, _clientSize.cx, _clientSize.cy});
			_vlanWindow->SetRect ({ GetVlanWindowLeft(), 0, GetVlanWindowRight(), _vlanWindow->GetHeight() });
			_editWindow->SetRect (edit_window_rect());
			::UpdateWindow (_log_window->hwnd());
			::UpdateWindow (_editWindow->hwnd());
			::UpdateWindow (_vlanWindow->hwnd());
		}
	}

	void ProcessWmLButtonUp (POINT pt, UINT modifierKeysDown)
	{
		if (_windowBeingResized == ToolWindow::Props)
		{
			WriteRegDword (RegValueNamePropertiesWindowWidth, (DWORD) _propertiesWindow->GetWidth());
			_windowBeingResized = ToolWindow::None;
			::ReleaseCapture();
		}
		else if (_windowBeingResized == ToolWindow::Log)
		{
			WriteRegDword (RegValueNameLogWindowWidth, (DWORD) _log_window->GetWidth());
			_windowBeingResized = ToolWindow::None;
			::ReleaseCapture();
		}
	}

	void SetCursor (POINT pt)
	{
		if ((_propertiesWindow != nullptr) && (pt.x >= _propertiesWindow->GetWidth()) && (pt.x < _propertiesWindow->GetWidth() + _splitterWidthPixels))
		{
			::SetCursor (LoadCursor(nullptr, IDC_SIZEWE));
		}
		else if ((_log_window != nullptr) && (pt.x >= _log_window->GetX() - _splitterWidthPixels) && (pt.x < _log_window->GetX()))
		{
			::SetCursor (LoadCursor(nullptr, IDC_SIZEWE));
		}
		else
			::SetCursor (LoadCursor(nullptr, IDC_ARROW));
	}

	LONG RestrictToolWindowWidth (LONG desiredWidth) const
	{
		if (desiredWidth > _clientSize.cx * 40 / 100)
			desiredWidth = _clientSize.cx * 40 / 100;
		else if (desiredWidth < 50)
			desiredWidth = 50;
		return desiredWidth;
	}

	void ProcessWmPaint()
	{
		PAINTSTRUCT ps;
		BeginPaint(hwnd(), &ps);

		RECT rect;

		if (_propertiesWindow != nullptr)
		{
			rect.left = _propertiesWindow->GetWidth();
			rect.top = 0;
			rect.right = rect.left + _splitterWidthPixels;
			rect.bottom = _clientSize.cy;
			FillRect (ps.hdc, &rect, GetSysColorBrush(COLOR_3DFACE));
		}

		if (_log_window != nullptr)
		{
			rect.right = _log_window->GetX();
			rect.left = rect.right - _splitterWidthPixels;
			rect.top = 0;
			rect.bottom = _clientSize.cy;
			FillRect (ps.hdc, &rect, GetSysColorBrush(COLOR_3DFACE));
		}

		EndPaint(hwnd(), &ps);
	}

	optional<LRESULT> ProcessWmCommand (WPARAM wParam, LPARAM lParam)
	{
		if (wParam == ID_VIEW_PROPERTIES)
		{
			if (_propertiesWindow != nullptr)
				DestroyPropertiesWindow();
			else
				CreatePropertiesWindow();
			ResizeChildWindows();
			return 0;
		}

		if (wParam == ID_VIEW_STPLOG)
		{
			if (_log_window != nullptr)
				DestroyLogWindow();
			else
				CreateLogWindow();
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
			wstring openPath;
			HRESULT hr = TryChooseFilePath (OpenOrSave::Open, hwnd(), nullptr, openPath);
			if (SUCCEEDED(hr))
				Open(openPath.c_str());
			return 0;
		}

		if (((HIWORD(wParam) == 0) || (HIWORD(wParam) == 1)) && (LOWORD(wParam) == ID_FILE_NEW))
		{
			auto project = projectFactory();
			project_window_create_params params = 
			{
				_app, project, selectionFactory, edit_area_factory, true, true, 1, SW_SHOW, _d3d_dc, _dwrite_factory
			};
			
			auto pw = projectWindowFactory(params);
			_app->add_project_window(std::move(pw));
			return 0;
		}

		if (wParam == ID_FILE_SAVEAS)
		{
			MessageBox (hwnd(), L"Not yet implemented.", _app->GetAppName(), 0);
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
				auto path = make_unique<wchar_t[]>(charCount + 1);
				::GetMenuString (fileMenu, (UINT)wParam, path.get(), charCount + 1, MF_BYCOMMAND);
				Open(path.get());
			}
		}

		if (wParam == ID_HELP_ABOUT)
		{
			wstring text (_app->GetAppName());
			text += L" v";
			text += _app->GetAppVersionString();
			MessageBox (hwnd(), text.c_str(), _app->GetAppName(), 0);
			return 0;
		}

		return nullopt;
	}

	void Open (const wchar_t* openPath)
	{
		for (auto& pw : _app->project_windows())
		{
			if (_wcsicmp (pw->GetProject()->GetFilePath().c_str(), openPath) == 0)
			{
				::BringWindowToTop (pw->hwnd());
				::FlashWindow (pw->hwnd(), FALSE);
				return;
			}
		}

		std::shared_ptr<IProject> projectToLoadTo = (_project->GetBridges().empty() && _project->GetWires().empty()) ? _project : projectFactory();

		assert(false);
		/*
		try
		{
			projectToLoadTo->Load(openPath);
		}
		catch (const exception& ex)
		{
			wstringstream ss;
			ss << ex.what() << endl << endl << openPath;
			TaskDialog (hwnd(), nullptr, _app->GetAppName(), L"Could Not Open", ss.str().c_str(), 0, nullptr, nullptr);
			return;
		}

		if (projectToLoadTo != _project)
		{
			assert(false);
			//auto newWindow = projectWindowFactory(_app, projectToLoadTo, selectionFactory, edit_area_factory, true, true, SW_SHOW, 1);
			//_app->add_project_window(newWindow);
		}
		*/
	}

	HRESULT Save()
	{
		HRESULT hr;

		auto savePath = _project->GetFilePath();
		if (savePath.empty())
		{
			hr = TryChooseFilePath (OpenOrSave::Save, hwnd(), L"", savePath);
			if (FAILED(hr))
				return hr;
		}

		hr = _project->Save (savePath.c_str());
		if (FAILED(hr))
		{
			TaskDialog (hwnd(), nullptr, _app->GetAppName(), L"Could Not Save", _com_error(hr).ErrorMessage(), 0, nullptr, nullptr);
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

		TASKDIALOGCONFIG tdc = { sizeof (tdc) };
		tdc.hwndParent = hwnd();
		tdc.pszWindowTitle = _app->GetAppName();
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

	static HRESULT TryChooseFilePath (OpenOrSave which, HWND fileDialogParentHWnd, const wchar_t* pathToInitializeDialogTo, wstring& sbOut)
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

			wstring dir (pathToInitializeDialogTo, filePtr - pathToInitializeDialogTo);
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
							   [this] (const std::unique_ptr<IProjectWindow>& pw) { return pw->GetProject() == _project.get(); });
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
		if (lstatus != ERROR_SUCCESS)
			throw exception();

		RegSetValueEx(key, valueName, 0, REG_DWORD, (BYTE*)&value, 4);
		RegCloseKey(key);
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
				RegSetValueEx(key, RegValueNameWindowLeft, 0, REG_DWORD, (BYTE*)&_restoreBounds.left, 4);
				RegSetValueEx(key, RegValueNameWindowTop, 0, REG_DWORD, (BYTE*)&_restoreBounds.top, 4);
				RegSetValueEx(key, RegValueNameWindowRight, 0, REG_DWORD, (BYTE*)&_restoreBounds.right, 4);
				RegSetValueEx(key, RegValueNameWindowBottom, 0, REG_DWORD, (BYTE*)&_restoreBounds.bottom, 4);
				RegSetValueEx(key, RegValueNameShowCmd, 0, REG_DWORD, (BYTE*)&wp.showCmd, 4);
				RegCloseKey(key);
			}
		}
	}

	virtual void SelectVlan (unsigned int vlanNumber) override final
	{
		if ((vlanNumber == 0) || (vlanNumber > 4095))
			throw invalid_argument (u8"Invalid VLAN number.");

		if (_selectedVlanNumber != vlanNumber)
		{
			_selectedVlanNumber = vlanNumber;
			event_invoker<SelectedVlanNumerChangedEvent>()(this, vlanNumber);
			::InvalidateRect (hwnd(), nullptr, FALSE);
			SetWindowTitle();
		}
	};

	virtual unsigned int selected_vlan_number() const override final { return _selectedVlanNumber; }

	virtual SelectedVlanNumerChangedEvent::subscriber GetSelectedVlanNumerChangedEvent() override final { return SelectedVlanNumerChangedEvent::subscriber(this); }

	virtual IProject* GetProject() const override final { return _project.get(); }

	virtual edit_area_i* GetEditArea() const override final { return _editWindow.get(); }

	static vector<wstring> GetRecentFileList()
	{
		// We ignore errors in this particular function.

		vector<wstring> fileList;

		com_ptr<IApplicationDocumentLists> docList;
		auto hr = CoCreateInstance (CLSID_ApplicationDocumentLists, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IApplicationDocumentLists), (void**) &docList);
		if (FAILED(hr))
			return fileList;

		// This function retrieves the list created via calls to SHAddToRecentDocs.
		// We use the standard file dialogs througout the application, which call SHAddToRecentDocs for us.
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

	void AddRecentFileMenuItems (const vector<wstring>& recentFiles)
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

extern const ProjectWindowFactory projectWindowFactory = [](const project_window_create_params& create_params) -> std::unique_ptr<IProjectWindow>
{
	return std::make_unique<ProjectWindow>(create_params);
};
