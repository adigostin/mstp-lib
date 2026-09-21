
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "Simulator.h"
#include "test_helpers.h"
#include "dispids.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

TEST_CLASS(bridge_tests)
{
	TEST_METHOD(create_bridge_test1)
	{
		uint32_t port_count = 4;
		uint32_t msti_count = 0;
		mac_address address = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };
		auto b = MakeBridge(port_count, msti_count, address);
	}

	TEST_METHOD(ports_test)
	{
		ULONG port_count = 4;
		uint32_t msti_count = 0;
		mac_address address = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };
		auto b = MakeBridge(port_count, msti_count, address);
		Assert::AreEqual(port_count, b->PortCount());
	}

	TEST_METHOD(disable_stp_test1)
	{
		uint32_t port_count = 4;
		uint32_t msti_count = 0;
		mac_address address = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };
		auto b = MakeBridge(port_count, msti_count, address);

		auto get_root_bridge_id = [&b] { return b->TreeAt(0)->root_bridge_id(); };

		b->set_stp_enabled(true);
		get_root_bridge_id();

		b->set_stp_enabled(false);
		Assert::ExpectException<const std::logic_error&>(get_root_bridge_id);

		b->set_stp_enabled(true);
		get_root_bridge_id();
	}

	TEST_METHOD(receive_more_mstis_on_same_mst_config)
	{
		size_t port_count = 4;
		size_t msti_count = 5;
		test_bridge bridge0 (port_count, msti_count, 16, { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		STP_SetStpVersion (bridge0, STP_VERSION_MSTP, 0);
		STP_SetMstConfigName (bridge0, "ABC", 0);
		STP_StartBridge (bridge0, 0);
		STP_OnPortEnabled (bridge0, 0, 100, true, 0);

		msti_count = 4;
		test_bridge bridge1 (port_count, msti_count, 16, { 0x10, 0x20, 0x30, 0x40, 0x50, 0x70 });
		STP_SetStpVersion (bridge1, STP_VERSION_MSTP, 0);
		STP_SetMstConfigName (bridge1, "ABC", 0);
		STP_StartBridge (bridge1, 0);
		STP_OnPortEnabled (bridge1, 0, 100, true, 0);

		exchange_bpdus (bridge0, 0, bridge1, 0); // shouldn't crash
	}

	TEST_METHOD(test_designated_bridge_priority_on_msti)
	{
		test_bridge bridge (4, 4, 16, { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		STP_SetStpVersion (bridge, STP_VERSION_MSTP, 0);
		STP_SetBridgePriority (bridge, 1, 0x6000, 0);
		STP_StartBridge (bridge, 0);

		// First 8 bytes are RootId and for MSTIs must always be zero (see definition of PRIORITY_VECTOR in stp_base_types.h)
		unsigned char rpv[36];
		STP_GetRootPriorityVector(bridge, 1, rpv);
		uint64_t root_id;
		memcpy (&root_id, rpv, 8);
		Assert::AreEqual (0ull, root_id);
	}

	TEST_METHOD(BridgeTreeNotifiesWhenRootTimeChanges)
	{
		auto bridge = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto sink = CreateTestPropertyChangeSink(bridge->TreeAt(0));
		
		STP_StartBridge(bridge->stp_bridge(), 0);

		Assert::IsTrue(sink->ChangingCalled(dispidRootId));
		Assert::IsTrue(sink->ChangedCalled(dispidRootId));
		Assert::IsTrue(sink->ChangingCalled(dispidHelloTime));
		Assert::IsTrue(sink->ChangedCalled(dispidHelloTime));
	}
};
