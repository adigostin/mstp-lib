
#include "pch.h"
#include "simulator.h"
#include "wire.h"
#include "bridge.h"
#include "port.h"
#include "win32/xml_serializer.h"

static const _bstr_t NextMacAddressString = "NextMacAddress";

class project : public edge::object, public project_i
{
	using base = edge::object;

	std::wstring _path;
	std::vector<std::unique_ptr<bridge>> _bridges;
	std::vector<std::unique_ptr<wire>> _wires;
	mac_address _next_mac_address = next_mac_address_property._default_value.value();
	bool _simulationPaused = false;
	bool _changedFlag = false;

public:
	virtual const std::vector<std::unique_ptr<bridge>>& bridges() const override final { return _bridges; }

	virtual void insert_bridge (size_t index, std::unique_ptr<bridge>&& bridge) override final
	{
		assert (index <= _bridges.size());
		assert (bridge->_project == nullptr);
		auto b = bridge.get();

		property_change_args args = { &bridges_property, index, collection_property_change_type::insert };
		this->on_property_changing(args);
		_bridges.insert (_bridges.begin() + index, std::move(bridge));
		static_cast<project_child*>(b)->on_added_to_project(this);
		this->on_property_changed(args);

		b->invalidated().add_handler (&on_project_child_invalidated, this);
		b->packet_transmit().add_handler (&on_packet_transmit, this);
		this->event_invoker<invalidate_e>()(this);
	}

	virtual std::unique_ptr<bridge> remove_bridge(size_t index) override final
	{
		assert (index < _bridges.size());
		bridge* b = _bridges[index].get();
		assert (b->_project == this);

		if (std::any_of (_wires.begin(), _wires.end(), [b, this](const std::unique_ptr<wire>& w) {
			return any_of (w->points().begin(), w->points().end(), [b, this] (wire_end p) {
				return std::holds_alternative<connected_wire_end>(p) && (std::get<connected_wire_end>(p)->bridge() == b);
			});
		}))
			assert(false); // can't remove a connected bridge

		b->packet_transmit().remove_handler (&on_packet_transmit, this);
		b->invalidated().remove_handler (&on_project_child_invalidated, this);

		property_change_args args = { &bridges_property, index, collection_property_change_type::remove };
		this->on_property_changing(args);
		static_cast<project_child*>(b)->on_removing_from_project(this);
		auto result = std::move(_bridges[index]);
		_bridges.erase (_bridges.begin() + index);
		this->on_property_changed(args);

		this->event_invoker<invalidate_e>()(this);
		return result;
	}

	virtual const std::vector<std::unique_ptr<wire>>& wires() const override final { return _wires; }

	virtual void insert_wire (size_t index, std::unique_ptr<wire>&& wire) override final
	{
		assert (index <= _wires.size());
		assert (wire->_project == nullptr);
		auto w = wire.get();

		property_change_args args = { &wires_property, index, collection_property_change_type::insert };
		this->on_property_changing(args);
		_wires.insert (_wires.begin() + index, std::move(wire));
		static_cast<project_child*>(w)->on_added_to_project(this);
		this->on_property_changed(args);

		w->invalidated().add_handler (&on_project_child_invalidated, this);
		this->event_invoker<invalidate_e>()(this);
	}

	virtual std::unique_ptr<wire> remove_wire (size_t index) override final
	{
		assert (index < _wires.size());
		wire* w = _wires[index].get();
		assert(w->_project == this);

		_wires[index]->invalidated().remove_handler (&on_project_child_invalidated, this);

		property_change_args args = { &wires_property, index, collection_property_change_type::remove };
		this->on_property_changing (args);
		static_cast<project_child*>(w)->on_removing_from_project(this);
		auto result = std::move(_wires[index]);
		_wires.erase (_wires.begin() + index);
		this->on_property_changed (args);

		this->event_invoker<invalidate_e>()(this);
		return result;
	}

	static void on_packet_transmit (void* callbackArg, bridge* bridge, size_t txPortIndex, packet_t&& pi)
	{
		auto project = static_cast<class project*>(callbackArg);
		auto tx_port = bridge->ports().at(txPortIndex).get();
		auto rx_port = project->find_connected_port(tx_port);
		if (rx_port != nullptr)
			rx_port->bridge()->enqueue_received_packet(std::move(pi), rx_port->port_index());
	}

	static void on_project_child_invalidated (void* callbackArg, renderable_object* object)
	{
		auto project = static_cast<class project*>(callbackArg);
		project->event_invoker<invalidate_e>()(project);
	}

	virtual invalidate_e::subscriber invalidated() override final { return invalidate_e::subscriber(this); }

