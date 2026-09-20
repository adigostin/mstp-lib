
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "stp.h"
#include "Simulator.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Microsoft::VisualStudio::CppUnitTestFramework
{
	template<>
	static inline std::wstring ToString(IPort* p)
	{
		return L"port";
	}

	template<>
	static inline std::wstring ToString (const STP_PORT_ROLE& role)
	{
		std::string_view str = STP_GetPortRoleString(role);
		return std::wstring (str.begin(), str.end());
	}
}

class test_bridge
{
	STP_BRIDGE* stp_bridge;

	static void* StpCallback_AllocAndZeroMemory (unsigned int size);
	static void  StpCallback_FreeMemory (void* p);
	static void* StpCallback_TransmitGetBuffer (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp);
	static void  StpCallback_TransmitReleaseBuffer (const STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer);
	static const STP_CALLBACKS callbacks;

	std::vector<uint8_t> tx_buffer;
	size_t tx_buffer_port_index;

public:
	test_bridge (size_t port_count, size_t msti_count, uint16_t max_vlan_number, const std::array<uint8_t, 6>& bridge_address);
	test_bridge (const test_bridge&) = delete;
	test_bridge& operator= (const test_bridge&) = delete;
	~test_bridge();

	operator STP_BRIDGE* () const { return stp_bridge; }

	using tx_queue = std::queue<std::vector<uint8_t>>;
	std::unordered_map<size_t, tx_queue> tx_queues;
};

bool exchange_bpdus (test_bridge& one, size_t one_port, test_bridge& other, size_t other_port);

class TestObjectCollectionChangeEvents : public IObjectCollectionChangeEvents
{
	ULONG _refCount = 1;
	WeakRefToThis _weakRefToThis;
	AdviseSinkToken _eventsToken;

public:
	struct CollectionChangeNotification
	{
		CollectionChangeType changeType;
		ULONG index;
		ULONG count;
		vector_nothrow<com_ptr<IDispatch>> childObjects;
	};

	vector_nothrow<CollectionChangeNotification> changingNotifications;
	vector_nothrow<CollectionChangeNotification> changedNotifications;

	TestObjectCollectionChangeEvents(edge::IObjectList* objectList)
	{
		auto hr = _weakRefToThis.InitInstance(static_cast<IObjectCollectionChangeEvents*>(this));
		Assert::AreEqual(S_OK, hr);
		hr = AdviseSink<IObjectCollectionChangeEvents>(objectList, _weakRefToThis, &_eventsToken);
		Assert::AreEqual(S_OK, hr);
		_refCount--;
	}

	void Unsubscribe() { _eventsToken.reset(); }

	#pragma region IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(static_cast<IObjectCollectionChangeEvents*>(this), riid, ppvObject)
			|| TryQI<IObjectCollectionChangeEvents>(this, riid, ppvObject))
			return S_OK;
		if (riid == __uuidof(IWeakRef))
			return _weakRefToThis.QueryIWeakRef(ppvObject);

		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }
	ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	HRESULT Record(const ObjectCollectionChangeArgs* args, vector_nothrow<CollectionChangeNotification>& notifications)
	{
		const auto& a = args->setInsertRemoveArgs;
		CollectionChangeNotification notification = { args->changeType, a.index, a.count };
		if (a.childObjs)
		{
			for (ULONG i = 0; i < a.count; i++)
				notification.childObjects.try_push_back(a.childObjs[i]);
		}
		notifications.try_push_back(std::move(notification));
		return S_OK;
	}

	#pragma region IObjectCollectionChangeEvents
	HRESULT STDMETHODCALLTYPE OnCollectionChanging(IUnknown*, const ObjectCollectionChangeArgs* args) override
	{
		return Record(args, changingNotifications);
	}

	HRESULT STDMETHODCALLTYPE OnCollectionChanged(IUnknown*, const ObjectCollectionChangeArgs* args) override
	{
		return Record(args, changedNotifications);
	}
	#pragma endregion
};

