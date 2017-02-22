
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
static wstring RegKeyPath;

int APIENTRY wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	int tmp = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	_CrtSetDbgFlag(tmp | _CRTDBG_LEAK_CHECK_DF);

	HRESULT hr = CoInitialize(0);

	int processExitValue;

	{
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
		RegKeyPath = ss.get();

		ComPtr<ID3D11Device1> d3dDevice;
		hr = device->QueryInterface(IID_PPV_ARGS(&d3dDevice)); ThrowIfFailed(hr);
		ComPtr<ID3D11DeviceContext1> d3dDeviceContext;
		hr = deviceContext->QueryInterface(IID_PPV_ARGS(&d3dDeviceContext)); ThrowIfFailed(hr);
	
		ComPtr<IDWriteFactory> dWriteFactory;
		hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof (IDWriteFactory), reinterpret_cast<IUnknown**>(&dWriteFactory)); ThrowIfFailed(hr);

		ComPtr<IWICImagingFactory2> wicFactory;
		hr = CoCreateInstance(CLSID_WICImagingFactory2, NULL, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory2), (void**)&wicFactory); ThrowIfFailed(hr);

		{
			auto onClosing = [](void* callbackArg, IProjectWindow* pw, bool* cancel)
			{
				pw->SaveWindowLocation(RegKeyPath.c_str());
				::PostQuitMessage(0);
			};

			//auto actionList = actionListFactory();
			auto selection = selectionFactory();
			auto project = projectFactory();//move(actionList));
			auto projectWindow = projectWindowFactory(project, hInstance, L"APPLICATION_RIBBON", selection, editAreaFactory, d3dDeviceContext, dWriteFactory, wicFactory);
			projectWindow->ShowAtSavedWindowLocation(RegKeyPath.c_str());
			projectWindow->GetProjectWindowClosingEvent().AddHandler(onClosing, nullptr);
		
			MSG msg;
			while (GetMessage(&msg, nullptr, 0, 0))
			{
				//TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			processExitValue = (int)msg.wParam;
		}
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

	CoUninitialize();

	return processExitValue;
}
