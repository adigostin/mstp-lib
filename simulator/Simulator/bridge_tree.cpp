
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "dispids.h"

using namespace edge;

class BridgeTreeImpl : public IBridgeTree, IBridgeTreeProperties, IConnectionPointContainer
{
	ULONG _refCount = 0;
	ULONG _sig = 0xAA550008;
	IBridge* _bridge = nullptr;
	uint32_t _tree_index;
	SYSTEMTIME _last_topology_change;
	uint32_t _topology_change_count;
	com_ptr<ConnectionPointImpl<IPropertyChangeSink>> _propChangeCP;

public:
	HRESULT InitInstance (IBridge* bridge, uint32_t tree_index)
	{
		HRESULT hr;

		_bridge = bridge;
		_tree_index = tree_index;
		::GetSystemTime(&_last_topology_change);
		_topology_change_count = 0;
		hr = MakeConnectionPoint<IPropertyChangeSink>(this, &_propChangeCP); RETURN_IF_FAILED(hr);
		//_parent->property_changing().add_handler<&bridge_tree::on_bridge_property_changing>(this);
		//_parent->property_changed().add_handler<&bridge_tree::on_bridge_property_changed>(this);
		return S_OK;
	}

	~BridgeTreeImpl()
	{
		//_parent->property_changed().remove_handler<&bridge_tree::on_bridge_property_changed>(this);
		//_parent->property_changing().remove_handler<&bridge_tree::on_bridge_property_changing>(this);
	}

	IUnknown* AsUnknown() { return static_cast<IBridgeTree*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IDispatch>(this, riid, ppvObject)
			|| TryQI<IBridgeTree>(this, riid, ppvObject)
			|| TryQI<IBridgeTreeProperties>(this, riid, ppvObject)
			|| TryQI<IConnectionPointContainer>(this, riid, ppvObject)
		)
			return S_OK;

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	IMPLEMENT_IDISPATCH(IBridgeTreeProperties);

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

	//virtual ::bridge* parent() const override { return _parent; }

	//static inline const value_property* const properties_changed_on_stp_enable_disable[] = {
	//	&bridge_tree::root_id_property,
	//	&bridge_tree::external_root_path_cost_property,
	//	&bridge_tree::regional_root_id_property,
	//	&bridge_tree::internal_root_path_cost_property,
	//	&bridge_tree::designated_bridge_id_property,
	//	&bridge_tree::designated_port_id_property,
	//	&bridge_tree::receiving_port_id_property,
	//};

	//void on_bridge_property_changing (edge::object* obj, const property_change_args& args)
	//{
	//	if (args.property == &bridge::stp_enabled_property)
	//	{
	//		//for (auto prop : properties_changed_on_stp_enable_disable)
	//		//	edge::property_changing_e::invoker(_em).invoke(this, value_property_change_args{ prop });
	//	}
	//}

	//void on_bridge_property_changed (edge::object* obj, const property_change_args& args)
	//{
	//	if (args.property == &bridge::stp_enabled_property)
	//	{
	//		//for (auto prop : properties_changed_on_stp_enable_disable)
	//		//	edge::property_changed_e::invoker(_em).invoke(this, value_property_change_args{ prop });
	//	}
	//}

	virtual void on_topology_change (unsigned int timestamp) override
	{
		NotifyPropertyChanging(_propChangeCP, AsUnknown(), dispidTopologyChangeCount);
		::GetSystemTime(&_last_topology_change);
		_topology_change_count++;
		NotifyPropertyChanged(_propChangeCP, AsUnknown(), dispidTopologyChangeCount);
	}

