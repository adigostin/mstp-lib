
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "CppUnitTest.h"
#include "stp.h"
#include "port.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Microsoft::VisualStudio::CppUnitTestFramework
{
	template<>
	static inline std::wstring ToString(port* p)
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
	static void  StpCallback_OnPortRoleChanged (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, STP_PORT_ROLE role, unsigned int timestamp);
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
	std::function<void(size_t portIndex, size_t treeIndex, STP_PORT_ROLE role)> port_role_changed;
};

bool exchange_bpdus (test_bridge& one, size_t one_port, test_bridge& other, size_t other_port);
