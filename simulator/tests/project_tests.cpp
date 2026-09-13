
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "test_helpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

TEST_CLASS(project_tests)
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

		SAFEARRAYBOUND bound;
		bound.cElements = 2;
		bound.lLbound = 0;
		SAFEARRAY* psaItems = SafeArrayCreate(VT_DISPATCH, 1, &bound); Assert::IsNotNull(psaItems);
		auto cleanup = wil::scope_exit([psaItems] { SafeArrayDestroy(psaItems); });

		IDispatch* disp0 = wil::com_query_failfast<IDispatch>(validBridge);
		IDispatch* disp1 = wil::com_query_failfast<IDispatch>(invalidWire);
		LONG i = 0;
		hr = SafeArrayPutElement(psaItems, &i, disp0); Assert::AreEqual(S_OK, hr);
		i = 1;
		hr = SafeArrayPutElement(psaItems, &i, disp1); Assert::AreEqual(S_OK, hr);

		hr = project.query<IProjectProperties>()->put_Bridges(psaItems);
		Assert::IsFalse(SUCCEEDED(hr));
		Assert::AreEqual(0ul, project->BridgeCount());
	}
};
