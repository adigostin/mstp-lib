
#include "pch.h"
#include "Simulator.h"
#include "Win32Defs.h"

#pragma comment (lib, "d2d1.lib")
#pragma comment (lib, "dwrite.lib")
#pragma comment (lib, "D3D11.lib")
#pragma comment (lib, "Dxgi.lib")
#pragma comment (lib, "Windowscodecs.lib")
#pragma comment (lib, "Shlwapi")
#pragma comment (lib, "Version")
#pragma comment (lib, "Comctl32")

using namespace std;
using namespace D2D1;

static const wchar_t CompanyName[] = L"Adrian Gostin";
static const wchar_t AppName[] = L"STP Simulator";

unique_ptr<ISimulatorApp> App;

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
	RECT rect = this->GetClientRect();
	return SIZE { rect.right, rect.bottom };
}

RECT IWin32Window::GetClientRect() const
{
	RECT rect;
	BOOL bRes = ::GetClientRect (GetHWnd(), &rect);
	if (!bRes)
		throw win32_exception(GetLastError());
	return rect;
};

class SimulatorApp : public ISimulatorApp
{
	wstring _regKeyPath;
	ComPtr<ID3D11Device1> _d3dDevice;
	ComPtr<ID3D11DeviceContext1> _d3dDeviceContext;
	ComPtr<IDWriteFactory> _dWriteFactory;
	ComPtr<IWICImagingFactory2> _wicFactory;

public:
	SimulatorApp()
	{
		HRESULT hr;

		bool tryDebugFirst = false;
		#ifdef _DEBUG
		tryDebugFirst = true;
		#endif

		auto d3dFeatureLevel = D3D_FEATURE_LEVEL_9_1;
		ComPtr<ID3D11Device> device;
		ComPtr<ID3D11DeviceContext> deviceContext;

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

		wstringstream ss;
		ss << L"SOFTWARE\\" << CompanyName << L"\\" << ::AppName;
		_regKeyPath = ss.get();

		hr = device->QueryInterface(IID_PPV_ARGS(&_d3dDevice)); ThrowIfFailed(hr);

		hr = deviceContext->QueryInterface(IID_PPV_ARGS(&_d3dDeviceContext)); ThrowIfFailed(hr);

		hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof (IDWriteFactory), reinterpret_cast<IUnknown**>(&_dWriteFactory)); ThrowIfFailed(hr);

		hr = CoCreateInstance(CLSID_WICImagingFactory2, NULL, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory2), (void**)&_wicFactory); ThrowIfFailed(hr);
	}

	virtual ~SimulatorApp()
	{
		/*
		if (device->GetCreationFlags() & D3D11_CREATE_DEVICE_DEBUG)
		{
			ComPtr<ID3D11Debug> debug;
			hr = device->QueryInterface(&debug);
			if (SUCCEEDED(hr))
				debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
		}
		*/
	}

	virtual const wchar_t* GetRegKeyPath() const override final { return _regKeyPath.c_str(); }

	virtual ID3D11DeviceContext1* GetD3DDeviceContext() const override final { return _d3dDeviceContext; }

	virtual IDWriteFactory* GetDWriteFactory() const override final { return _dWriteFactory; }

	virtual IWICImagingFactory2* GetWicFactory() const override final { return _wicFactory; }
};

int APIENTRY wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	int tmp = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	_CrtSetDbgFlag(tmp | _CRTDBG_LEAK_CHECK_DF);

	HRESULT hr = CoInitialize(0);
	::App = unique_ptr<ISimulatorApp>(new SimulatorApp());

	int processExitValue;

	{
		//auto actionList = actionListFactory();
		auto selection = selectionFactory();
		auto project = projectFactory();//move(actionList));
		auto projectWindow = projectWindowFactory(project, hInstance, L"APPLICATION_RIBBON", selection, editAreaFactory, nCmdShow);
		
		MSG msg;
		while (GetMessage(&msg, nullptr, 0, 0))
		{
			//TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		processExitValue = (int)msg.wParam;
	}

	::App = nullptr;
	CoUninitialize();

	return processExitValue;
}
