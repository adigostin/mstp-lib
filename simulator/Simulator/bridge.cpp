
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "edge/Z80Xml.h"

using namespace D2D1;

static constexpr UINT WM_PACKET_RECEIVED = WM_APP + 1;

static constexpr uint8_t BpduDestAddress[6] = { 1, 0x80, 0xC2, 0, 0, 0 };

class BridgeImpl : public IBridge, IBridgeProperties, IConnectionPointContainer, IXmlParent, IInvalidateSink
{
	ULONG _refCount = 0;
	WeakRefToThis _weakRefToThis;
	IStpProject* _project = nullptr;
	LONG _x;
	LONG _y;
	LONG _width;
	LONG _height;

	struct PortAndTokens
	{
		com_ptr<IPort> port;
		AdviseSinkToken invalidateToken;
	};
	vector_nothrow<PortAndTokens> _ports;
	STP_BRIDGE* _stpBridge = nullptr;
	bool _bpdu_trapping_enabled = false;
	std::vector<std::unique_ptr<BridgeLogLine>> _logLines;
	BridgeLogLine _currentLogLine;
	std::queue<std::pair<uint32_t, packet_t>> _rxQueue;
	vector_nothrow<com_ptr<IBridgeTree>> _trees;
	bool _deserializing = false;
	bool _enable_stp_after_deserialize;
	vector_nothrow<uint8_t> _missedLinkPulseCounters;
	static constexpr uint8_t MissedLinkPulseCounterMax = 3;

	// Let's keep things simple and do everything on the GUI thread.
	static inline UINT_PTR _link_pulse_timer_id;
	static inline UINT_PTR _one_second_timer_id;
	static inline std::unordered_set<BridgeImpl*> _created_bridges;
	HWND _helper_window = nullptr;

	// variables used by TransmitGetBuffer/ReleaseBuffer
	std::vector<uint8_t> _txPacketData;
	IPort*               _txTransmittingPort;
	unsigned int         _txTimestamp;

	static constexpr wchar_t helper_window_class_name[] = L"{C2A14267-93FD-44FD-89A3-809FBB66A20B}";

	com_ptr<ConnectionPointImpl<IPropertyChangeSink>> _propChangeCP;
	com_ptr<ConnectionPointImpl<IInvalidateSink>> _invalidateCP;
	com_ptr<ConnectionPointImpl<IBridgeEvents>> _bridgeEventsCP;
	com_ptr<ConnectionPointImpl<IStpPropertyChangeSink>> _stpPropertyChangedCP;
	com_ptr<IMSTConfigProperties> _mstConfig;

public:
	HRESULT InitInstance (uint32_t port_count, uint32_t msti_count, mac_address macAddress)
	{
		HRESULT hr;

		hr = _weakRefToThis.InitInstance(AsUnknown()); RETURN_IF_FAILED(hr);

		bool resized = _missedLinkPulseCounters.try_resize(port_count, MissedLinkPulseCounterMax); RETURN_HR_IF(E_OUTOFMEMORY, !resized);

		hr = MakeConnectionPoint(this, &_propChangeCP); RETURN_IF_FAILED(hr);
		hr = MakeConnectionPoint(this, &_invalidateCP); RETURN_IF_FAILED(hr);
		hr = MakeConnectionPoint(this, &_bridgeEventsCP); RETURN_IF_FAILED(hr);
		hr = MakeConnectionPoint(this, &_stpPropertyChangedCP); RETURN_IF_FAILED(hr);

		bool reserved = _trees.try_reserve(1 + msti_count); RETURN_HR_IF(E_OUTOFMEMORY, !reserved);
		for (uint32_t i = 0; i < 1 + msti_count; i++)
		{
			com_ptr<IBridgeTree> tree;
			hr = MakeBridgeTree(this, i, &tree); RETURN_IF_FAILED(hr);
			_trees.try_push_back(std::move(tree));
		}

		LONG offset = 0;
		for (uint32_t i = 0; i < port_count; i++)
		{
			offset += (PortToPortSpacing / 2 + PortInteriorWidth / 2);
			com_ptr<IPort> port;
			hr = MakePort (this, i, PortSide::Bottom, offset, &port); RETURN_IF_FAILED(hr);
			AdviseSinkToken token;
			hr = AdviseSink<IInvalidateSink>(port, _weakRefToThis, &token); RETURN_IF_FAILED(hr);
			bool pushed = _ports.try_push_back({ std::move(port), std::move(token) }); RETURN_HR_IF(E_OUTOFMEMORY, !pushed);
			offset += (PortInteriorWidth / 2 + PortToPortSpacing / 2);
		}

		_x = 0;
		_y = 0;
		_width = std::max (offset, MinWidth);
		_height = DefaultHeight;

		_stpBridge = STP_CreateBridge ((unsigned int)port_count, (unsigned int)msti_count, max_vlan_number, &StpCallbacks, macAddress.data(), 256);
		STP_EnableLogging (_stpBridge, true);
		STP_SetApplicationContext (_stpBridge, this);
		STP_RegisterPropertyChangeCallback(_stpBridge, &StpCallback_PropertyChanging, &StpCallback_PropertyChanged);

		auto mstConfig = com_ptr(new (std::nothrow) MstConfigImpl()); RETURN_IF_NULL_ALLOC(mstConfig);
		hr = mstConfig->InitInstance(this); RETURN_IF_FAILED(hr);
		_mstConfig = std::move(mstConfig);

		// ----------------------------------------------------------------------------

		if (_created_bridges.empty())
		{
			static constexpr auto link_pulse_callback = [](HWND, UINT, UINT_PTR, DWORD)
			{
				for (auto& bridge : _created_bridges)
				{
					if (bridge->parent())
						bridge->OnLinkPulseTick();
				}
			};
			_link_pulse_timer_id = ::SetTimer (nullptr, 0, 16, link_pulse_callback); WI_ASSERT(_link_pulse_timer_id);

			DWORD period = 950 + (std::random_device()() % 100);
			static constexpr auto one_second_callback = [](HWND, UINT, UINT_PTR, DWORD)
			{
				for (auto& bridge : _created_bridges)
				{
					if (bridge->_project && !bridge->parent()->simulation_paused())
						STP_OnOneSecondTick (bridge->_stpBridge, ::GetMessageTime());
				}
			};
			_one_second_timer_id = ::SetTimer (nullptr, 0, 1000, one_second_callback); WI_ASSERT(_one_second_timer_id);

			static constexpr WNDPROC helper_window_proc = [](HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) -> LRESULT
			{
				if (msg == WM_PACKET_RECEIVED)
				{
					auto bridge = reinterpret_cast<BridgeImpl*>(::GetWindowLongPtr (hwnd, GWLP_USERDATA));
					bridge->ProcessReceivedPackets();
					return 0;
				}
				return ::DefWindowProc (hwnd, msg, wparam, lparam);
			};

			WNDCLASS wc = { sizeof(WNDCLASS) };
			wc.hInstance = (HMODULE)&__ImageBase;
			wc.lpfnWndProc = helper_window_proc;
			wc.lpszClassName = helper_window_class_name;
			ATOM atom = ::RegisterClass(&wc);
		}
	
		WI_ASSERT (_helper_window == nullptr);
		_helper_window = ::CreateWindow (helper_window_class_name, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, (HMODULE)&__ImageBase, 0); WI_ASSERT (_helper_window != nullptr);
		::SetWindowLongPtr (_helper_window, GWLP_USERDATA, (LONG_PTR)this);

		_created_bridges.insert(this);
		return S_OK;
	}

