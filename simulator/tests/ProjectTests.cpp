
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "Simulator_.h"
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

extern HRESULT selection_factory(IStpProject* project, ISelection** ppSelection);

TEST_CLASS(ProjectTests)
{
public:
	TEST_METHOD(TestMethod1)
	{
		uint32_t port_count = 4;
		uint32_t msti_count = 0;
		com_ptr<IStpProject> p;
		auto hr = MakeProject(&p); Assert::AreEqual(S_OK, hr);
		p->AddBridge(MakeBridge(port_count, msti_count, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 }));
		p->AddBridge(MakeBridge(port_count, msti_count, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x70 }));
		p->AddBridge(MakeBridge(port_count, msti_count, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x80 }));

		auto w = MakeWire();
		w->set_p0(p->BridgeAt(1)->PortAt(2));
		w->set_p1(p->BridgeAt(2)->PortAt(3));
		p->AddWire(std::move(w));

		Assert::AreEqual (p->BridgeAt(1)->PortAt(2), std::get<connected_wire_end>(p->WireAt(0)->p0()));
		Assert::AreEqual (p->BridgeAt(2)->PortAt(3), std::get<connected_wire_end>(p->WireAt(0)->p1()));

		p->RemoveBridge(0);

		Assert::AreEqual (p->BridgeAt(0)->PortAt(2), std::get<connected_wire_end>(p->WireAt(0)->p0()));
		Assert::AreEqual (p->BridgeAt(1)->PortAt(3), std::get<connected_wire_end>(p->WireAt(0)->p1()));
	}

	TEST_METHOD(SaveLoadRoundTripPreservesNextBridgeAddress)
	{
		HRESULT hr;

		auto project1 = MakeProject();
		auto props1 = project1.query<IProjectProperties>();

		hr = props1->put_NextBridgeAddress(wil::make_bstr_failfast(L"00:AA:55:AA:55:99").get()); Assert::AreEqual(S_OK, hr);

		TempProjectFile file(L"project.xml");

		hr = project1->Save(file.path.c_str()); Assert::AreEqual(S_OK, hr);

		auto project2 = MakeProject();
		hr = project2->Load(file.path.c_str()); Assert::AreEqual(S_OK, hr);

		auto props2 = project2.query<IProjectProperties>();

		wil::unique_bstr savedAddress;
		hr = props1->get_NextBridgeAddress(&savedAddress); Assert::AreEqual(S_OK, hr);
		wil::unique_bstr loadedAddress;
		hr = props2->get_NextBridgeAddress(&loadedAddress); Assert::AreEqual(S_OK, hr);

		Assert::AreEqual(static_cast<const wchar_t*>(savedAddress.get()), static_cast<const wchar_t*>(loadedAddress.get()));
	}

	TEST_METHOD(SaveLoadRoundTripPreservesBridgeAddress)
	{
		HRESULT hr;

		auto project1 = MakeProject();
		project1->AddBridge(MakeBridge(4, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 }));

		TempProjectFile file(L"project.xml");

		hr = project1->Save(file.path.c_str()); Assert::AreEqual(S_OK, hr);

		auto project2 = MakeProject();
		hr = project2->Load(file.path.c_str()); Assert::AreEqual(S_OK, hr);

		auto bridgeProps1 = wil::com_query_failfast<IBridgeProperties>(project1->BridgeAt(0));
		auto bridgeProps2 = wil::com_query_failfast<IBridgeProperties>(project2->BridgeAt(0));

		wil::unique_bstr savedAddress;
		hr = bridgeProps1->get_BridgeAddress(&savedAddress); Assert::AreEqual(S_OK, hr);
		wil::unique_bstr loadedAddress;
		hr = bridgeProps2->get_BridgeAddress(&loadedAddress); Assert::AreEqual(S_OK, hr);

		Assert::AreEqual(static_cast<const wchar_t*>(savedAddress.get()), static_cast<const wchar_t*>(loadedAddress.get()));
	}

	TEST_METHOD(SaveLoadRoundTripPreservesPortsAndTrees)
	{
		HRESULT hr;

		auto project1 = MakeProject();
		auto bridge1 = MakeBridge(3, 2, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		wil::com_query_failfast<IPortProperties>(bridge1->PortAt(1))->put_Side(PortSide::Left);
		wil::com_query_failfast<IPortProperties>(bridge1->PortAt(1))->put_Offset(123);
		project1->AddBridge(bridge1);

		TempProjectFile file(L"project.stp");

		hr = project1->Save(file.path.c_str()); Assert::AreEqual(S_OK, hr);

		auto project2 = MakeProject();
		hr = project2->Load(file.path.c_str()); Assert::AreEqual(S_OK, hr);

		Assert::AreEqual(1ul, project2->BridgeCount());
		auto bridge2 = project2->BridgeAt(0);
		Assert::AreEqual<ULONG>(3, bridge2->PortCount());

		for (ULONG i = 0; i < bridge2->PortCount(); i++)
			Assert::AreEqual<uint32_t>(3, bridge2->PortAt(i)->treeCount());

		auto portProps = wil::com_query_failfast<IPortProperties>(bridge2->PortAt(1));
		PortSide side;
		hr = portProps->get_Side(&side); Assert::AreEqual(S_OK, hr);
		Assert::AreEqual(static_cast<int>(PortSide::Left), static_cast<int>(side));

		LONG offset;
		hr = portProps->get_Offset(&offset); Assert::AreEqual(S_OK, hr);
		Assert::AreEqual<DWORD>(123, offset);
	}

	TEST_METHOD(SaveLoadRoundTripPreservesWireEndpoints)
	{
		HRESULT hr;

		auto project = MakeProject();
		auto bridge = MakeBridge(4, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		project->AddBridge(bridge);

		auto wire1 = MakeWire();
		wire1->set_p0(static_cast<IPort*>(bridge->PortAt(2)));
		wire1->set_p1(POINT{ 123, -456 });
		project->AddWire(std::move(wire1));

		TempProjectFile file(L"project.stp");

		hr = project->Save(file.path.c_str()); Assert::AreEqual(S_OK, hr);

		auto project2 = MakeProject();
		hr = project2->Load(file.path.c_str()); Assert::AreEqual(S_OK, hr);

		Assert::AreEqual(1ul, project2->BridgeCount());
		Assert::AreEqual(1ul, project2->WireCount());

		auto wire2 = project2->WireAt(0);
		auto bridge2 = project2->BridgeAt(0);

		Assert::AreEqual(static_cast<IPort*>(bridge2->PortAt(2)), std::get<connected_wire_end>(wire2->p0()));
		Assert::IsTrue(std::holds_alternative<loose_wire_end>(wire2->p1()));
		auto loose = std::get<loose_wire_end>(wire2->p1());
		Assert::AreEqual(123l, (LONG)loose.x);
		Assert::AreEqual(-456l, (LONG)loose.y);
	}

	TEST_METHOD(PutBridgesIsAtomicWhenAChildFailsToQueryInterface)
	{
		HRESULT hr;

		auto project = MakeProject();
		auto validBridge = MakeBridge(4, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto invalidWire = MakeWire();

		auto psaItems = unique_safearray(SafeArrayCreateVector(VT_DISPATCH, 0, 2)); Assert::IsNotNull(psaItems.get());

		IDispatch* disp0 = wil::com_query_failfast<IDispatch>(validBridge);
		IDispatch* disp1 = wil::com_query_failfast<IDispatch>(invalidWire);
		LONG i = 0;
		hr = SafeArrayPutElement(psaItems.get(), &i, disp0); Assert::AreEqual(S_OK, hr);
		i = 1;
		hr = SafeArrayPutElement(psaItems.get(), &i, disp1); Assert::AreEqual(S_OK, hr);

		hr = project.query<IProjectProperties>()->put_Bridges(psaItems.get());
		Assert::IsFalse(SUCCEEDED(hr));
		Assert::AreEqual(0ul, project->BridgeCount());
	}

	TEST_METHOD(TreeSelectionUpdatesAfterVlanChange)
	{
		HRESULT hr;

		auto project = MakeProject();
		auto bridge = MakeBridge(1, 1, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		project->AddBridge(bridge);

		wil::com_ptr_failfast<ISelection> selection;
		hr = selection_factory(project, &selection);

		auto vlanSelection = new TestVlanSelection(1);

		wil::com_ptr_failfast<edge::IObjectList> treeSelection;
		hr = MakeTreeSelection(selection, vlanSelection, &treeSelection); Assert::AreEqual(S_OK, hr);
		hr = vlanSelection->SelectVlan(2); Assert::AreEqual(S_OK, hr);
		hr = vlanSelection->SelectVlan(1); Assert::AreEqual(S_OK, hr);

		selection->Select(bridge.query<IDispatch>());

		wil::unique_bstr title;
		hr = treeSelection->GetListTitle(&title); Assert::AreEqual(S_OK, hr);
		Assert::IsNotNull(wcsstr(title.get(), L"(VLAN 1)"));

		hr = vlanSelection->SelectVlan(2); Assert::AreEqual(S_OK, hr);
		title.reset();
		hr = treeSelection->GetListTitle(&title); Assert::AreEqual(S_OK, hr);
		Assert::IsNotNull(wcsstr(title.get(), L"(VLAN 2)"));

		hr = selection->Clear(); Assert::AreEqual(S_OK, hr);
		title.reset();
		hr = treeSelection->GetListTitle(&title); Assert::AreEqual(S_FALSE, hr);
		Assert::IsNull(title.get());
	}

	TEST_METHOD(TreeSelectionHandlesMultipleSelectionAtCreationAndDestruction)
	{
		HRESULT hr;

		auto project = MakeProject();
		auto bridge1 = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto bridge2 = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x61 });
		project->AddBridge(bridge1);
		project->AddBridge(bridge2);

		wil::com_ptr_failfast<ISelection> selection;
		hr = selection_factory(project, &selection); Assert::AreEqual(S_OK, hr);
		hr = selection->Add(bridge1.query<IDispatch>()); Assert::AreEqual(S_OK, hr);
		hr = selection->Add(bridge2.query<IDispatch>()); Assert::AreEqual(S_OK, hr);

		auto vlanSelection = wil::com_ptr_failfast(new TestVlanSelection(1));

		wil::com_ptr_failfast<edge::IObjectList> treeSelection;
		hr = MakeTreeSelection(selection, vlanSelection, &treeSelection); Assert::AreEqual(S_OK, hr);

		Assert::AreEqual(2u, treeSelection->size());
		Assert::IsTrue(wil::com_query_failfast<IDispatch>(bridge1->TreeAt(0)) == treeSelection->operator[](0));
		Assert::IsTrue(wil::com_query_failfast<IDispatch>(bridge2->TreeAt(0)) == treeSelection->operator[](1));

		ULONG refCount = treeSelection.detach()->Release();
		Assert::AreEqual(0ul, refCount);
	}

	TEST_METHOD(TreeSelectionUpdatesTwoPortsAfterVlanChange)
	{
		HRESULT hr;

		auto project = MakeProject();
		auto bridge = MakeBridge(2, 1, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		project->AddBridge(bridge);
		STP_SetStpVersion(bridge->stp_bridge(), STP_VERSION_MSTP, 0);
		STP_SetMstConfigTableEntry(bridge->stp_bridge(), 2, 1, 0);
		auto port1 = bridge->PortAt(0);
		auto port2 = bridge->PortAt(1);

		wil::com_ptr_failfast<ISelection> selection;
		hr = selection_factory(project, &selection); Assert::AreEqual(S_OK, hr);
		auto vlanSelection = wil::com_ptr_failfast(new TestVlanSelection(1));

		wil::com_ptr_failfast<edge::IObjectList> treeSelection;
		hr = MakeTreeSelection(selection, vlanSelection, &treeSelection); Assert::AreEqual(S_OK, hr);
		hr = selection->Add(wil::com_query_failfast<IDispatch>(port1)); Assert::AreEqual(S_OK, hr);
		hr = selection->Add(wil::com_query_failfast<IDispatch>(port2)); Assert::AreEqual(S_OK, hr);
		Assert::IsTrue(wil::com_query_failfast<IDispatch>(port1->treeAt(0)).get() == treeSelection->operator[](0));
		Assert::IsTrue(wil::com_query_failfast<IDispatch>(port2->treeAt(0)).get() == treeSelection->operator[](1));

		hr = vlanSelection->SelectVlan(2); Assert::AreEqual(S_OK, hr);
		Assert::IsTrue(wil::com_query_failfast<IDispatch>(port1->treeAt(1)).get() == treeSelection->operator[](0));
		Assert::IsTrue(wil::com_query_failfast<IDispatch>(port2->treeAt(1)).get() == treeSelection->operator[](1));

		hr = selection->Clear(); Assert::AreEqual(S_OK, hr);
	}

	TEST_METHOD(TreeSelectionUpdatesAfterStpVersionAndMstConfigChanges)
	{
		HRESULT hr;

		auto project = MakeProject();
		auto bridge = MakeBridge(1, 1, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		project->AddBridge(bridge);

		wil::com_ptr_failfast<ISelection> selection;
		hr = selection_factory(project, &selection); Assert::AreEqual(S_OK, hr);
		auto vlanSelection = wil::com_ptr_failfast(new TestVlanSelection(2));

		wil::com_ptr_failfast<edge::IObjectList> treeSelection;
		hr = MakeTreeSelection(selection, vlanSelection, &treeSelection); Assert::AreEqual(S_OK, hr);
		hr = selection->Select(bridge.query<IDispatch>()); Assert::AreEqual(S_OK, hr);

		STP_StartBridge(bridge->stp_bridge(), 0);
		STP_SetStpVersion(bridge->stp_bridge(), STP_VERSION_MSTP, 1);
		Assert::IsTrue(wil::com_query_failfast<IDispatch>(bridge->TreeAt(0)) == treeSelection->operator[](0));

		STP_SetMstConfigTableEntry(bridge->stp_bridge(), 2, 1, 2);
		Assert::IsTrue(wil::com_query_failfast<IDispatch>(bridge->TreeAt(1)) == treeSelection->operator[](0));

		STP_SetStpVersion(bridge->stp_bridge(), STP_VERSION_RSTP, 3);
		Assert::IsTrue(wil::com_query_failfast<IDispatch>(bridge->TreeAt(0)) == treeSelection->operator[](0));

		hr = selection->Clear(); Assert::AreEqual(S_OK, hr);
	}

	TEST_METHOD(TreeSelectionNotifiesBridgeSelectionChanges)
	{
		HRESULT hr;

		auto project = MakeProject();
		auto bridge1 = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto bridge2 = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x61 });
		project->AddBridge(bridge1);
		project->AddBridge(bridge2);

		wil::com_ptr_failfast<ISelection> selection;
		hr = selection_factory(project, &selection); Assert::AreEqual(S_OK, hr);

		wil::com_ptr_failfast<edge::IObjectList> treeSelection;
		hr = MakeTreeSelection(selection, nullptr, &treeSelection); Assert::AreEqual(S_OK, hr);
		auto events = wil::com_ptr_failfast(new TestObjectCollectionChangeEvents(treeSelection));

		auto bridge1Disp = bridge1.query<IDispatch>();
		auto bridge2Disp = bridge2.query<IDispatch>();

		hr = selection->Add(bridge1Disp); Assert::AreEqual(S_OK, hr);
		hr = selection->Add(bridge2Disp); Assert::AreEqual(S_OK, hr);

		Assert::AreEqual(2u, events->changingNotifications.size());
		Assert::AreEqual(2u, events->changedNotifications.size());
		for (ULONG i = 0; i < 2; i++)
		{
			const auto& changing = events->changingNotifications[i];
			Assert::IsTrue(CollectionChangeType::Insert == changing.changeType);
			Assert::AreEqual(i, changing.index);
			Assert::AreEqual(1ul, changing.count);
			Assert::AreEqual(1u, changing.childObjects.size());

			const auto& changed = events->changedNotifications[i];
			Assert::IsTrue(CollectionChangeType::Insert == changed.changeType);
			Assert::AreEqual(i, changed.index);
			Assert::AreEqual(1ul, changed.count);
		}
		Assert::IsTrue(events->changingNotifications[0].childObjects[0] == wil::com_query_failfast<IDispatch>(bridge1->TreeAt(0)));
		Assert::IsTrue(events->changingNotifications[1].childObjects[0] == wil::com_query_failfast<IDispatch>(bridge2->TreeAt(0)));

		hr = selection->Clear(); Assert::AreEqual(S_OK, hr);

		Assert::AreEqual(3u, events->changingNotifications.size());
		Assert::AreEqual(3u, events->changedNotifications.size());
		const auto& changing = events->changingNotifications.back();
		Assert::IsTrue(CollectionChangeType::Remove == changing.changeType);
		Assert::AreEqual(0ul, changing.index);
		Assert::AreEqual(2ul, changing.count);
		const auto& changed = events->changedNotifications.back();
		Assert::IsTrue(CollectionChangeType::Remove == changed.changeType);
		Assert::AreEqual(0ul, changed.index);
		Assert::AreEqual(2ul, changed.count);
		Assert::AreEqual(2u, changed.childObjects.size());
		Assert::IsTrue(changed.childObjects[0] == wil::com_query_failfast<IDispatch>(bridge1->TreeAt(0)));
		Assert::IsTrue(changed.childObjects[1] == wil::com_query_failfast<IDispatch>(bridge2->TreeAt(0)));
	}

	TEST_METHOD(IsWireForwardingFindsLoopAfterVisitedPort)
	{
		auto project = MakeProject();

		auto bridgeA = MakeBridge(3, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto bridgeB = MakeBridge(4, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x70 });
		auto bridgeC = MakeBridge(2, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x80 });
		auto deadEnd = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x90 });
		for (auto bridge : { bridgeA.get(), bridgeB.get(), bridgeC.get(), deadEnd.get() })
		{
			Assert::IsFalse(STP_IsBridgeStarted(bridge->stp_bridge()));
			project->AddBridge(bridge);
		}

		// +----------+                +----------+    +----------+
		// | bridgeA  |  queriedWire   | bridgeB  |    | bridgeC  |
		// |        2 +----------------+ 0      1 +----+ 0        |
		// |        1 +----------------+ 2      3 +----+ 1        |
		// |    0     |                +----------+    +----------+
		// +----+-----+
		//      |
		// +----+-----+
		// |    0     |
		// | deadEnd  |
		// +----------+
		auto queriedWire = ConnectPorts(project, bridgeA->PortAt(2), bridgeB->PortAt(0));
		ConnectPorts(project, bridgeB->PortAt(1), bridgeC->PortAt(0));
		ConnectPorts(project, bridgeC->PortAt(1), bridgeB->PortAt(3));
		ConnectPorts(project, bridgeB->PortAt(2), bridgeA->PortAt(1));
		ConnectPorts(project, bridgeA->PortAt(0), deadEnd->PortAt(0));

		// The B-C detour visits A's dead-end port before the search reaches A through
		// the second A-B wire. Skipping that visited port must still allow closing the loop at A:2.
		bool isPartOfLoop = false;
		Assert::IsTrue(project->IsWireForwarding(queriedWire, 1, &isPartOfLoop));
		Assert::IsTrue(isPartOfLoop, L"A visited port must not hide the loop through a later port.");
	}

	TEST_METHOD(IsWireForwardingDoesNotMarkWireJoiningSeparateCycles)
	{
		auto project = MakeProject();

		auto bridgeA = MakeBridge(2, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto bridgeB = MakeBridge(3, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x70 });
		auto bridgeC = MakeBridge(3, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x80 });
		auto bridgeD = MakeBridge(2, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x90 });
		for (auto bridge : { bridgeA.get(), bridgeB.get(), bridgeC.get(), bridgeD.get() })
		{
			Assert::IsFalse(STP_IsBridgeStarted(bridge->stp_bridge()));
			project->AddBridge(bridge);
		}

		// +----------+    +----------+                  +----------+    +----------+
		// | bridgeA  |    | bridgeB  |   queriedWire    | bridgeC  |    | bridgeD  |
		// |        0 +----+ 0      2 +------------------+ 2      0 +----+ 0        |
		// |        1 +----+ 1        |                  |        1 +----+ 1        |
		// +----------+    +----------+                  +----------+    +----------+
		ConnectPorts(project, bridgeA->PortAt(0), bridgeB->PortAt(0));
		ConnectPorts(project, bridgeA->PortAt(1), bridgeB->PortAt(1));
		ConnectPorts(project, bridgeC->PortAt(0), bridgeD->PortAt(0));
		ConnectPorts(project, bridgeC->PortAt(1), bridgeD->PortAt(1));
		auto queriedWire = ConnectPorts(project, bridgeB->PortAt(2), bridgeC->PortAt(2));

		for (ULONG i = 0; i < 4; i++)
		{
			bool isPartOfLoop = false;
			Assert::IsTrue(project->IsWireForwarding(project->WireAt(i), 1, &isPartOfLoop));
			Assert::IsTrue(isPartOfLoop, L"Each wire within a parallel pair belongs to a cycle.");
		}

		for (bool reverse : { false, true })
		{
			queriedWire->set_p0(reverse ? bridgeC->PortAt(2) : bridgeB->PortAt(2));
			queriedWire->set_p1(reverse ? bridgeB->PortAt(2) : bridgeC->PortAt(2));
			bool isPartOfLoop = true;
			Assert::IsTrue(project->IsWireForwarding(queriedWire, 1, &isPartOfLoop));
			Assert::IsFalse(isPartOfLoop, L"The only wire joining two separate cycles does not belong to a cycle.");
		}
	}

	TEST_METHOD(SaveLoadRoundTripPreservesMSTConfig)
	{
		HRESULT hr;

		auto project1 = MakeProject();
		auto bridge1 = MakeBridge(4, 3, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		project1->AddBridge(bridge1);
		STP_CONFIG_TABLE_ENTRY table[1 + max_vlan_number] = { };
		for (LONG i = 1; i <= max_vlan_number; i += 2)
			table[i].treeIndex = (BYTE)((i % 3) + 1);
		STP_SetMstConfigTable(bridge1->stp_bridge(), table, 1 + max_vlan_number, 0);

		TempProjectFile file(L"project.stp");
		hr = project1->Save(file.path.c_str()); Assert::AreEqual(S_OK, hr);

		auto project2 = MakeProject();
		hr = project2->Load(file.path.c_str()); Assert::AreEqual(S_OK, hr);
		auto bridge2 = project2->BridgeAt(0);
		const STP_MST_CONFIG_ID* configId1 = STP_GetMstConfigId(bridge1->stp_bridge());
		const STP_MST_CONFIG_ID* configId2 = STP_GetMstConfigId(bridge2->stp_bridge());
		Assert::IsTrue(memcmp(configId1, configId2, sizeof(*configId1)) == 0);
		unsigned int entryCount1;
		const STP_CONFIG_TABLE_ENTRY* table1 = STP_GetMstConfigTable(bridge1->stp_bridge(), &entryCount1);
		unsigned int entryCount2;
		const STP_CONFIG_TABLE_ENTRY* table2 = STP_GetMstConfigTable(bridge2->stp_bridge(), &entryCount2);
		Assert::AreEqual(entryCount1, entryCount2);
		for (unsigned int i = 0; i < entryCount1; i++)
			Assert::AreEqual(table1[i].treeIndex, table2[i].treeIndex);
	}
};
