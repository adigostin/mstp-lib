#include "pch.h"
#include "Simulator.h"
#include "Win32Defs.h"
#include "Resource.h"
#include "RibbonCommandHandlers/RCHBase.h"
#include "Bridge.h"
#include "Port.h"

using namespace std;

static ATOM wndClassAtom;
static constexpr wchar_t ProjectWindowWndClassName[] = L"ProjectWindow-{24B42526-2970-4B3C-A753-2DABD22C4BB0}";
static constexpr wchar_t RegValueNameWindowState[] = L"WindowState";
static constexpr wchar_t RegValueNameWindowLeft[] = L"WindowLeft";
static constexpr wchar_t RegValueNameWindowTop[] = L"WindowTop";
static constexpr wchar_t RegValueNameWindowRight[] = L"WindowRight";
static constexpr wchar_t RegValueNameWindowBottom[] = L"WindowBottom";

class ProjectWindow : public IProjectWindow, IUIApplication
{
	ULONG _refCount = 1;
	ComPtr<IProject> const _project;
	ComPtr<ISelection> const _selection;
	ComPtr<IEditArea> _editArea;
	ComPtr<ILogArea> _logArea;
	ComPtr<IUIFramework> _rf;
	HWND _hwnd;
	SIZE _clientSize;
	EventManager _em;
	RECT _restoreBounds;
	unordered_map<UINT32, ComPtr<IUICommandHandler>> _commandHandlers;
	LONG _logAreaWidth = 500;

public:
	ProjectWindow (IProject* project, HINSTANCE rfResourceHInstance, const wchar_t* rfResourceName, ISelection* selection,
		EditAreaFactory editAreaFactory, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory)
		: _project(project), _selection(selection)
	{
		HINSTANCE hInstance;
		BOOL bRes = GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)&wndClassAtom, &hInstance);
		if (!bRes)
			throw win32_exception(GetLastError());

		if (wndClassAtom == 0)
		{
			WNDCLASSEX wndClassEx =
			{
				sizeof(wndClassEx),
				CS_DBLCLKS, // style
				&ProjectWindow::WindowProcStatic, // lpfnWndProc
				0, // cbClsExtra
				0, // cbWndExtra
				hInstance, // hInstance
				LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DESIGNER)), // hIcon
				LoadCursor(nullptr, IDC_ARROW), // hCursor
				nullptr,//(HBRUSH)(COLOR_WINDOW + 1), // hbrBackground
				nullptr, // lpszMenuName
				ProjectWindowWndClassName, // lpszClassName
				LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DESIGNER))
			};

			wndClassAtom = RegisterClassEx(&wndClassEx);
			if (wndClassAtom == 0)
				throw win32_exception(GetLastError());
		}

		int x = 100; //  InitialBounds.left;
		int y = 100; //  InitialBounds.top;
		int w = 800; //  InitialBounds.right - InitialBounds.left;
		int h = 600; //  InitialBounds.bottom - InitialBounds.top;
		auto hwnd = ::CreateWindowEx(WS_EX_OVERLAPPEDWINDOW, ProjectWindowWndClassName, L"", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, w, h, nullptr, 0, hInstance, this);
		if (hwnd == nullptr)
			throw win32_exception(GetLastError());
		assert(hwnd == _hwnd);

		if ((rfResourceHInstance != nullptr) && (rfResourceName != nullptr))
		{
			auto hr = CoCreateInstance(CLSID_UIRibbonFramework, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_rf));
			ThrowIfFailed(hr);
		}

		_editArea = editAreaFactory (project, this, 333, selection, _rf, { 0, 0, 0, 0 }, deviceContext, dWriteFactory, wicFactory);

		_logArea = logAreaFactory (_hwnd, 444, { 0, 0, _logAreaWidth, 0 }, deviceContext, dWriteFactory, wicFactory);
		_logArea->GetLogAreaResizingEvent().AddHandler (&OnLogAreaResizing, this);

		LONG ribbonHeight = 0;
		if ((rfResourceHInstance != nullptr) && (rfResourceName != nullptr))
		{
			const RCHDeps deps = { this, _rf, _project, _editArea, _selection };
			for (auto& info : GetRCHInfos())
			{
				auto handler = info->_factory(deps);
				for (UINT32 command : info->_commands)
					_commandHandlers.insert ({ command, handler });
			}

			auto hr = _rf->Initialize(hwnd, this); ThrowIfFailed(hr);
			hr = _rf->LoadUI(rfResourceHInstance, rfResourceName); ThrowIfFailed(hr);

			ComPtr<IUIRibbon> ribbon;
			hr = _rf->GetView(0, IID_PPV_ARGS(&ribbon)); ThrowIfFailed(hr);
			hr = ribbon->GetHeight((UINT32*)&ribbonHeight); ThrowIfFailed(hr);
		}

		ResizeChildWindows();

		_selection->GetSelectionChangedEvent().AddHandler (&OnSelectionChanged, this);
	}

	~ProjectWindow()
	{
		_selection->GetSelectionChangedEvent().RemoveHandler (&OnSelectionChanged, this);

		_editArea = nullptr;
		_logArea = nullptr;
		::DestroyWindow(_hwnd);
	}

	static void OnSelectionChanged (void* callbackArg, ISelection* selection)
	{
		auto pw = static_cast<ProjectWindow*>(callbackArg);
		if (selection->GetObjects().size() != 1)
			pw->_logArea->SelectBridge(nullptr);
		else
		{
			auto b = dynamic_cast<Bridge*>(selection->GetObjects()[0].Get());
			if (b == nullptr)
			{
				auto port = dynamic_cast<Port*>(selection->GetObjects()[0].Get());
				if (port != nullptr)
					b = port->GetBridge();
			}

			pw->_logArea->SelectBridge(b);
		}
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
			window->_hwnd = hwnd;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
			// GDI now holds a pointer to this object, so let's call AddRef.
			window->AddRef();
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
			window->Release(); // Release the reference we added on WM_NCCREATE.
		}

		return result;
	}

	LRESULT WindowProc(UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (msg == WM_CREATE)
		{
			auto cs = reinterpret_cast<const CREATESTRUCT*>(lParam);
			_clientSize = { cs->cx, cs->cy };
			return 0;
		}

		if (msg == WM_DESTROY)
		{
			if (_rf != nullptr)
				_rf->Destroy();
			return 0;
		}

		if (msg == WM_SIZE)
		{
			if (wParam == SIZE_RESTORED)
				::GetWindowRect(_hwnd, &_restoreBounds);

			_clientSize = { LOWORD(lParam), HIWORD(lParam) };
			ResizeChildWindows();

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

		if (msg == WM_ERASEBKGND)
			return 1;

		if (msg == WM_PAINT)
		{
			PAINTSTRUCT ps;
			BeginPaint(GetHWnd(), &ps);
			EndPaint(GetHWnd(), &ps);
			return 0;
		}

		if (msg == WM_CLOSE)
		{
			bool cancel = false;
			ProjectWindowClosingEvent::InvokeHandlers(_em, this, &cancel);
			if (cancel)
				return 0;

			return DefWindowProc(_hwnd, msg, wParam, lParam);
		}

		return DefWindowProc(_hwnd, msg, wParam, lParam);
	}

	static void OnLogAreaResizing (void* callbackArg, ILogArea* lw, Side side, LONG offset)
	{
		auto pw = static_cast<ProjectWindow*>(callbackArg);
		if (side == Side::Left)
		{
			pw->_logAreaWidth -= offset;
			pw->ResizeChildWindows();
		}
	}

	void ResizeChildWindows()
	{
		LONG ribbonHeight = 0;
		if (_rf != nullptr)
		{
			ComPtr<IUIRibbon> ribbon;
			auto hr = _rf->GetView(0, IID_PPV_ARGS(&ribbon)); ThrowIfFailed(hr);
			hr = ribbon->GetHeight((UINT32*)&ribbonHeight); ThrowIfFailed(hr);
		}

		LONG laWidth = 0;
		if (_logArea != nullptr)
		{
			laWidth = _logAreaWidth;
			::MoveWindow (_logArea->GetHWnd(), _clientSize.cx - laWidth, ribbonHeight, laWidth, _clientSize.cy - ribbonHeight, TRUE);
		}

		if (_editArea != nullptr)
			::MoveWindow(_editArea->GetHWnd(), 0, ribbonHeight, _clientSize.cx - laWidth, _clientSize.cy - ribbonHeight, TRUE);
	}

	//virtual IProject* GetProject() const override final { return _project.get(); }

	virtual void ShowAtSavedWindowLocation(const wchar_t* regKeyPath) override final
	{
		auto ReadDword = [regKeyPath](const wchar_t* valueName, DWORD* valueOut) -> bool
		{
			DWORD dataSize = 4;
			auto lresult = RegGetValue(HKEY_CURRENT_USER, regKeyPath, valueName, RRF_RT_REG_DWORD, nullptr, valueOut, &dataSize);
			return lresult == ERROR_SUCCESS;
		};

		DWORD windowState;
		if (ReadDword(RegValueNameWindowState, &windowState)
			&& ReadDword(RegValueNameWindowLeft, (DWORD*)&_restoreBounds.left)
			&& ReadDword(RegValueNameWindowTop, (DWORD*)&_restoreBounds.top)
			&& ReadDword(RegValueNameWindowRight, (DWORD*)&_restoreBounds.right)
			&& ReadDword(RegValueNameWindowBottom, (DWORD*)&_restoreBounds.bottom))
		{
			::SetWindowPos(GetHWnd(), nullptr,
				_restoreBounds.left,
				_restoreBounds.top,
				_restoreBounds.right - _restoreBounds.left,
				_restoreBounds.bottom - _restoreBounds.top,
				SWP_HIDEWINDOW);

			::ShowWindow(GetHWnd(), windowState);
		}
		else
			::ShowWindow(GetHWnd(), SW_SHOWMAXIMIZED);
	}

	virtual void SaveWindowLocation(const wchar_t* regKeyPath) const override final
	{
		WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
		BOOL bRes = GetWindowPlacement(GetHWnd(), &wp);
		if (bRes && ((wp.showCmd == SW_NORMAL) || (wp.showCmd == SW_MAXIMIZE)))
		{
			HKEY key;
			auto lstatus = RegCreateKeyEx(HKEY_CURRENT_USER, regKeyPath, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
			if (lstatus == ERROR_SUCCESS)
			{
				RegSetValueEx(key, RegValueNameWindowLeft, 0, REG_DWORD, (BYTE*)&_restoreBounds.left, 4);
				RegSetValueEx(key, RegValueNameWindowTop, 0, REG_DWORD, (BYTE*)&_restoreBounds.top, 4);
				RegSetValueEx(key, RegValueNameWindowRight, 0, REG_DWORD, (BYTE*)&_restoreBounds.right, 4);
				RegSetValueEx(key, RegValueNameWindowBottom, 0, REG_DWORD, (BYTE*)&_restoreBounds.bottom, 4);
				RegSetValueEx(key, RegValueNameWindowState, 0, REG_DWORD, (BYTE*)&wp.showCmd, 4);
				RegCloseKey(key);
			}
		}
	}

	virtual ProjectWindowClosingEvent::Subscriber GetProjectWindowClosingEvent() override final { return ProjectWindowClosingEvent::Subscriber(_em); }

	virtual unsigned int GetSelectedTreeIndex() const override final { return 0; }

	virtual SelectedTreeIndexChangedEvent::Subscriber GetSelectedTreeIndexChangedEvent() override final { return SelectedTreeIndexChangedEvent::Subscriber(_em); }

	std::optional<LRESULT> ProcessWmClose()
	{
		/*
		auto& allWindowsForProject = _project->GetProjectWindows();

		rassert (!allWindowsForProject.empty());

		if (allWindowsForProject.size() > 1)
		{
		_project->RemoveAndDestroyProjectWindow(this);
		return;
		}

		// Closing the last remaining window of this project. Let's check for changes and ask the user whether to save them.
		bool continueClosing = SaveProject (_project->GetFilePath(), true, SaveProjectOption::SaveIfChangedAskUserFirst, L"Save changes?");
		if (!continueClosing)
		return;
		*/
		/*
		// Copy some pointers from "this" to the stack cause destroying the project window might release the last reference to it and destroy this object.
		IEditorProject* project = _project;
		IEditorApplication* app = _app;

		_project->RemoveAndDestroyProjectWindow (this);
		*/

		if (_rf != nullptr)
			_rf->Destroy();
		::DestroyWindow(_hwnd);
		PostQuitMessage(0);
		//if (project->GetProjectWindows().empty())
		//{
		//	app->CloseProject (project);
		//	if (app->GetOpenProjects().empty())
		//		PostQuitMessage (0);
		//}
	}

	#pragma region IUIApplication
	virtual HRESULT STDMETHODCALLTYPE OnViewChanged(UINT32 viewId, UI_VIEWTYPE typeID, IUnknown *view, UI_VIEWVERB verb, INT32 uReasonCode) override final
	{
		ResizeChildWindows();
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnCreateUICommand(UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler **commandHandler) override final
	{
		auto it = _commandHandlers.find(commandId);
		if (it == _commandHandlers.end())
			return E_NOTIMPL;

		*commandHandler = it->second;
		it->second->AddRef();
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnDestroyUICommand(UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler *commandHandler) override final
	{
		return E_NOTIMPL;
	}
	#pragma endregion

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		if (!ppvObject)
			return E_INVALIDARG;

		*ppvObject = NULL;
		if (riid == __uuidof(IUnknown))
		{
			*ppvObject = static_cast<IUnknown*>((IProjectWindow*) this);
			AddRef();
			return S_OK;
		}
		else if (riid == __uuidof(IUIApplication))
		{
			*ppvObject = static_cast<IUIApplication*>(this);
			AddRef();
			return S_OK;
		}

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override
	{
		return InterlockedIncrement(&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release() override
	{
		ULONG newRefCount = InterlockedDecrement(&_refCount);
		if (newRefCount == 0)
			delete this;
		return newRefCount;
	}
	#pragma endregion
};

extern const ProjectWindowFactory projectWindowFactory = [](auto... params) { return ComPtr<IProjectWindow>(new ProjectWindow(params...), false); };
