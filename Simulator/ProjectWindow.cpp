#include "pch.h"
#include "Simulator.h"
#include "Resource.h"
#include "Bridge.h"
#include "Port.h"

using namespace std;

#define WM_WORK  (WM_APP + 1)

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
static constexpr LONG LogWindowDefaultWidthDips = 200;

class ProjectWindow : public EventManager, public IProjectWindow
{
	ULONG _refCount = 1;
	ISimulatorApp* const _app;
	IProjectPtr    const _project;
	ISelectionPtr  const _selection;
	IEditAreaPtr         _editWindow;
	IPropertiesWindowPtr _propertiesWindow;
	ILogAreaPtr          _logWindow;
	IVlanWindowPtr       _vlanWindow;
	HWND _hwnd;
	SIZE _clientSize;
	RECT _restoreBounds;
	unsigned int _selectedVlanNumber = 1;
	queue<function<void()>> _workQueue;
	int _dpiX;
	int _dpiY;
	LONG _splitterWidthPixels;

	enum class ToolWindow { None, Props, Vlan, Log };
	ToolWindow _windowBeingResized = ToolWindow::None;
	LONG _resizeOffset;

public:
	ProjectWindow (ISimulatorApp* app,
				   IProject* project,
				   SelectionFactory selectionFactory,
				   EditAreaFactory editAreaFactory,
				   bool showPropertiesWindow,
				   bool showLogWindow,
				   int nCmdShow,
				   unsigned int selectedVlan)
		: _app(app)
		, _project(project)
		, _selection(selectionFactory(project))
		, _selectedVlanNumber(selectedVlan)
	{
		if (wndClassAtom == 0)
		{
			WNDCLASSEX wndClassEx =
			{
				sizeof(wndClassEx),
				CS_DBLCLKS, // style
				&WindowProcStatic, // lpfnWndProc
				0, // cbClsExtra
				0, // cbWndExtra
				_app->GetHInstance(),
				LoadIcon(_app->GetHInstance(), MAKEINTRESOURCE(IDI_DESIGNER)), // hIcon
				LoadCursor(nullptr, IDC_ARROW), // hCursor
				(HBRUSH)(COLOR_WINDOW + 1), // hbrBackground
				MAKEINTRESOURCE(IDR_MAIN_MENU), // lpszMenuName
				ProjectWindowWndClassName,      // lpszClassName
				LoadIcon(_app->GetHInstance(), MAKEINTRESOURCE(IDI_DESIGNER))
			};

			wndClassAtom = RegisterClassEx(&wndClassEx); assert (wndClassAtom != 0);
		}

		bool read = TryGetSavedWindowLocation (&_restoreBounds, &nCmdShow);
		LONG x = CW_USEDEFAULT, y = CW_USEDEFAULT, w = CW_USEDEFAULT, h = CW_USEDEFAULT;
		if (read)
		{
			x = _restoreBounds.left;
			y = _restoreBounds.top;
			w = _restoreBounds.right - _restoreBounds.left;
			h = _restoreBounds.bottom - _restoreBounds.top;
		}
		auto hwnd = ::CreateWindow(ProjectWindowWndClassName, L"STP Simulator", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, w, h, nullptr, 0, _app->GetHInstance(), this); assert (hwnd != nullptr);
		assert(hwnd == _hwnd);
		if (!read)
			::GetWindowRect(_hwnd, &_restoreBounds);
		::ShowWindow (_hwnd, nCmdShow);

		const RECT clientRect = { 0, 0, _clientSize.cx, _clientSize.cy };

		if (showPropertiesWindow)
			CreatePropertiesWindow();

		if (showLogWindow)
			CreateLogWindow();

		_vlanWindow = vlanWindowFactory (_app, this, _project, _selection, _hwnd, { GetVlanWindowLeft(), 0 });
		SetMainMenuItemCheck (ID_VIEW_VLANS, true);

		_editWindow = editAreaFactory (app, this, _project, _selection, _hwnd, GetEditWindowRect(), _app->GetDWriteFactory());

		_project->GetChangedFlagChangedEvent().AddHandler (&OnProjectChangedFlagChanged, this);
		_app->GetProjectWindowAddedEvent().AddHandler (&OnProjectWindowAdded, this);
		_app->GetProjectWindowRemovedEvent().AddHandler (&OnProjectWindowRemoved, this);
		_project->GetLoadedEvent().AddHandler (&OnProjectLoaded, this);
	}

