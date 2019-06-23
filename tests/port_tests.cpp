
#include "pch.h"
#include "bridge.h"
#include "test_helpers.h"
#include "internal/stp_bridge.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

TEST_CLASS(port_tests)
{
	TEST_METHOD(test_port_roles)
	{
		test_bridge bridge0 (4, 0, 0, { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		STP_StartBridge (bridge0, 0);

		test_bridge bridge1 (4, 0, 0, { 0x10, 0x20, 0x30, 0x40, 0x50, 0x70 });
		STP_StartBridge (bridge1, 0);

		// Let's connect a 100Mbps cable between the first port of each bridge...
		STP_OnPortEnabled (bridge0, 0, 100, true, 0);
		STP_OnPortEnabled (bridge1, 0, 100, true, 0);
		// ... and another 100Mbps cable between their second ports.
		STP_OnPortEnabled (bridge0, 1, 100, true, 0);
		STP_OnPortEnabled (bridge1, 1, 100, true, 0);
		// Let BPDUs pass through.
		while (exchange_bpdus(bridge0, 0, bridge1, 0) || exchange_bpdus(bridge0, 1, bridge1, 1))
			;
		// And check the port roles.
		Assert::AreEqual (STP_PORT_ROLE_DESIGNATED, STP_GetPortRole(bridge0, 0, 0));
		Assert::AreEqual (STP_PORT_ROLE_DESIGNATED, STP_GetPortRole(bridge0, 1, 0));
		Assert::AreEqual (STP_PORT_ROLE_ROOT,       STP_GetPortRole(bridge1, 0, 0));
		Assert::AreEqual (STP_PORT_ROLE_ALTERNATE,  STP_GetPortRole(bridge1, 1, 0));
	}

	void test_port_path_cost (bool internal)
	{
		constexpr size_t port_count = 4;
		constexpr size_t msti_count = 4;
		test_bridge bridge0 (port_count, msti_count, 16, { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		STP_SetStpVersion (bridge0, STP_VERSION_MSTP, 0);
		STP_StartBridge (bridge0, 0);

		test_bridge bridge1 (port_count, msti_count, 16, { 0x10, 0x20, 0x30, 0x40, 0x50, 0x70 });
		STP_SetStpVersion (bridge1, STP_VERSION_MSTP, 0);
		STP_StartBridge (bridge1, 0);

		// STP_CreateBridge initializes the MST Config Name to a string generated from the MAC Address of the bridge.
		// If we keep this default, we'll be testing the External port path cost.
		// If we set the same MST Config Name to both our bridges, we'll be testing the Internal port path cost.
		if (internal)
		{
			STP_SetMstConfigName (bridge0, "ABC", 0);
			STP_SetMstConfigName (bridge1, "ABC", 0);
		}

		// ----------------------------------------------------------------
		// Let's connect a 100Mbps cable between the first port of each bridge...
		STP_OnPortEnabled (bridge0, 0, 100, true, 0);
		STP_OnPortEnabled (bridge0, 1, 100, true, 0);
		// ... and another 100Mbps cable between their second ports.
		STP_OnPortEnabled (bridge1, 0, 100, true, 0);
		STP_OnPortEnabled (bridge1, 1, 100, true, 0);
		// Let BPDUs pass through.
		while (exchange_bpdus(bridge0, 0, bridge1, 0) || exchange_bpdus(bridge0, 1, bridge1, 1))
			;
		// And check the port roles.
		Assert::AreEqual (STP_PORT_ROLE_DESIGNATED, STP_GetPortRole(bridge0, 0, CIST_INDEX));
		Assert::AreEqual (STP_PORT_ROLE_DESIGNATED, STP_GetPortRole(bridge0, 1, CIST_INDEX));
		Assert::AreEqual (STP_PORT_ROLE_ROOT,       STP_GetPortRole(bridge1, 0, CIST_INDEX));
		Assert::AreEqual (STP_PORT_ROLE_ALTERNATE,  STP_GetPortRole(bridge1, 1, CIST_INDEX));

		// ----------------------------------------------------------------
		// Take out the second cable and check the port roles.
		STP_OnPortDisabled (bridge0, 1, 0);
		STP_OnPortDisabled (bridge1, 1, 0);
		while (exchange_bpdus(bridge0, 0, bridge1, 0))
			;
		Assert::AreEqual (STP_PORT_ROLE_DESIGNATED, STP_GetPortRole(bridge0, 0, CIST_INDEX));
		Assert::AreEqual (STP_PORT_ROLE_DISABLED,   STP_GetPortRole(bridge0, 1, CIST_INDEX));
		Assert::AreEqual (STP_PORT_ROLE_ROOT,       STP_GetPortRole(bridge1, 0, CIST_INDEX));
		Assert::AreEqual (STP_PORT_ROLE_DISABLED,   STP_GetPortRole(bridge1, 1, CIST_INDEX));

		// ----------------------------------------------------------------
		// And put back a 1Gbit cable. Now we have 100Mbps between ports 0, and 1Gbit between ports 1.
		STP_OnPortEnabled (bridge0, 1, 1000, true, 0);
		STP_OnPortEnabled (bridge1, 1, 1000, true, 0);
		while (exchange_bpdus(bridge0, 0, bridge1, 0) || exchange_bpdus(bridge0, 1, bridge1, 1))
			;
		// Now the second cable should be forwarding since it's faster.
		Assert::AreEqual (STP_PORT_ROLE_DESIGNATED, STP_GetPortRole(bridge0, 0, CIST_INDEX));
		Assert::AreEqual (STP_PORT_ROLE_DESIGNATED, STP_GetPortRole(bridge0, 1, CIST_INDEX));
		Assert::AreEqual (STP_PORT_ROLE_ALTERNATE,  STP_GetPortRole(bridge1, 0, CIST_INDEX));
		Assert::AreEqual (STP_PORT_ROLE_ROOT,       STP_GetPortRole(bridge1, 1, CIST_INDEX));

		// ----------------------------------------------------------------
		// On the first port of second bridge, force the port path cost to half of that at 1GB; the port should become a Root Port.
		if (internal)
		{
			auto cost_at_1gbit = STP_GetInternalPortPathCost (bridge1, 1, CIST_INDEX);
			STP_SetAdminInternalPortPathCost (bridge1, 0, CIST_INDEX, cost_at_1gbit / 2, 0);
		}
		else
		{
			auto cost_at_1gbit = STP_GetExternalPortPathCost (bridge1, 1);
			STP_SetAdminExternalPortPathCost (bridge1, 0, cost_at_1gbit / 2, 0);
		}

		while (exchange_bpdus(bridge0, 0, bridge1, 0) || exchange_bpdus(bridge0, 1, bridge1, 1))
			;
		// Now the first cable should be forwarding since the first port of second bridge has the lowest cost.
		Assert::AreEqual (STP_PORT_ROLE_DESIGNATED, STP_GetPortRole(bridge0, 0, CIST_INDEX));
		Assert::AreEqual (STP_PORT_ROLE_DESIGNATED, STP_GetPortRole(bridge0, 1, CIST_INDEX));
		Assert::AreEqual (STP_PORT_ROLE_ROOT,       STP_GetPortRole(bridge1, 0, CIST_INDEX));
		Assert::AreEqual (STP_PORT_ROLE_ALTERNATE,  STP_GetPortRole(bridge1, 1, CIST_INDEX));

		// ----------------------------------------------------------------

		volatile int a = 0;
	}

	TEST_METHOD(test_external_port_path_cost)
	{
		test_port_path_cost(false);
	}

	TEST_METHOD(test_internal_port_path_cost)
	{
		test_port_path_cost(true);
	}
};
