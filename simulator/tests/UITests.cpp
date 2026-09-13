
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "resource.h"
#include "test_helpers.h"

#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")

#define FORCE_EXPLICIT_DTE_NAMESPACE
#include <dte.h>
namespace VxDTE
{
	#include <dte80.h>
	#include <dte90.h>
}

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

struct SimulatorInstance
{
	wil::unique_process_information pi;
	HWND projectWindow;
	wil::com_ptr_failfast<VxDTE::Process> attachedProcess;

	~SimulatorInstance()
	{
		if (attachedProcess)
		{
			if (FAILED(attachedProcess->Detach(VARIANT_FALSE)))
				__debugbreak();
		}

		// Comment this macro to keep VS running after running the test. Useful for debugging.
		#define CLOSE_IT

		#ifdef CLOSE_IT
		DWORD waitResult = WaitForSingleObject(pi.hProcess, 0);
		if (waitResult == WAIT_TIMEOUT)
		{
			// Process still running.
			PostMessageW(projectWindow, WM_CLOSE, 0, 0);
			if (WaitForSingleObject(pi.hProcess, 5000) == WAIT_TIMEOUT)
				TerminateProcess(pi.hProcess, 1);
		}
		else if (waitResult == WAIT_FAILED)
		{
			DWORD le = GetLastError();
			__debugbreak();
		}
		#endif
	}
};

static HRESULT GetDTE(DWORD processId, VxDTE::DTE2** ppDTE)
{
	wil::unique_process_heap_string fileName;
	HRESULT hr = wil::GetModuleFileNameW(nullptr, fileName); RETURN_IF_FAILED(hr);
	DWORD ignored;
	DWORD versionLength = GetFileVersionInfoSizeW(fileName.get(), &ignored);
	RETURN_LAST_ERROR_IF(versionLength == 0);
	auto versionBuffer = wil::make_unique_nothrow<char[]>(versionLength);
	RETURN_IF_WIN32_BOOL_FALSE(GetFileVersionInfoW(fileName.get(), 0, versionLength, versionBuffer.get()));
	void* value;
	UINT valueLength;
	RETURN_IF_WIN32_BOOL_FALSE(VerQueryValueW(versionBuffer.get(), L"\\", &value, &valueLength));
	VS_FIXEDFILEINFO* versionInfo = static_cast<VS_FIXEDFILEINFO*>(value);
	DWORD majorVersion = versionInfo->dwProductVersionMS >> 16;
	auto progId = wil::str_printf_failfast<wil::unique_process_heap_string>(L"!VisualStudio.DTE.%u.0:%u", majorVersion, processId);

	wil::com_ptr_failfast<IBindCtx> bindContext;
	wil::com_ptr_failfast<IRunningObjectTable> runningObjectTable;
	wil::com_ptr_failfast<IEnumMoniker> monikers;
	RETURN_IF_FAILED(CreateBindCtx(0, &bindContext));
	RETURN_IF_FAILED(bindContext->GetRunningObjectTable(&runningObjectTable));
	RETURN_IF_FAILED(runningObjectTable->EnumRunning(&monikers));

	wil::com_ptr_failfast<IMoniker> moniker;
	ULONG fetched;
	while (monikers->Next(1, moniker.addressof(), &fetched) == S_OK)
	{
		wil::unique_cotaskmem_string name;
		hr = moniker->GetDisplayName(bindContext, nullptr, &name);
		if (hr == E_ACCESSDENIED)
			continue;
		RETURN_IF_FAILED(hr);
		if (name && wcscmp(name.get(), progId.get()) == 0)
		{
			wil::com_ptr_failfast<IUnknown> runningObject;
			RETURN_IF_FAILED(runningObjectTable->GetObject(moniker, &runningObject));
			return runningObject->QueryInterface(IID_PPV_ARGS(ppDTE));
		}
	}

	return E_FAIL;
}