	~ProjectWindow()
	{
		_project->GetLoadedEvent().RemoveHandler (&OnProjectLoaded, this);
		_app->GetProjectWindowRemovedEvent().RemoveHandler (&OnProjectWindowRemoved, this);
		_app->GetProjectWindowAddedEvent().RemoveHandler (&OnProjectWindowAdded, this);
		_project->GetChangedFlagChangedEvent().RemoveHandler (&OnProjectChangedFlagChanged, this);

		if (_hwnd != nullptr)
			::DestroyWindow(_hwnd);
	}

	void CreatePropertiesWindow()
	{
		LONG w = (PropertiesWindowDefaultWidthDips * _dpiX + 96 / 2) / 96;
		TryReadRegDword (RegValueNamePropertiesWindowWidth, (DWORD*) &w);
		w = RestrictToolWindowWidth(w);
		_propertiesWindow = propertiesWindowFactory (_app, this, _project, _selection, { 0, 0, w, _clientSize.cy }, _hwnd);
		SetMainMenuItemCheck (ID_VIEW_PROPERTIES, true);
	}

	void DestroyPropertiesWindow()
	{
		_propertiesWindow = nullptr;
		SetMainMenuItemCheck (ID_VIEW_PROPERTIES, false);
	}

	void CreateLogWindow()
	{
		LONG w = (LogWindowDefaultWidthDips * _dpiX + 96 / 2) / 96;
		TryReadRegDword (RegValueNameLogWindowWidth, (DWORD*) &w);
		w = RestrictToolWindowWidth(w);
		_logWindow = logAreaFactory (_app->GetHInstance(), _hwnd, { _clientSize.cx - w, 0, _clientSize.cx, _clientSize.cy }, _app->GetDWriteFactory(), _selection);
		SetMainMenuItemCheck (ID_VIEW_STPLOG, true);
	}

	void DestroyLogWindow()
	{
		_logWindow = nullptr;
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
			return _propertiesWindow->GetWidth() + _splitterWidthPixels;
		else
			return 0;
	}

	LONG GetVlanWindowRight() const
	{
		if (_logWindow != nullptr)
			return _clientSize.cx - _logWindow->GetWidth() - _splitterWidthPixels;
		else
			return _clientSize.cx;
	}

	RECT GetEditWindowRect() const
	{
		auto rect = GetClientRectPixels();

		if (_propertiesWindow != nullptr)
			rect.left += _propertiesWindow->GetWidth() + _splitterWidthPixels;

		if (_logWindow != nullptr)
			rect.right -= _logWindow->GetWidth() + _splitterWidthPixels;

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

		auto& pws = _app->GetProjectWindows();
		if (any_of (pws.begin(), pws.end(), [this](const IProjectWindowPtr& pw) { return (pw.GetInterfacePtr() != this) && (pw->GetProject() == _project.GetInterfacePtr()); }))
			windowTitle << L" - VLAN " << _selectedVlanNumber;

		::SetWindowText (_hwnd, windowTitle.str().c_str());
	}

	void SetMainMenuItemCheck (UINT item, bool checked)
	{
		auto menu = ::GetMenu(_hwnd);
		MENUITEMINFO mii = { sizeof(mii) };
		mii.fMask = MIIM_STATE;
		mii.fState = checked ? MFS_CHECKED : MFS_UNCHECKED;
		::SetMenuItemInfo (menu, item, FALSE, &mii);
	}

	virtual HWND GetHWnd() const override { return _hwnd; }

