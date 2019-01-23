
#include "assert.h"

bool assert_function_running = false;

#ifdef _MSC_VER
	#include <Windows.h>
	#include <CommCtrl.h>
	#include <sstream>

	#pragma comment (lib, "Comctl32")


	class MainWindowFinder
	{
		HWND bestHandle;
		int processId;

	public:
		HWND FindMainWindow (int i_iprocessId)
		{
			this->bestHandle = nullptr;
			this->processId = i_iprocessId;
			::EnumWindows (&MainWindowFinder::EnumWindowsCallback, (LPARAM) (void*) this);
			return bestHandle;
		}

		static bool IsMainWindow (HWND handle)
		{
			return (::GetWindow (handle, GW_OWNER) == nullptr) && ::IsWindowVisible (handle);
		}

		static BOOL CALLBACK EnumWindowsCallback (HWND handle, LPARAM extraParameter)
		{
			MainWindowFinder* finder = (MainWindowFinder*) (void*) extraParameter;
			DWORD processId;
			::GetWindowThreadProcessId (handle, &processId);
			if ((int)processId == finder->processId)
			{
				if (IsMainWindow(handle))
				{
					finder->bestHandle = handle;
					return false;
				}
			}
			return true;
		}
	};

	__declspec(noinline) void __stdcall assert_function (const char* expression, const char* file, unsigned line)
	{
		if (assert_function_running)
			return;
		assert_function_running = true;

		// We remove WM_QUIT because if it is in the queue then the message box won't display.
		MSG msg;
		BOOL bQuit = PeekMessage (&msg, NULL, WM_QUIT, WM_QUIT, PM_REMOVE);

		std::wstringstream ss;
		ss << "Assertion Failed.\r\n"
			<< "Command Line: " << GetCommandLineA() << "\r\n"
			<< "File: " << file << "\r\n"
			<< "Line: " << line << "\r\n"
			<< "Expression: " << expression;
		auto message = ss.str();

		MainWindowFinder finder;
		auto hwndParent = finder.FindMainWindow (GetProcessId(GetCurrentProcess()));
		auto hwndThread = GetWindowThreadProcessId (hwndParent, nullptr);
		if (hwndThread != GetCurrentThreadId())
			hwndParent = nullptr;

		static constexpr int BUTTON_ID_CLOSE_APP = 10;
		static constexpr int BUTTON_ID_IGNORE    = 11;
		static constexpr int BUTTON_ID_DEBUG     = 12;

		static constexpr TASKDIALOG_BUTTON Buttons[] =
		{
			{ BUTTON_ID_CLOSE_APP,	L"Close Application\nYou will lose unsaved work." },
			{ BUTTON_ID_IGNORE,		L"Ignore Error\nApplication might eventually crash." },
			{ BUTTON_ID_DEBUG,		L"Debug Application" },
		};

		TASKDIALOGCONFIG config;
		memset (&config, 0, sizeof (config));
		config.cbSize = sizeof (config);
		config.hwndParent = hwndParent;
		config.hInstance = ::GetModuleHandle(nullptr);
		config.dwFlags = TDF_SIZE_TO_CONTENT | TDF_USE_COMMAND_LINKS;
		config.dwCommonButtons = 0;// TDCBF_OK_BUTTON;
		config.pszWindowTitle = nullptr; // Executable filename used as title if nullptr here
		config.pszMainIcon = TD_WARNING_ICON;
		config.cButtons = _countof(Buttons);
		config.pButtons = Buttons;
		config.pszMainInstruction = L"Application Error";
		config.pszContent = message.c_str();
		int pressedButtonID;
		HRESULT hr = TaskDialogIndirect (&config, &pressedButtonID, nullptr, nullptr);
		if (FAILED(hr))
			TerminateProcess (GetCurrentProcess(), ERROR_ASSERTION_FAILURE);

		switch (pressedButtonID)
		{
			case BUTTON_ID_CLOSE_APP:
				TerminateProcess (GetCurrentProcess(), ERROR_ASSERTION_FAILURE);

			case BUTTON_ID_DEBUG:
			case IDCANCEL:
				__debugbreak();
				break;

			case BUTTON_ID_IGNORE:
				break;

			default:
				TerminateProcess (GetCurrentProcess(), ERROR_ASSERTION_FAILURE);
		}

		if (bQuit)
			PostQuitMessage((int) msg.wParam);

		assert_function_running = false;
	}
#else
	#error
#endif