static void AttachToProcess(DWORD processId, VxDTE::DTE2* dte, VxDTE::Debugger* debugger, VxDTE::Process** attachedProcess)
{
	wil::com_ptr_failfast<VxDTE::Debugger3> debugger3;
	wil::com_ptr_failfast<VxDTE::Processes> processes;
	wil::com_ptr_failfast<IUnknown> enumeratorUnknown;
	wil::com_ptr_failfast<IEnumVARIANT> enumerator;
	Assert::IsTrue(SUCCEEDED(debugger->QueryInterface(IID_PPV_ARGS(&debugger3))));
	Assert::IsTrue(SUCCEEDED(debugger3->get_LocalProcesses(&processes)));
	Assert::IsTrue(SUCCEEDED(processes->_NewEnum(&enumeratorUnknown)));
	Assert::IsTrue(SUCCEEDED(enumeratorUnknown->QueryInterface(IID_PPV_ARGS(&enumerator))));

	wil::unique_variant value;
	ULONG fetched;
	while (enumerator->Next(1, &value, &fetched) == S_OK)
	{
		if (auto process = wil::try_com_query_failfast<VxDTE::Process3>(value.pdispVal))
		{
			long processIdFromDte;
			if (SUCCEEDED(process->get_ProcessID(&processIdFromDte)) && static_cast<DWORD>(processIdFromDte) == processId)
			{
				dte->put_SuppressUI(VARIANT_TRUE);
				HRESULT hr = process->Attach2(wil::make_variant_bstr_failfast(L"Native"));
				dte->put_SuppressUI(VARIANT_FALSE);
				Assert::IsTrue(SUCCEEDED(hr));
				*attachedProcess = process.detach();
				return;
			}
		}
	}
	Assert::Fail(L"The Visual Studio debugger could not attach to the simulator.");
}

static wil::com_ptr_failfast<VxDTE::Process> FindAndAttachToSimulator(DWORD simulatorProcessId)
{
	HRESULT hr;

	// We (testhost.exe) are being debugged. Find the VS instance that's debugging us
	// and tell it to debug the target VS too.
	DWORD selfProcessId = GetCurrentProcessId();

	// Get all running processes.
	DWORD cap = 300;
	wil::unique_hlocal_ptr<DWORD[]> pids;
	DWORD processCount;
	while(true)
	{
		pids = wil::make_unique_hlocal_failfast<DWORD[]>(cap);
		DWORD neededBytes;
		BOOL bres = EnumProcesses (pids.get(), cap * sizeof(DWORD), &neededBytes);
		Assert::IsTrue(bres);
		processCount = neededBytes / sizeof(DWORD);
		if (processCount < cap)
			break;
		cap += cap;
	}

	// Retain only the "devenv.exe" processes.
	DWORD vsCount = 0;
	for (DWORD i = 0; i < cap; i++)
	{
		wil::unique_process_handle h (OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pids.get()[i]));
		if (h.is_valid())
		{
			wil::unique_process_heap_string path;
			hr = wil::QueryFullProcessImageNameW (h.get(), 0, path);
			if (SUCCEEDED(hr))
			{
				auto p = wcsrchr(path.get(), L'\\');
				if (p && !wcscmp(p + 1, L"devenv.exe"))
					pids.get()[vsCount++] = pids.get()[i];
			}
		}
	}

	// Now find the VS process that's debugging us.
	for (DWORD i = 0; i < vsCount; i++)
	{
		wil::com_ptr_failfast<VxDTE::DTE2> dte;
		if (FAILED(GetDTE(pids[i], &dte)))
			continue;
		//OutputDebugString(L"Got DTE\r\n");
		wil::com_ptr_failfast<VxDTE::Debugger> debugger;
		wil::com_ptr_failfast<VxDTE::Processes> debuggedProcesses;
		wil::com_ptr_failfast<IUnknown> unknown;
		wil::com_ptr_failfast<IEnumVARIANT> enumerator;
		VxDTE::dbgDebugMode mode;
		if (   FAILED(dte->get_Debugger(&debugger))
			|| FAILED(debugger->get_CurrentMode(&mode)) || mode == VxDTE::dbgDesignMode
			|| FAILED(debugger->get_DebuggedProcesses(&debuggedProcesses))
			|| FAILED(debuggedProcesses->_NewEnum(&unknown))
			|| FAILED(unknown->QueryInterface(IID_PPV_ARGS(&enumerator))))
			continue;

		wil::unique_variant value;
		ULONG fetched;
		while (enumerator->Next(1, &value, &fetched) == S_OK)
		{
			wil::com_ptr_failfast<VxDTE::Process> debuggedProcess;
			long debuggedProcessId;
			if (value.vt == VT_DISPATCH
				&& SUCCEEDED(value.pdispVal->QueryInterface(IID_PPV_ARGS(&debuggedProcess)))
				&& SUCCEEDED(debuggedProcess->get_ProcessID(&debuggedProcessId))
				&& static_cast<DWORD>(debuggedProcessId) == selfProcessId)
			{
				VxDTE::Process* attachedProcess = nullptr;
				AttachToProcess(simulatorProcessId, dte.get(), debugger.get(), &attachedProcess);
				wil::com_ptr_failfast<VxDTE::Process> result;
				result.attach(attachedProcess);
				return result;
			}
		}
	}

	Assert::Fail(L"Could not find the Visual Studio instance debugging vstest.console.exe.");
	return nullptr;
}

