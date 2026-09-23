
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"

#include <initguid.h>
#include "edge/PropDefs.h"
#include "SimulatorAO.h"
#include "Simulator_.h"
#include "resource.h"

#pragma comment (lib, "Shlwapi")
#pragma comment (lib, "Version")
#pragma comment (lib, "Comctl32")
#pragma comment (lib, "comsuppwd.lib")

using namespace D2D1;
using namespace edge;

HRESULT CreateProjectWindowAO(IProjectWindow* projectWindow, IProjectWindowAO** ppProjectWindowAO);

static const char company_name[] = "Adi Gostin";
static const wchar_t app_name[] = L"STP Simulator";

const char stp_disabled_text[] = "(STP disabled)";

extern selection_factory_t selection_factory;
extern edit_window_factory_t edit_window_factory;
extern project_window_factory_t MakeProjectWindow;

extern HRESULT MakeMSTConfigIdEditorFactory (pg::ICustomPropertyEditorFactory** ppFactory);

HRESULT CreateVlanWindow (const VlanWindowCreateParams* params, IVlanWindow** ppVlanWindow);

#pragma region IStpProject
std::pair<IWire*, size_t> IStpProject::GetWireConnectedToPort (IPort* port) const
{
	for (ULONG i = 0; i < WireCount(); i++)
	{
		IWire* w = WireAt(i);
		if (std::holds_alternative<connected_wire_end>(w->p0()) && (std::get<connected_wire_end>(w->p0()) == port))
			return { w, 0 };
		else if (std::holds_alternative<connected_wire_end>(w->p1()) && (std::get<connected_wire_end>(w->p1()) == port))
			return { w, 1 };
	}

	return { };
}

IPort* IStpProject::find_connected_port (IPort* tx_port) const
{
	for (ULONG i = 0; i < WireCount(); i++)
	{
		IWire* w = WireAt(i);
		for (size_t i = 0; i < 2; i++)
		{
			auto& thisEnd = w->points()[i];
			if (std::holds_alternative<connected_wire_end>(thisEnd) && (std::get<connected_wire_end>(thisEnd) == tx_port))
			{
				auto& otherEnd = w->points()[1 - i];
				if (std::holds_alternative<connected_wire_end>(otherEnd))
					return std::get<connected_wire_end>(otherEnd);
				else
					return nullptr;
			}
		}
	}

	return nullptr;
}

bool IStpProject::IsWireForwarding (IWire* wire, unsigned int vlanNumber, _Out_opt_ bool* isPartOfLoop) const
{
	if (isPartOfLoop)
		*isPartOfLoop = false;

	if (!std::holds_alternative<connected_wire_end>(wire->p0()) || !std::holds_alternative<connected_wire_end>(wire->p1()))
		return false;

	auto portA = std::get<connected_wire_end>(wire->p0());
	auto portB = std::get<connected_wire_end>(wire->p1());
	bool portAFw = portA->IsForwarding(vlanNumber);
	bool portBFw = portB->IsForwarding(vlanNumber);
	if (!portAFw || !portBFw)
		return false;

	if (isPartOfLoop != nullptr)
	{
		// Block the queried wire's reverse direction so the return path uses other wires.
		std::unordered_set<IPort*> txPorts = { portB };

		auto transmitsTo = [this, vlanNumber, &txPorts, targetPort=portA](auto& self, IPort* txPort) -> bool
			{
				if (txPort->IsForwarding(vlanNumber))
				{
					IPort* rx = find_connected_port(txPort);
					if ((rx != nullptr) && rx->IsForwarding(vlanNumber))
					{
						txPorts.insert(txPort);

						for (unsigned int i = 0; i < (unsigned int) rx->bridge()->PortCount(); i++)
						{
							IPort* otherTxPort = rx->bridge()->PortAt(i);
							if ((i != rx->port_index()) && otherTxPort->IsForwarding(vlanNumber))
							{
								if (otherTxPort == targetPort)
									return true;

								if (txPorts.find(otherTxPort) != txPorts.end())
									continue;

								if (self(self, otherTxPort))
									return true;
							}
						}
					}
				}

				return false;
			};

		*isPartOfLoop = transmitsTo(transmitsTo, portA);
	}

	return true;
}


