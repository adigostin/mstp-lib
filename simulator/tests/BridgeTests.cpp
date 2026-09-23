
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "Simulator.h"
#include "TestHelpers.h"

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

		STP_StartBridge(b->stp_bridge(), 0);
		get_root_bridge_id();

		STP_StopBridge(b->stp_bridge(), 0);
		Assert::ExpectException<const std::logic_error&>(get_root_bridge_id);

		STP_StartBridge(b->stp_bridge(), 0);
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

	TEST_METHOD(BridgeNotifiesWhenMSTConfigTableChanges)
	{
		HRESULT hr;

		auto bridge = MakeBridge(1, 1, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto sink = CreateTestPropertyChangeSink(bridge.get());
		auto bridgeProperties = wil::com_query_failfast<IBridgeProperties>(bridge);
		com_ptr<IMSTConfigProperties> mstConfig;
		hr = bridgeProperties->get_MSTConfig(&mstConfig); Assert::AreEqual(S_OK, hr);

		unique_safearray values;
		hr = mstConfig->get_Values(&values); Assert::AreEqual(S_OK, hr);
		LONG index = 1;
		BYTE value = 1;
		hr = SafeArrayPutElement(values.get(), &index, &value); Assert::AreEqual(S_OK, hr);

		hr = mstConfig->put_Values(values.get()); Assert::AreEqual(S_OK, hr);

		Assert::IsTrue(sink->ChangingCalled(dispidMstConfigTable));
		Assert::IsTrue(sink->ChangedCalled(dispidMstConfigTable));
	}

	TEST_METHOD(BridgeNotifiesWhenMSTConfigNameChanges)
	{
		auto bridge = MakeBridge(1, 1, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto sink = CreateTestPropertyChangeSink(bridge.get());
		auto bridgeProperties = wil::com_query_failfast<IBridgeProperties>(bridge);
		com_ptr<IMSTConfigProperties> mstConfig;
		auto hr = bridgeProperties->get_MSTConfig(&mstConfig); Assert::AreEqual(S_OK, hr);

		hr = mstConfig->put_Name(wil::make_bstr_failfast(L"Changed").get()); Assert::AreEqual(S_OK, hr);
		Assert::IsTrue(sink->ChangingCalled(dispidMstConfigName));
		Assert::IsTrue(sink->ChangedCalled(dispidMstConfigName));
	}

	TEST_METHOD(BridgeNotifiesWhenMSTConfigRevisionLevelChanges)
	{
		auto bridge = MakeBridge(1, 1, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto sink = CreateTestPropertyChangeSink(bridge.get());
		auto bridgeProperties = wil::com_query_failfast<IBridgeProperties>(bridge);
		com_ptr<IMSTConfigProperties> mstConfig;
		auto hr = bridgeProperties->get_MSTConfig(&mstConfig); Assert::AreEqual(S_OK, hr);

		hr = mstConfig->put_RevisionLevel(1); Assert::AreEqual(S_OK, hr);
		Assert::IsTrue(sink->ChangingCalled(dispidMstConfigRevLevel));
		Assert::IsTrue(sink->ChangedCalled(dispidMstConfigRevLevel));
	}

	TEST_METHOD(BridgeNotifiesWhenStpStarts)
	{
		auto bridge = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto sink = CreateTestPropertyChangeSink(bridge.get());

		STP_StartBridge(bridge->stp_bridge(), 0);

		Assert::IsTrue(sink->ChangingCalled(dispidStpEnabled));
		Assert::IsTrue(sink->ChangedCalled(dispidStpEnabled));
	}

	TEST_METHOD(PortNotifiesWhenOperEdgeChanges)
	{
		auto bridge = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto sink = CreateTestPropertyChangeSink(bridge->PortAt(0));

		STP_SetPortAdminEdge(bridge->stp_bridge(), 0, true, 0);
		STP_StartBridge(bridge->stp_bridge(), 0);

		Assert::IsTrue(sink->ChangingCalled(dispidOperEdge));
		Assert::IsTrue(sink->ChangedCalled(dispidOperEdge));
	}

	TEST_METHOD(PortTreeNotifiesWhenRoleChanges)
	{
		auto bridge = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto sink = CreateTestPropertyChangeSink(bridge->PortAt(0)->treeAt(0));

		STP_StartBridge(bridge->stp_bridge(), 0);

		Assert::IsTrue(sink->ChangingCalled(dispidPortRole));
		Assert::IsTrue(sink->ChangedCalled(dispidPortRole));
	}

	TEST_METHOD(PortNotifiesWhenDetectedP2PChanges)
	{
		auto bridge = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto sink = CreateTestPropertyChangeSink(bridge->PortAt(0));

		STP_OnPortEnabled(bridge->stp_bridge(), 0, 100, true, 0);

		Assert::IsTrue(sink->ChangingCalled(dispidPortDetectedP2P));
		Assert::IsTrue(sink->ChangedCalled(dispidPortDetectedP2P));
	}

	TEST_METHOD(PortNotifiesWhenOperP2PChanges)
	{
		auto bridge = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto sink = CreateTestPropertyChangeSink(bridge->PortAt(0));

		STP_OnPortEnabled(bridge->stp_bridge(), 0, 100, true, 0);

		Assert::IsTrue(sink->ChangingCalled(dispidPortOperP2P));
		Assert::IsTrue(sink->ChangedCalled(dispidPortOperP2P));
	}

	TEST_METHOD(PortNotifiesWhenAdminP2PChanges)
	{
		auto bridge = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto sink = CreateTestPropertyChangeSink(bridge->PortAt(0));

		STP_SetAdminPointToPointMAC(bridge->stp_bridge(), 0, STP_ADMIN_P2P_FORCE_TRUE, 0);

		Assert::IsTrue(sink->ChangingCalled(dispidPortAdminP2P));
		Assert::IsTrue(sink->ChangedCalled(dispidPortAdminP2P));
	}
};
