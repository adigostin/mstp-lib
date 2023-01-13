
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "wire.h"
#include "bridge.h"
#include "port.h"
#include "bridge_tree.h"
#include "edge/xml_serializer.h"

using edge::com_exception;
using edge::throw_if_failed;

static const _bstr_t NextMacAddressString = "NextMacAddress";

class project : public project_i
{
	edge::event_manager _em;
	std::wstring _path;
	std::vector<std::unique_ptr<bridge>> _bridges;
	std::vector<std::unique_ptr<wire>> _wires;
	mac_address _next_mac_address = next_mac_address_property.default_value().value();
	bool _simulationPaused = false;
	bool _changedFlag = false;

public:
	~project()
	{
		// Need to call remove_bridge explicitly in order to unregister the event handlers that we registered in insert_bridge.
		while(!_wires.empty())
			remove_wire(_wires.size() - 1);
		while(!_bridges.empty())
			remove_bridge(_bridges.size() - 1);
	}

	virtual hierarchy_object_i* parent() const override { return nullptr; }

	virtual const std::vector<std::unique_ptr<bridge>>& bridges() const override { return _bridges; }

	virtual size_t bridge_count() const override { return _bridges.size(); }

	virtual bridge* bridge_at(size_t index) const override { return _bridges[index].get(); }

	void insert_bridge (size_t i, std::unique_ptr<bridge> b)
	{
		bridge* raw = b.get();

		edge::object_collection_property_change_args args = { &bridges_prop, i, edge::collection_property_change_type::insert, raw };
		edge::property_changing_e::invoker(_em).invoke(this, args);
		raw->set_parent(this);
		_bridges.insert(_bridges.begin() + i, std::move(b));
		args.child = nullptr;
		edge::property_changed_e::invoker(_em).invoke(this, args);

		raw->invalidate().add_handler<&project::on_bridge_invalidate>(this);
		raw->packet_transmit().add_handler<&project::on_packet_transmit>(this);
		invalidate_e::invoker(_em).invoke(this);
	}

	std::unique_ptr<bridge> remove_bridge (size_t i)
	{
		bridge* b = _bridges[i].get();
		if (std::any_of (_wires.begin(), _wires.end(), [b, this](const std::unique_ptr<wire>& w) {
			return any_of (w->points().begin(), w->points().end(), [b, this] (wire_end p) {
				return std::holds_alternative<connected_wire_end>(p) && (std::get<connected_wire_end>(p)->bridge() == b);
			});
		}))
			rassert(false); // can't remove a connected bridge

		b->packet_transmit().remove_handler<&project::on_packet_transmit>(this);
		b->invalidate().remove_handler<&project::on_bridge_invalidate>(this);
		
		edge::object_collection_property_change_args args = { &bridges_prop, i, edge::collection_property_change_type::remove, nullptr };
		edge::property_changing_e::invoker(_em).invoke(this, args);
		auto res = std::move(_bridges[i]);
		_bridges.erase(_bridges.begin() + i);
		b->set_parent(nullptr);
		args.child = b;
		edge::property_changed_e::invoker(_em).invoke(this, args);

		invalidate_e::invoker(_em).invoke(this);

		return res;
	}

	virtual const std::vector<std::unique_ptr<wire>>& wires() const override { return _wires; }

	virtual size_t wire_count() const override { return _wires.size(); }

	virtual wire* wire_at(size_t index) const override { return _wires[index].get(); }

	void insert_wire (size_t i, std::unique_ptr<wire> w)
	{
		wire* raw = w.get();
		edge::object_collection_property_change_args args = { &wires_prop, i, edge::collection_property_change_type::insert, raw };
		edge::property_changing_e::invoker(_em).invoke(this, args);
		raw->set_parent(this);
		_wires.insert(_wires.begin() + i, std::move(w));
		args.child = nullptr;
		edge::property_changed_e::invoker(_em).invoke(this, args);
		raw->invalidate().add_handler<&project::on_wire_invalidated>(this);
		invalidate_e::invoker(_em).invoke(this);
	}

	std::unique_ptr<wire> remove_wire (size_t i)
	{
		auto raw = _wires[i].get();
		raw->invalidate().remove_handler<&project::on_wire_invalidated>(this);
		edge::object_collection_property_change_args args = { &wires_prop, i, edge::collection_property_change_type::remove, nullptr };
		edge::property_changing_e::invoker(_em).invoke(this, args);
		auto res = std::move(_wires[i]);
		_wires.erase(_wires.begin() + i);
		raw->set_parent(nullptr);
		args.child = raw;
		edge::property_changing_e::invoker(_em).invoke(this, args);
		invalidate_e::invoker(_em).invoke(this);
		return res;
	}

