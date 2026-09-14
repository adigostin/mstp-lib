
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "test_helpers.h"
#include "internal/stp_bridge.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using port_key = std::pair<IBridge*, unsigned>;

struct port_key_hash
{
	size_t operator()(const port_key& key) const
	{
		return std::hash<IBridge*>{}(key.first) ^ (std::hash<unsigned>{}(key.second) << 1);
	}
};

class STPPropertyChangedSink : public IStpPropertyChangedSink
{
	ULONG _refCount = 0;
	WeakRefToThis _weakRefToThis;
	std::unordered_map<port_key, STP_PORT_ROLE, port_key_hash> _mostRecentRoles;

public:
	STPPropertyChangedSink()
	{
		auto hr = _weakRefToThis.InitInstance(AsUnknown());
		Assert::AreEqual(S_OK, hr);
	}

	IWeakRef* GetWeakRef() { return _weakRefToThis; }

	STP_PORT_ROLE GetMostRecentRole(IBridge* bridge, unsigned portIndex) const
	{
		auto it = _mostRecentRoles.find({ bridge, portIndex });
		return it == _mostRecentRoles.end() ? STP_PORT_ROLE_UNDEFINED : it->second;
	}

	IUnknown* AsUnknown() { return static_cast<IStpPropertyChangedSink*>(this); }

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		if (TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IStpPropertyChangedSink>(this, riid, ppvObject))
			return S_OK;

		if (riid == __uuidof(IWeakRef))
			return _weakRefToThis.QueryIWeakRef(ppvObject);

		Assert::Fail();
		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }
	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }

	virtual HRESULT STDMETHODCALLTYPE OnStpPropertyChanged(IBridge* bridge, unsigned int portIndex,
		unsigned int treeIndex, STP_PROPERTY prop, unsigned int timestamp) noexcept override
	{
		if (prop == STP_PROPERTY_PORT_ROLE)
			_mostRecentRoles[{ bridge, portIndex }] = STP_GetPortRole(bridge->stp_bridge(), portIndex, treeIndex);
		return S_OK;
	}
};

template<typename predicate_t>
static void RunMessageLoopUntilCondition(predicate_t&& predicate)
{
	DWORD startTime = GetTickCount();
	while (!predicate())
	{
		if (GetTickCount() - startTime >= 500)
			Assert::Fail();

		MSG msg;
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		Sleep(20);
	}
}

