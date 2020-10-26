
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "wire.h"
#include "bridge.h"
#include "port.h"
#include "xml_serializer.h"

using edge::com_exception;
using edge::throw_if_failed;

static const _bstr_t NextMacAddressString = "NextMacAddress";

class project : public edge::object, public project_i
{
	using base = edge::object;

	std::wstring _path;
	std::vector<std::unique_ptr<bridge>> _bridges;
	std::vector<std::unique_ptr<wire>> _wires;
	mac_address _next_mac_address = next_mac_address_property.default_value.value();
	bool _simulationPaused = false;
	bool _changedFlag = false;

public:
	~project()
	{
		while(!_wires.empty())
			wire_collection_i::remove_last();
		while(!_bridges.empty())
			bridge_collection_i::remove_last();
	}

private:
	// object_collection_i
	virtual void call_property_changing (const property_change_args& args) override final { this->on_property_changing(args); }
	virtual void call_property_changed  (const property_change_args& args) override final { this->on_property_changed(args); }

	// bridge_collection_i
	virtual void children_store (std::vector<std::unique_ptr<bridge>>** out) override final { *out = &_bridges; }

	virtual void collection_property (const typed_object_collection_property<bridge>** out) const override final { *out = &bridges_property; }

	virtual void on_child_inserted (size_t index, bridge* b) override
	{
		bridge_collection_i::on_child_inserted(index, b);

		b->invalidated().add_handler<&project::on_project_child_invalidated>(this);
		b->packet_transmit().add_handler<&project::on_packet_transmit>(this);
		this->event_invoker<invalidate_e>()(this);
	}

	virtual void on_child_removing (size_t index, bridge* b) override
	{
		if (std::any_of (_wires.begin(), _wires.end(), [b, this](const std::unique_ptr<wire>& w) {
			return any_of (w->points().begin(), w->points().end(), [b, this] (wire_end p) {
				return std::holds_alternative<connected_wire_end>(p) && (std::get<connected_wire_end>(p)->bridge() == b);
			});
		}))
			rassert(false); // can't remove a connected bridge

		b->packet_transmit().remove_handler<&project::on_packet_transmit>(this);
		b->invalidated().remove_handler<&project::on_project_child_invalidated>(this);

		this->event_invoker<invalidate_e>()(this);

		bridge_collection_i::on_child_removing(index, b);
	}

	// wire_collection_i
	virtual void children_store (std::vector<std::unique_ptr<wire>>** out) override final {*out = &_wires; }

	virtual void collection_property (const typed_object_collection_property<wire>** out) const override final { *out = &wires_property; }

	virtual void on_child_inserted (size_t index, wire* wire) override
	{
		wire_collection_i::on_child_inserted (index, wire);
		wire->invalidated().add_handler<&project::on_project_child_invalidated>(this);
		this->event_invoker<invalidate_e>()(this);
	}

	virtual void on_child_removing (size_t index, wire* wire) override
	{
		wire->invalidated().remove_handler<&project::on_project_child_invalidated>(this);
		this->event_invoker<invalidate_e>()(this);
		wire_collection_i::on_child_removing (index, wire);
	}

	void on_packet_transmit (bridge* bridge, size_t txPortIndex, packet_t&& pi)
	{
		auto tx_port = bridge->ports().at(txPortIndex).get();
		auto rx_port = find_connected_port(tx_port);
		if (rx_port != nullptr)
			rx_port->bridge()->enqueue_received_packet(std::move(pi), rx_port->port_index());
	}

	void on_project_child_invalidated (renderable_object* object)
	{
		event_invoker<invalidate_e>()(this);
	}

	// project_i
	virtual invalidate_e::subscriber invalidated() override final { return invalidate_e::subscriber(this); }

	virtual loaded_e::subscriber loaded() override final { return loaded_e::subscriber(this); }

	virtual saved_e::subscriber saved() override final { return saved_e::subscriber(this); }

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
		this->event_invoker<saved_e>()(this);
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

		auto de = create_deserializer(known_types, static_cast<project_i*>(this));
		de->deserialize_to (projectElement, this);

		_path = filePath;
		this->event_invoker<loaded_e>()(this);
	}

	virtual void pause_simulation() override final
	{
		_simulationPaused = true;
		this->event_invoker<invalidate_e>()(this);
	}

	virtual void resume_simulation() override final
	{
		_simulationPaused = false;
		this->event_invoker<invalidate_e>()(this);
	}

	virtual bool simulation_paused() const override final { return _simulationPaused; }

	virtual bool GetChangedFlag() const override final { return _changedFlag; }

	virtual void SetChangedFlag (bool changedFlag) override final
	{
		if (changedFlag)
		{
			this->event_invoker<ChangedEvent>()(this);
			this->event_invoker<invalidate_e>()(this);
		}

		if (_changedFlag != changedFlag)
		{
			_changedFlag = changedFlag;
			this->event_invoker<changed_flag_changed_event>()(this);
		}
	}

	virtual changed_flag_changed_event::subscriber changed_flag_changed() override final { return changed_flag_changed_event::subscriber(this); }

	virtual ChangedEvent::subscriber GetChangedEvent() override final { return ChangedEvent::subscriber(this); }

	virtual const typed_object_collection_property<bridge>* bridges_prop() const override final { return &bridges_property; }

	virtual const typed_object_collection_property<wire>* wires_prop() const override final { return &wires_property; }

	virtual property_changing_e::subscriber property_changing() override final { return property_changing_e::subscriber(this); }

	virtual property_changed_e::subscriber property_changed() override final { return property_changed_e::subscriber(this); }

	mac_address next_mac_address() const { return _next_mac_address; }

	void set_next_mac_address (mac_address value)
	{
		if (_next_mac_address != value)
		{
			this->on_property_changing(&next_mac_address_property);
			_next_mac_address = value;
			this->on_property_changed(&next_mac_address_property);
		}
	}

	static constexpr mac_address_p next_mac_address_property = {
		"NextMacAddress", nullptr, nullptr, false,
		&next_mac_address,
		&set_next_mac_address,
		mac_address{ 0x00, 0xAA, 0x55, 0xAA, 0x55, 0x80 },
	};

	static const typed_object_collection_property<bridge> bridges_property;
	static const typed_object_collection_property<wire> wires_property;
	static constexpr const property* const _properties[] = { &next_mac_address_property, &bridges_property, &wires_property };
public:
	static inline const xtype<project> _type = { "Project", &base::_type, _properties };
	virtual const concrete_type* type() const { return &_type; }
};

const typed_object_collection_property<bridge> project::bridges_property = {
	"Bridges", nullptr, nullptr, false,
	false, [](object* o) -> typed_object_collection_i<bridge>* { return static_cast<project*>(o); }
};

const typed_object_collection_property<wire> project::wires_property {
	"Wires", nullptr, nullptr, false,
	false, [](object* o) -> typed_object_collection_i<wire>* { return static_cast<project*>(o); }
};

extern std::shared_ptr<project_i> project_factory() { return std::make_shared<project>(); };
