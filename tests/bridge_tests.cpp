
#include "pch.h"
#include "CppUnitTest.h"
#include "bridge.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

TEST_CLASS(bridge_tests)
{
public:
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
};