	// From http://blogs.msdn.com/b/oldnewthing/archive/2005/04/22/410773.aspx
	static LRESULT CALLBACK WindowProcStatic(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		//if (AssertFunctionRunning)
		//{
		//	// Let's try not to run application code while the assertion dialog is shown. We'll probably mess things up even more.
		//	return DefWindowProc(hwnd, uMsg, wParam, lParam);
		//}

		ProjectWindow* window;
		if (uMsg == WM_NCCREATE)
		{
			LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
			window = reinterpret_cast<ProjectWindow*>(lpcs->lpCreateParams);
			window->AddRef();
			window->_hwnd = hwnd;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
		}
		else
			window = reinterpret_cast<ProjectWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

		if (window == nullptr)
		{
			// this must be one of those messages sent before WM_NCCREATE or after WM_NCDESTROY.
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}

		LRESULT result = window->WindowProc(uMsg, wParam, lParam);

		if (uMsg == WM_NCDESTROY)
		{
			window->_hwnd = nullptr;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
			window->Release();
		}

		return result;
	}

	LRESULT WindowProc(UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (msg == WM_CREATE)
		{
			auto cs = reinterpret_cast<const CREATESTRUCT*>(lParam);
			_clientSize = { cs->cx, cs->cy };

			auto hdc = ::GetDC(_hwnd);
			_dpiX = GetDeviceCaps (hdc, LOGPIXELSX);
			_dpiY = GetDeviceCaps (hdc, LOGPIXELSY);
			::ReleaseDC (_hwnd, hdc);

			_splitterWidthPixels = (SplitterWidthDips * _dpiX + 96 / 2) / 96;
			return 0;
		}

		if (msg == WM_CLOSE)
		{
			TryClose();
			return 0;
		}

		if (msg == WM_DESTROY)
		{
			DestroyingEvent::InvokeHandlers(this, this);
			return 0;
		}

		if (msg == WM_SIZE)
		{
			ProcessWmSize (wParam, { LOWORD(lParam), HIWORD(lParam) });
			return 0;
		}

		if (msg == WM_MOVE)
		{
			WINDOWPLACEMENT wp = { sizeof(wp) };
			::GetWindowPlacement(GetHWnd(), &wp);
			if (wp.showCmd == SW_NORMAL)
				::GetWindowRect(GetHWnd(), &_restoreBounds);
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

			return DefWindowProc(_hwnd, msg, wParam, lParam);
		}

		if (msg == WM_WORK)
		{
			_workQueue.front()();
			_workQueue.pop();
			return 0;
		}

		if (msg == WM_SETCURSOR)
		{
			if (((HWND) wParam == _hwnd) && (LOWORD (lParam) == HTCLIENT))
			{
				POINT pt;
				BOOL bRes = ::GetCursorPos (&pt);
				if (bRes)
				{
					bRes = ::ScreenToClient (_hwnd, &pt); assert(bRes);
					this->SetCursor(pt);
					return 0;
				}
			}

			return DefWindowProc (_hwnd, msg, wParam, lParam);
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

		return DefWindowProc(_hwnd, msg, wParam, lParam);
	}

	void ProcessWmSize (WPARAM wParam, SIZE newClientSize)
	{
		if (wParam == SIZE_RESTORED)
			::GetWindowRect(_hwnd, &_restoreBounds);

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

		if (_logWindow != nullptr)
			_logWindow->SetRect ({ _clientSize.cx - RestrictToolWindowWidth(_logWindow->GetWidth()), 0, _clientSize.cx, _clientSize.cy });

		if (_vlanWindow != nullptr)
			_vlanWindow->SetRect ({ GetVlanWindowLeft(), 0, GetVlanWindowRight(), _vlanWindow->GetHeight() });

		if (_editWindow != nullptr)
			_editWindow->SetRect (GetEditWindowRect());
	}

	void ProcessWmLButtonDown (POINT pt, UINT modifierKeysDown)
	{
		if ((_propertiesWindow != nullptr) && (pt.x >= _propertiesWindow->GetWidth()) && (pt.x < _propertiesWindow->GetWidth() + _splitterWidthPixels))
		{
			_windowBeingResized = ToolWindow::Props;
			_resizeOffset = pt.x - _propertiesWindow->GetWidth();
			::SetCapture(_hwnd);
		}
		else if ((_logWindow != nullptr) && (pt.x >= _logWindow->GetX() - _splitterWidthPixels) && (pt.x < _logWindow->GetX()))
		{
			_windowBeingResized = ToolWindow::Log;
			_resizeOffset = _logWindow->GetX() - pt.x;
			::SetCapture(_hwnd);
		}
	}

	void ProcessWmMouseMove (POINT pt, UINT modifierKeysDown)
	{
		if (_windowBeingResized == ToolWindow::Props)
		{
			_propertiesWindow->SetWidth (RestrictToolWindowWidth(pt.x - _resizeOffset));
			_vlanWindow->SetRect ({ GetVlanWindowLeft(), 0, GetVlanWindowRight(), _vlanWindow->GetHeight() });
			_editWindow->SetRect (GetEditWindowRect());
			::UpdateWindow (_propertiesWindow->GetHWnd());
			::UpdateWindow (_editWindow->GetHWnd());
		}
		else if (_windowBeingResized == ToolWindow::Log)
		{
			_logWindow->SetRect ({ _clientSize.cx - RestrictToolWindowWidth(_clientSize.cx - pt.x - _resizeOffset), 0, _clientSize.cx, _clientSize.cy});
			_vlanWindow->SetRect ({ GetVlanWindowLeft(), 0, GetVlanWindowRight(), _vlanWindow->GetHeight() });
			_editWindow->SetRect (GetEditWindowRect());
			::UpdateWindow (_logWindow->GetHWnd());
			::UpdateWindow (_editWindow->GetHWnd());
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
			WriteRegDword (RegValueNameLogWindowWidth, (DWORD) _logWindow->GetWidth());
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
		else if ((_logWindow != nullptr) && (pt.x >= _logWindow->GetX() - _splitterWidthPixels) && (pt.x < _logWindow->GetX()))
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
		BeginPaint(GetHWnd(), &ps);

		RECT rect;

		if (_propertiesWindow != nullptr)
		{
			rect.left = _propertiesWindow->GetWidth();
			rect.top = 0;
			rect.right = rect.left + _splitterWidthPixels;
			rect.bottom = _clientSize.cy;
			FillRect (ps.hdc, &rect, GetSysColorBrush(COLOR_3DFACE));
		}

		if (_logWindow != nullptr)
		{
			rect.right = _logWindow->GetX();
			rect.left = rect.right - _splitterWidthPixels;
			rect.top = 0;
			rect.bottom = _clientSize.cy;
			FillRect (ps.hdc, &rect, GetSysColorBrush(COLOR_3DFACE));
		}

		EndPaint(GetHWnd(), &ps);
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
			if (_logWindow != nullptr)
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
			Open();
			return 0;
		}

		if (((HIWORD(wParam) == 0) || (HIWORD(wParam) == 1)) && (LOWORD(wParam) == ID_FILE_NEW))
		{
			MessageBox (_hwnd, L"Not yet implemented.", _app->GetAppName(), 0);
			return 0;
		}

		if (wParam == ID_FILE_SAVEAS)
		{
			MessageBox (_hwnd, L"Not yet implemented.", _app->GetAppName(), 0);
			return 0;
		}

		if (wParam == ID_FILE_EXIT)
		{
			PostMessage (_hwnd, WM_CLOSE, 0, 0);
			return 0;
		}

		if (wParam == ID_HELP_ABOUT)
		{
			wstring text (_app->GetAppName());
			text += L" v";
			text += _app->GetAppVersionString();
			MessageBox (_hwnd, text.c_str(), _app->GetAppName(), 0);
			return 0;
		}

		return nullopt;
	}

	void Open()
	{
		wstring openPath;
		HRESULT hr = TryChooseFilePath (OpenOrSave::Open, _hwnd, nullptr, openPath);
		if (FAILED(hr))
			return;

		IProjectPtr projectToLoadTo = _project->GetChangedFlag() ? projectFactory() : _project;

		try
		{
			projectToLoadTo->Load(openPath.c_str());
		}
		catch (const exception& ex)
		{
			wstringstream ss;
			ss << ex.what() << endl << endl << openPath;
			TaskDialog (_hwnd, nullptr, _app->GetAppName(), L"Could Not Open", ss.str().c_str(), 0, nullptr, nullptr);
			return;
		}

		if (projectToLoadTo.GetInterfacePtr() != _project.GetInterfacePtr())
		{
			auto newWindow = projectWindowFactory(_app, projectToLoadTo, selectionFactory, editAreaFactory, true, true, SW_SHOW, 1);
			_app->AddProjectWindow(newWindow);
		}
	}

	HRESULT Save()
	{
		HRESULT hr;

		auto savePath = _project->GetFilePath();
		if (savePath.empty())
		{
			hr = TryChooseFilePath (OpenOrSave::Save, _hwnd, L"", savePath);
			if (FAILED(hr))
				return hr;
		}

		hr = _project->Save (savePath.c_str());
		if (FAILED(hr))
		{
			TaskDialog (_hwnd, nullptr, _app->GetAppName(), L"Could Not Save", _com_error(hr).ErrorMessage(), 0, nullptr, nullptr);
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
		tdc.hwndParent = _hwnd;
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
		IFileDialogPtr dialog;
		HRESULT hr = dialog.CreateInstance ((which == OpenOrSave::Save) ? CLSID_FileSaveDialog : CLSID_FileOpenDialog);
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
			IShellItemPtr si;
			hr = SHCreateItemFromParsingName (dir.c_str(), nullptr, IID_PPV_ARGS(&si));
			if (SUCCEEDED(hr))
				dialog->SetFolder(si);
		}

		hr = dialog->Show(fileDialogParentHWnd);
		if (FAILED(hr))
			return hr;

		IShellItemPtr item;
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
		auto count = count_if (_app->GetProjectWindows().begin(), _app->GetProjectWindows().end(),
							   [this] (auto& pw) { return pw->GetProject() == _project.GetInterfacePtr(); });
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
		::DestroyWindow (_hwnd);
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
		BOOL bRes = GetWindowPlacement(GetHWnd(), &wp);
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
			SelectedVlanNumerChangedEvent::InvokeHandlers(this, this, vlanNumber);
			::InvalidateRect (GetHWnd(), nullptr, FALSE);
			SetWindowTitle();
		}
	};