class TestVlanSelection : public IVlanSelection, IConnectionPointContainer
{
	ULONG _refCount = 1;
	DWORD _vlan;
	com_ptr<ConnectionPointImpl<IVlanSelectionEvents>> _events;

public:
	TestVlanSelection(DWORD vlan)
		: _vlan(vlan)
	{
		auto hr = MakeConnectionPoint(this, &_events);
		Assert::AreEqual(S_OK, hr);
		_refCount--;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		if (   TryQI<IUnknown>(static_cast<IVlanSelection*>(this), riid, ppvObject)
			|| TryQI<IVlanSelection>(this, riid, ppvObject)
			|| TryQI<IConnectionPointContainer>(this, riid, ppvObject))
			return S_OK;

		Assert::Fail();
	}

	ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }
	ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }

	HRESULT STDMETHODCALLTYPE SelectVlan(DWORD vlan) override
	{
		if (_vlan != vlan)
		{
			auto hr = _events->Notify([this](IVlanSelectionEvents* sink) { return sink->OnVlanSelectionChanging(_vlan); });
			Assert::AreEqual(S_OK, hr);

			_vlan = vlan;

			hr = _events->Notify([this](IVlanSelectionEvents* sink) { return sink->OnVlanSelectionChanged(_vlan); });
			Assert::AreEqual(S_OK, hr);
		}

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetSelectedVlan(DWORD* pdwVlan) override
	{
		*pdwVlan = _vlan;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE EnumConnectionPoints(IEnumConnectionPoints**) override { return E_NOTIMPL; }

	HRESULT STDMETHODCALLTYPE FindConnectionPoint(REFIID riid, IConnectionPoint** ppCP) override
	{
		if (riid == __uuidof(IVlanSelectionEvents))
		{
			auto hr = wil::com_query_to_nothrow(_events, ppCP);
			Assert::AreEqual(S_OK, hr);
			return S_OK;
		}

		Assert::Fail();
	}
};

inline wil::com_ptr_failfast<IBridge> MakeBridge (uint32_t portCount, uint32_t mstiCount, mac_address addr)
{
	com_ptr<IBridge> b;
	auto hr = MakeBridge(portCount, mstiCount, addr, &b); Assert::AreEqual(S_OK, hr);
	return b;
}

inline wil::com_ptr_failfast<IWire> MakeWire()
{
	com_ptr<IWire> w;
	auto hr = MakeWire(&w); Assert::AreEqual(S_OK, hr);
	return w;
}

inline wil::com_ptr_failfast<IStpProject> MakeProject()
{
	wil::com_ptr_failfast<IStpProject> p;
	auto hr = MakeProject(&p); Assert::AreEqual(S_OK, hr);
	return p;
}

struct TempProjectFile
{
	std::wstring folder;
	std::wstring path;

	TempProjectFile(const wchar_t* fileName)
	{
		wchar_t tempPath[MAX_PATH];
		DWORD cch = GetTempPathW(_countof(tempPath), tempPath);
		Assert::IsTrue(cch > 0 && cch < _countof(tempPath));

		folder = tempPath;
		folder += L"mstp-lib-project-tests-";
		folder += std::to_wstring(GetCurrentProcessId());
		folder += L"-";
		folder += std::to_wstring(GetTickCount64());
		path = folder + L"\\" + fileName;

		BOOL ok = CreateDirectoryW(folder.c_str(), nullptr);
		if (!ok)
		{
			DWORD err = GetLastError();
			Assert::AreEqual((DWORD)ERROR_ALREADY_EXISTS, err);
		}
	}

	~TempProjectFile()
	{
		DeleteFileW(path.c_str());
		RemoveDirectoryW(folder.c_str());
	}
};

class TestObjectList : public edge::IObjectList, public IConnectionPointContainer
{
	ULONG _refCount = 1;
	vector_nothrow<com_ptr<IDispatch>> _objects;
	com_ptr<ConnectionPointImpl<IObjectCollectionChangeEvents>> _events;

public:
	TestObjectList(IDispatch* first, IDispatch* second)
	{
		_objects.try_push_back(first);
		_objects.try_push_back(second);
		auto hr = MakeConnectionPoint(this, &_events);
		Assert::AreEqual(S_OK, hr);
		_refCount--;
	}

	#pragma region IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(static_cast<edge::IObjectList*>(this), riid, ppvObject)
			|| TryQI<edge::IObjectList>(this, riid, ppvObject)
			|| TryQI<IConnectionPointContainer>(this, riid, ppvObject))
			return S_OK;

		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }
	ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	#pragma region IObjectList
	uint32_t size() const override { return _objects.size(); }
	IDispatch* operator[](uint32_t index) const override { return _objects[index]; }
	HRESULT STDMETHODCALLTYPE GetListTitle(BSTR* pbstrTitle) noexcept override
	{
		*pbstrTitle = SysAllocString(L"Test objects");
		return S_OK;
	}
	#pragma endregion

	#pragma region IConnectionPointContainer
	HRESULT STDMETHODCALLTYPE EnumConnectionPoints(IEnumConnectionPoints**) override { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE FindConnectionPoint(REFIID riid, IConnectionPoint** ppCP) override
	{
		if (riid == __uuidof(IObjectCollectionChangeEvents))
			return wil::com_query_to_nothrow(_events, ppCP);

		return E_NOINTERFACE;
	}
	#pragma endregion

	ConnectionPointImpl<IObjectCollectionChangeEvents>* GetEventSinks() { return _events.get(); }
};
