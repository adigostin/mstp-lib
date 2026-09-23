
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "Simulator.h"

using namespace edge;

class PortTreeImpl : public IPortTree, IPortTreeProperties, IConnectionPointContainer, IPropertyChangeSink
{
	ULONG _refCount = 0;
	WeakRefToThis _weakRefToThis;
	IPort* _parent;
	uint32_t _tree_index;
	ULONGLONG _flush_tick_count = 0;
	bool _flush_text_visible = false;
	com_ptr<ConnectionPointImpl<IPropertyChangeSink>> _propChangeCP;
	AdviseSinkToken _bridgePropChangeToken;

	static inline UINT_PTR _flush_timer;
	static inline std::unordered_set<PortTreeImpl*> _trees;

public:
	HRESULT InitInstance (IPort* parent, uint32_t tree_index)
	{
		HRESULT hr = _weakRefToThis.InitInstance(AsUnknown()); RETURN_IF_FAILED(hr);

		_parent = parent;
		_tree_index = tree_index;

		hr = MakeConnectionPoint<IPropertyChangeSink>(this, &_propChangeCP); RETURN_IF_FAILED(hr);

		hr = AdviseSink<IPropertyChangeSink>(_parent->bridge(), _weakRefToThis, &_bridgePropChangeToken); RETURN_IF_FAILED(hr);

		if (_trees.empty())
			_flush_timer = ::SetTimer (nullptr, 0, 100, flush_timer_proc);
		_trees.insert(this);
		return S_OK;
	}

	~PortTreeImpl()
	{
		_trees.erase(this);
		if (_trees.empty())
			::KillTimer (nullptr, _flush_timer);
	}

	IUnknown* AsUnknown() { return static_cast<IPortTree*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IDispatch>(this, riid, ppvObject)
			|| TryQI<IPortTree>(this, riid, ppvObject)
			|| TryQI<IPortTreeProperties>(this, riid, ppvObject)
			|| TryQI<IConnectionPointContainer>(this, riid, ppvObject)
			|| TryQI<IPropertyChangeSink>(this, riid, ppvObject)
		)
			return S_OK;

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	IMPLEMENT_IDISPATCH(IPortTreeProperties);

