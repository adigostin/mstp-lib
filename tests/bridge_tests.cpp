
#include "pch.h"
#include "bridge.h"
#include "test_helpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

TEST_CLASS(bridge_tests)
{
	#pragma region Default STP callbacks
	static void* StpCallback_AllocAndZeroMemory (unsigned int size)
	{
		void* res = malloc(size);
		memset (res, 0, size);
		return res;
	}

	static void StpCallback_FreeMemory (void* p)
	{
		free(p);
	}

	static void* StpCallback_TransmitGetBuffer (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp)
	{
		return nullptr;
	}

	static void StpCallback_TransmitReleaseBuffer (const STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer)
	{
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

	static void StpCallback_OnPortRoleChanged (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, STP_PORT_ROLE role, unsigned int timestamp)
	{
	}

	STP_CALLBACKS default_callbacks() const
	{
		STP_CALLBACKS callbacks;
		memset (&callbacks, 0, sizeof(callbacks));
		callbacks.allocAndZeroMemory = &StpCallback_AllocAndZeroMemory;
		callbacks.freeMemory         = &StpCallback_FreeMemory;
		callbacks.enableBpduTrapping = &StpCallback_EnableBpduTrapping;
		callbacks.enableLearning     = &StpCallback_EnableLearning;
		callbacks.enableForwarding   = &StpCallback_EnableForwarding;
		callbacks.onTopologyChange   = &StpCallback_OnTopologyChange;
		callbacks.flushFdb           = &StpCallback_FlushFdb;
		callbacks.onPortRoleChanged  = &StpCallback_OnPortRoleChanged;
		callbacks.debugStrOut        = &StpCallback_DebugStrOut;
		callbacks.transmitGetBuffer  = &StpCallback_TransmitGetBuffer;
		callbacks.transmitReleaseBuffer = &StpCallback_TransmitReleaseBuffer;
		return callbacks;
	}
	#pragma endregion

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

	TEST_METHOD(undefined_role_test)
	{
		STP_PORT_ROLE role = STP_PORT_ROLE_UNDEFINED;
		auto callbacks = default_callbacks();
		callbacks.onPortRoleChanged = [](const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_PORT_ROLE role, unsigned int timestamp)
		{
			STP_PORT_ROLE* role_ptr = static_cast<STP_PORT_ROLE*>(STP_GetApplicationContext(bridge));
			*role_ptr = role;
		};
		mac_address address = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };
		STP_BRIDGE* bridge = STP_CreateBridge(4, 0, 1, &callbacks, address.data(), 256);
		STP_SetApplicationContext(bridge, &role);

		Assert::AreEqual (STP_PORT_ROLE_UNDEFINED, role);

		STP_StartBridge(bridge, 0);

		Assert::AreEqual (STP_PORT_ROLE_DISABLED, role);
	}

	TEST_METHOD(receive_more_mstis_on_same_mst_config)
	{
		std::vector<unsigned char> buffer;

		auto transmitGetBuffer = [](const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp) -> void*
		{
			auto& buffer = *static_cast<std::vector<unsigned char>*>(STP_GetApplicationContext(bridge));
			buffer.resize(bpduSize);
			return buffer.data();
		};

		auto transmitReleaseBuffer = [](const struct STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer)
		{
		};

		uint32_t port_count = 4;
		uint32_t msti_count = 5;
		auto bridge0_callbacks = default_callbacks();
		bridge0_callbacks.transmitGetBuffer = transmitGetBuffer;
		bridge0_callbacks.transmitReleaseBuffer = transmitReleaseBuffer;
		const uint8_t bridge0_address[] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };
		auto bridge0 = STP_CreateBridge (port_count, msti_count, 16, &bridge0_callbacks, bridge0_address, 256);
		STP_SetApplicationContext (bridge0, &buffer);
		STP_SetStpVersion (bridge0, STP_VERSION_MSTP, 0);
		STP_SetMstConfigName (bridge0, "ABC", 0);
		STP_StartBridge (bridge0, 0);
		STP_OnPortEnabled (bridge0, 0, 100, true, 0);

		msti_count = 4;
		auto bridge1_callbacks = default_callbacks();
		const uint8_t bridge1_address[] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x70 };
		auto bridge1 = STP_CreateBridge (port_count, msti_count, 16, &bridge1_callbacks, bridge1_address, 256);
		STP_SetStpVersion (bridge1, STP_VERSION_MSTP, 0);
		STP_SetMstConfigName (bridge1, "ABC", 0);
		STP_StartBridge (bridge1, 0);
		STP_OnPortEnabled (bridge1, 0, 100, true, 0);
		STP_OnBpduReceived (bridge1, 0, buffer.data(), (unsigned int)buffer.size(), 0);

		STP_DestroyBridge (bridge0);
		STP_DestroyBridge (bridge1);
	}
};
