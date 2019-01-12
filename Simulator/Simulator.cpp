
#include "pch.h"
#include "Simulator.h"
#include "Bridge.h"
#include "Wire.h"
#include "Resource.h"

#pragma comment (lib, "d2d1.lib")
#pragma comment (lib, "dwrite.lib")
#pragma comment (lib, "D3D11.lib")
#pragma comment (lib, "Dxgi.lib")
#pragma comment (lib, "Shlwapi")
#pragma comment (lib, "Version")
#pragma comment (lib, "Comctl32")
#pragma comment (lib, "msxml6.lib")
#pragma comment (lib, "comsuppwd.lib")

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace std;
using namespace D2D1;
using namespace edge;

static const wchar_t CompanyName[] = L"Adi Gostin";
static const wchar_t AppName[] = L"STP Simulator";
static const wchar_t AppVersionString[] = L"2.1";

#pragma region IProject
pair<Wire*, size_t> IProject::GetWireConnectedToPort (const Port* port) const
{
	for (auto& w : GetWires())
	{
		if (holds_alternative<ConnectedWireEnd>(w->GetP0()) && (get<ConnectedWireEnd>(w->GetP0()) == port))
			return { w.get(), 0 };
		else if (holds_alternative<ConnectedWireEnd>(w->GetP1()) && (get<ConnectedWireEnd>(w->GetP1()) == port))
			return { w.get(), 1 };
	}

	return { };
}

Port* IProject::FindConnectedPort (Port* txPort) const
{
	for (auto& w : GetWires())
	{
		for (size_t i = 0; i < 2; i++)
		{
			auto& thisEnd = w->GetPoints()[i];
			if (holds_alternative<ConnectedWireEnd>(thisEnd) && (get<ConnectedWireEnd>(thisEnd) == txPort))
			{
				auto& otherEnd = w->GetPoints()[1 - i];
				if (holds_alternative<ConnectedWireEnd>(otherEnd))
					return get<ConnectedWireEnd>(otherEnd);
				else
					return nullptr;
			}
		}
	}

	return nullptr;
}

std::unique_ptr<Wire> IProject::RemoveWire (Wire* w)
{
	auto& wires = this->GetWires();
	for (size_t wi = 0; wi < wires.size(); wi++)
	{
		if (wires[wi].get() == w)
			return this->RemoveWire(wi);
	}

	assert(false); return nullptr;
}

std::unique_ptr<Bridge> IProject::RemoveBridge (Bridge* b)
{
	auto& bridges = this->GetBridges();
	for (size_t bi = 0; bi < bridges.size(); bi++)
	{
		if (bridges[bi].get() == b)
			return this->RemoveBridge(bi);
	}

	assert(false); return nullptr;
}
#pragma endregion

class SimulatorApp : public event_manager, public ISimulatorApp
{
	HINSTANCE const _hInstance;
	wstring _regKeyPath;
	vector<std::unique_ptr<IProjectWindow>> _projectWindows;

public:
	SimulatorApp (HINSTANCE hInstance)
		: _hInstance(hInstance)
	{
		wstringstream ss;
		ss << L"SOFTWARE\\" << CompanyName << L"\\" << ::AppName << L"\\" << ::AppVersionString;
		_regKeyPath = ss.str();
	}

	virtual HINSTANCE GetHInstance() const override final { return _hInstance; }

	virtual void AddProjectWindow (std::unique_ptr<IProjectWindow>&& pw) override final
	{
		pw->destroying().add_handler(&OnProjectWindowDestroying, this);
		_projectWindows.push_back(std::move(pw));
		this->event_invoker<ProjectWindowAddedEvent>()(_projectWindows.back().get());
	}

	static void OnProjectWindowDestroying (void* callbackArg, edge::win32_window_i* w)
	{
		auto pw = dynamic_cast<IProjectWindow*>(w);
		auto app = static_cast<SimulatorApp*>(callbackArg);

		pw->destroying().remove_handler (&OnProjectWindowDestroying, app);

		auto it = find_if (app->_projectWindows.begin(), app->_projectWindows.end(), [pw](auto& p) { return p.get() == pw; });
		assert (it != app->_projectWindows.end());
		app->event_invoker<ProjectWindowRemovingEvent>()(pw);
		auto pwLastRef = std::move(*it);
		app->_projectWindows.erase(it);
		app->event_invoker<ProjectWindowRemovedEvent>()(pwLastRef.get());
		if (app->_projectWindows.empty())
			PostQuitMessage(0);
	}