	bool on_packet_transmit (bridge* bridge, size_t txPortIndex, packet_t&& pi)
	{
		auto tx_port = bridge->ports().at(txPortIndex).get();
		auto rx_port = find_connected_port(tx_port);
		if (rx_port != nullptr)
		{
			rx_port->bridge()->enqueue_received_packet(std::move(pi), rx_port->port_index());
			return true;
		}

		return false;
	}

	void on_bridge_invalidate (bridge* b)
	{
		invalidate_e::invoker(_em).invoke(this);
	}

	void on_wire_invalidated (wire* w)
	{
		invalidate_e::invoker(_em).invoke(this);
	}

	// project_i
	virtual invalidate_e::subscriber invalidated() override final { return invalidate_e::subscriber(_em); }

	virtual loaded_e::subscriber loaded() override final { return loaded_e::subscriber(_em); }

	virtual saved_e::subscriber saved() override final { return saved_e::subscriber(_em); }

	virtual bool IsWireForwarding (wire* wire, unsigned int vlanNumber, _Out_opt_ bool* hasLoop) const override final
	{
		if (!std::holds_alternative<connected_wire_end>(wire->p0()) || !std::holds_alternative<connected_wire_end>(wire->p1()))
			return false;

		auto portA = std::get<connected_wire_end>(wire->p0());
		auto portB = std::get<connected_wire_end>(wire->p1());
		bool portAFw = portA->IsForwarding(vlanNumber);
		bool portBFw = portB->IsForwarding(vlanNumber);
		if (!portAFw || !portBFw)
			return false;

		if (hasLoop != nullptr)
		{
			std::unordered_set<port*> txPorts;

			std::function<bool(port* txPort)> transmitsTo = [this, vlanNumber, &txPorts, &transmitsTo, targetPort=portA](port* txPort) -> bool
			{
				if (txPort->IsForwarding(vlanNumber))
				{
					auto rx = find_connected_port(txPort);
					if ((rx != nullptr) && rx->IsForwarding(vlanNumber))
					{
						txPorts.insert(txPort);

						for (unsigned int i = 0; i < (unsigned int) rx->bridge()->ports().size(); i++)
						{
							if ((i != rx->port_index()) && rx->IsForwarding(vlanNumber))
							{
								port* otherTxPort = rx->bridge()->ports()[i].get();
								if (otherTxPort == targetPort)
									return true;

								if (txPorts.find(otherTxPort) != txPorts.end())
									return false;

								if (transmitsTo(otherTxPort))
									return true;
							}
						}
					}
				}

				return false;
			};

			*hasLoop = transmitsTo(portA);
		}

		return true;
	}

	virtual mac_address alloc_mac_address_range (size_t count) override final
	{
		if (count >= 128)
			throw std::range_error("count must be lower than 128.");

		auto result = _next_mac_address;
		_next_mac_address[5] += (uint8_t)count;
		if (_next_mac_address[5] < count)
		{
			_next_mac_address[4]++;
			if (_next_mac_address[4] == 0)
				rassert(false); // not implemented
		}

		return result;
	}

	virtual const std::wstring& file_path() const override final { return _path; }

	virtual void save (const wchar_t* path) override final
	{
		rassert (path || !_path.empty());

		com_ptr<IXMLDOMDocument3> doc;
		HRESULT hr = CoCreateInstance (CLSID_DOMDocument60, nullptr, CLSCTX_INPROC_SERVER, __uuidof(doc), (void**) &doc);
		throw_if_failed(hr);

		auto serializer = edge::create_serializer (doc, static_cast<project_i*>(this));
		auto project_element = serializer->serialize_object(this, true);

		hr = doc->appendChild (project_element, nullptr);
		throw_if_failed(hr);

		hr = format_and_save_to_file (doc, path ? path : _path.c_str());
		throw_if_failed(hr);

		if (path)
			_path = path;

		this->SetChangedFlag(false);
		saved_e::invoker(_em).invoke(this);
	}

	static constexpr const concrete_type* const known_types[]
		= { &bridge::_type, &bridge_tree::_type, &port::_type, &port_tree::_type, &wire::_type };

