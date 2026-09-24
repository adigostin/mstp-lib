
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "TestHelpers.h"
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

class STPPropertyChangedSink : public IStpPropertyChangeSink
{
	ULONG _refCount;
	WeakRefToThis _weakRefToThis;
	std::unordered_map<port_key, STP_PORT_ROLE, port_key_hash> _mostRecentRoles;
	vector_nothrow<AdviseSinkToken> _tokens;
	
public:
	STPPropertyChangedSink(std::initializer_list<IBridge*> bridges)
		: _refCount(1)
	{
		_weakRefToThis.InitInstance(AsUnknown());

		for (auto bridge : bridges)
		{
			AdviseSinkToken token;
			auto hr = AdviseSink<IStpPropertyChangeSink>(bridge, _weakRefToThis, &token); Assert::AreEqual(S_OK, hr);
			_tokens.try_push_back(std::move(token));
		}

		_refCount--;
	}

	STP_PORT_ROLE GetMostRecentRole(IBridge* bridge, unsigned portIndex) const
	{
		auto it = _mostRecentRoles.find({ bridge, portIndex });
		return it == _mostRecentRoles.end() ? STP_PORT_ROLE_UNDEFINED : it->second;
	}

	IUnknown* AsUnknown() { return static_cast<IStpPropertyChangeSink*>(this); }

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		if (TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IStpPropertyChangeSink>(this, riid, ppvObject))
			return S_OK;

		if (riid == __uuidof(IWeakRef))
			return _weakRefToThis.QueryIWeakRef(ppvObject);

