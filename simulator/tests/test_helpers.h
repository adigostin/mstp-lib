
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "stp.h"
#include "Simulator.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Microsoft::VisualStudio::CppUnitTestFramework
{
	template<>
	static inline std::wstring ToString(IPort* p)
	{
		return L"port";
	}

	template<>
	static inline std::wstring ToString (const STP_PORT_ROLE& role)
	{
		std::string_view str = STP_GetPortRoleString(role);
		return std::wstring (str.begin(), str.end());
	}
}

class test_bridge
{
	STP_BRIDGE* stp_bridge;

	static void* StpCallback_AllocAndZeroMemory (unsigned int size);
	static void  StpCallback_FreeMemory (void* p);
	static void* StpCallback_TransmitGetBuffer (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp);
	static void  StpCallback_TransmitReleaseBuffer (const STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer);
	static const STP_CALLBACKS callbacks;

	std::vector<uint8_t> tx_buffer;
	size_t tx_buffer_port_index;

public:
	test_bridge (size_t port_count, size_t msti_count, uint16_t max_vlan_number, const std::array<uint8_t, 6>& bridge_address);
	test_bridge (const test_bridge&) = delete;
	test_bridge& operator= (const test_bridge&) = delete;
	~test_bridge();

	operator STP_BRIDGE* () const { return stp_bridge; }

	using tx_queue = std::queue<std::vector<uint8_t>>;
	std::unordered_map<size_t, tx_queue> tx_queues;
};

bool exchange_bpdus (test_bridge& one, size_t one_port, test_bridge& other, size_t other_port);

inline wil::com_ptr_failfast<IBridge> MakeBridge (uint32_t portCount, uint32_t mstiCount, mac_address addr)
{
	com_ptr<IBridge> b;
	auto hr = MakeBridge(portCount, mstiCount, addr, &b); Assert::AreEqual(S_OK, hr);
	return b;
}

inline wil::com_ptr_failfast<IWire> MakeWire()
{
	com_ptr<IWire> w;
	auto hr = MakeWire(&w); Assert::AreEqual(S_OK, hr);
	return w;
}

inline wil::com_ptr_failfast<IStpProject> MakeProject()
{
	wil::com_ptr_failfast<IStpProject> p;
	auto hr = MakeProject(&p); Assert::AreEqual(S_OK, hr);
	return p;
}

struct TempProjectFile
{
	std::wstring folder;
	std::wstring path;

	TempProjectFile(const wchar_t* fileName)
	{
		wchar_t tempPath[MAX_PATH];
		DWORD cch = GetTempPathW(_countof(tempPath), tempPath);
		Assert::IsTrue(cch > 0 && cch < _countof(tempPath));

		folder = tempPath;
		folder += L"mstp-lib-project-tests-";
		folder += std::to_wstring(GetCurrentProcessId());
		folder += L"-";
		folder += std::to_wstring(GetTickCount64());
		path = folder + L"\\" + fileName;

		BOOL ok = CreateDirectoryW(folder.c_str(), nullptr);
		if (!ok)
		{
			DWORD err = GetLastError();
			Assert::AreEqual((DWORD)ERROR_ALREADY_EXISTS, err);
		}
	}

	~TempProjectFile()
	{
		DeleteFileW(path.c_str());
		RemoveDirectoryW(folder.c_str());
	}
};