	virtual void load (const wchar_t* filePath) override final
	{
		com_ptr<IXMLDOMDocument3> doc;
		HRESULT hr = CoCreateInstance (CLSID_DOMDocument60, nullptr, CLSCTX_INPROC_SERVER, __uuidof(doc), (void**) &doc);
		throw_if_failed(hr);

		if (!PathFileExists(filePath))
			throw com_exception (HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

		VARIANT_BOOL isSuccessful;
		hr = doc->load(_variant_t(filePath), &isSuccessful);
		throw_if_failed(hr);
		if (isSuccessful != VARIANT_TRUE)
			throw std::exception("Load failed");

		com_ptr<IXMLDOMNode> xmlDeclarationNode;
		hr = doc->get_firstChild(&xmlDeclarationNode);
		throw_if_failed(hr);
		_bstr_t nodeName;
		hr = xmlDeclarationNode->get_nodeName(nodeName.GetAddress());
		throw_if_failed(hr);
		if (_wcsicmp (nodeName.GetBSTR(), L"xml") != 0)
			throw com_exception(E_FAIL);

		com_ptr<IXMLDOMNode> projectNode;
		hr = xmlDeclarationNode->get_nextSibling(&projectNode);
		throw_if_failed(hr);
		hr = projectNode->get_nodeName(nodeName.GetAddress());
		throw_if_failed(hr);
		if (_wcsicmp (nodeName.GetBSTR(), L"Project") != 0)
			throw com_exception(E_FAIL);
		com_ptr<IXMLDOMElement> projectElement = projectNode;

		auto de = create_deserializer(known_types, this);
		de->deserialize_object (projectElement, this);

		_path = filePath;
		loaded_e::invoker(_em).invoke(this);
	}

	virtual void pause_simulation() override final
	{
		_simulationPaused = true;
		invalidate_e::invoker(_em).invoke(this);
	}

	virtual void resume_simulation() override final
	{
		_simulationPaused = false;
		invalidate_e::invoker(_em).invoke(this);
	}

	virtual bool simulation_paused() const override final { return _simulationPaused; }

	virtual bool GetChangedFlag() const override final { return _changedFlag; }

	virtual void SetChangedFlag (bool changedFlag) override final
	{
		if (changedFlag)
		{
			ChangedEvent::invoker(_em).invoke(this);
			invalidate_e::invoker(_em).invoke(this);
		}

		if (_changedFlag != changedFlag)
		{
			_changedFlag = changedFlag;
			changed_flag_changed_event::invoker(_em).invoke(this);
		}
	}

	virtual changed_flag_changed_event::subscriber changed_flag_changed() override final { return changed_flag_changed_event::subscriber(_em); }

	virtual ChangedEvent::subscriber GetChangedEvent() override final { return ChangedEvent::subscriber(_em); }

	virtual const edge::typed_object_collection_property1<bridge>* bridges_property() const override final { return &bridges_prop; }

	virtual const edge::typed_object_collection_property1<wire>* wires_property() const override final { return &wires_prop; }

	virtual edge::property_changing_e::subscriber property_changing() override final { return edge::property_changing_e::subscriber(_em); }

	virtual edge::property_changed_e::subscriber property_changed() override final { return edge::property_changed_e::subscriber(_em); }

	mac_address next_mac_address() const { return _next_mac_address; }

	void set_next_mac_address (mac_address value)
	{
		if (_next_mac_address != value)
		{
			edge::value_property_change_args args(next_mac_address_property);
			edge::property_changing_e::invoker(_em).invoke(this, args);
			_next_mac_address = value;
			edge::property_changed_e::invoker(_em).invoke(this, args);
		}
	}

	static inline const mac_address_p next_mac_address_property = {
		"NextMacAddress", nullptr, nullptr, false,
		&next_mac_address,
		&set_next_mac_address,
		mac_address{ 0x00, 0xAA, 0x55, 0xAA, 0x55, 0x80 },
	};

	static const edge::typed_object_collection_property1<bridge> bridges_prop;
	static const edge::typed_object_collection_property1<wire> wires_prop;
	static inline const property* const _properties[] = { &next_mac_address_property, &bridges_prop, &wires_prop };
public:
	static inline const xtype<project> _type = { "Project", nullptr, _properties };
	virtual const concrete_type* type() const { return &_type; }
};

const edge::typed_object_collection_property1<bridge> project::bridges_prop = {
	"Bridges",
	&project::bridge_count,
	&project::bridge_at,
	&project::insert_bridge,
	&project::remove_bridge,
};

const edge::typed_object_collection_property1<wire> project::wires_prop {
	"Wires",
	&project::wire_count,
	&project::wire_at,
	&project::insert_wire,
	&project::remove_wire,
};

extern std::shared_ptr<project_i> project_factory() { return std::make_shared<project>(); };