#pragma endregion

struct ThemeColorProvider : ID2DThemeColorProvider, IConnectionPointContainer
{
	ULONG _refCount = 0;
	WeakRefToThis _weakRefToThis;
	com_ptr<ConnectionPointImpl<IThemeChangedEvents>> _themeChangedCP;

	HRESULT InitInstance()
	{
		auto hr = _weakRefToThis.InitInstance(AsUnknown()); RETURN_IF_FAILED(hr);
		hr = MakeConnectionPoint(this, &_themeChangedCP); RETURN_IF_FAILED(hr);
		return S_OK;
	}

	IUnknown* AsUnknown() { return static_cast<ID2DThemeColorProvider*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IConnectionPointContainer>(this, riid, ppvObject)
			|| TryQI<IThemeColorProvider>(this, riid, ppvObject)
			|| TryQI<ID2DThemeColorProvider>(this, riid, ppvObject)
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
		if (riid == __uuidof(IThemeChangedEvents))
			return wil::com_query_to_nothrow(_themeChangedCP, ppCP);
		RETURN_HR(E_NOTIMPL);
	}
	#pragma endregion

	#pragma region IThemeColorProvider
	virtual uint32_t argb (theme_color color) const override
	{
		switch (color)
		{
			case theme_color::background: return get_sys_color_argb(COLOR_WINDOW);
			case theme_color::foreground: return get_sys_color_argb(COLOR_WINDOWTEXT);
			case theme_color::disabled_fore: return get_sys_color_argb(COLOR_GRAYTEXT);
			case theme_color::selected_back_focused: return get_sys_color_argb(COLOR_HIGHLIGHT);
			case theme_color::selected_back_not_focused: return get_sys_color_argb(COLOR_HIGHLIGHT);
			case theme_color::selected_fore: return get_sys_color_argb(COLOR_HIGHLIGHTTEXT);
			case theme_color::tooltip_back: return get_sys_color_argb(COLOR_INFOBK);
			case theme_color::tooltip_fore: return get_sys_color_argb(COLOR_INFOTEXT);
			case theme_color::active_caption_back: return get_sys_color_argb(COLOR_ACTIVECAPTION);
			case theme_color::active_caption_fore: return get_sys_color_argb(COLOR_CAPTIONTEXT);
			case theme_color::text_editor_back: return get_sys_color_argb(COLOR_WINDOW);
			case theme_color::text_editor_fore: return get_sys_color_argb(COLOR_WINDOWTEXT);
			default:
				_ASSERT(false);
				return 0;
		}
	}

	static uint32_t get_sys_color_argb (int nIndex)
	{
		DWORD c = ::GetSysColor(nIndex);
		auto argb = 0xFF000000u | (GetRValue(c) << 16) | (GetGValue(c) << 8) | GetBValue(c);
		return argb;
	}
	#pragma endregion
};

class SimulatorApp : public ISimulatorApp, ISimulatorAppAO, IProjectWindowEventsSink, IConnectionPointContainer
{
	ULONG _refCount = 0;
	ULONG _sig = 0xAA550006;
	WeakRefToThis _weakRefToThis;
	com_ptr<ConnectionPointImpl<IProjectWindowCollectionEventsSink>> _windowCollectionEventsCP;