		Assert::Fail();
		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }
	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }

	virtual HRESULT STDMETHODCALLTYPE OnStpPropertyChanging(IBridge*, unsigned int portIndex,
		unsigned int treeIndex, STP_PROPERTY prop, unsigned int timestamp) noexcept override
	{
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnStpPropertyChanged(IBridge* bridge, unsigned int portIndex,
		unsigned int treeIndex, STP_PROPERTY prop, unsigned int timestamp) noexcept override
	{
		if (prop == STP_PROP_PORT_ROLE)
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
		auto project = MakeProject();
		auto bridge0 = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto bridge1 = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x70 });
		project->AddBridge(bridge0);
		project->AddBridge(bridge1);

		auto wire = MakeWire();
		wire->set_p0(bridge0->PortAt(0));
		wire->set_p1(bridge1->PortAt(0));
		project->AddWire(std::move(wire));

		auto sink = wil::com_ptr_failfast(new STPPropertyChangedSink({ bridge0, bridge1 }));

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

		auto sink = wil::com_ptr_failfast(new STPPropertyChangedSink({ bridge0, bridge1 }));

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
		auto project = MakeProject();
		auto bridge = MakeBridge(2, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		project->AddBridge(bridge);

		auto wire = MakeWire();
		wire->set_p0(bridge->PortAt(0));
		wire->set_p1(bridge->PortAt(1));
		project->AddWire(std::move(wire));

		auto sink = wil::com_ptr_failfast(new STPPropertyChangedSink({ bridge }));

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

		auto sink = wil::com_ptr_failfast(new STPPropertyChangedSink({ bridge }));

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

	TEST_METHOD(TestPortRoleTransition_Undefined_Disabled)
	{
		auto bridge = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto sink = wil::com_ptr_failfast(new STPPropertyChangedSink({ bridge }));

		Assert::AreEqual(STP_PORT_ROLE_UNDEFINED, sink->GetMostRecentRole(bridge.get(), 0));
		STP_StartBridge(bridge->stp_bridge(), 0);
		Assert::AreEqual(STP_PORT_ROLE_DISABLED, sink->GetMostRecentRole(bridge.get(), 0));
	}

	TEST_METHOD(BpdusUseDifferentSourceAddressesForDifferentPorts)
	{
		struct BridgeEventsSink : IBridgeEvents
		{
			ULONG refCount = 1;
			WeakRefToThis weakRefToThis;
			AdviseSinkToken token;
			std::array<std::optional<mac_address>, 2> bpduSourceAddresses;

			BridgeEventsSink(IBridge* bridge)
			{
				weakRefToThis.InitInstance(static_cast<IBridgeEvents*>(this));
				AdviseSink<IBridgeEvents>(bridge, weakRefToThis, &token);
				refCount--;
			}

			HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
			{
				if (TryQI<IUnknown>(this, riid, ppvObject) || TryQI<IBridgeEvents>(this, riid, ppvObject))
					return S_OK;

				if (riid == __uuidof(IWeakRef))
					return weakRefToThis.QueryIWeakRef(ppvObject);

				Assert::Fail();
			}

			ULONG STDMETHODCALLTYPE AddRef() override { return ++refCount; }
			ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, refCount); }

			HRESULT STDMETHODCALLTYPE OnLogLineGenerated(IBridge*, const BridgeLogLine*) override { return S_OK; }
			HRESULT STDMETHODCALLTYPE OnLogCleared(IBridge*) override { return S_OK; }

			HRESULT STDMETHODCALLTYPE OnPacketTransmit(IBridge*, ULONG txPortIndex, packet_t&& packet) override
			{
				if (auto frame = std::get_if<frame_t>(&packet); frame && frame->data.size() >= 12)
				{
					mac_address sourceAddress;
					memcpy(sourceAddress.data(), frame->data.data() + 6, sourceAddress.size());
					bpduSourceAddresses[txPortIndex] = sourceAddress;
				}

				return S_OK;
			}
		};

		auto bridge = MakeBridge(2, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto sink = wil::com_ptr_failfast(new BridgeEventsSink(bridge));

		STP_StartBridge(bridge->stp_bridge(), 0);
		STP_OnPortEnabled(bridge->stp_bridge(), 0, 100, true, 0);
		STP_OnPortEnabled(bridge->stp_bridge(), 1, 100, true, 0);

		Assert::IsFalse(*sink->bpduSourceAddresses[0] == *sink->bpduSourceAddresses[1]);
	}

	TEST_METHOD(PortNotifiesBeforeAndAfterAdminEdgeChanges)
	{
		auto bridge = MakeBridge(1, 0, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		auto sink = CreateTestPropertyChangeSink(bridge->PortAt(0));

		STP_SetPortAdminEdge(bridge->stp_bridge(), 0, true, 0);

		Assert::IsTrue(sink->ChangingCalled(dispidAdminEdge));
		Assert::IsTrue(sink->ChangedCalled(dispidAdminEdge));
	}

	struct BridgeForPathCostValues
	{
		STP_BRIDGE* _stpBridge;

		std::optional<uint32_t> detectedBefore, detectedAfter;
		std::optional<uint32_t> externalBefore, externalAfter;
		std::optional<uint32_t> internalBefore, internalAfter;

		inline static const STP_CALLBACKS callbacks =
		{
			[](auto...) { },
			[](auto...) { },
			[](auto...) { },
			[](auto...) -> void* { return nullptr; },
			[](auto...) { },
			[](auto...) { },
			[](auto...) { },
			[](auto...) { },
			[](auto...) { },
			[](unsigned int size)
			{
				void* result = malloc(size);
				memset(result, 0, size);
				return result;
			},
			[](void* p) { free(p); },
		};

		BridgeForPathCostValues (unsigned portCount, unsigned mstiCount, uint16_t maxVlanNumber, const mac_address& address)
		{
			_stpBridge = STP_CreateBridge(portCount, mstiCount, maxVlanNumber, &callbacks, address.data(), 256);
			STP_SetApplicationContext(_stpBridge, this);
			STP_RegisterPropertyChangeCallbacks(_stpBridge, OnStpPropChanging, OnStpPropChanged);
		}

		~BridgeForPathCostValues()
		{
			STP_DestroyBridge(_stpBridge);
		}

		static void OnStpPropChanging(const STP_BRIDGE* stpb, unsigned int portIndex, unsigned int treeIndex, STP_PROPERTY prop, unsigned int)
		{
			auto b = static_cast<BridgeForPathCostValues*>(STP_GetApplicationContext(stpb));
			if (portIndex == 0)
			{
				if (prop == STP_PROP_DETECTED_PORT_PATH_COST)
					b->detectedBefore = STP_GetDetectedPortPathCost(stpb, 0);
				if (prop == STP_PROP_EXTERNAL_PORT_PATH_COST)
					b->externalBefore = STP_GetExternalPortPathCost(stpb, 0);
				if (prop == STP_PROP_INTERNAL_PORT_PATH_COST && treeIndex == CIST_INDEX)
					b->internalBefore = STP_GetInternalPortPathCost(stpb, 0, CIST_INDEX);
			}
		}

		static void OnStpPropChanged(const STP_BRIDGE* stpb, unsigned int portIndex, unsigned int treeIndex, STP_PROPERTY prop, unsigned int)
		{
			auto b = static_cast<BridgeForPathCostValues*>(STP_GetApplicationContext(stpb));
			if (portIndex == 0)
			{
				if (prop == STP_PROP_DETECTED_PORT_PATH_COST)
					b->detectedAfter = STP_GetDetectedPortPathCost(stpb, 0);
				if (prop == STP_PROP_EXTERNAL_PORT_PATH_COST)
					b->externalAfter = STP_GetExternalPortPathCost(stpb, 0);
				if (prop == STP_PROP_INTERNAL_PORT_PATH_COST && treeIndex == CIST_INDEX)
					b->internalAfter = STP_GetInternalPortPathCost(stpb, 0, CIST_INDEX);
			}
		}

		void AssertCostsZeroToNonzero()
		{
			Assert::AreEqual(0u, *detectedBefore);
			Assert::AreEqual(0u, *externalBefore);
			Assert::AreEqual(0u, *internalBefore);
			Assert::AreNotEqual(0u, *detectedAfter);
			Assert::AreNotEqual(0u, *externalAfter);
			Assert::AreNotEqual(0u, *internalAfter);
		}

		void AssertCostsNonzeroToZero()
		{
			Assert::AreNotEqual(0u, *detectedBefore);
			Assert::AreNotEqual(0u, *externalBefore);
			Assert::AreNotEqual(0u, *internalBefore);
			Assert::AreEqual(0u, *detectedAfter);
			Assert::AreEqual(0u, *externalAfter);
			Assert::AreEqual(0u, *internalAfter);
		}
	};

	TEST_METHOD(PortPathCostsPropChangeCallbacksOnPortEnableDisable)
	{
		BridgeForPathCostValues b (4, 4, 16, { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });

		STP_StartBridge(b._stpBridge, 0);

		STP_OnPortEnabled(b._stpBridge, 0, 100, true, 0);
		b.AssertCostsZeroToNonzero();

		STP_OnPortDisabled(b._stpBridge, 0, 0);
		b.AssertCostsNonzeroToZero();
	}

	TEST_METHOD(PortPathCostsPropChangeCallbacksOnBridgeStartStop)
	{
		BridgeForPathCostValues b (4, 4, 16, { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });

		STP_OnPortEnabled(b._stpBridge, 0, 100, true, 0);

		STP_StartBridge(b._stpBridge, 0);
		b.AssertCostsZeroToNonzero();

		STP_StopBridge(b._stpBridge, 0);
		b.AssertCostsNonzeroToZero();
	}

	TEST_METHOD(PortTransitionsToDesignatedForwardingOperEdgeAfterMigrateTime)
	{
		test_bridge bridge (1, 0, 16, { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		STP_OnPortEnabled (bridge, 0, 100, true, 0);
		STP_StartBridge (bridge, 0);

		Assert::IsFalse (STP_GetPortOperEdge (bridge, 0));

		// Table 13-5 - Timer and related parameter values
		unsigned MigrateTime = 3;
		for (unsigned i = 0; i < MigrateTime; i++)
			STP_OnOneSecondTick (bridge, 0);

		Assert::AreEqual(STP_PORT_ROLE_DESIGNATED, STP_GetPortRole(bridge, 0, CIST_INDEX));
		Assert::IsTrue (STP_GetPortForwarding (bridge, 0, 0));
		Assert::IsTrue (STP_GetPortOperEdge (bridge, 0));
	}
};