static SimulatorInstance RunSimulator(const wchar_t* projectPath)
{
	wchar_t modulePath[MAX_PATH];
	DWORD length = GetModuleFileNameW((HMODULE)&__ImageBase, modulePath, _countof(modulePath));
	Assert::IsTrue(length > 0 && length < _countof(modulePath));
	PathRemoveFileSpecW(modulePath);
	std::wstring simulatorPath = std::wstring(modulePath) + L"\\simulator.exe";

	// I tried launching the process with CREATE_SUSPENDED, then attaching to it, then resuming it.
	// But the process was always hitting a breakpoint at LdrpDoDebuggerBreak - too annoying,
	// that's why I introduced "-waitForDebugger".

	std::wstring commandLine = L"\"" + simulatorPath + L"\"";
	if (IsDebuggerPresent())
		commandLine = commandLine + L" -waitForDebugger";
	if (projectPath)
		commandLine = commandLine + L" \"" + projectPath + L"\"";
	STARTUPINFOW startupInfo = { sizeof(startupInfo) };
	wil::unique_process_information processInfo;
	BOOL created = CreateProcessW(simulatorPath.c_str(), (LPWSTR)commandLine.c_str(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startupInfo, &processInfo);
	Assert::IsTrue(created);

	wil::com_ptr_failfast<VxDTE::Process> attachedProcess;
	if (IsDebuggerPresent())
		attachedProcess = FindAndAttachToSimulator(processInfo.dwProcessId);

	HWND projectWindow = nullptr;
	for (DWORD i = 0; i != 100 && projectWindow == nullptr; ++i)
	{
		std::pair<DWORD, HWND*> search = { processInfo.dwProcessId, &projectWindow };
		EnumWindows([](HWND window, LPARAM parameter) -> BOOL {
			auto values = reinterpret_cast<std::pair<DWORD, HWND*>*>(parameter);
			DWORD windowProcessId;
			GetWindowThreadProcessId(window, &windowProcessId);
			wchar_t className[32];
			if ((windowProcessId == values->first) && GetClassNameW(window, className, _countof(className)) && !_wcsicmp(className, L"ProjectWindow"))
				*values->second = window;
			return TRUE;
		}, reinterpret_cast<LPARAM>(&search));
		if (projectWindow == nullptr && WaitForSingleObject(processInfo.hProcess, 0) == WAIT_TIMEOUT)
			Sleep(50);
	}
	Assert::IsNotNull(projectWindow);

	return { std::move(processInfo), projectWindow, std::move(attachedProcess) };
}

namespace UITests
{
	TEST_CLASS(UITests)
	{
	public:
		TEST_METHOD(ApplicationCanSaveProjectLoadedFromDisk)
		{
			auto project = MakeProject();
			project->AddBridge(MakeBridge(4, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 }));
			TempProjectFile file(L"project.stp");
			Assert::AreEqual(S_OK, project->Save(file.path.c_str()));

			auto simulator = RunSimulator(file.path.c_str());

			HRESULT hr = (HRESULT)SendMessageW(simulator.projectWindow, WM_COMMAND, ID_FILE_SAVE, 0);
			Assert::AreEqual(S_OK, hr);
		}
	};
}
