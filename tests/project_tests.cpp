
#include "pch.h"
#include "CppUnitTest.h"
#include "bridge.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

TEST_CLASS(project_tests)
{
public:
	TEST_METHOD(TestMethod1)
	{
		uint32_t port_count = 4;
		uint32_t msti_count = 0;
		mac_address address = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };
		auto b = std::make_unique<bridge>(port_count, msti_count, address);
	}
};
