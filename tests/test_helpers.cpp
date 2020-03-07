
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "test_helpers.h"

void* test_bridge::StpCallback_AllocAndZeroMemory (unsigned int size)
{
	void* res = malloc(size);
	memset (res, 0, size);
	return res;
}

void test_bridge::StpCallback_FreeMemory (void* p)
{
	free(p);
}

void* test_bridge::StpCallback_TransmitGetBuffer (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp)
{
	test_bridge* tb = static_cast<test_bridge*>(STP_GetApplicationContext(bridge));
	tb->tx_buffer_port_index = portIndex;
	tb->tx_buffer.resize(bpduSize);
	return tb->tx_buffer.data();
}

void test_bridge::StpCallback_TransmitReleaseBuffer (const STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer)
{
	test_bridge* tb = static_cast<test_bridge*>(STP_GetApplicationContext(bridge));
	tb->tx_queues[tb->tx_buffer_port_index].push(std::move(tb->tx_buffer));
}

static void StpCallback_EnableBpduTrapping (const STP_BRIDGE* bridge, bool enable, unsigned int timestamp)
{
}

static void StpCallback_EnableLearning (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable, unsigned int timestamp)
{
}

static void StpCallback_EnableForwarding (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable, unsigned int timestamp)
{
}

static void StpCallback_FlushFdb (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType, unsigned int timestamp)
{
}

static void StpCallback_DebugStrOut (const STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, unsigned int flush)
{
}

static void StpCallback_OnTopologyChange (const STP_BRIDGE* bridge, unsigned int treeIndex, unsigned int timestamp)
{
}

void test_bridge::StpCallback_OnPortRoleChanged (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, STP_PORT_ROLE role, unsigned int timestamp)
{
	test_bridge* tb = static_cast<test_bridge*>(STP_GetApplicationContext(bridge));
	if (tb->port_role_changed)
		tb->port_role_changed (portIndex, treeIndex, role);
}

const STP_CALLBACKS test_bridge::callbacks =
{
	&StpCallback_EnableBpduTrapping,
	&StpCallback_EnableLearning,
	&StpCallback_EnableForwarding,
	&StpCallback_TransmitGetBuffer,
	&StpCallback_TransmitReleaseBuffer,
	&StpCallback_FlushFdb,
	&StpCallback_DebugStrOut,
	&StpCallback_OnTopologyChange,
	&StpCallback_OnPortRoleChanged,
	&StpCallback_AllocAndZeroMemory,
	&StpCallback_FreeMemory,
};

test_bridge::test_bridge (size_t port_count, size_t msti_count, uint16_t max_vlan_number, const std::array<uint8_t, 6>& bridge_address)
{
	stp_bridge = STP_CreateBridge ((unsigned int)port_count, (unsigned int)msti_count, max_vlan_number, &callbacks, bridge_address.data(), 256);
	STP_SetApplicationContext (stp_bridge, this);
}

test_bridge::~test_bridge()
{
	STP_DestroyBridge (stp_bridge);
	stp_bridge = nullptr;
}

bool exchange_bpdus (test_bridge& one, size_t one_port, test_bridge& other, size_t other_port)
{
	bool exchanged = false;
	while (true)
	{
		if (!one.tx_queues[one_port].empty())
		{
			auto bpdu = std::move(one.tx_queues[one_port].back());
			one.tx_queues[one_port].pop();
			STP_OnBpduReceived (other, (unsigned int)other_port, bpdu.data(), (unsigned int) bpdu.size(), 0);
			exchanged = true;
		}
		else if (!other.tx_queues[other_port].empty())
		{
			auto bpdu = std::move(other.tx_queues[other_port].back());
			other.tx_queues[other_port].pop();
			STP_OnBpduReceived (one, (unsigned int)one_port, bpdu.data(), (unsigned int) bpdu.size(), 0);
			exchanged = true;
		}
		else
			break;
	}
	return exchanged;
};
