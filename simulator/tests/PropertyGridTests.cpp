
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License 2.0.

#include "pch.h"
#include "simulator.h"
#include "test_helpers.h"
#include "pg/property_grid.h"
#include "edge/com.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

extern HRESULT selection_factory(IStpProject* project, ISelection** ppSelection);

namespace PG
{
	TEST_CLASS(PGTests)
	{
	public:
		TEST_METHOD(OperEdgePGValueChangesAfterAdminEdge)
		{
			HRESULT hr;

			auto project = MakeProject();
			auto bridge = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
			project->AddBridge(bridge);
			auto port = bridge->PortAt(0);

			STP_StartBridge(bridge->stp_bridge(), 0);
			STP_OnPortEnabled(bridge->stp_bridge(), 0, 100, true, 0);

			com_ptr<ISelection> selection;
			hr = selection_factory(project, &selection); Assert::AreEqual(S_OK, hr);
			com_ptr<IDispatch> portDispatch;
			hr = port->QueryInterface(IID_PPV_ARGS(&portDispatch)); Assert::AreEqual(S_OK, hr);
			selection->Select(portDispatch);

			com_ptr<pg::IPropertyGrid> propertyGrid;
			hr = MakePropertyGrid(nullptr, { }, nullptr, &propertyGrid); Assert::AreEqual(S_OK, hr);
			hr = propertyGrid->AddSection(selection, true, nullptr); Assert::AreEqual(S_OK, hr);

			pg::read_state state;
			wil::unique_bstr value;
			hr = propertyGrid->GetValueText(selection, portDispatch, dispidAdminEdge, &state, &value); Assert::AreEqual(S_OK, hr);
			Assert::AreEqual(L"False", value.get());
			hr = propertyGrid->GetValueText(selection, portDispatch, dispidOperEdge, &state, &value); Assert::AreEqual(S_OK, hr);
			Assert::AreEqual(L"False", value.get());

			STP_SetPortAdminEdge(bridge->stp_bridge(), 0, true, 100);
			hr = propertyGrid->GetValueText(selection, portDispatch, dispidAdminEdge, &state, &value); Assert::AreEqual(S_OK, hr);
			Assert::AreEqual(L"True", value.get());

			STP_OnOneSecondTick(bridge->stp_bridge(), 101);
			STP_OnOneSecondTick(bridge->stp_bridge(), 102);
			STP_OnOneSecondTick(bridge->stp_bridge(), 103);
			hr = propertyGrid->GetValueText(selection, portDispatch, dispidOperEdge, &state, &value); Assert::AreEqual(S_OK, hr);
			Assert::AreEqual(L"True", value.get());
		}

		TEST_METHOD(P2PPGValuesChangeAfterPortAndAdminP2PChanges)
		{
			HRESULT hr;

			auto project = MakeProject();
			auto bridge = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
			project->AddBridge(bridge);
			auto port = bridge->PortAt(0);

			STP_StartBridge(bridge->stp_bridge(), 0);

			com_ptr<ISelection> selection;
			hr = selection_factory(project, &selection); Assert::AreEqual(S_OK, hr);
			com_ptr<IDispatch> portDispatch;
			hr = port->QueryInterface(IID_PPV_ARGS(&portDispatch)); Assert::AreEqual(S_OK, hr);
			selection->Select(portDispatch);

			com_ptr<pg::IPropertyGrid> propertyGrid;
			hr = MakePropertyGrid(nullptr, { }, nullptr, &propertyGrid); Assert::AreEqual(S_OK, hr);
			hr = propertyGrid->AddSection(selection, true, nullptr); Assert::AreEqual(S_OK, hr);

			pg::read_state state;
			wil::unique_bstr value;
			hr = propertyGrid->GetValueText(selection, portDispatch, dispidPortDetectedP2P, &state, &value); Assert::AreEqual(S_OK, hr);
			Assert::AreEqual(L"False", value.get());
			hr = propertyGrid->GetValueText(selection, portDispatch, dispidPortOperP2P, &state, &value); Assert::AreEqual(S_OK, hr);
			Assert::AreEqual(L"False", value.get());
			hr = propertyGrid->GetValueText(selection, portDispatch, dispidPortAdminP2P, &state, &value); Assert::AreEqual(S_OK, hr);
			Assert::AreEqual(L"Auto", value.get());

			STP_OnPortEnabled(bridge->stp_bridge(), 0, 100, true, 100);
			hr = propertyGrid->GetValueText(selection, portDispatch, dispidPortDetectedP2P, &state, &value); Assert::AreEqual(S_OK, hr);
			Assert::AreEqual(L"True", value.get());

			STP_OnOneSecondTick(bridge->stp_bridge(), 101);
			STP_OnOneSecondTick(bridge->stp_bridge(), 102);
			STP_OnOneSecondTick(bridge->stp_bridge(), 103);
			hr = propertyGrid->GetValueText(selection, portDispatch, dispidPortOperP2P, &state, &value); Assert::AreEqual(S_OK, hr);
			Assert::AreEqual(L"True", value.get());

			STP_SetAdminPointToPointMAC(bridge->stp_bridge(), 0, STP_ADMIN_P2P_FORCE_FALSE, 104);
			hr = propertyGrid->GetValueText(selection, portDispatch, dispidPortAdminP2P, &state, &value); Assert::AreEqual(S_OK, hr);
			Assert::AreEqual(L"Force False", value.get());

			STP_OnOneSecondTick(bridge->stp_bridge(), 105);
			hr = propertyGrid->GetValueText(selection, portDispatch, dispidPortOperP2P, &state, &value); Assert::AreEqual(S_OK, hr);
			Assert::AreEqual(L"False", value.get());

			STP_SetAdminPointToPointMAC(bridge->stp_bridge(), 0, STP_ADMIN_P2P_FORCE_TRUE, 106);
			hr = propertyGrid->GetValueText(selection, portDispatch, dispidPortAdminP2P, &state, &value); Assert::AreEqual(S_OK, hr);
			Assert::AreEqual(L"Force True", value.get());

			STP_OnOneSecondTick(bridge->stp_bridge(), 107);
			hr = propertyGrid->GetValueText(selection, portDispatch, dispidPortOperP2P, &state, &value); Assert::AreEqual(S_OK, hr);
			Assert::AreEqual(L"True", value.get());
		}

		TEST_METHOD(PortMacOperationalChangesAfterConnectingWire)
		{
			HRESULT hr;

			auto project = MakeProject();
			auto bridge = MakeBridge(2, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
			project->AddBridge(bridge);
			auto port = bridge->PortAt(0);

			STP_StartBridge(bridge->stp_bridge(), 0);

			com_ptr<ISelection> selection;
			hr = selection_factory(project, &selection); Assert::AreEqual(S_OK, hr);
			com_ptr<IDispatch> portDispatch;
			hr = port->QueryInterface(IID_PPV_ARGS(&portDispatch)); Assert::AreEqual(S_OK, hr);
			selection->Select(portDispatch);

			com_ptr<pg::IPropertyGrid> propertyGrid;
			hr = MakePropertyGrid(nullptr, { }, nullptr, &propertyGrid); Assert::AreEqual(S_OK, hr);
			hr = propertyGrid->AddSection(selection, true, nullptr); Assert::AreEqual(S_OK, hr);

			pg::read_state state;
			wil::unique_bstr value;
			hr = propertyGrid->GetValueText(selection, portDispatch, dispidPortMacOperational, &state, &value);
			Assert::AreEqual(S_OK, hr);
			Assert::AreEqual(L"False", value.get());

			com_ptr<IWire> wire;
			hr = MakeWire(&wire); Assert::AreEqual(S_OK, hr);
			hr = project->AddWire(wire); Assert::AreEqual(S_OK, hr);

			wire->points()[0] = connected_wire_end{ port };
			wire->points()[1] = connected_wire_end{ bridge->PortAt(1) };

			bool macOperational = false;

			DWORD startTime = GetTickCount();
			while (true)
			{
				MSG msg;
				while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}

				hr = propertyGrid->GetValueText(selection, portDispatch, dispidPortMacOperational, &state, &value);
				Assert::AreEqual(S_OK, hr);
				if (!wcscmp(value.get(), L"True"))
					return;

				if (GetTickCount() - startTime >= 500)
					Assert::Fail();

				Sleep(10);
			}
		}

		TEST_METHOD(PartialSetWithoutChildObjectsDoesNotAssert)
		{
			HRESULT hr;

			auto bridge1 = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
			auto bridge2 = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x61 });
			auto objects = wil::com_ptr_failfast(new TestObjectList(
				bridge1.query<IDispatch>().get(), bridge2.query<IDispatch>().get()));

			com_ptr<pg::IPropertyGrid> propertyGrid;
			hr = MakePropertyGrid(nullptr, { }, nullptr, &propertyGrid); Assert::AreEqual(S_OK, hr);
			hr = propertyGrid->AddSection(objects, true, nullptr); Assert::AreEqual(S_OK, hr);

			// Notify Partial Set
			ObjectCollectionChangeArgs args = {
				.changeType = CollectionChangeType::Set,
				.setInsertRemoveArgs = { .index = 0, .count = 1 },
			};

			auto punkObjects = wil::com_query_failfast<IUnknown>(objects);

			hr = objects->GetEventSinks()->Notify([this, &punkObjects, &args](IObjectCollectionChangeEvents* sink) {
				return sink->OnCollectionChanging(punkObjects, &args);
			});
			Assert::AreEqual(S_OK, hr);	
			
			hr = objects->GetEventSinks()->Notify([this, &punkObjects, &args](IObjectCollectionChangeEvents* sink) {
				return sink->OnCollectionChanged(punkObjects, &args);
			});
			Assert::AreEqual(S_OK, hr);	
		}
	};
}
