
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

#pragma region Object class
ULONG STDMETHODCALLTYPE Object::AddRef()
{
	return InterlockedIncrement(&_refCount);
}

ULONG STDMETHODCALLTYPE Object::Release()
{
	ULONG newRefCount = InterlockedDecrement(&_refCount);
	if (newRefCount == 0)
		delete this;
	return newRefCount;
}
#pragma endregion

unsigned int GetTimestampMilliseconds()
{
	SYSTEMTIME currentUtcTime;
	GetSystemTime(&currentUtcTime);

	FILETIME currentUtcFileTime;
	SystemTimeToFileTime(&currentUtcTime, &currentUtcFileTime);

	FILETIME creationTime;
	FILETIME exitTime;
	FILETIME kernelTime;
	FILETIME userTime;
	GetProcessTimes (GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime);

	uint64_t start = ((uint64_t) creationTime.dwHighDateTime << 32) | creationTime.dwLowDateTime;
	uint64_t now = ((uint64_t) currentUtcFileTime.dwHighDateTime << 32) | currentUtcFileTime.dwLowDateTime;
	uint64_t milliseconds = (now - start) / 10000;
	return (unsigned int)milliseconds;
}

ColorF GetD2DSystemColor (int sysColorIndex)
{
	DWORD brg = GetSysColor (sysColorIndex);
	DWORD rgb = ((brg & 0xff0000) >> 16) | (brg & 0xff00) | ((brg & 0xff) << 16);
	return ColorF (rgb);
}

bool HitTestLine (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance, D2D1_POINT_2F p0w, D2D1_POINT_2F p1w, float lineWidth)
{
	auto fd = zoomable->GetDLocationFromWLocation(p0w);
	auto td = zoomable->GetDLocationFromWLocation(p1w);

	float halfw = zoomable->GetDLengthFromWLength(lineWidth) / 2.0f;
	if (halfw < tolerance)
		halfw = tolerance;

	float angle = atan2(td.y - fd.y, td.x - fd.x);
	float s = sin(angle);
	float c = cos(angle);

	array<D2D1_POINT_2F, 4> vertices = 
	{
		D2D1_POINT_2F { fd.x + s * halfw, fd.y - c * halfw },
		D2D1_POINT_2F { fd.x - s * halfw, fd.y + c * halfw },
		D2D1_POINT_2F { td.x - s * halfw, td.y + c * halfw },
		D2D1_POINT_2F { td.x + s * halfw, td.y - c * halfw }
	};

	return PointInPolygon (&vertices[0], 4, dLocation);
}

bool PointInPolygon (const D2D1_POINT_2F* vertices, size_t vertexCount, D2D1_POINT_2F point)
{
	// Taken from http://stackoverflow.com/a/2922778/451036
	bool c = false;
	for (size_t i = 0, j = vertexCount - 1; i < vertexCount; j = i++)
	{
		if (((vertices[i].y > point.y) != (vertices[j].y > point.y)) &&
			(point.x < (vertices[j].x - vertices[i].x) * (point.y - vertices[i].y) / (vertices[j].y - vertices[i].y) + vertices[i].x))
			c = !c;
	}

	return c;
}

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
/*
unsigned long long GetMacAddressValueFromBytes (unsigned char* address)
{
	unsigned int high = ((unsigned int) address[0] << 8) | (unsigned int) address[1];
	unsigned int low = ((unsigned int) address[2] << 24) | ((unsigned int) address[3] << 16) | ((unsigned int) address[4] << 8) | (unsigned int) address[5];

	return (((unsigned long long) high) << 32) | low;
}

void GetMacAddressBytesFromValue (unsigned long long value, unsigned char* addressOut6Bytes)
{
	unsigned int low  = (unsigned int) value;
	unsigned int high = (unsigned int) (value >> 32);
	addressOut6Bytes [0] = (unsigned char) (high >> 8);
	addressOut6Bytes [1] = (unsigned char) high;
	addressOut6Bytes [2] = (unsigned char) (low >> 24);
	addressOut6Bytes [3] = (unsigned char) (low >> 16);
	addressOut6Bytes [4] = (unsigned char) (low >> 8);
	addressOut6Bytes [5] = (unsigned char) low;
}

void MacAddressToString (unsigned long long address, wchar_t* bufferOut18WChars)
{
	_snwprintf_s (bufferOut18WChars, 18, 18, L"%02X:%02X:%02X:%02X:%02X:%02X",
		(unsigned int) (address >> 40) & 0xFF,
		(unsigned int) (address >> 32) & 0xFF,
		((unsigned int) address >> 24) & 0xFF,
		((unsigned int) address >> 16) & 0xFF,
		((unsigned int) address >> 8) & 0xFF,
		((unsigned int) address & 0xFF));
}

void MacAddressToString (unsigned char* address6Bytes, wchar_t* bufferOut18WChars)
{
	_snwprintf_s (bufferOut18WChars, 18, 18, L"%02X:%02X:%02X:%02X:%02X:%02X",
		address6Bytes [0], address6Bytes [1], address6Bytes [2], address6Bytes [3], address6Bytes [4], address6Bytes [5]);
}

bool TryParseMacAddress (const wchar_t* text, unsigned long long* addressOut)
{
	if ((wcslen (text) == 17)
		&& iswxdigit (text [0]) && iswxdigit (text [1]) && (text [2] == L':')
		&& iswxdigit (text [3]) && iswxdigit (text [4]) && (text [5] == L':')
		&& iswxdigit (text [6]) && iswxdigit (text [7]) && (text [8] == L':')
		&& iswxdigit (text [9]) && iswxdigit (text [10]) && (text [11] == L':')
		&& iswxdigit (text [12]) && iswxdigit (text [13]) && (text [14] == L':')
		&& iswxdigit (text [15]) && iswxdigit (text [16]))
	{
		*addressOut = (_wcstoui64 (&text [0],  NULL, 16) << 40)
					| (_wcstoui64 (&text [3],  NULL, 16) << 32)
					| (_wcstoui64 (&text [6],  NULL, 16) << 24)
					| (_wcstoui64 (&text [9],  NULL, 16) << 16)
					| (_wcstoui64 (&text [12], NULL, 16) << 8)
					| (_wcstoui64 (&text [15], NULL, 16));
		return true;
	}

	return false;
}

bool TryParseMacAddress (const wchar_t* text, unsigned char* addressOut6Bytes)
{
	unsigned long long value;
	if (TryParseMacAddress (text, &value) == false)
		return false;

	GetMacAddressBytesFromValue (value, addressOut6Bytes);
	return true;
}
*/