	ID3D11DeviceContext1* _d3dDC;
	ID2D1Factory1* _d2dFactory;
	IDWriteFactory* _dwriteFactory;
	std::wstring _regKeyPath;
	struct ProjectWindowInfo
	{
		com_ptr<IProjectWindow> projectWindow;
		com_ptr<IProjectWindowAO> projectWindowAO;
		AdviseSinkToken eventsToken;
	};
	vector_nothrow<ProjectWindowInfo> _projectWindows;
	com_ptr<IThemeColorProvider> _tcp;

public:
	HRESULT InitInstance (ID3D11DeviceContext1* d3dDC, ID2D1Factory1* d2dFactory, IDWriteFactory* dwriteFactory)
	{
		auto hr = _weakRefToThis.InitInstance(AsUnknown()); RETURN_IF_FAILED(hr);

		hr = MakeConnectionPoint<IProjectWindowCollectionEventsSink>(this, &_windowCollectionEventsCP); RETURN_IF_FAILED(hr);
		_d3dDC = d3dDC;
		_d2dFactory = d2dFactory;
		_dwriteFactory = dwriteFactory;

		auto p = com_ptr(new ThemeColorProvider());
		hr = p->InitInstance(); LOG_IF_FAILED(hr);
		_tcp = std::move(p);

		std::wstringstream ss;
		ss << L"SOFTWARE\\" << company_name << L"\\" << ::app_name << L"\\" << ::app_version_string;
		_regKeyPath = ss.str();

		return S_OK;
	}

	IUnknown* AsUnknown() { return static_cast<ISimulatorApp*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<ISimulatorApp>(this, riid, ppvObject)
			|| TryQI<IProjectWindowEventsSink>(this, riid, ppvObject)
			|| TryQI<IConnectionPointContainer>(this, riid, ppvObject)
			|| TryQI<IDispatch>(this, riid, ppvObject)
			|| TryQI<ISimulatorAppAO>(this, riid, ppvObject)
		)
			return S_OK;

		if (riid == __uuidof(IWeakRef))
			return _weakRefToThis.QueryIWeakRef(ppvObject);

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	IMPLEMENT_IDISPATCH_(ISimulatorAppAO, nullptr, ID_TYPELIB_SIMULATOR_AO);

	#pragma region ISimulatorAppAO
	virtual HRESULT STDMETHODCALLTYPE GetProjectWindow (LONG hWnd, IProjectWindowAO** ppProjectWindow) override
	{
		if (!ppProjectWindow) return E_POINTER;
		*ppProjectWindow = nullptr;

		for (auto& pw : _projectWindows)
		{
			if (pw.projectWindow->hwnd() == (HWND)(LONG_PTR)hWnd)
			{
				if (!pw.projectWindowAO)
				{
					auto hr = CreateProjectWindowAO(pw.projectWindow, &pw.projectWindowAO); RETURN_IF_FAILED(hr);
				}

				return pw.projectWindowAO.copy_to(ppProjectWindow);
			}
		}

		return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	}

	virtual HRESULT STDMETHODCALLTYPE OpenWindowForVlan(IProjectAO* projectAO, DWORD vlanNumber, LONG* pHWnd) override
	{
		RETURN_HR_IF(E_POINTER, !projectAO || !pHWnd);
		*pHWnd = 0;
		com_ptr<IGetWrappedObject> getWrapped;
		auto hr = projectAO->QueryInterface(IID_PPV_ARGS(getWrapped.addressof())); RETURN_IF_FAILED(hr);
		com_ptr<IStpProject> project;
		hr = getWrapped->GetWrappedObject(IID_PPV_ARGS(project.addressof())); RETURN_IF_FAILED(hr);
		HWND hwnd;
		hr = OpenWindowForVlan(project, vlanNumber, &hwnd); RETURN_IF_FAILED(hr);
		*pHWnd = (LONG)(LONG_PTR)hwnd;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE EnableFailFastOnAssertions() override
	{
		auto hook = [](int reportType, char*, int*) -> int
			{
				if (reportType == _CRT_ASSERT)
					RaiseFailFastException(nullptr, nullptr, 0);
				return FALSE;
			};

		// When running under debugger, we _want_ the assertion dialog, so we ignore this call.
		if (!IsDebuggerPresent())
		{
			_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
			RETURN_HR_IF(E_FAIL, _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, hook) < 0);
		}

		return S_OK;
	}
	#pragma endregion

