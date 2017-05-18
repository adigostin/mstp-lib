
#include "pch.h"
#include "Simulator.h"
#include "Bridge.h"
#include "Wire.h"

#pragma comment (lib, "d2d1.lib")
#pragma comment (lib, "dwrite.lib")
#pragma comment (lib, "D3D11.lib")
#pragma comment (lib, "Dxgi.lib")
#pragma comment (lib, "Windowscodecs.lib")
#pragma comment (lib, "Shlwapi")
#pragma comment (lib, "Version")
#pragma comment (lib, "Comctl32")

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace std;
using namespace D2D1;

static const wchar_t CompanyName[] = L"Adi Gostin";
static const wchar_t AppName[] = L"STP Simulator";
static const wchar_t AppVersion[] = L"2.0";

#pragma region IWin32Window

bool IWin32Window::IsVisible() const
{
	return (GetWindowLongPtr (GetHWnd(), GWL_STYLE) & WS_VISIBLE) != 0;
}

RECT IWin32Window::GetWindowRect() const
{
	RECT rect;
	BOOL bRes = ::GetWindowRect (GetHWnd(), &rect);
	if (!bRes)
		throw win32_exception(GetLastError());
	return rect;
}

SIZE IWin32Window::GetWindowSize() const
{
	auto rect = GetWindowRect();
	return SIZE { rect.right - rect.left, rect.bottom - rect.top };
}

SIZE IWin32Window::GetClientSize() const
{
	RECT rect = this->GetClientRectPixels();
	return SIZE { rect.right, rect.bottom };
}

RECT IWin32Window::GetClientRectPixels() const
{
	RECT rect;
	BOOL bRes = ::GetClientRect (GetHWnd(), &rect);
	if (!bRes)
		throw win32_exception(GetLastError());
	return rect;
};
#pragma endregion

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
#pragma endregion

class SimulatorApp : public EventManager, public ISimulatorApp
{
	HINSTANCE const _hInstance;
	ID3D11Device1Ptr _d3dDevice;
	ID3D11DeviceContext1Ptr _d3dDeviceContext;
	IDWriteFactoryPtr _dWriteFactory;

	wstring _regKeyPath;
	vector<IProjectWindowPtr> _projectWindows;

public:
	SimulatorApp (HINSTANCE hInstance)
		: _hInstance(hInstance)
	{
		wstringstream ss;
		ss << L"SOFTWARE\\" << CompanyName << L"\\" << ::AppName << L"\\" << ::AppVersion;
		_regKeyPath = ss.str();

		bool tryDebugFirst = false;
		#ifdef _DEBUG
		tryDebugFirst = true;
		#endif

		auto d3dFeatureLevel = D3D_FEATURE_LEVEL_9_1;
		ID3D11DevicePtr device;
		ID3D11DeviceContextPtr deviceContext;

		HRESULT hr;
		if (tryDebugFirst)
		{
			hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
								   D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
								   &d3dFeatureLevel, 1,
								   D3D11_SDK_VERSION, &device, nullptr, &deviceContext);
		}

		if (!tryDebugFirst || FAILED(hr))
		{
			hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
								   D3D11_CREATE_DEVICE_BGRA_SUPPORT,
								   &d3dFeatureLevel, 1,
								   D3D11_SDK_VERSION, &device, nullptr, &deviceContext);
			ThrowIfFailed(hr);
		}

		hr = device->QueryInterface(IID_PPV_ARGS(&_d3dDevice)); ThrowIfFailed(hr);

		hr = deviceContext->QueryInterface(IID_PPV_ARGS(&_d3dDeviceContext)); ThrowIfFailed(hr);

		hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof (IDWriteFactory), reinterpret_cast<IUnknown**>(&_dWriteFactory)); ThrowIfFailed(hr);

		//IWICImagingFactory2Ptr wicFactory;
		//hr = CoCreateInstance(CLSID_WICImagingFactory2, NULL, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory2), (void**)&wicFactory); ThrowIfFailed(hr);
	}

	virtual HINSTANCE GetHInstance() const override final { return _hInstance; }

	virtual void AddProjectWindow (IProjectWindow* pw) override final
	{
		pw->GetDestroyingEvent().AddHandler (&OnProjectWindowDestroying, this);
		_projectWindows.push_back(move(pw));
		ProjectWindowAddedEvent::InvokeHandlers(*this, pw);
	}

	static void OnProjectWindowDestroying (void* callbackArg, IProjectWindow* pw)
	{
		auto app = static_cast<SimulatorApp*>(callbackArg);

		pw->GetDestroyingEvent().RemoveHandler (&OnProjectWindowDestroying, app);

		auto it = find_if (app->_projectWindows.begin(), app->_projectWindows.end(), [pw](const IProjectWindowPtr& p) { return p.GetInterfacePtr() == pw; });
		assert (it != app->_projectWindows.end());
		ProjectWindowRemovingEvent::InvokeHandlers(*app, pw);
		IProjectWindowPtr pwLastRef = move(*it);
		app->_projectWindows.erase(it);
		ProjectWindowRemovedEvent::InvokeHandlers(*app, pwLastRef);
		if (app->_projectWindows.empty())
			PostQuitMessage(0);
	}

	virtual const std::vector<IProjectWindowPtr>& GetProjectWindows() const override final { return _projectWindows; }

	virtual ID3D11DeviceContext1* GetD3DDeviceContext() const override final { return _d3dDeviceContext; }

	virtual IDWriteFactory* GetDWriteFactory() const override final { return _dWriteFactory; }

	virtual const wchar_t* GetRegKeyPath() const override final { return _regKeyPath.c_str(); }

	virtual const wchar_t* GetAppName() const override final { return AppName; }

	virtual ProjectWindowAddedEvent::Subscriber GetProjectWindowAddedEvent() override final { return ProjectWindowAddedEvent::Subscriber(this); }

	virtual ProjectWindowRemovingEvent::Subscriber GetProjectWindowRemovingEvent() override final { return ProjectWindowRemovingEvent::Subscriber(this); }

	virtual ProjectWindowRemovedEvent::Subscriber GetProjectWindowRemovedEvent() override final { return ProjectWindowRemovedEvent::Subscriber(this); }

	WPARAM RunMessageLoop()
	{
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

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		return msg.wParam;
	}
};

int APIENTRY wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	int tmp = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	_CrtSetDbgFlag(tmp | _CRTDBG_LEAK_CHECK_DF);

	HRESULT hr = CoInitialize(0);

	int processExitValue;
	{
		SimulatorApp app (hInstance);

		{
			auto actionList = actionListFactory();
			auto project = projectFactory();
			auto projectWindow = projectWindowFactory (&app, project, selectionFactory, actionList, editAreaFactory, nCmdShow, 1);
			app.AddProjectWindow(move(projectWindow));
		}

		processExitValue = (int)app.RunMessageLoop();
	}
	/*
	if (device->GetCreationFlags() & D3D11_CREATE_DEVICE_DEBUG)
	{
		deviceContext = nullptr;
		ID3D11DebugPtr debug;
		hr = device->QueryInterface(&debug);
		if (SUCCEEDED(hr))
			debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
	}
	*/
	CoUninitialize();

	return processExitValue;
}
