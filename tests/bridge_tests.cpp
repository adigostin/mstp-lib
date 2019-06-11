
#include "pch.h"
#include "bridge.h"
#include "test_helpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

TEST_CLASS(bridge_tests)
{
	TEST_METHOD(create_bridge_test1)
	{
		uint32_t port_count = 4;
		uint32_t msti_count = 0;
		mac_address address = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };
		auto b = std::make_unique<bridge>(port_count, msti_count, address);
	}

	TEST_METHOD(disable_stp_test1)
	{
		uint32_t port_count = 4;
		uint32_t msti_count = 0;
		mac_address address = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };
		auto b = std::make_unique<bridge>(port_count, msti_count, address);

		auto get_root_bridge_id = [&b] { return b->trees()[0]->root_bridge_id(); };
		
		b->set_stp_enabled(true);
		get_root_bridge_id();

		b->set_stp_enabled(false);
		Assert::ExpectException<const std::logic_error&>(get_root_bridge_id);

		b->set_stp_enabled(true);
		get_root_bridge_id();
	}

	TEST_METHOD(undefined_role_test)
	{
		STP_PORT_ROLE role_from_callback = STP_PORT_ROLE_UNDEFINED;
		auto bridge = test_bridge(4, 0, 0, { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		bridge.port_role_changed = [&role_from_callback](size_t portIndex, size_t treeIndex, enum STP_PORT_ROLE role)
		{
			role_from_callback = role;
		};

		Assert::AreEqual (STP_PORT_ROLE_UNDEFINED, role_from_callback);

		STP_StartBridge(bridge, 0);

		Assert::AreEqual (STP_PORT_ROLE_DISABLED, role_from_callback);
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
		auto callbacks = default_callbacks();
		uint8_t address[] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };
		auto bridge = STP_CreateBridge (4, 4, 16, &callbacks, address, 2);
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
};