	#pragma region IConnectionPointContainer
	virtual HRESULT STDMETHODCALLTYPE EnumConnectionPoints (IEnumConnectionPoints **ppEnum) override
	{
		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE FindConnectionPoint (REFIID riid, IConnectionPoint **ppCP) override
	{
		if (riid == __uuidof(IProjectWindowCollectionEventsSink))
			return wil::com_query_to_nothrow(_windowCollectionEventsCP, ppCP);
		RETURN_HR(E_NOTIMPL);
	}
	#pragma endregion

	virtual HRESULT STDMETHODCALLTYPE AddProjectWindow (IProjectWindow* pw) override
	{
		HRESULT hr;
		ProjectWindowInfo info;
		info.projectWindow = pw;
		AdviseSinkToken token;
		hr = AdviseSink<IProjectWindowEventsSink>(pw, _weakRefToThis, &token); RETURN_IF_FAILED(hr);
		_windowCollectionEventsCP->Notify([this,pw](IProjectWindowCollectionEventsSink* sink) {
			return sink->OnProjectWindowInserting(pw); });
		info.eventsToken = std::move(token);
		bool pushed = _projectWindows.try_push_back(std::move(info)); RETURN_HR_IF(E_OUTOFMEMORY, !pushed);
		_windowCollectionEventsCP->Notify([this](IProjectWindowCollectionEventsSink* sink) {
			return sink->OnProjectWindowInserted(_projectWindows.back().projectWindow); });
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OpenWindowForVlan(IStpProject* project, DWORD vlanNumber, _Out_opt_ HWND* phWnd) override
	{
		RETURN_HR_IF(E_INVALIDARG, vlanNumber == 0 || vlanNumber > 4094);
		if (phWnd)
			*phWnd = nullptr;

		for (ULONG i = 0; i < ProjectWindowCount(); ++i)
		{
			auto pw = ProjectWindowAt(i);
			com_ptr<IVlanSelection> vlanSelection;
			DWORD pwVlan;
			if (pw->project() == project
				&& SUCCEEDED(pw->GetVlanSelection(&vlanSelection))
				&& SUCCEEDED(vlanSelection->GetSelectedVlan(&pwVlan))
				&& pwVlan == vlanNumber)
			{
				::BringWindowToTop(pw->hwnd());
				::FlashWindow(pw->hwnd(), FALSE);
				if (phWnd)
					*phWnd = pw->hwnd();
				return S_OK;
			}
		}

		project_window_create_params create_params = { this, project, false, false, vlanNumber, SW_SHOW };
		com_ptr<IProjectWindow> pw;
		auto hr = project_window_factory()(create_params, &pw); RETURN_IF_FAILED(hr);
		if (phWnd)
			*phWnd = pw->hwnd();
		return AddProjectWindow(pw);
	}

	#pragma region IProjectWindowEventsSink
	virtual HRESULT STDMETHODCALLTYPE OnProjectWindowClosed (IProjectWindow* pw) override
	{
		auto it = std::find_if (_projectWindows.begin(), _projectWindows.end(), [pw](auto& p) { return p.projectWindow == pw; });
		_ASSERT (it != _projectWindows.end());
		_windowCollectionEventsCP->Notify([this,pw](IProjectWindowCollectionEventsSink* sink) {
			return sink->OnProjectWindowRemoving(pw); });
		auto pwLastRef = std::move(*it);
		_projectWindows.erase(it);
		_windowCollectionEventsCP->Notify([this,&pwLastRef](IProjectWindowCollectionEventsSink* sink) {
			return sink->OnProjectWindowRemoved(pwLastRef.projectWindow); });
		if (_projectWindows.empty())
			PostQuitMessage(0);
		return S_OK;
	}
	#pragma endregion

	virtual ULONG STDMETHODCALLTYPE ProjectWindowCount() const override
	{
		return _projectWindows.size();
	}

	virtual IProjectWindow* STDMETHODCALLTYPE ProjectWindowAt (ULONG i) const override
	{
		return _projectWindows[i].projectWindow;
	}

	virtual const wchar_t* GetRegKeyPath() const override final { return _regKeyPath.c_str(); }

	virtual const wchar_t* app_name() const override final { return ::app_name; }

	virtual const wchar_t* app_version_string() const override final { return ::app_version_string; }

	virtual selection_factory_t* selection_factory() const override final { return &::selection_factory; }

	virtual edit_window_factory_t* edit_window_factory() const override final { return &::edit_window_factory; }

	virtual project_window_factory_t* project_window_factory() const override final { return &::MakeProjectWindow; }

	virtual project_factory_t* project_factory() const override { return MakeProject; }

	virtual properties_window_factory_t* properties_window_factory() const override final { return MakePropertiesWindow; }

	virtual HRESULT STDMETHODCALLTYPE CreateVlanWindow (const VlanWindowCreateParams* params, IVlanWindow** ppVlanWindow) override
	{
		return ::CreateVlanWindow(params, ppVlanWindow);
	}

	virtual edge::IThemeColorProvider* GetThemeColorProvider() const override { return _tcp; }

	virtual ID3D11DeviceContext1* GetD3DDC() override { return _d3dDC; }

	virtual IDWriteFactory* GetDWriteFactory() override { return _dwriteFactory; }

	virtual ID2D1Factory1* GetD2DFactory() override { return _d2dFactory; }

	WPARAM RunMessageLoop()
	{
		auto hinstance = (HINSTANCE)&__ImageBase;
		auto accelerators = LoadAccelerators (hinstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));

		MSG msg;
		while (GetMessage(&msg, nullptr, 0, 0))
		{
			if (msg.message == WM_MOUSEWHEEL)
			{
				HWND h = WindowFromPoint ({ GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam) });
				if (h != nullptr)
				{
					SendMessage (h, msg.message, msg.wParam, msg.lParam);
					continue;
				}
			}

			int translatedAccelerator = 0;
			for (auto& pw : _projectWindows)
			{
				if ((msg.hwnd == pw.projectWindow->hwnd()) || ::IsChild(pw.projectWindow->hwnd(), msg.hwnd))
				{
					translatedAccelerator = TranslateAccelerator (pw.projectWindow->hwnd(), accelerators, &msg);
					break;
				}
			}

			if (!translatedAccelerator)
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		return msg.wParam;
	}
};

static HRESULT RegisterAutomationTypeLib()
{
	wil::unique_process_heap_string modulefn;
	auto hr = wil::GetModuleFileNameW((HMODULE)&__ImageBase, modulefn); RETURN_IF_FAILED(hr);
	wil::unique_process_heap_string fn;
	hr = wil::str_printf_nothrow(fn, L"%s\\%u", modulefn, ID_TYPELIB_SIMULATOR_AO); RETURN_IF_FAILED(hr);
	com_ptr<ITypeLib> aotypelib;
	hr = LoadTypeLibEx (fn.get(), REGKIND_NONE, &aotypelib); RETURN_IF_FAILED(hr);
	hr = RegisterTypeLibForUser (aotypelib, fn.get(), nullptr); RETURN_IF_FAILED(hr);
	return S_OK;
}

static void RegisterApplicationAndFileTypes()
{
	auto exePath = std::make_unique<wchar_t[]>(MAX_PATH);
	DWORD dwRes = GetModuleFileName (nullptr, exePath.get(), MAX_PATH); _ASSERT(dwRes);
	std::wstringstream ss;
	ss << L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" << PathFindFileName(exePath.get());
	auto appPathKeyName = ss.str();

	bool notifyShell = false;
	auto buffer = std::make_unique<wchar_t[]>(MAX_PATH);
	DWORD cbData = MAX_PATH;
	auto ls = RegGetValue (HKEY_CURRENT_USER, appPathKeyName.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, buffer.get(), &cbData);
	if ((ls != ERROR_SUCCESS) || (_wcsicmp (buffer.get(), exePath.get()) != 0))
	{
		RegSetValue (HKEY_CURRENT_USER, appPathKeyName.c_str(), REG_SZ, exePath.get(), 0);
		notifyShell = true;
	}

	static constexpr wchar_t ProgID[] = L"AGO.StpFile.1";
	ss.str(L"");
	ss << L"SOFTWARE\\Classes\\" << ProgID << L"\\shell\\open\\command";
	auto progIdKeyName = ss.str();
	ss.str(L"");
	ss << L"\"" << exePath.get() << L"\" \"%1\"";
	auto progIdKeyValue = ss.str();
	cbData = MAX_PATH;
	ls = RegGetValue (HKEY_CURRENT_USER, progIdKeyName.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, buffer.get(), &cbData);
	if ((ls != ERROR_SUCCESS) || (_wcsicmp (buffer.get(), progIdKeyValue.c_str()) != 0))
	{
		RegSetValue (HKEY_CURRENT_USER, progIdKeyName.c_str(), REG_SZ, progIdKeyValue.c_str(), 0);
		notifyShell = true;
	}

	ss.str(L"");
	ss << L"SOFTWARE\\Classes\\" << FileExtensionWithDot;
	auto fileExtKeyName = ss.str();
	cbData = MAX_PATH;
	ls = RegGetValue (HKEY_CURRENT_USER, fileExtKeyName.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, buffer.get(), &cbData);
	if ((ls != ERROR_SUCCESS) || (_wcsicmp (buffer.get(), ProgID) != 0))
	{
		RegSetValue (HKEY_CURRENT_USER, fileExtKeyName.c_str(), REG_SZ, ProgID, 0);
		notifyShell = true;
	}

	if (notifyShell)
		SHChangeNotify (SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

int APIENTRY wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	bool waitForDebugger = false;
	wil::unique_process_heap_string fileToOpen;

	const wchar_t* p = lpCmdLine;
	while(*p)
	{
		static const wchar_t WaitDbg[] = L"-waitForDebugger";
		static const size_t WaitDbgLen = sizeof(WaitDbg) / 2 - 1;
		if (!_wcsnicmp(p, WaitDbg, WaitDbgLen) && (p[WaitDbgLen] == ' ' || p[WaitDbgLen] == 0))
		{
			waitForDebugger = true;
			p += WaitDbgLen;
			while(*p == ' ')
				p++;
		}
		else
		{
			bool quoted = false;
			const wchar_t* filename = p;
			if (*p == '"')
			{
				quoted = true;
				p++;
				filename++;
			}

			while(true)
			{
				if (*p == 0)
				{
					fileToOpen = wil::make_process_heap_string_nothrow(filename, p - filename);
					break;
				}
				else if (!quoted && *p == ' ')
				{
					fileToOpen = wil::make_process_heap_string_nothrow(filename, p - filename);
					while(*p == ' ')
						p++;
					break;
				}
				else if (quoted && *p == '"')
				{
					fileToOpen = wil::make_process_heap_string_nothrow(filename, p - filename);
					p++;
					while(*p == ' ')
						p++;
					break;
				}
				else
					p++;
			}
		}
	}

	if (waitForDebugger)
	{
		DWORD startTime = GetTickCount();
		while (true)
		{
			if (IsDebuggerPresent())
				break;
			if (GetTickCount() - startTime >= 10000)
				return ERROR_TIMEOUT;
			Sleep(50);
		}
	}

	int tmp = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	_CrtSetDbgFlag(tmp | _CRTDBG_LEAK_CHECK_DF);

	wil::g_fBreakOnFailure = IsDebuggerPresent();

	// We set DPI awareness programatically because Windows 10 won't run an executable whose manifest contains both <dpiAwareness> and <dpiAware>.
	// (Windows 10 looks only for first, Windows 7 looks only for the second, and we want to run on both.)
	if (auto proc_addr = GetProcAddress (GetModuleHandleA("User32.dll"), "SetProcessDpiAwarenessContext"))
	{
		auto proc = reinterpret_cast<BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT)>(proc_addr);
		proc (DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	}
	else
		SetProcessDPIAware();

	HRESULT hr = CoInitialize(0);

	com_ptr<pg::ICustomPropertyEditorFactory> mstConfigIdEditorFactory;
	hr = MakeMSTConfigIdEditorFactory(&mstConfigIdEditorFactory); RETURN_IF_FAILED(hr);
	DWORD mstceCookie;
	hr = CoRegisterClassObject (guidMSTConfigIdEditor, mstConfigIdEditorFactory, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &mstceCookie); RETURN_IF_FAILED(hr);
	auto unreg = wil::scope_exit([mstceCookie] { CoRevokeClassObject(mstceCookie); });

	hr = RegisterAutomationTypeLib(); RETURN_IF_FAILED(hr);

	RegisterApplicationAndFileTypes();

	bool tryDebugFirst = false;
#ifdef _DEBUG
	tryDebugFirst = (LoadLibraryA("dxgidebug.dll") != nullptr);
#endif

	com_ptr<ID3D11Device> d3d_device;
	com_ptr<ID3D11DeviceContext1> d3d_dc;

	{
		com_ptr<IDXGIFactory1> dxgi_factory;
		hr = CreateDXGIFactory1 (IID_PPV_ARGS(&dxgi_factory)); _ASSERT(SUCCEEDED(hr));
		std::vector<std::pair<com_ptr<IDXGIAdapter1>, SIZE_T>> adapters;
		size_t best = -1;
		while(true)
		{
			com_ptr<IDXGIAdapter1> a;
			hr = dxgi_factory->EnumAdapters1 ((UINT)adapters.size(), &a);
			if (FAILED(hr))
				break;

			DXGI_ADAPTER_DESC1 desc;
			hr = a->GetDesc1(&desc); _ASSERT(SUCCEEDED(hr));

			if ((best == -1) || (desc.DedicatedVideoMemory > adapters[best].second))
				best = adapters.size();

			adapters.push_back({ std::move(a), desc.DedicatedVideoMemory });
		}

		auto d3dFeatureLevel = D3D_FEATURE_LEVEL_9_1;
		com_ptr<ID3D11DeviceContext> deviceContext;

		if (tryDebugFirst)
		{
			hr = D3D11CreateDevice(adapters[best].first, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
			                       D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
			                       &d3dFeatureLevel, 1,
			                       D3D11_SDK_VERSION, &d3d_device, nullptr, &deviceContext);
		}

		if (!tryDebugFirst || FAILED(hr))
		{
			hr = D3D11CreateDevice(adapters[best].first, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
			                       D3D11_CREATE_DEVICE_BGRA_SUPPORT,
			                       &d3dFeatureLevel, 1,
			                       D3D11_SDK_VERSION, &d3d_device, nullptr, &deviceContext);
			_ASSERT(SUCCEEDED(hr));
		}

		d3d_dc = wil::com_query_failfast<ID3D11DeviceContext1>(deviceContext);
	}

	com_ptr<IDWriteFactory> dwrite_factory;
	hr = DWriteCreateFactory (DWRITE_FACTORY_TYPE_SHARED, __uuidof (IDWriteFactory), reinterpret_cast<IUnknown**>(&dwrite_factory)); _ASSERT(SUCCEEDED(hr));

	com_ptr<ID2D1Factory1> d2d_factory;

	if (tryDebugFirst)
	{
		D2D1_FACTORY_OPTIONS fo = { D2D1_DEBUG_LEVEL_WARNING };
		hr = D2D1CreateFactory (D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(**(&d2d_factory)), &fo, (void**) &d2d_factory);
	}

	if (!tryDebugFirst || FAILED(hr))
	{
		D2D1_FACTORY_OPTIONS fo = { D2D1_DEBUG_LEVEL_NONE };
		hr = D2D1CreateFactory (D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(**(&d2d_factory)), &fo, (void**) &d2d_factory);
		_ASSERT(SUCCEEDED(hr));
	}

	hr = BufferedPaintInit(); RETURN_IF_FAILED(hr);
	auto bpuninit = wil::scope_exit([] { BufferedPaintUnInit(); });

	int processExitValue;
	{
		auto app = com_ptr(new (std::nothrow) SimulatorApp()); RETURN_IF_NULL_ALLOC(app);
		hr = app->InitInstance (d3d_dc, d2d_factory, dwrite_factory); RETURN_IF_FAILED(hr);

		com_ptr<IStpProject> project;
		hr = MakeProject(&project); RETURN_IF_FAILED(hr);
		if (fileToOpen)
		{
			hr = project->Load(fileToOpen.get()); RETURN_IF_FAILED(hr);
		}

		project_window_create_params params = { app, project, true, true, 1, SW_SHOW };
		com_ptr<IProjectWindow> projectWindow;
		hr = MakeProjectWindow (params, &projectWindow);
		app->AddProjectWindow(projectWindow);
		projectWindow.reset();

		com_ptr<IBindCtx> bindContext;
		hr = CreateBindCtx(0, &bindContext); RETURN_IF_FAILED(hr);
		com_ptr<IRunningObjectTable> runningObjectTable;
		hr = bindContext->GetRunningObjectTable(&runningObjectTable); RETURN_IF_FAILED(hr);
		wil::unique_process_heap_string name;
		hr = wil::str_printf_nothrow(name, L"mstp-lib.Simulator.%s:%u", app_version_string, GetCurrentProcessId()); RETURN_IF_FAILED(hr);
		com_ptr<IMoniker> moniker;
		hr = CreateItemMoniker(L"!", name.get(), &moniker); RETURN_IF_FAILED(hr);
		DWORD runningObjectCookie;
		hr = runningObjectTable->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE,
			app->AsUnknown(), moniker, &runningObjectCookie); RETURN_IF_FAILED(hr);
		auto revokeRunningObject = wil::scope_exit([&] { runningObjectTable->Revoke(runningObjectCookie); });

		processExitValue = (int)app->RunMessageLoop();
	}
	/*
	if (d3d_device->GetCreationFlags() & D3D11_CREATE_DEVICE_DEBUG)
	{
		deviceContext = nullptr;
		ID3D11DebugPtr debug;
		hr = d3d_device->QueryInterface(&debug);
		if (SUCCEEDED(hr))
			debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
	}
	*/
	CoUninitialize();

	return processExitValue;
}

HRESULT BridgeAddressToString (const mac_address& addr, BSTR* pbstrBridgeAddress)
{
	wil::unique_process_heap_string str;
	auto hr = wil::str_printf_nothrow(str, L"%02X%02X%02X%02X%02X%02X",
									  addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]); RETURN_IF_FAILED(hr);
	*pbstrBridgeAddress = SysAllocString(str.get()); RETURN_IF_NULL_ALLOC(*pbstrBridgeAddress);
	return S_OK;
}