	~BridgeImpl()
	{
		_created_bridges.erase(this);

		::DestroyWindow (_helper_window);
		_helper_window = nullptr;

		if (_created_bridges.empty())
		{
			::UnregisterClass (helper_window_class_name, (HMODULE)&__ImageBase);

			BOOL bres = ::KillTimer (nullptr, _one_second_timer_id); WI_ASSERT(bres);
			_one_second_timer_id = 0;

			bres = ::KillTimer (nullptr, _link_pulse_timer_id); WI_ASSERT(bres);
			_link_pulse_timer_id = 0;
		}

		// ----------------------------------------------------------------

//		for (auto& port : _ports)
//			port->invalidate().remove_handler(&IBridge::on_port_invalidated, this);

		//while (!_trees.empty())
		//	this->bridge_tree_collection_i::remove_last();

		STP_UnregisterPropertyChangeCallback(_stpBridge);
		STP_SetApplicationContext (_stpBridge, nullptr);
		STP_DestroyBridge (_stpBridge);
	}

	IUnknown* AsUnknown() { return static_cast<IBridge*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IBridge>(this, riid, ppvObject)
			|| TryQI<IBridgeProperties>(this, riid, ppvObject)
			|| TryQI<IDispatch>(static_cast<IBridgeProperties*>(this), riid, ppvObject)
			|| TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IConnectionPointContainer>(this, riid, ppvObject)
			|| TryQI<ISelectableObject>(this, riid, ppvObject)
			|| TryQI<IXmlParent>(this, riid, ppvObject)
			|| TryQI<IInvalidateSink>(this, riid, ppvObject)
		)
			return S_OK;

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	IMPLEMENT_IDISPATCH(IBridgeProperties);

