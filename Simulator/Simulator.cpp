
#include "pch.h"
#include "SimulatorDefs.h"
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

static const wchar_t CompanyName[] = L"Adrian Gostin";
static const wchar_t AppName[] = L"STP Simulator";
static wstring RegKeyPath;

/*
BOOL CStpSimulatorApp::InitInstance ()
{
	_invokeGuiThreadRegisteredMessage = RegisterWindowMessage (L"{391CC5EE-942B-448E-B100-0ADE2521F34B}");
	if (_invokeGuiThreadRegisteredMessage == 0)
		throw Win32Exception (GetLastError (), L"RegisterWindowMessage");


	//BOOL bRes = SymInitialize (GetCurrentProcess (), NULL, TRUE);
	//ASSERT (bRes);
	//SymSetOptions (SYMOPT_LOAD_LINES);

	SYSTEMTIME startupUtcTime;
	GetSystemTime (&startupUtcTime);
	SystemTimeToFileTime (&startupUtcTime, &startupUtcFileTime);

	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinAppEx::InitInstance();

	EnableTaskbarInteraction(FALSE);

	// AfxInitRichEdit2() is required to use RichEdit control	
	// AfxInitRichEdit2();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	SetRegistryKey (L"Adrian Gostin");
	LoadStdProfileSettings (4);  // Load standard INI file options (including MRU)

	InitContextMenuManager();

	InitKeyboardManager();

	InitTooltipManager();
	CMFCToolTipInfo ttParams;
	ttParams.m_bVislManagerTheme = TRUE;
	theApp.GetTooltipManager()->SetTooltipParams(AFX_TOOLTIP_TYPE_ALL,
		RUNTIME_CLASS(CMFCToolTipCtrl), &ttParams);

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views
	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CStpSimulatorDoc),
		RUNTIME_CLASS(CMainFrame),       // main SDI frame window
		RUNTIME_CLASS(CSimulatorView));
	if (!pDocTemplate)
		return FALSE;
	AddDocTemplate(pDocTemplate);


	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// Enable DDE Execute open
	EnableShellOpen();
	RegisterShellFileTypes(TRUE);


	// Dispatch commands specified on the command line.  Will return FALSE if
	// app was launched with /RegServer, /Register, /Unregserver or /Unregister.
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

	// The one and only window has been initialized, so show and update it
	m_pMainWnd->ShowWindow(SW_SHOW);
	m_pMainWnd->UpdateWindow();
	// call DragAcceptFiles only if there's a suffix
	//  In an SDI app, this should occur after ProcessShellCommand
	// Enable drag/drop open
	m_pMainWnd->DragAcceptFiles();
	return TRUE;
}

afx_msg void CStpSimulatorApp::OnInvokeGuiThreadMessage (WPARAM wParam, LPARAM lParam)
{
	InvokeFunction targetFunction = (InvokeFunction) wParam;
	void* arg = (void*) lParam;
	targetFunction (arg);
}

void CStpSimulatorApp::PostInvokeOnGuiThread (InvokeFunction target, void* param)
{
	BOOL bRes = this->PostThreadMessage (_invokeGuiThreadRegisteredMessage, (WPARAM) target, (LPARAM) param);
	if (bRes == FALSE)
		throw Win32Exception (GetLastError (), L"PostThreadMessage");
}

unsigned int GetTimeMillis()
{
	SYSTEMTIME currentUtcTime;
	GetSystemTime (&currentUtcTime);

	FILETIME currentUtcFileTime;
	SystemTimeToFileTime (&currentUtcTime, &currentUtcFileTime);

	unsigned long long start = *(unsigned long long *) &startupUtcFileTime;
	unsigned long long now   = *(unsigned long long *) &currentUtcFileTime;
	
	unsigned long long milliseconds = (now - start) / 10000;
	return (unsigned int) milliseconds;
}

static const wchar_t NoStackTraceString [] = L"Could not get stack trace.";

void GetStackTraceString (CString* str, unsigned int framesToSkip)
{
	HANDLE process = GetCurrentProcess ();
	void* stack [100];
	unsigned int frameCount = CaptureStackBackTrace (framesToSkip, sizeof (stack) / sizeof (void*), stack, NULL);
	if (frameCount == 0)
	{
		str->Format (NoStackTraceString);
		return;
	}

	union
	{
		SYMBOL_INFO symbol;
		unsigned char symbolBuffer [sizeof (SYMBOL_INFO) + 256];
	};

	ZeroMemory (symbolBuffer, sizeof (symbolBuffer));
	symbol.MaxNameLen   = 255;
	symbol.SizeOfStruct = sizeof (SYMBOL_INFO);

	IMAGEHLP_LINEW64 line;
	ZeroMemory (&line, sizeof (line));
	line.SizeOfStruct = sizeof (line);

	for (unsigned int i = 0; i < frameCount; i++)
	{
		DWORD address = (DWORD64) (stack [i]);
		BOOL bRes = SymFromAddr (process, address, NULL, &symbol);
		if (!bRes)
		{
			str->Format (NoStackTraceString);
			return;
		}

		str->AppendFormat (L"@ %S", symbol.Name);

		DWORD displacement;
		bRes = SymGetLineFromAddrW64 (process, address, &displacement, &line);
		if (bRes)
		{
			const wchar_t* filePart = PathFindFileName (line.FileName);

			str->AppendFormat (L" (%s: %d)", filePart, line.LineNumber);
		}

		str->Append (L"\r\n");
	}
}
*/

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
		/*
		hr = _d3dDevice->QueryInterface(IID_PPV_ARGS(&_dxgiDevice)); ThrowIfFailed(hr);

		ComPtr<IDXGIAdapter> dxgiAdapter;
		hr = _dxgiDevice->GetAdapter(&dxgiAdapter); ThrowIfFailed(hr);

		hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&_dxgiFactory)); ThrowIfFailed(hr);
		*/
		ComPtr<IDWriteFactory> dWriteFactory;
		hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof (IDWriteFactory), reinterpret_cast<IUnknown**>(&dWriteFactory)); ThrowIfFailed(hr);

		ComPtr<IWICImagingFactory2> wicFactory;
		hr = CoCreateInstance(CLSID_WICImagingFactory2, NULL, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory2), (void**)&wicFactory); ThrowIfFailed(hr);

		{
			//App app(hInstance, device, deviceContext);
		
			auto onClosing = [](void* callbackArg, IProjectWindow* pw, bool* cancel)
			{
				//App* app = static_cast<App*>(callbackArg);
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