	virtual unsigned int GetSelectedVlanNumber() const override final { return _selectedVlanNumber; }

	virtual SelectedVlanNumerChangedEvent::Subscriber GetSelectedVlanNumerChangedEvent() override final { return SelectedVlanNumerChangedEvent::Subscriber(this); }

	virtual DestroyingEvent::Subscriber GetDestroyingEvent() override final { return DestroyingEvent::Subscriber(this); }

	virtual IProject* GetProject() const override final { return _project; }

	virtual IEditArea* GetEditArea() const override final { return _editWindow; }

	virtual void PostWork (std::function<void()>&& work) override final
	{
		_workQueue.push (move(work));
		::PostMessage (_hwnd, WM_WORK, 0, 0);
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override { return E_NOTIMPL; }

	virtual ULONG STDMETHODCALLTYPE AddRef() override final
	{
		return InterlockedIncrement(&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release() override final
	{
		assert (_refCount > 0);
		ULONG newRefCount = InterlockedDecrement(&_refCount);
		if (newRefCount == 0)
			delete this;
		return newRefCount;
	}
};

template<typename... Args>
static IProjectWindowPtr Create (Args... args)
{
	return IProjectWindowPtr(new ProjectWindow (std::forward<Args>(args)...), false);
}

extern const ProjectWindowFactory projectWindowFactory = &Create;