	virtual loaded_e::subscriber GetLoadedEvent() override final { return loaded_e::subscriber(this); }

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
				assert(false); // not implemented
		}

		return result;
	}

	virtual const std::wstring& file_path() const override final { return _path; }

	virtual HRESULT save (const wchar_t* filePath) override final
	{
		com_ptr<IXMLDOMDocument3> doc;
		HRESULT hr = CoCreateInstance (CLSID_DOMDocument60, nullptr, CLSCTX_INPROC_SERVER, __uuidof(doc), (void**) &doc);
		if (FAILED(hr))
			return hr;

		auto project_element = serialize (doc, this, true);

		hr = doc->appendChild (project_element, nullptr);
		if (FAILED(hr))
			return hr;

		hr = format_and_save_to_file (doc, filePath);
		if (FAILED(hr))
			return hr;

		_path = filePath;
		return S_OK;
	}
	
	virtual HRESULT load (const wchar_t* filePath) override final
	{
		com_ptr<IXMLDOMDocument3> doc;
		HRESULT hr = CoCreateInstance (CLSID_DOMDocument60, nullptr, CLSCTX_INPROC_SERVER, __uuidof(doc), (void**) &doc); if (FAILED(hr)) return hr;

		if (!PathFileExists(filePath))
			return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

		VARIANT_BOOL isSuccessful;
		hr = doc->load(_variant_t(filePath), &isSuccessful); if (FAILED(hr)) return hr;
		if (isSuccessful != VARIANT_TRUE)
			return E_FAIL;

		com_ptr<IXMLDOMNode> xmlDeclarationNode;
		hr = doc->get_firstChild(&xmlDeclarationNode); if (FAILED(hr)) return hr;
		_bstr_t nodeName;
		hr = xmlDeclarationNode->get_nodeName(nodeName.GetAddress()); if (FAILED(hr)) return hr;
		if (_wcsicmp (nodeName.GetBSTR(), L"xml") != 0)
			return E_FAIL;

		com_ptr<IXMLDOMNode> projectNode;
		hr = xmlDeclarationNode->get_nextSibling(&projectNode); assert(SUCCEEDED(hr));
		hr = projectNode->get_nodeName(nodeName.GetAddress()); assert(SUCCEEDED(hr));
		if (_wcsicmp (nodeName.GetBSTR(), L"Project") != 0)
			return E_FAIL;
		com_ptr<IXMLDOMElement> projectElement = projectNode;

		deserialize_to (projectElement, this);

		_path = filePath;
		this->event_invoker<loaded_e>()(this);
		return S_OK;
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
			this->event_invoker<ChangedFlagChangedEvent>()(this);
		}
	}

	virtual ChangedFlagChangedEvent::subscriber GetChangedFlagChangedEvent() override final { return ChangedFlagChangedEvent::subscriber(this); }

	virtual ChangedEvent::subscriber GetChangedEvent() override final { return ChangedEvent::subscriber(this); }

	virtual const object_collection_property* bridges_prop() const override final { return &bridges_property; }
	
	virtual const object_collection_property* wires_prop() const override final { return &wires_property; }

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
		"NextMacAddress", nullptr, nullptr, ui_visible::yes,
		static_cast<mac_address_p::member_getter_t>(&next_mac_address),
		static_cast<mac_address_p::member_setter_t>(&set_next_mac_address),
		mac_address{ 0x00, 0xAA, 0x55, 0xAA, 0x55, 0x80 },
	};

	size_t bridge_count() const { return _bridges.size(); }
	bridge* bridge_at(size_t index) const { return _bridges[index].get(); }
	size_t wire_count() const { return _wires.size(); }
	wire* wire_at(size_t index) const { return _wires[index].get(); }

	static const typed_object_collection_property<class project, bridge> bridges_property;
	static const typed_object_collection_property<class project, wire> wires_property;
	static const property* const _properties[];
	static const xtype<class project> _type;
	virtual const struct type* type() const { return &_type; }
};

const typed_object_collection_property<project, bridge> project::bridges_property = {
	"Bridges", nullptr, nullptr, ui_visible::no,
	&bridge_count, &bridge_at, &insert_bridge, &remove_bridge
};

const typed_object_collection_property<project, wire> project::wires_property {
	"Wires", nullptr, nullptr, ui_visible::no,
	&wire_count, &wire_at, &insert_wire, &remove_wire
};

const property* const project::_properties[] = { &next_mac_address_property, &bridges_property, &wires_property };

const xtype<project> project::_type = { "Project", &base::_type, _properties, [] { return new project(); } };

extern const project_factory_t project_factory = []() -> std::shared_ptr<project_i> { return std::make_shared<project>(); };