	#pragma region IBridgeTreeProperties
	virtual HRESULT STDMETHODCALLTYPE get___id (BSTR *pName) override
	{
		HRESULT hr;

		uint32_t entryCount;
		auto table = STP_GetMstConfigTable(_bridge->stp_bridge(), &entryCount);
		uint32_t vlan = 0;
		do
		{
			if (table[vlan].treeIndex == _tree_index)
				break;
			vlan++;
		} while (vlan < entryCount);

		wil::unique_process_heap_string str;
		if (_tree_index == 0)
		{
			hr = wil::str_printf_nothrow (str, L"VLAN %u (CIST)", vlan); RETURN_IF_FAILED(hr);
		}
		else
		{
			hr = wil::str_printf_nothrow (str, L"VLAN %u (MSTI %u)", vlan, _tree_index); RETURN_IF_FAILED(hr);
		}

		*pName = SysAllocString(str.get()); RETURN_IF_NULL_ALLOC(*pName);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_BridgePriority (enum BridgePriority* pPrio) override
	{
		*pPrio = (enum BridgePriority)STP_GetBridgePriority(_bridge->stp_bridge(), _tree_index);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_BridgePriority (enum BridgePriority prio) override
	{
		auto old_prio = (enum BridgePriority)STP_GetBridgePriority (_bridge->stp_bridge(), _tree_index);
		if (old_prio != prio)
		{
			NotifyPropertyChanging (_propChangeCP, AsUnknown(), dispidBridgePrio);
			STP_SetBridgePriority (_bridge->stp_bridge(), _tree_index, (unsigned short) prio, GetMessageTime());
			NotifyPropertyChanged  (_propChangeCP, AsUnknown(), dispidBridgePrio);
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_TopologyChangeCount (DWORD *pCount) override
	{
		*pCount = _topology_change_count;
		return S_OK;
	}
	#pragma endregion

	std::array<unsigned char, 36> root_priorty_vector() const
	{
		std::array<unsigned char, 36> prioVector;
		STP_GetRootPriorityVector(_bridge->stp_bridge(), (unsigned int)_tree_index, prioVector.data());
		return prioVector;
	}

	virtual std::string root_bridge_id() const override
	{
		if (!STP_IsBridgeStarted (_bridge->stp_bridge()))
			throw std::logic_error(stp_disabled_text);

		auto rpv = root_priorty_vector();
		std::stringstream ss;
		ss << std::uppercase << std::setfill('0') << std::hex
			<< std::setw(2) << (int)rpv[0] << std::setw(2) << (int)rpv[1] << "."
			<< std::setw(2) << (int)rpv[2] << std::setw(2) << (int)rpv[3] << std::setw(2) << (int)rpv[4]
			<< std::setw(2) << (int)rpv[5] << std::setw(2) << (int)rpv[6] << std::setw(2) << (int)rpv[7];
		return ss.str();
	}

	uint32_t external_root_path_cost() const
	{
		if (!STP_IsBridgeStarted(_bridge->stp_bridge()))
			throw std::logic_error(stp_disabled_text);

		auto rpv = root_priorty_vector();
		auto cost = ((uint32_t) rpv[8] << 24) | ((uint32_t) rpv[9] << 16) | ((uint32_t) rpv[10] << 8) | rpv[11];
		return cost;
	}

	std::string regional_root_id() const
	{
		if (!STP_IsBridgeStarted (_bridge->stp_bridge()))
			throw std::logic_error(stp_disabled_text);

		auto rpv = root_priorty_vector();
		std::stringstream ss;
		ss << std::uppercase << std::setfill('0') << std::hex
			<< std::setw(2) << (int)rpv[12] << std::setw(2) << (int)rpv[13] << "."
			<< std::setw(2) << (int)rpv[14] << std::setw(2) << (int)rpv[15] << std::setw(2) << (int)rpv[16]
			<< std::setw(2) << (int)rpv[17] << std::setw(2) << (int)rpv[18] << std::setw(2) << (int)rpv[19];
		return ss.str();
	}

	uint32_t internal_root_path_cost() const
	{
		if (!STP_IsBridgeStarted(_bridge->stp_bridge()))
			throw std::logic_error(stp_disabled_text);

		auto rpv = root_priorty_vector();
		auto cost = ((uint32_t) rpv[20] << 24) | ((uint32_t) rpv[21] << 16) | ((uint32_t) rpv[22] << 8) | rpv[23];
		return cost;
	}

	std::string designated_bridge_id() const
	{
		if (!STP_IsBridgeStarted(_bridge->stp_bridge()))
			throw std::logic_error(stp_disabled_text);

		auto rpv = root_priorty_vector();
		std::stringstream ss;
		ss << std::uppercase << std::setfill('0') << std::hex
			<< std::setw(2) << (int)rpv[24] << std::setw(2) << (int)rpv[25] << "."
			<< std::setw(2) << (int)rpv[26] << std::setw(2) << (int)rpv[27] << std::setw(2) << (int)rpv[28]
			<< std::setw(2) << (int)rpv[29] << std::setw(2) << (int)rpv[30] << std::setw(2) << (int)rpv[31];
		return ss.str();
	}

	std::string designated_port_id() const
	{
		if (!STP_IsBridgeStarted(_bridge->stp_bridge()))
			throw std::logic_error(stp_disabled_text);

		auto rpv = root_priorty_vector();
		std::stringstream ss;
		ss << std::uppercase << std::setfill('0') << std::hex << std::setw(2) << (int)rpv[32] << std::setw(2) << (int)rpv[33];
		return ss.str();
	}

	std::string receiving_port_id() const
	{
		if (!STP_IsBridgeStarted(_bridge->stp_bridge()))
			throw std::logic_error(stp_disabled_text);

		auto rpv = root_priorty_vector();
		std::stringstream ss;
		ss << std::uppercase << std::setfill('0') << std::hex << std::setw(2) << (int)rpv[34] << std::setw(2) << (int)rpv[35];
		return ss.str();
	}

	uint32_t hello_time() const
	{
		unsigned short ht;
		STP_GetRootTimes(_bridge->stp_bridge(), (unsigned int)_tree_index, nullptr, &ht, nullptr, nullptr, nullptr);
		return ht;
	}

	uint32_t max_age() const
	{
		unsigned short ma;
		STP_GetRootTimes(_bridge->stp_bridge(), (unsigned int)_tree_index, nullptr, nullptr, &ma, nullptr, nullptr);
		return ma;
	}

	uint32_t bridge_forward_delay() const
	{
		unsigned short fd;
		STP_GetRootTimes(_bridge->stp_bridge(), (unsigned int)_tree_index, &fd, nullptr, nullptr, nullptr, nullptr);
		return fd;
	}

	uint32_t message_age() const
	{
		unsigned short ma;
		STP_GetRootTimes(_bridge->stp_bridge(), (unsigned int)_tree_index, nullptr, nullptr, nullptr, &ma, nullptr);
		return ma;
	}

	uint32_t remaining_hops() const
	{
		unsigned char rh;
		STP_GetRootTimes(_bridge->stp_bridge(), (unsigned int)_tree_index, nullptr, nullptr, nullptr, nullptr, &rh);
		return rh;
	}

	uint32_t topology_change_count() const { return _topology_change_count; }

	// ============================================================================

	//static const pg::property_group rpv_group = { 4, "Root Priority Vector" };
	//static const pg::property_group root_times_group = { 5, "Root Times" };

	/*
	const string_p root_id_property = {
		"RootID", &rpv_group, nullptr, true, &root_bridge_id, nullptr, };

	const uint32_p external_root_path_cost_property = {
		"ExternalRootPathCost", &rpv_group, nullptr, true, &external_root_path_cost, nullptr };

	const string_p regional_root_id_property = {
		"RegionalRootId", &rpv_group, nullptr, true, &regional_root_id, nullptr };

	const uint32_p internal_root_path_cost_property =
		{ "InternalRootPathCost", &rpv_group, nullptr, true, &internal_root_path_cost, nullptr };

	const string_p designated_bridge_id_property =
		{ "DesignatedBridgeId", &rpv_group, nullptr, true, &designated_bridge_id, nullptr };

	const string_p designated_port_id_property =
		{ "DesignatedPortId", &rpv_group, nullptr, true, &designated_port_id, nullptr };

	const string_p receiving_port_id_property =
		{ "ReceivingPortId", &rpv_group, nullptr, true, &receiving_port_id, nullptr };

	const uint32_p hello_time_property =
		{ "HelloTime", &root_times_group, nullptr, true, &hello_time, nullptr };

	const uint32_p max_age_property =
		{ "MaxAge", &root_times_group, nullptr, true, &max_age, nullptr };

	const uint32_p forward_delay_property =
		{ "ForwardDelay", &root_times_group, nullptr, true, &bridge_forward_delay, nullptr };

	const uint32_p message_age_property =
		{ "MessageAge", &root_times_group, nullptr, true, &message_age, nullptr };

	const uint32_p remaining_hops_property =
		{ "remainingHops", &root_times_group, nullptr, true, &remaining_hops, nullptr };

	const uint32_p topology_change_count_property =
		{ "Topology Change Count", nullptr, nullptr, true, &topology_change_count, nullptr };

	const edge::property* const _properties[] =
	{
		&root_id_property,
		&topology_change_count_property,
		&external_root_path_cost_property,
		&regional_root_id_property,
		&internal_root_path_cost_property,
		&designated_bridge_id_property,
		&designated_port_id_property,
		&receiving_port_id_property,
		&hello_time_property,
		&max_age_property,
		&forward_delay_property,
		&message_age_property,
		&remaining_hops_property
	};

	const edge::xtype<bridge_tree> _type = { "BridgeTree", nullptr, _properties };
	*/
};

HRESULT MakeBridgeTree (IBridge* bridge, uint32_t treeIndex, IBridgeTree** ppBridgeTree)
{
	auto p = com_ptr(new (std::nothrow) BridgeTreeImpl()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance (bridge, treeIndex); RETURN_IF_FAILED(hr);
	*ppBridgeTree = p.detach();
	return S_OK;
}