	#pragma region IBridgeProperties
   virtual HRESULT STDMETHODCALLTYPE get_Name (BSTR *pName) override
	{
		*pName = SysAllocString(L"Bridge"); RETURN_IF_NULL_ALLOC(*pName);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_ClassName (BSTR *pClassName) override
	{
		*pClassName = SysAllocString(L"Bridge"); RETURN_IF_NULL_ALLOC(*pClassName);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_X (LONG* pX) override
	{
		*pX = _x;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_X (LONG x) override
	{
		if (_x != x)
		{
			NotifyInvalidate(_invalidateCP, extent());
			NotifyPropertyChanging (_propChangeCP, AsUnknown(), dispidBridgeX, nullptr);
			_x = x;
			NotifyPropertyChanged (_propChangeCP, AsUnknown(), dispidBridgeX, nullptr);
			NotifyInvalidate(_invalidateCP, extent());
		}
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_Y (LONG* pY) override
	{
		*pY = _y;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_Y (LONG y) override
	{
		if (_y != y)
		{
			NotifyInvalidate(_invalidateCP, extent());
			NotifyPropertyChanging (_propChangeCP, AsUnknown(), dispidBridgeY, nullptr);
			_y = y;
			NotifyPropertyChanged (_propChangeCP, AsUnknown(), dispidBridgeY, nullptr);
			NotifyInvalidate(_invalidateCP, extent());
		}
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_Width (LONG *pWidth) override
	{
		*pWidth = _width;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_Width (LONG width) override
	{
		if (_width != width)
		{
			NotifyInvalidate(_invalidateCP, extent());
			NotifyPropertyChanging (_propChangeCP, AsUnknown(), dispidBridgeWidth, nullptr);
			_width = width;
			NotifyPropertyChanged (_propChangeCP, AsUnknown(), dispidBridgeWidth, nullptr);
			NotifyInvalidate(_invalidateCP, extent());
		}
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_Height (LONG *pHeight) override
	{
		*pHeight = (LONG)_height;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_Height (LONG height) override
	{
		if (_height != height)
		{
			NotifyInvalidate(_invalidateCP, extent());
			NotifyPropertyChanging (_propChangeCP, AsUnknown(), dispidBridgeHeight, nullptr);
			_height = height;
			NotifyPropertyChanged (_propChangeCP, AsUnknown(), dispidBridgeHeight, nullptr);
			NotifyInvalidate(_invalidateCP, extent());
		}
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_Ports (SAFEARRAY** ppsaItems) override
	{
		return GetItems (_ports.size(), [this](ULONG i, IDispatch** ppDisp) -> HRESULT {
			return _ports[i].port->QueryInterface(ppDisp);
		}, ppsaItems);
	}

	virtual HRESULT STDMETHODCALLTYPE get_BridgeAddress (BSTR* pbstrBridgeAddress) override
	{
		mac_address addr;
		memcpy (addr.data(), STP_GetBridgeAddress(_stpBridge)->bytes, 6);
		return BridgeAddressToString (addr, pbstrBridgeAddress);
	}

	virtual HRESULT STDMETHODCALLTYPE put_BridgeAddress (BSTR bstrBridgeAddress) override
	{
		mac_address addr;
		auto hr = BridgeAddressFromString(bstrBridgeAddress, addr); RETURN_IF_FAILED_EXPECTED(hr);

		NotifyPropertyChanging(_propChangeCP, AsUnknown(), dispidBridgeAddress, nullptr);
		STP_SetBridgeAddress(_stpBridge, addr.data(), ::GetMessageTime());
		NotifyPropertyChanged(_propChangeCP, AsUnknown(), dispidBridgeAddress, nullptr);
		NotifyInvalidate(_invalidateCP, extent());

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_STPEnabled (VARIANT_BOOL *pbEnabled) override
	{
		*pbEnabled = STP_IsBridgeStarted(_stpBridge) ? VARIANT_TRUE : VARIANT_FALSE;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_STPEnabled (VARIANT_BOOL bEnabled) override
	{
		set_stp_enabled(bEnabled);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_STPVersion (enum STPVersion *pVersion) override
	{
		*pVersion = (STPVersion)STP_GetStpVersion(_stpBridge);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_STPVersion (enum STPVersion version) override
	{
		STP_SetStpVersion(_stpBridge, (STP_VERSION)version, GetMessageTime());
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_PortCount (DWORD* pdwPortCount) override
	{
		*pdwPortCount = STP_GetPortCount(_stpBridge);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_MSTICount (DWORD* pdwMstiCount) override
	{
		*pdwMstiCount = STP_GetMstiCount(_stpBridge);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_MigrateTime (DWORD* pdwMigrateTime) override
	{
		*pdwMigrateTime = 3;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_BridgeHelloTime (DWORD *pdwBridgeHelloTime) override
	{
		*pdwBridgeHelloTime = 2;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_BridgeMaxAge (DWORD* pdwBridgeMaxAge) override
	{
		*pdwBridgeMaxAge = STP_GetBridgeMaxAge(stp_bridge());
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_BridgeMaxAge (DWORD dwBridgeMaxAge) override
	{
		if ((dwBridgeMaxAge < 6) || (dwBridgeMaxAge > 40))
			return SetErrorInfo(E_INVALIDARG, L"MaxAge must be in the range 6..40.\r\nThe default value, also recommended by the standard, is 20.");

		if (STP_GetBridgeMaxAge(_stpBridge) != dwBridgeMaxAge)
		{
			NotifyPropertyChanging(_propChangeCP, AsUnknown(), dispidBridgeMaxAge, nullptr);
			STP_SetBridgeMaxAge (_stpBridge, dwBridgeMaxAge, ::GetMessageTime());
			NotifyPropertyChanged(_propChangeCP, AsUnknown(), dispidBridgeMaxAge, nullptr);
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_BridgeForwardDelay (DWORD* pdwBridgeForwardDelay) override
	{
		*pdwBridgeForwardDelay = STP_GetBridgeForwardDelay(_stpBridge);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_BridgeForwardDelay (DWORD dwBridgeForwardDelay) override
	{
		if ((dwBridgeForwardDelay < 4) || (dwBridgeForwardDelay > 30))
			return SetErrorInfo(E_INVALIDARG, L"BridgeForwardDelay must be in the range 4..30.\r\nThe default value, also recommended by the standard, is 15.");

		if (STP_GetBridgeForwardDelay(_stpBridge) != dwBridgeForwardDelay)
		{
			NotifyPropertyChanging(_propChangeCP, AsUnknown(), dispidBridgeForwardDelay, nullptr);
			STP_SetBridgeForwardDelay (_stpBridge, dwBridgeForwardDelay, ::GetMessageTime());
			NotifyPropertyChanged(_propChangeCP, AsUnknown(), dispidBridgeForwardDelay, nullptr);
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_TxHoldCount (DWORD* pdwTxHoldCount) override
	{
		*pdwTxHoldCount = STP_GetTxHoldCount(_stpBridge);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_TxHoldCount (DWORD dwTxHoldCount) override
	{
		if ((dwTxHoldCount < 1) || (dwTxHoldCount > 10))
			return SetErrorInfo(E_INVALIDARG, L"TxHoldCount must be in the range 1..10.\r\nThe default value, also recommended by the standard, is 6.");

		if (STP_GetTxHoldCount(_stpBridge) != dwTxHoldCount)
		{
			NotifyPropertyChanging(_propChangeCP, AsUnknown(), dispidTxHoldCount, nullptr);
			STP_SetTxHoldCount(_stpBridge, dwTxHoldCount, ::GetMessageTime());
			NotifyPropertyChanged(_propChangeCP, AsUnknown(), dispidTxHoldCount, nullptr);
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_MaxHops (DWORD *pdwMaxHops) override
	{
		*pdwMaxHops = 20;
		return S_OK;
	}

	class MstConfigImpl : public IMSTConfigProperties
	{
		ULONG _refCount = 0;
		BridgeImpl* _bridge = nullptr;

	public:
		HRESULT InitInstance(BridgeImpl* bridge)
		{
			_bridge = bridge;
			return S_OK;
		}

		#pragma region IUnknown
		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
		{
			RETURN_HR_IF(E_POINTER, !ppvObject);
			*ppvObject = nullptr;

			if (   TryQI<IUnknown>(this, riid, ppvObject)
				|| TryQI<IDispatch>(this, riid, ppvObject)
				|| TryQI<IMSTConfigProperties>(this, riid, ppvObject))
				return S_OK;

			return E_NOINTERFACE;
		}

		virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

		virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
		#pragma endregion

		IMPLEMENT_IDISPATCH(IMSTConfigProperties);

		virtual HRESULT STDMETHODCALLTYPE get_Name(BSTR* pbstrName) override
		{
			const STP_MST_CONFIG_ID* configId = STP_GetMstConfigId(_bridge->_stpBridge);
			uint32_t len = (uint32_t)strnlen(configId->ConfigurationName, 32);
			wchar_t str[32];
			for (uint32_t i = 0; i < len; i++)
				str[i] = configId->ConfigurationName[i];
			*pbstrName = SysAllocStringLen(str, len); RETURN_IF_NULL_ALLOC(*pbstrName);
			return S_OK;
		}

		virtual HRESULT STDMETHODCALLTYPE put_Name(BSTR bstrName) override
		{
			UINT len = SysStringLen(bstrName);
			if (len > 32)
				return SetErrorInfo(E_INVALIDARG, L"Invalid MST Config Name: more than 32 characters.");

			char str[33];
			for (UINT i = 0; i < len; i++)
			{
				if (bstrName[i] >= 256)
					return SetErrorInfo(E_INVALIDARG, L"Invalid MST Config Name: non-ASCII chars.");
				str[i] = (char)bstrName[i];
			}
			str[len] = 0;

			STP_SetMstConfigName(_bridge->_stpBridge, str, GetMessageTime());
			NotifyInvalidate(_bridge->_invalidateCP, _bridge->extent());
			return S_OK;
		}

		virtual HRESULT STDMETHODCALLTYPE get_RevisionLevel(WORD* pwRevisionLevel) override
		{
			const STP_MST_CONFIG_ID* id = STP_GetMstConfigId(_bridge->_stpBridge);
			*pwRevisionLevel = ((WORD)id->RevisionLevelHigh << 8) | (WORD)id->RevisionLevelLow;
			return S_OK;
		}

		virtual HRESULT STDMETHODCALLTYPE put_RevisionLevel(WORD wRevisionLevel) override
		{
			const STP_MST_CONFIG_ID* id = STP_GetMstConfigId(_bridge->_stpBridge);
			WORD existing = ((WORD)id->RevisionLevelHigh << 8) | (WORD)id->RevisionLevelLow;
			if (existing != wRevisionLevel)
			{
				STP_SetMstConfigRevisionLevel(_bridge->_stpBridge, wRevisionLevel, GetMessageTime());
			}

			return S_OK;
		}

		virtual HRESULT STDMETHODCALLTYPE get_Values(SAFEARRAY** pValues) override
		{
			unsigned int entryCount;
			const STP_CONFIG_TABLE_ENTRY* entries = STP_GetMstConfigTable(_bridge->_stpBridge, &entryCount);
			unique_safearray values(SafeArrayCreateVector(VT_UI1, 0, entryCount)); RETURN_IF_NULL_ALLOC(values);
			BYTE* data;
			auto hr = SafeArrayAccessData(values.get(), reinterpret_cast<void**>(&data)); RETURN_IF_FAILED(hr);
			for (unsigned int i = 0; i < entryCount; i++)
				data[i] = entries[i].treeIndex;
			hr = SafeArrayUnaccessData(values.get()); RETURN_IF_FAILED(hr);
			*pValues = values.release();
			return S_OK;
		}

		virtual HRESULT STDMETHODCALLTYPE put_Values(SAFEARRAY* values) override
		{
			VARTYPE vartype;
			auto hr = SafeArrayGetVartype(values, &vartype); RETURN_IF_FAILED(hr);
			RETURN_HR_IF(E_INVALIDARG, vartype != VT_UI1 || SafeArrayGetDim(values) != 1);
			LONG lowerBound, upperBound;
			hr = SafeArrayGetLBound(values, 1, &lowerBound); RETURN_IF_FAILED(hr);
			hr = SafeArrayGetUBound(values, 1, &upperBound); RETURN_IF_FAILED(hr);
			unsigned int entryCount;
			STP_GetMstConfigTable(_bridge->_stpBridge, &entryCount);
			RETURN_HR_IF(E_INVALIDARG, lowerBound != 0 || upperBound != (LONG)entryCount - 1);

			BYTE* data;
			hr = SafeArrayAccessData(values, reinterpret_cast<void**>(&data)); RETURN_IF_FAILED(hr);
			auto unaccessData = wil::scope_exit([values] { SafeArrayUnaccessData(values); });
			STP_CONFIG_TABLE_ENTRY entries[1 + max_vlan_number];
			for (unsigned int i = 0; i < entryCount; i++)
			{
				RETURN_HR_IF(E_INVALIDARG, data[i] >= 1 + STP_GetMstiCount(_bridge->_stpBridge));
				entries[i] = { .unused = 0, .treeIndex = data[i] };
			}

			STP_SetMstConfigTable(_bridge->_stpBridge, entries, entryCount, GetMessageTime());
			return S_OK;
		}
	};

	virtual HRESULT STDMETHODCALLTYPE get_MSTConfigName (BSTR *pbstrName) override
	{
		return _mstConfig->get_Name(pbstrName);
	}

	virtual HRESULT STDMETHODCALLTYPE put_MSTConfigName (BSTR bstrName) override
	{
		return _mstConfig->put_Name(bstrName);
	}

	virtual HRESULT STDMETHODCALLTYPE get_MSTConfigRevLevel (WORD* pwConfigRevLevel) override
	{
		return _mstConfig->get_RevisionLevel(pwConfigRevLevel);
	}

	virtual HRESULT STDMETHODCALLTYPE put_MSTConfigRevLevel (WORD wRevLevel) override
	{
		return _mstConfig->put_RevisionLevel(wRevLevel);
	}

	virtual HRESULT STDMETHODCALLTYPE get_MSTConfigDigest (BSTR *pbstrDigest) override
	{
		const unsigned char* digest = STP_GetMstConfigId(_stpBridge)->ConfigurationDigest;
		wchar_t buffer[33];
		for (int i = 0; i < 16; i++)
		{
			uint8_t x = digest[i] >> 4;
			buffer[2 * i] = (x < 10) ? (x + '0') : (x - 10 + 'A');
			x = digest[i] & 15;
			buffer[2 * i + 1] = (x < 10) ? (x + '0') : (x - 10 + 'A');
		}
		buffer[32] = 0;
		*pbstrDigest = SysAllocString(buffer); RETURN_IF_NULL_ALLOC(*pbstrDigest);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_MSTConfig(IMSTConfigProperties** ppMSTConfig) override
	{
		*ppMSTConfig = _mstConfig.get();
		(*ppMSTConfig)->AddRef();
		return S_OK;
	}
	#pragma endregion

	#pragma region IInvalidateSink
	virtual HRESULT STDMETHODCALLTYPE OnInvalidate (const RECT* rectw) noexcept override
	{
		return NotifyInvalidate(_invalidateCP, extent());
	}
	#pragma endregion

	#pragma region IXmlParent
	virtual HRESULT STDMETHODCALLTYPE GetChildSerializeInfo (
		_In_ DISPID dispidProperty,
		_In_ IDispatch* pChild,
		_Outptr_ BSTR* pbstrXmlElementName,
		_Outptr_opt_result_buffer_(*pcFactoryDispids) DISPID** ppFactoryDispids,
		_Out_ ULONG* pcFactoryDispids) override
	{
		if (dispidProperty == dispidMstConfig)
		{
			*pbstrXmlElementName = nullptr;
			if (ppFactoryDispids)
				*ppFactoryDispids = nullptr;
			if (pcFactoryDispids)
				*pcFactoryDispids = 0;
			return S_OK;
		}
		else if (dispidProperty == dispidPorts)
		{
			*pbstrXmlElementName = SysAllocString(L"Port"); RETURN_IF_NULL_ALLOC(*pbstrXmlElementName);
			if (ppFactoryDispids)
			{
				auto dispids = wil::make_unique_cotaskmem_nothrow<DISPID[]>(2); RETURN_IF_NULL_ALLOC(dispids);
				dispids.get()[0] = dispidPortSide;
				dispids.get()[1] = dispidPortOffset;
				*ppFactoryDispids = dispids.release();
			}
			if (pcFactoryDispids)
				*pcFactoryDispids = 2;
			return S_OK;
		}
		else
			RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE GetChildDeserializeInfo (
		_In_ DISPID dispidProperty,
		_In_ PCWSTR xmlElementName,
		_Outptr_ ITypeInfo** ppTypeInfo,
		_Outptr_result_buffer_(*pcFactoryDispids) DISPID** ppFactoryDispids,
		_Out_ ULONG* pcFactoryDispids) override
	{
		if (dispidProperty == dispidPorts)
		{
			RETURN_HR(E_UNEXPECTED); // This is a read-only property, so we're not supposed to get here.
		}
		else
			RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE CreateChild (
		_In_ DISPID dispidProperty,
		_In_ PCWSTR xmlElementName,
		_In_reads_(factoryPropCount) const VARIANT* factoryPropValues,
		_In_ ULONG factoryPropCount,
		_Outptr_ IDispatch** childOut) override
	{
		if (dispidProperty == dispidPorts)
		{
			RETURN_HR(E_UNEXPECTED); // This is a read-only property, so we're not supposed to get here.
		}
		else
			RETURN_HR(E_NOTIMPL);
	}
	#pragma endregion

	#pragma region IConnectionPointContainer
	virtual HRESULT STDMETHODCALLTYPE EnumConnectionPoints (IEnumConnectionPoints **ppEnum) override
	{
		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE FindConnectionPoint (REFIID riid, IConnectionPoint **ppCP) override
	{
		if (riid == __uuidof(IPropertyChangeSink))
			return wil::com_query_to_nothrow(_propChangeCP, ppCP);
		if (riid == __uuidof(IInvalidateSink))
			return wil::com_query_to_nothrow(_invalidateCP, ppCP);
		if (riid == __uuidof(IBridgeEvents))
			return wil::com_query_to_nothrow(_bridgeEventsCP, ppCP);
		if (riid == __uuidof(IStpPropertyChangeSink))
			return wil::com_query_to_nothrow(_stpPropertyChangedCP, ppCP);
		RETURN_HR(E_NOTIMPL);
	}
	#pragma endregion

	virtual SIZE size() const noexcept override { return { _width, _height }; }

	virtual RECT extent() const noexcept override
	{
		return { _x - PortExteriorHeight, _y - PortExteriorHeight, _x + _width + PortExteriorHeight, _y + _height + PortExteriorHeight };
	}

	virtual IStpProject* parent() const override
	{
		return _project;
	}

	virtual void set_parent (IStpProject* parent) override
	{
		_project = parent;
	}

	void on_port_invalidated (void* arg, IPort* p)
	{
		NotifyInvalidate(_invalidateCP, extent());
	}

	// Checks the wires and computes macOperational for each port on this bridge.
	void OnLinkPulseTick()
	{
		uint32_t now = (uint32_t) ::GetMessageTime();

		bool invalidate = false;
		for (uint32_t portIndex = 0; portIndex < _ports.size(); portIndex++)
		{
			IPort* port = _ports[portIndex].port;
			if (_missedLinkPulseCounters[portIndex] < MissedLinkPulseCounterMax)
			{
				_missedLinkPulseCounters[portIndex]++;
				if (_missedLinkPulseCounters[portIndex] == MissedLinkPulseCounterMax)
				{
					port->SetActualSpeed(0);
					STP_OnPortDisabled (_stpBridge, (unsigned int) portIndex, now);
					invalidate = true;
				}
			}

//			packet_transmit_e::invoker(_em).invoke(this, portIndex, link_pulse_t { now, port->supported_speed() });
			_bridgeEventsCP->Notify([this,now,portIndex](IBridgeEvents* e) {
				return e->OnPacketTransmit(this, portIndex, link_pulse_t { now, _ports[portIndex].port->SupportedSpeed() });
			});
		}

		if (invalidate)
			NotifyInvalidate(_invalidateCP, extent());
	}

	virtual const std::vector<std::unique_ptr<BridgeLogLine>>& GetLogLines() const override { return _logLines; }

	virtual void enqueue_received_packet (packet_t&& packet, ULONG rxPortIndex) override
	{
		_rxQueue.push ({ rxPortIndex, std::move(packet) });
		::PostMessage (_helper_window, WM_PACKET_RECEIVED, 0, 0);
	}

	void ProcessReceivedPackets()
	{
		bool invalidate = false;

		while (!_rxQueue.empty())
		{
			uint32_t rxPortIndex = _rxQueue.front().first;
			IPort* port = _ports[rxPortIndex].port;
			auto sd = std::move(_rxQueue.front().second);
			_rxQueue.pop();

			if (std::holds_alternative<link_pulse_t>(sd))
			{
				auto lpsd = std::get<link_pulse_t>(sd);
				bool oldMacOperational = _missedLinkPulseCounters[rxPortIndex] < MissedLinkPulseCounterMax;
				_missedLinkPulseCounters[rxPortIndex] = 0;
				if (oldMacOperational == false)
				{
					// Send a link pulse right away, to make sure the other port goes up before we send it any frame.
					//packet_transmit_e::invoker(_em).invoke(this, rxPortIndex, link_pulse_t { lpsd.timestamp, port->supported_speed() });
					_bridgeEventsCP->Notify([this,rxPortIndex,&lpsd] (IBridgeEvents* e) {
						return e->OnPacketTransmit(this, rxPortIndex, link_pulse_t { lpsd.timestamp, _ports[rxPortIndex].port->SupportedSpeed() });
					});

					auto actual_speed = std::min (lpsd.sender_supported_speed, port->SupportedSpeed());
					port->SetActualSpeed(actual_speed);

					STP_OnPortEnabled (_stpBridge, (unsigned int) rxPortIndex, actual_speed, true, lpsd.timestamp);
					invalidate = true;
				}
			}
			else if (std::holds_alternative<frame_t>(sd))
			{
				auto fsd = std::get<frame_t>(sd);

				if (!port->mac_operational())
				{
					// The sender must be misbehaving (forgot to send link pulses). Currently the simulator is single-threaded so this shouldn't happen.
					WI_ASSERT(false);
				}
				else
				{
					if ((fsd.data.size() >= 6) && (memcmp (&fsd.data[0], BpduDestAddress, 6) == 0))
					{
						// It's a BPDU.
						if (_bpdu_trapping_enabled)
						{
							STP_OnBpduReceived (_stpBridge, (unsigned int) rxPortIndex, &fsd.data[21], (unsigned int) (fsd.data.size() - 21), fsd.timestamp);
						}
						else
						{
							// broadcast it to the other ports.
							for (ULONG txPortIndex = 0; txPortIndex < (ULONG)_ports.size(); txPortIndex++)
							{
								if (txPortIndex == rxPortIndex)
									continue;

								auto txPortAddress = GetPortAddress(txPortIndex);

								// If it already went through this port, we have a loop that would hang our UI.
								if (std::find (fsd.tx_path_taken.begin(), fsd.tx_path_taken.end(), txPortAddress) != fsd.tx_path_taken.end())
								{
									// We don't do anything here; we have code in wire.cpp that shows loops to the user - as thick red wires.
									//volatile int a = 0;
								}
								else
								{
									frame_t f;
									f.timestamp = fsd.timestamp;
									f.data = fsd.data;
									f.tx_path_taken = fsd.tx_path_taken;
									f.tx_path_taken.push_back (txPortAddress);

									//packet_transmit_e::invoker(_em).invoke(this, txPortIndex, std::move(f));
									auto hr = _bridgeEventsCP->Notify([this,txPortIndex,&f](IBridgeEvents* e) {
										return e->OnPacketTransmit(this, txPortIndex, std::move(f));
									});
								}
							}
						}
					}
					else
						WI_ASSERT(false); // not implemented
				}
			}
			else
				WI_ASSERT(false);
		}

		if (invalidate)
			NotifyInvalidate(_invalidateCP, extent());
	}

	virtual POINT location() const noexcept override { return { _x, _y }; }

	virtual void set_location (POINT location) noexcept override
	{
		if (_x != location.x || _y != location.y)
		{
			NotifyInvalidate(_invalidateCP, extent());
			NotifyPropertyChanging(_propChangeCP, AsUnknown(), dispidBridgeX, nullptr);
			NotifyPropertyChanging(_propChangeCP, AsUnknown(), dispidBridgeY, nullptr);
			_x = location.x;
			_y = location.y;
			NotifyPropertyChanged(_propChangeCP, AsUnknown(), dispidBridgeY, nullptr);
			NotifyPropertyChanged(_propChangeCP, AsUnknown(), dispidBridgeX, nullptr);
			NotifyInvalidate(_invalidateCP, extent());
		}
	}

	virtual ULONG TreeCount() const override { return (ULONG)_trees.size(); }

	virtual IBridgeTree* TreeAt(ULONG i) const override { return _trees[i]; }

	virtual ULONG PortCount() const override { return _ports.size(); }

	virtual IPort* PortAt (ULONG i) const override { return _ports[i].port; }

	virtual HRESULT STDMETHODCALLTYPE Render (ID2D1RenderTarget* dc, const drawing_resources& dos, unsigned int vlanNumber, const D2D1_COLOR_F& configIdColor) const override
	{
		HRESULT hr;

		auto treeIndex = STP_GetTreeIndexFromVlanNumber (_stpBridge, vlanNumber);

		const unsigned char* addr = STP_GetBridgeAddress(_stpBridge)->bytes;

		wil::unique_process_heap_string text;
		float bridgeOutlineWidth = OutlineWidth;
		if (STP_IsBridgeStarted(_stpBridge))
		{
			auto stpVersion = STP_GetStpVersion(_stpBridge);
			auto treeIndex = STP_GetTreeIndexFromVlanNumber(_stpBridge, vlanNumber);
			bool isCistRoot = STP_IsCistRoot(_stpBridge);
			bool isRegionalRoot = (treeIndex > 0) && STP_IsRegionalRoot(_stpBridge, treeIndex);

			if ((treeIndex == 0) ? isCistRoot : isRegionalRoot)
				bridgeOutlineWidth *= 2;

			hr = wil::str_printf_nothrow(
				text, L"%04X.%02X%02X%02X%02X%02X%02X\r\nSTP enabled (%S)\r\n%s",
				STP_GetBridgePriority(_stpBridge, treeIndex), addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
				STP_GetVersionString(stpVersion), isCistRoot ? L"CIST Root Bridge\r\n" : L""); RETURN_IF_FAILED(hr);
			if (stpVersion >= STP_VERSION_MSTP)
			{
				wil::unique_process_heap_string s;
				hr = wil::str_printf_nothrow (
					s, L"VLAN %u. Spanning tree: %S\r\n%S",
					vlanNumber, ((treeIndex == 0) ? "CIST(0)" : (std::string("MSTI") + std::to_string(treeIndex)).c_str()),
					(isRegionalRoot ? "Regional Root\r\n" : "")); RETURN_IF_FAILED(hr);
				hr = wil::str_concat_nothrow(text, s); RETURN_IF_FAILED(hr);
			}
		}
		else
		{
			hr = wil::str_printf_nothrow(
				text, L"%02X%02X%02X%02X%02X%02X\r\nSTP disabled\r\n(right-click to enable)",
				addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]); RETURN_IF_FAILED(hr);
		}

		// Draw bridge outline.
		D2D1_ROUNDED_RECT rr = RoundedRect ({ (float)_x, (float)_y, (float)right(), (float)bottom() }, RoundRadius, RoundRadius);
		edge::inflate (&rr, -bridgeOutlineWidth / 2);
		com_ptr<ID2D1SolidColorBrush> brush;
		dc->CreateSolidColorBrush (configIdColor, &brush);
		dc->FillRoundedRectangle (&rr, brush);//_powered ? dos._poweredFillBrush : dos._unpoweredBrush*
		dc->DrawRoundedRectangle (&rr, dos._brushWindowText, bridgeOutlineWidth);

		// Draw bridge text.
		com_ptr<IDWriteTextLayout> tl;
		hr = dos._dWriteFactory->CreateTextLayout (text.get(), (UINT32)wcslen(text.get()), dos._regularTextFormat, 10000, 10000, &tl);
		dc->DrawTextLayout ({ _x + OutlineWidth * 2 + 3, _y + OutlineWidth * 2 + 3}, tl, dos._brushWindowText);

		for (auto& port : _ports) {
			hr = port.port->Render (dc, dos, vlanNumber); RETURN_IF_FAILED(hr);
		}

		return S_OK;
	}

	#pragma region ISelectableObject
	virtual void render_selection (ID2D1DeviceContext* rt, const edge::IZoomer* zoomer, const drawing_resources& dos) const
	{
		auto oldaa = rt->GetAntialiasMode();
		rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

		auto tl = zoomer->pointw_to_pointd ({ _x - OutlineWidth / 2, _y - OutlineWidth / 2 });
		auto br = zoomer->pointw_to_pointd ({ _x + _width + OutlineWidth / 2, _y + _height + OutlineWidth / 2 });
		rt->DrawRectangle ({ tl.x - 10, tl.y - 10, br.x + 10, br.y + 10 }, dos._brushHighlight, 2, dos._strokeStyleSelectionRect);

		rt->SetAntialiasMode(oldaa);
	}


	virtual int32_t hit_test (const D2D1::Matrix3x2F& wtr, D2D1_POINT_2F dLocation, float tolerance)
	{
		auto tl = wtr.TransformPoint({ (float)left(), (float)top() });
		auto br = wtr.TransformPoint({ (float)right(), (float)bottom() });

		if ((dLocation.x >= tl.x) && (dLocation.y >= tl.y) && (dLocation.x < br.x) && (dLocation.y < br.y))
			return HTCodeInner;

		return {};
	}
	#pragma endregion

	virtual STP_BRIDGE* stp_bridge() const { return _stpBridge; }

	mac_address GetPortAddress (uint32_t portIndex) const
	{
		auto address = bridge_address();
		size_t increment = portIndex + 1;
		for (size_t i = 6; i-- > 3 && increment; )
		{
			uint16_t sum = address[i] + (increment & 0xff);
			address[i] = (uint8_t)sum;
			increment = (increment >> 8) + (sum >> 8);
		}
		LOG_HR_IF(HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW), increment != 0);
		return address;
	}

	mac_address bridge_address() const
	{
		mac_address address;
		auto x = sizeof(address);
		memcpy (address.data(), STP_GetBridgeAddress(_stpBridge)->bytes, 6);
		return address;
	}

	virtual void set_bridge_address (mac_address address) override
	{
		if (memcmp(STP_GetBridgeAddress(_stpBridge)->bytes, address.data(), 6) != 0)
		{
//			edge::value_property_change_args args = { bridge_address_property };
//			edge::property_changing_e::invoker(_em).invoke(this, args);
			STP_SetBridgeAddress(_stpBridge, address.data(), GetMessageTime());
//			edge::property_changed_e::invoker(_em).invoke(this, args);
		}
	}

	virtual void clear_log() override
	{
		_logLines.clear();
		_currentLogLine.text.clear();
		_bridgeEventsCP->Notify([this](IBridgeEvents* e) { return e->OnLogCleared(this); });
	}

	std::string GetMstConfigIdDigest() const
	{
	}

	virtual void set_stp_enabled (bool value) override
	{
		if (_deserializing)
		{
			_enable_stp_after_deserialize = value;
			return;
		}

		if (value && !STP_IsBridgeStarted(_stpBridge))
		{
			NotifyPropertyChanging(_propChangeCP, AsUnknown(), { dispidStpEnabled }, nullptr);
			STP_StartBridge (_stpBridge, GetMessageTime());
			NotifyPropertyChanged(_propChangeCP, AsUnknown(), { dispidStpEnabled }, nullptr);
			NotifyInvalidate(_invalidateCP, extent());
		}
		else if (!value && STP_IsBridgeStarted(_stpBridge))
		{
			NotifyPropertyChanging(_propChangeCP, AsUnknown(), { dispidStpEnabled }, nullptr);
			STP_StopBridge (_stpBridge, GetMessageTime());
			NotifyPropertyChanged(_propChangeCP, AsUnknown(), { dispidStpEnabled }, nullptr);
			NotifyInvalidate(_invalidateCP, extent());
		}
	}

	void set_stp_version (STP_VERSION stp_version)
	{
		if (STP_GetStpVersion(_stpBridge) != stp_version)
		{
			WI_ASSERT(false);
			//this->on_property_changing(&stp_version_property);
			//STP_SetStpVersion(_stpBridge, stp_version, GetMessageTime());
			//this->on_property_changed(&stp_version_property);
		}
	}

	#pragma region properties
	size_t mst_config_table_get_value_count() const
	{
		unsigned int entry_count;
		STP_GetMstConfigTable(_stpBridge, &entry_count);
		return entry_count;
	}

	uint32_t mst_config_table_get_value(size_t i) const
	{
		unsigned int entry_count;
		auto entries = STP_GetMstConfigTable(_stpBridge, &entry_count);
		return entries[i].treeIndex;
	}

	void mst_config_table_set_value(size_t i, uint32_t value)
	{
		unsigned int entry_count;
		auto table = STP_GetMstConfigTable (_stpBridge, &entry_count);
		WI_ASSERT (i < entry_count);
		if (table->treeIndex != value)
		{
			//edge::value_collection_property_change_args args = { &mst_config_table_property, i, edge::collection_property_change_type::set };
			WI_ASSERT(false);
			//this->on_property_changing(args);
			//STP_SetMstConfigTableEntry (_stpBridge, (unsigned int)i, value, ::GetMessageTime());
			//this->on_property_changed(args);
		}
	}

	bool mst_config_table_changed (size_t i) const
	{
		unsigned int entry_count;
		const STP_CONFIG_TABLE_ENTRY* entries = STP_GetMstConfigTable (_stpBridge, &entry_count);
		WI_ASSERT(i < entry_count);
		return entries[i].treeIndex != 0;
	}

	//void on_deserializing (edge::xml_deserializer_i* de)
	//{
	//	_deserializing = true;
	//	_enable_stp_after_deserialize = stp_enabled_property.default_value().value();
	//}
	//
	//void on_deserialized  (edge::xml_deserializer_i* de)
	//{
	//	if (_enable_stp_after_deserialize)
	//		STP_StartBridge (_stpBridge, ::GetMessageTime());
	//	_deserializing = false;
	//}
	/*
	static const pg::property_group bridge_times_group = { 5, "Timer Params (Table 13-5)" };
	static const pg::property_group mst_group = { 10, "MST Config Id" };

	const edge::typed_value_collection_property<edge::uint32_property_traits> bridge::mst_config_table_property = {
		"MstConfigTable",
		&mst_config_table_get_value_count,
		&mst_config_table_get_value,
		&mst_config_table_set_value,
		&mst_config_table_changed,
	};
	#pragma endregion
	*/
	#pragma region STP Callbacks
	static void* StpCallback_AllocAndZeroMemory(unsigned int size)
	{
		void* p = malloc(size);
		memset (p, 0, size);
		return p;
	}

	static void StpCallback_FreeMemory(void* p)
	{
		free(p);
	}

	static void* StpCallback_TransmitGetBuffer (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp)
	{
		auto b = static_cast<BridgeImpl*>(STP_GetApplicationContext(bridge));
		IPort* txPort = b->_ports[portIndex].port;

		b->_txPacketData.resize (bpduSize + 21);
		memcpy (&b->_txPacketData[0], BpduDestAddress, 6);
		memcpy (&b->_txPacketData[6], &b->GetPortAddress(portIndex)[0], 6);
		b->_txTransmittingPort = txPort;
		b->_txTimestamp = timestamp;
		return &b->_txPacketData[21];
	}

	static void StpCallback_TransmitReleaseBuffer (const STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer)
	{
		auto b = static_cast<BridgeImpl*>(STP_GetApplicationContext(bridge));

		frame_t info;
		info.data = std::move(b->_txPacketData);
		info.timestamp = b->_txTimestamp;
		auto hr = b->_bridgeEventsCP->Notify([b,&info](IBridgeEvents* e) {
			return e->OnPacketTransmit(b, b->_txTransmittingPort->port_index(), std::move(info));
		});
	}

	static void StpCallback_EnableBpduTrapping (const STP_BRIDGE* bridge, bool enable, unsigned int timestamp)
	{
		auto b = static_cast<BridgeImpl*>(STP_GetApplicationContext(bridge));
		b->_bpdu_trapping_enabled = enable;
	}

	static void StpCallback_EnableLearning (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable, unsigned int timestamp)
	{
		auto b = static_cast<BridgeImpl*>(STP_GetApplicationContext(bridge));
		NotifyInvalidate(b->_invalidateCP, b->extent());
	}

	static void StpCallback_EnableForwarding (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable, unsigned int timestamp)
	{
		auto b = static_cast<BridgeImpl*>(STP_GetApplicationContext(bridge));
		NotifyInvalidate(b->_invalidateCP, b->extent());
	}

	static void StpCallback_FlushFdb (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType, unsigned int timestamp)
	{
		auto b = static_cast<BridgeImpl*>(STP_GetApplicationContext(bridge));
		b->_ports[portIndex].port->treeAt(treeIndex)->flush_fdb(timestamp);
	}

	static void StpCallback_DebugStrOut (const STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, unsigned int flush)
	{
		auto b = static_cast<BridgeImpl*>(STP_GetApplicationContext(bridge));

		if (stringLength > 0)
		{
			if (b->_currentLogLine.text.empty())
			{
				b->_currentLogLine.text.assign (nullTerminatedString, (size_t) stringLength);
				b->_currentLogLine.portIndex = portIndex;
				b->_currentLogLine.treeIndex = treeIndex;
			}
			else
			{
				if ((b->_currentLogLine.portIndex != portIndex) || (b->_currentLogLine.treeIndex != treeIndex))
				{
					b->_logLines.push_back(std::make_unique<BridgeLogLine>(std::move(b->_currentLogLine)));
					b->_bridgeEventsCP->Notify([b](IBridgeEvents* e) { return e->OnLogLineGenerated(b, b->_logLines.back().get()); });
				}

				b->_currentLogLine.text.append (nullTerminatedString, (size_t) stringLength);
			}

			if (!b->_currentLogLine.text.empty() && (b->_currentLogLine.text.back() == L'\n'))
			{
				b->_logLines.push_back(std::make_unique<BridgeLogLine>(std::move(b->_currentLogLine)));
				b->_bridgeEventsCP->Notify([b](IBridgeEvents* e) { return e->OnLogLineGenerated(b, b->_logLines.back().get()); });
			}
		}

		if (flush && !b->_currentLogLine.text.empty())
		{
			b->_logLines.push_back(std::make_unique<BridgeLogLine>(std::move(b->_currentLogLine)));
			b->_bridgeEventsCP->Notify([b](IBridgeEvents* e) { return e->OnLogLineGenerated(b, b->_logLines.back().get()); });
		}
	}

	static void StpCallback_OnTopologyChange (const STP_BRIDGE* bridge, unsigned int treeIndex, unsigned int timestamp)
	{
		auto b = static_cast<BridgeImpl*>(STP_GetApplicationContext(bridge));
		b->_trees[treeIndex]->on_topology_change(timestamp);
	}

	static void StpCallback_OnPortRoleChanged (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, STP_PORT_ROLE role, unsigned int timestamp)
	{
		auto b = static_cast<BridgeImpl*>(STP_GetApplicationContext(bridge));
		NotifyInvalidate(b->_invalidateCP, b->extent());
	}

	static inline const STP_CALLBACKS StpCallbacks =
	{
		&StpCallback_EnableBpduTrapping,
		&StpCallback_EnableLearning,
		&StpCallback_EnableForwarding,
		&StpCallback_TransmitGetBuffer,
		&StpCallback_TransmitReleaseBuffer,
		&StpCallback_FlushFdb,
		&StpCallback_DebugStrOut,
		&StpCallback_OnTopologyChange,
		&StpCallback_OnPortRoleChanged,
		&StpCallback_AllocAndZeroMemory,
		&StpCallback_FreeMemory,
	};
	#pragma endregion

	static void StpCallback_PropertyChanging (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, STP_PROPERTY prop, unsigned int timestamp)
	{
		auto b = static_cast<BridgeImpl*>(STP_GetApplicationContext(bridge));
		b->_stpPropertyChangedCP->Notify([b,portIndex,treeIndex,prop,timestamp](IStpPropertyChangeSink* sink) {
			return sink->OnStpPropertyChanging(b, portIndex, treeIndex, prop, timestamp);
		});

		if (portIndex == -1 && treeIndex == -1)
		{
			// STP properties that are relevant to the bridge, not to a bridge tree or to a port.
			if (prop == STP_PROPERTY_MST_CONFIG_NAME)
				NotifyPropertyChanging(b->_propChangeCP, b->AsUnknown(), dispidMstConfigName);
			if (prop == STP_PROPERTY_MST_CONFIG_REVISION_LEVEL)
				NotifyPropertyChanging(b->_propChangeCP, b->AsUnknown(), dispidMstConfigRevLevel);
			if (prop == STP_PROPERTY_MST_CONFIG_TABLE)
				NotifyPropertyChanging(b->_propChangeCP, b->AsUnknown(), dispidMstConfigTable);
		}
	}

	static void StpCallback_PropertyChanged (const struct STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, STP_PROPERTY prop, unsigned int timestamp)
	{
		auto b = static_cast<BridgeImpl*>(STP_GetApplicationContext(bridge));
		b->_stpPropertyChangedCP->Notify([b,portIndex,treeIndex,prop,timestamp](IStpPropertyChangeSink* sink) {
			return sink->OnStpPropertyChanged(b, portIndex, treeIndex, prop, timestamp);
		});

		if (portIndex == -1 && treeIndex == -1)
		{
			if (prop == STP_PROPERTY_STP_VERSION)
				NotifyPropertyChanged(b->_propChangeCP, b->AsUnknown(), dispidStpVersion);
			if (prop == STP_PROPERTY_MST_CONFIG_NAME)
				NotifyPropertyChanged(b->_propChangeCP, b->AsUnknown(), dispidMstConfigName);
			if (prop == STP_PROPERTY_MST_CONFIG_REVISION_LEVEL)
				NotifyPropertyChanged(b->_propChangeCP, b->AsUnknown(), dispidMstConfigRevLevel);
			if (prop == STP_PROPERTY_MST_CONFIG_TABLE)
				NotifyPropertyChanged(b->_propChangeCP, b->AsUnknown(), dispidMstConfigTable);

			NotifyInvalidate (b->_invalidateCP, b->extent());
		}
	}
};

HRESULT MakeBridge (uint32_t portCount, uint32_t mstiCount, mac_address macAddress, IBridge** ppBridge)
{
	auto p = com_ptr (new (std::nothrow) BridgeImpl()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance (portCount, mstiCount, macAddress); RETURN_IF_FAILED(hr);
	*ppBridge = p.detach();
	return S_OK;
}

HRESULT SetErrorInfo (HRESULT return_hr, const wchar_t* text)
{
	com_ptr<ICreateErrorInfo> cei;
	auto hr = ::CreateErrorInfo(&cei); RETURN_IF_FAILED(hr);
	cei->SetDescription(const_cast<LPOLESTR>(text));
	auto ei = wil::try_com_query_nothrow<IErrorInfo>(cei);
	hr = ::SetErrorInfo(0, ei); RETURN_IF_FAILED(hr);
	return return_hr;
}

HRESULT SetErrorInfoStpDisabled()
{
	return SetErrorInfo(E_UNEXPECTED, L"(STP disabled)");
}