HRESULT BridgeAddressFromString (const wchar_t* str, mac_address& to)
{
	static const wchar_t format_error_message[] = L"Invalid address format. The Bridge Address format is XX:XX:XX:XX:XX:XX or XXXXXXXXXXXX (6 hex bytes).";

	uint32_t strSize = (uint32_t)wcslen(str);

	size_t offset_multiplier;
	if (strSize == 12)
	{
		offset_multiplier = 2;
	}
	else if (strSize == 17)
	{
		if ((str[2] != ':') || (str[5] != ':') || (str[8] != ':') || (str[11] != ':') || (str[14] != ':'))
			return SetErrorInfo(E_INVALIDARG, format_error_message);
		offset_multiplier = 3;
	}
	else
		return SetErrorInfo(E_INVALIDARG, format_error_message);

	for (uint32_t i = 0; i < 6; i++)
	{
		wchar_t ch0 = str[i * offset_multiplier];
		wchar_t ch1 = str[i * offset_multiplier + 1];

		if (!iswxdigit(ch0) || !iswxdigit(ch1))
			return SetErrorInfo(E_INVALIDARG, format_error_message);

		auto hn = (ch0 <= '9') ? (ch0 - '0') : ((ch0 >= 'a') ? (ch0 - 'a' + 10) : (ch0 - 'A' + 10));
		auto ln = (ch1 <= '9') ? (ch1 - '0') : ((ch1 >= 'a') ? (ch1 - 'a' + 10) : (ch1 - 'A' + 10));
		to[i] = (hn << 4) | ln;
	}

	return S_OK;
}

namespace natvis
{
	struct hex_dummy_low { unsigned char c; };
	struct hex_dummy_high { unsigned char c; };
	static volatile hex_dummy_low dummylo;
	static volatile hex_dummy_high dummyhi;
}