	virtual const std::vector<std::unique_ptr<IProjectWindow>>& GetProjectWindows() const override final { return _projectWindows; }

	virtual const wchar_t* GetRegKeyPath() const override final { return _regKeyPath.c_str(); }

	virtual const wchar_t* GetAppName() const override final { return AppName; }

	virtual const wchar_t* GetAppVersionString() const override final { return AppVersionString; }

	virtual ProjectWindowAddedEvent::subscriber GetProjectWindowAddedEvent() override final { return ProjectWindowAddedEvent::subscriber(this); }

	virtual ProjectWindowRemovingEvent::subscriber GetProjectWindowRemovingEvent() override final { return ProjectWindowRemovingEvent::subscriber(this); }

	virtual ProjectWindowRemovedEvent::subscriber GetProjectWindowRemovedEvent() override final { return ProjectWindowRemovedEvent::subscriber(this); }

	WPARAM RunMessageLoop()
	{
		auto accelerators = LoadAccelerators (_hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));

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
				if ((msg.hwnd == pw->hwnd()) || ::IsChild(pw->hwnd(), msg.hwnd))
				{
					translatedAccelerator = TranslateAccelerator (pw->hwnd(), accelerators, &msg);
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

static void RegisterApplicationAndFileTypes()
{
	auto exePath = make_unique<wchar_t[]>(MAX_PATH);
	DWORD dwRes = GetModuleFileName (nullptr, exePath.get(), MAX_PATH); assert(dwRes);
	wstringstream ss;
	ss << L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" << PathFindFileName(exePath.get());
	auto appPathKeyName = ss.str();

	bool notifyShell = false;
	auto buffer = make_unique<wchar_t[]>(MAX_PATH);
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
	ss << L"\"" << exePath.get() << L"\" \"%%1\"";
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
	int tmp = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	_CrtSetDbgFlag(tmp | _CRTDBG_LEAK_CHECK_DF);

	HRESULT hr = CoInitialize(0);

	RegisterApplicationAndFileTypes();

	bool tryDebugFirst = false;
#ifdef _DEBUG
	tryDebugFirst = true;
#endif

	com_ptr<ID3D11Device> d3d_device;
	com_ptr<ID3D11DeviceContext1> d3d_dc;

	{
		auto d3dFeatureLevel = D3D_FEATURE_LEVEL_9_1;
		com_ptr<ID3D11DeviceContext> deviceContext;

		if (tryDebugFirst)
		{
			hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
								   D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
								   &d3dFeatureLevel, 1,
								   D3D11_SDK_VERSION, &d3d_device, nullptr, &deviceContext);
		}

		if (!tryDebugFirst || FAILED(hr))
		{
			hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
								   D3D11_CREATE_DEVICE_BGRA_SUPPORT,
								   &d3dFeatureLevel, 1,
								   D3D11_SDK_VERSION, &d3d_device, nullptr, &deviceContext);
			assert(SUCCEEDED(hr));
		}

		d3d_dc = deviceContext;
	}

	com_ptr<IDWriteFactory> dwrite_factory;
	hr = DWriteCreateFactory (DWRITE_FACTORY_TYPE_SHARED, __uuidof (IDWriteFactory), reinterpret_cast<IUnknown**>(&dwrite_factory)); assert(SUCCEEDED(hr));

	int processExitValue;
	{
		SimulatorApp app (hInstance);

		auto project = projectFactory();
		project_window_create_params params = 
		{
			&app, project, selectionFactory, edit_area_factory, true, true, 1, SW_SHOW, d3d_dc, dwrite_factory
		};

		auto projectWindow = projectWindowFactory (params);
		app.AddProjectWindow(move(projectWindow));

		processExitValue = (int)app.RunMessageLoop();
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