	#pragma region IConnectionPointContainer
	virtual HRESULT STDMETHODCALLTYPE EnumConnectionPoints (IEnumConnectionPoints **ppEnum) override
	{
		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE FindConnectionPoint (REFIID riid, IConnectionPoint **ppCP) override
	{
		if (riid == __uuidof(IPropertyChangeSink))
			return wil::com_query_to_nothrow(_propChangeCP, ppCP);
		RETURN_HR(E_NOTIMPL);
	}
	#pragma endregion

	#pragma region IPortTreeProperties
	virtual HRESULT STDMETHODCALLTYPE get_Name (BSTR *pName) override
	{
		if (_tree_index == 0)
		{
			*pName = SysAllocString(L"Port Tree CIST"); RETURN_IF_NULL_ALLOC(*pName);
			return S_OK;
		}
		else
		{
			wil::unique_process_heap_string str;
			auto hr = wil::str_printf_nothrow (str, L"Port Tree MSTI %u", _tree_index); RETURN_IF_FAILED(hr);
			*pName = SysAllocString(str.get()); RETURN_IF_NULL_ALLOC(*pName);
			return S_OK;
		}
	}

	virtual HRESULT STDMETHODCALLTYPE get_ClassName (BSTR *pClassName) override
	{
		*pClassName = SysAllocString(L"PortTree"); RETURN_IF_NULL_ALLOC(*pClassName);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_PortPriority (enum PortPriority* pPriority) override
	{
		*pPriority = (PortPriority)STP_GetPortPriority(_parent->bridge()->stp_bridge(), _parent->port_index(), _tree_index);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_PortPriority (enum PortPriority priority) override
	{
		auto existing = (PortPriority)STP_GetPortPriority(_parent->bridge()->stp_bridge(), _parent->port_index(), _tree_index);
		if (existing != priority)
		{
			NotifyPropertyChanging (_propChangeCP, AsUnknown(), dispidPortPriority);
			STP_SetPortPriority (port()->bridge()->stp_bridge(), port()->port_index(), _tree_index, (unsigned char) priority, GetMessageTime());
			NotifyPropertyChanged  (_propChangeCP, AsUnknown(), dispidPortPriority);
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_learning (VARIANT_BOOL* pbLearning) override
	{
		if (!STP_IsBridgeStarted(port()->bridge()->stp_bridge()))
			return SetErrorInfoStpDisabled();
		bool learning = STP_GetPortLearning(_parent->bridge()->stp_bridge(), _parent->port_index(), _tree_index);
		*pbLearning = learning ? VARIANT_TRUE : VARIANT_FALSE;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_forwarding (VARIANT_BOOL *pbForwarding) override
	{
		if (!STP_IsBridgeStarted(port()->bridge()->stp_bridge()))
			return SetErrorInfoStpDisabled();
		bool forwarding = STP_GetPortForwarding(_parent->bridge()->stp_bridge(), _parent->port_index(), _tree_index);
		*pbForwarding = forwarding ? VARIANT_TRUE : VARIANT_FALSE;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_role (enum PortRole *pRole) override
	{
		if (!STP_IsBridgeStarted(port()->bridge()->stp_bridge()))
			return SetErrorInfoStpDisabled();
		auto stpRole= STP_GetPortRole (port()->bridge()->stp_bridge(), port()->port_index(), _tree_index);
		*pRole = (enum PortRole)stpRole;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_AdminInternalPortPathCost (DWORD* pdwAdminInternalPortPathCost) override
	{
		*pdwAdminInternalPortPathCost = STP_GetAdminInternalPortPathCost(port()->bridge()->stp_bridge(), port()->port_index(), _tree_index);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_AdminInternalPortPathCost (DWORD dwAdminInternalPortPathCost) override
	{
		if (STP_GetAdminInternalPortPathCost (port()->bridge()->stp_bridge(), port()->port_index(), _tree_index) != dwAdminInternalPortPathCost)
		{
			NotifyPropertyChanging(_propChangeCP, AsUnknown(), dispidAdminInternalPortPathCost, nullptr);
			NotifyPropertyChanging(_propChangeCP, AsUnknown(), dispidInternalPortPathCost, nullptr);
			STP_SetAdminInternalPortPathCost (port()->bridge()->stp_bridge(), port()->port_index(), _tree_index, dwAdminInternalPortPathCost, ::GetMessageTime());
			NotifyPropertyChanged(_propChangeCP, AsUnknown(), dispidInternalPortPathCost, nullptr);
			NotifyPropertyChanged(_propChangeCP, AsUnknown(), dispidAdminInternalPortPathCost, nullptr);
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_InternalPortPathCost (DWORD *pdwInternalPortPathCost) override
	{
		if (!STP_IsBridgeStarted(port()->bridge()->stp_bridge()))
			return SetErrorInfoStpDisabled();
		*pdwInternalPortPathCost = STP_GetInternalPortPathCost(port()->bridge()->stp_bridge(), port()->port_index(), _tree_index);
		return S_OK;
	}
	#pragma endregion

	virtual IPort* port() const override
	{
		return _parent;
	}

	#pragma region IPropertyChangeSink
	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanging (IUnknown *obj, DISPID dispID, const struct PropertyChangeArgs *args) override
	{
		if (dispID == dispidStpEnabled)
		{
			NotifyPropertyChanging(_propChangeCP, AsUnknown(), dispidPortLearning);
			NotifyPropertyChanging(_propChangeCP, AsUnknown(), dispidPortForwarding);
			NotifyPropertyChanging(_propChangeCP, AsUnknown(), dispidPortRole);
			NotifyPropertyChanging(_propChangeCP, AsUnknown(), dispidInternalPortPathCost);
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanged (IUnknown *obj, DISPID dispID, const struct PropertyChangeArgs *args) override
	{
		if (dispID == dispidStpEnabled)
		{
			NotifyPropertyChanged(_propChangeCP, AsUnknown(), dispidInternalPortPathCost);
			NotifyPropertyChanged(_propChangeCP, AsUnknown(), dispidPortRole);
			NotifyPropertyChanged(_propChangeCP, AsUnknown(), dispidPortForwarding);
			NotifyPropertyChanged(_propChangeCP, AsUnknown(), dispidPortLearning);
		}

		return S_OK;
	}
	#pragma endregion

	static void CALLBACK flush_timer_proc (HWND hwnd, UINT, UINT_PTR timer_id, DWORD)
	{
		auto now = ::GetTickCount64();

		for (PortTreeImpl* tree : _trees)
		{
			auto time_since_last_flush = now - tree->_flush_tick_count;
			bool text_visible = time_since_last_flush < 2000;
			if (tree->_flush_text_visible != text_visible)
			{
				tree->_flush_text_visible = text_visible;
//				tree->port()->invalidate();
			}
		}
	}

	virtual void flush_fdb (unsigned int timestamp) override
	{
		_flush_tick_count = ::GetTickCount64();
		if (!_flush_text_visible)
		{
			_flush_text_visible = true;
			//port()->invalidate();
		}
	}

	virtual bool fdb_flush_text_visible() const override { return _flush_text_visible; }

	virtual size_t tree_index() const override { return _tree_index; }

	//const size_p tree_index_property {
	//	"TreeIndex", nullptr, nullptr, false,
	//	&tree_index,
	//	nullptr,
	//};


	//static const pg::property_group port_path_cost_group = { 5, "Port Path Cost" };
	// admin_internal_port_path_cost_property
	// internal_port_path_cost_property

	//const edge::property* const _properties[] = {
	//	&tree_index_property,
	//	&admin_internal_port_path_cost_property,
	//	&internal_port_path_cost_property,
	//};

	//const xtype<port_tree> _type = { "PortTree", nullptr, _properties };

	//const concrete_type* type() const { return &_type; }
};

HRESULT MakePortTree (IPort* port, uint32_t treeIndex, IPortTree** ppPortTree)
{
	auto p = com_ptr (new (std::nothrow) PortTreeImpl()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(port, treeIndex); RETURN_IF_FAILED(hr);
	*ppPortTree = p.detach();
	return S_OK;
}
