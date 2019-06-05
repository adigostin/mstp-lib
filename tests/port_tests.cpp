
#include "pch.h"
#include "bridge.h"
#include "test_helpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

TEST_CLASS(port_tests)
{
	TEST_METHOD(test_port_roles)
	{
		test_bridge bridge0 (4, 0, 0, { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		STP_StartBridge (bridge0, 0);
		STP_OnPortEnabled (bridge0, 0, 100, true, 0);
		STP_OnPortEnabled (bridge0, 1, 100, true, 0);

		test_bridge bridge1 (4, 0, 0, { 0x10, 0x20, 0x30, 0x40, 0x50, 0x70 });
		STP_StartBridge (bridge1, 0);
		STP_OnPortEnabled (bridge1, 0, 100, true, 0);
		STP_OnPortEnabled (bridge1, 1, 100, true, 0);

		while (exchange_bpdus(bridge0, 0, bridge1, 0) || exchange_bpdus(bridge0, 1, bridge1, 1))
			;

		Assert::AreEqual (STP_PORT_ROLE_DESIGNATED, STP_GetPortRole(bridge0, 0, 0));
		Assert::AreEqual (STP_PORT_ROLE_DESIGNATED, STP_GetPortRole(bridge0, 1, 0));
		Assert::AreEqual (STP_PORT_ROLE_ROOT, STP_GetPortRole(bridge1, 0, 0));
		Assert::AreEqual (STP_PORT_ROLE_ALTERNATE, STP_GetPortRole(bridge1, 1, 0));

		volatile int a = 0;
	}

	TEST_METHOD(test_internal_port_path_cost)
	{
		constexpr size_t port_count = 4;
		constexpr size_t msti_count = 4;
		test_bridge bridge0 (port_count, msti_count, 16, { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		STP_SetStpVersion (bridge0, STP_VERSION_MSTP, 0);
		STP_SetMstConfigName (bridge0, "ABC", 0);
		STP_StartBridge (bridge0, 0);
		STP_OnPortEnabled (bridge0, 0, 100, true, 0);
		STP_OnPortEnabled (bridge0, 1, 100, true, 0);

		test_bridge bridge1 (port_count, msti_count, 16, { 0x10, 0x20, 0x30, 0x40, 0x50, 0x70 });
		STP_SetStpVersion (bridge1, STP_VERSION_MSTP, 0);
		STP_SetMstConfigName (bridge1, "ABC", 0);
		STP_StartBridge (bridge1, 0);
		STP_OnPortEnabled (bridge1, 0, 100, true, 0);
		STP_OnPortEnabled (bridge1, 1, 100, true, 0);

		auto exchange_bpdus = [](test_bridge& one, size_t one_port, test_bridge& other, size_t other_port)
		{
			bool exchanged = false;
			while (true)
			{
				if (!one.tx_queues[one_port].empty())
				{
					auto bpdu = std::move(one.tx_queues[one_port].back());
					one.tx_queues[one_port].pop();
					STP_OnBpduReceived (other, other_port, bpdu.data(), (unsigned int) bpdu.size(), 0);
					exchanged = true;
				}
				else if (!other.tx_queues[other_port].empty())
				{
					auto bpdu = std::move(other.tx_queues[other_port].back());
					other.tx_queues[other_port].pop();
					STP_OnBpduReceived (one, one_port, bpdu.data(), (unsigned int) bpdu.size(), 0);
					exchanged = true;
				}
				else
					break;
			}
			return exchanged;
		};

		while (exchange_bpdus(bridge0, 0, bridge1, 0) || exchange_bpdus(bridge0, 1, bridge1, 1))
			;

		Assert::AreEqual (STP_PORT_ROLE_DESIGNATED, STP_GetPortRole(bridge0, 0, 0));
		Assert::AreEqual (STP_PORT_ROLE_DESIGNATED, STP_GetPortRole(bridge0, 1, 0));
		Assert::AreEqual (STP_PORT_ROLE_ROOT, STP_GetPortRole(bridge1, 0, 0));
		Assert::AreEqual (STP_PORT_ROLE_ALTERNATE, STP_GetPortRole(bridge1, 1, 0));

		volatile int a = 0;
	}
};