TEST_CLASS(port_tests)
{
	TEST_METHOD(TestPortRoleTransition_Designated_Root)
	{
		HRESULT hr;
		auto project = MakeProject();
		auto bridge0 = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto bridge1 = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x70 });
		project->AddBridge(bridge0);
		project->AddBridge(bridge1);

		auto wire = MakeWire();
		wire->set_p0(bridge0->PortAt(0));
		wire->set_p1(bridge1->PortAt(0));
		project->AddWire(std::move(wire));

		auto sink = com_ptr(new STPPropertyChangedSink());

		AdviseSinkToken token0;
		hr = AdviseSink<IStpPropertyChangedSink>(bridge0, sink->GetWeakRef(), &token0); Assert::AreEqual(S_OK, hr);
		AdviseSinkToken token1;
		hr = AdviseSink<IStpPropertyChangedSink>(bridge1, sink->GetWeakRef(), &token1); Assert::AreEqual(S_OK, hr);

		STP_StartBridge(bridge0->stp_bridge(), 0);
		STP_StartBridge(bridge1->stp_bridge(), 0);

		RunMessageLoopUntilCondition([&]
			{
				return sink->GetMostRecentRole(bridge0.get(), 0) == STP_PORT_ROLE_DESIGNATED
					&& sink->GetMostRecentRole(bridge1.get(), 0) == STP_PORT_ROLE_ROOT;
			});
	}

	TEST_METHOD(TestPortRoleTransition_Alternate)
	{
		HRESULT hr;
		auto project = MakeProject();
		auto bridge0 = MakeBridge(2, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto bridge1 = MakeBridge(2, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x70 });
		project->AddBridge(bridge0);
		project->AddBridge(bridge1);

		auto wire0 = MakeWire();
		wire0->set_p0(bridge0->PortAt(0));
		wire0->set_p1(bridge1->PortAt(0));
		project->AddWire(std::move(wire0));
		auto wire1 = MakeWire();
		wire1->set_p0(bridge0->PortAt(1));
		wire1->set_p1(bridge1->PortAt(1));
		project->AddWire(std::move(wire1));

		auto sink = com_ptr(new STPPropertyChangedSink());
		AdviseSinkToken token0;
		hr = AdviseSink<IStpPropertyChangedSink>(bridge0, sink->GetWeakRef(), &token0); Assert::AreEqual(S_OK, hr);
		AdviseSinkToken token1;
		hr = AdviseSink<IStpPropertyChangedSink>(bridge1, sink->GetWeakRef(), &token1); Assert::AreEqual(S_OK, hr);

		STP_StartBridge(bridge0->stp_bridge(), 0);
		STP_StartBridge(bridge1->stp_bridge(), 0);

		RunMessageLoopUntilCondition([&]
			{
				return sink->GetMostRecentRole(bridge0.get(), 0) == STP_PORT_ROLE_DESIGNATED
					&& sink->GetMostRecentRole(bridge0.get(), 1) == STP_PORT_ROLE_DESIGNATED
					&& sink->GetMostRecentRole(bridge1.get(), 0) == STP_PORT_ROLE_ROOT
					&& sink->GetMostRecentRole(bridge1.get(), 1) == STP_PORT_ROLE_ALTERNATE;
			});
	}

	TEST_METHOD(TestPortRoleTransition_Backup)
	{
		HRESULT hr;
		auto project = MakeProject();
		auto bridge = MakeBridge(2, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		project->AddBridge(bridge);

		auto wire = MakeWire();
		wire->set_p0(bridge->PortAt(0));
		wire->set_p1(bridge->PortAt(1));
		project->AddWire(std::move(wire));

		auto sink = com_ptr(new STPPropertyChangedSink());
		AdviseSinkToken token;
		hr = AdviseSink<IStpPropertyChangedSink>(bridge, sink->GetWeakRef(), &token); Assert::AreEqual(S_OK, hr);

		STP_StartBridge(bridge->stp_bridge(), 0);

		RunMessageLoopUntilCondition([&]
			{
				return sink->GetMostRecentRole(bridge.get(), 0) == STP_PORT_ROLE_DESIGNATED
					&& sink->GetMostRecentRole(bridge.get(), 1) == STP_PORT_ROLE_BACKUP;
			});
	}

	TEST_METHOD(TestPortRoleTransition_Disabled)
	{
		HRESULT hr;
		auto project = MakeProject();
		auto bridge = MakeBridge(2, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		project->AddBridge(bridge);

		auto wire = MakeWire();
		wire->set_p0(bridge->PortAt(0));
		wire->set_p1(bridge->PortAt(1));
		project->AddWire(std::move(wire));

		auto sink = com_ptr(new STPPropertyChangedSink());
		AdviseSinkToken token;
		hr = AdviseSink<IStpPropertyChangedSink>(bridge, sink->GetWeakRef(), &token); Assert::AreEqual(S_OK, hr);

		STP_StartBridge(bridge->stp_bridge(), 0);

		RunMessageLoopUntilCondition([&]
			{
				return sink->GetMostRecentRole(bridge.get(), 0) == STP_PORT_ROLE_DESIGNATED
					&& sink->GetMostRecentRole(bridge.get(), 1) == STP_PORT_ROLE_BACKUP;
			});

		hr = project->RemoveWire(0); Assert::AreEqual(S_OK, hr);

		RunMessageLoopUntilCondition([&]
			{
				return sink->GetMostRecentRole(bridge.get(), 0) == STP_PORT_ROLE_DISABLED
					&& sink->GetMostRecentRole(bridge.get(), 1) == STP_PORT_ROLE_DISABLED;
			});
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
