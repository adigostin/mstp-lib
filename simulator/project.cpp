
#include "pch.h"
#include "simulator.h"
#include "wire.h"
#include "bridge.h"
#include "port.h"
#include "win32/xml_serializer.h"

using namespace std;
using namespace edge;

static const _bstr_t NextMacAddressString = "NextMacAddress";

class project : public edge::object, public project_i
{
	using base = edge::object;

	wstring _path;
	vector<unique_ptr<bridge>> _bridges;
	vector<unique_ptr<wire>> _wires;
	mac_address _next_mac_address = next_mac_address_property._default_value.value();
	bool _simulationPaused = false;
	bool _changedFlag = false;

public:
	virtual const vector<unique_ptr<bridge>>& bridges() const override final { return _bridges; }

	virtual void insert_bridge (size_t index, unique_ptr<bridge>&& bridge) override final
	{
		assert (index <= _bridges.size());
		auto b = bridge.get();
		assert (b->project() == nullptr);
		auto it = _bridges.insert (_bridges.begin() + index, (move(bridge)));
		static_cast<project_child*>(b)->on_added_to_project(this);
		b->GetInvalidateEvent().add_handler (&OnObjectInvalidate, this);
		b->GetPacketTransmitEvent().add_handler (&OnPacketTransmit, this);
		b->GetLinkPulseEvent().add_handler (&OnLinkPulse, this);
		this->event_invoker<bridge_inserted_e>()(this, index, b);
		this->event_invoker<invalidate_e>()(this);
	}

	virtual unique_ptr<bridge> remove_bridge(size_t index) override final
	{
		assert (index < _bridges.size());
		bridge* b = _bridges[index].get();
		assert (b->project() == this);

		if (any_of (_wires.begin(), _wires.end(), [b](const unique_ptr<wire>& w) {
			return any_of (w->points().begin(), w->points().end(), [b] (wire_end p) {
				return std::holds_alternative<connected_wire_end>(p) && (std::get<connected_wire_end>(p)->bridge() == b);
			});
		}))
			assert(false); // can't remove a connected bridge

		this->event_invoker<bridge_removing_e>()(this, index, b);
		_bridges[index]->GetLinkPulseEvent().remove_handler (&OnLinkPulse, this);
		_bridges[index]->GetPacketTransmitEvent().remove_handler (&OnPacketTransmit, this);
		_bridges[index]->GetInvalidateEvent().remove_handler (&OnObjectInvalidate, this);
		static_cast<project_child*>(b)->on_removing_from_project(this);
		auto result = move(_bridges[index]);
		_bridges.erase (_bridges.begin() + index);
		this->event_invoker<invalidate_e>()(this);
		return result;
	}

	virtual const vector<unique_ptr<wire>>& wires() const override final { return _wires; }

	virtual void insert_wire (size_t index, unique_ptr<wire>&& wire) override final
	{
		assert (index <= _wires.size());
		property_change_args args = { &wires_property, index, collection_property_change_type::insert };
		this->on_property_changing (args);
		_wires.insert (_wires.begin() + index, move(wire));
		this->on_property_changed (args);

		_wires[index]->GetInvalidateEvent().add_handler (&OnObjectInvalidate, this);
		this->event_invoker<wire_inserted_e>()(this, index, _wires[index].get());

		this->event_invoker<invalidate_e>()(this);
	}

	virtual unique_ptr<wire> remove_wire (size_t index) override final
	{
		assert (index < _wires.size());

		this->event_invoker<wire_removing_e>()(this, index, _wires[index].get());

		_wires[index]->GetInvalidateEvent().remove_handler (&OnObjectInvalidate, this);

		property_change_args args = { &wires_property, index, collection_property_change_type::remove };
		this->on_property_changing (args);
		auto result = move(_wires[index]);
		_wires.erase (_wires.begin() + index);
		this->on_property_changed (args);

		this->event_invoker<invalidate_e>()(this);
		return result;
	}

	static void OnPacketTransmit (void* callbackArg, bridge* bridge, size_t txPortIndex, PacketInfo&& pi)
	{
		auto project = static_cast<class project*>(callbackArg);
		auto txPort = bridge->GetPorts().at(txPortIndex).get();
		auto rxPort = project->FindConnectedPort(txPort);
		if (rxPort != nullptr)
		{
			pi.txPortPath.push_back (bridge->GetPortAddress(txPortIndex));
			rxPort->bridge()->EnqueuePacket(move(pi), rxPort->port_index());
		}
	}

	static void OnLinkPulse (void* callbackArg, bridge* bridge, size_t txPortIndex, unsigned int timestamp)
	{
		auto project = static_cast<class project*>(callbackArg);
		auto txPort = bridge->GetPorts().at(txPortIndex).get();
		auto rxPort = project->FindConnectedPort(txPort);
		if (rxPort != nullptr)
			rxPort->bridge()->ProcessLinkPulse(rxPort->port_index(), timestamp);
	}

	static void OnObjectInvalidate (void* callbackArg, renderable_object* object)
	{
		auto project = static_cast<class project*>(callbackArg);
		project->event_invoker<invalidate_e>()(project);
	}

	virtual bridge_inserted_e::subscriber bridge_inserted() override final { return bridge_inserted_e::subscriber(this); }
	virtual bridge_removing_e::subscriber bridge_removing() override final { return bridge_removing_e::subscriber(this); }

	virtual wire_inserted_e::subscriber wire_inserted() override final { return wire_inserted_e::subscriber(this); }
	virtual wire_removing_e::subscriber wire_removing() override final { return wire_removing_e::subscriber(this); }

	virtual invalidate_e::subscriber GetInvalidateEvent() override final { return invalidate_e::subscriber(this); }
	virtual LoadedEvent::subscriber GetLoadedEvent() override final { return LoadedEvent::subscriber(this); }

	virtual bool IsWireForwarding (wire* wire, unsigned int vlanNumber, _Out_opt_ bool* hasLoop) const override final
	{
		if (!holds_alternative<connected_wire_end>(wire->p0()) || !holds_alternative<connected_wire_end>(wire->p1()))
			return false;

		auto portA = get<connected_wire_end>(wire->p0());
		auto portB = get<connected_wire_end>(wire->p1());
		bool portAFw = portA->IsForwarding(vlanNumber);
		bool portBFw = portB->IsForwarding(vlanNumber);
		if (!portAFw || !portBFw)
			return false;

		if (hasLoop != nullptr)
		{
			unordered_set<port*> txPorts;

			function<bool(port* txPort)> transmitsTo = [this, vlanNumber, &txPorts, &transmitsTo, targetPort=portA](port* txPort) -> bool
			{
				if (txPort->IsForwarding(vlanNumber))
				{
					auto rx = FindConnectedPort(txPort);
					if ((rx != nullptr) && rx->IsForwarding(vlanNumber))
					{
						txPorts.insert(txPort);

						for (unsigned int i = 0; i < (unsigned int) rx->bridge()->GetPorts().size(); i++)
						{
							if ((i != rx->port_index()) && rx->IsForwarding(vlanNumber))
							{
								port* otherTxPort = rx->bridge()->GetPorts()[i].get();
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

	virtual mac_address AllocMacAddressRange (size_t count) override final
	{
		if (count >= 128)
			throw range_error("count must be lower than 128.");

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

	virtual const std::wstring& GetFilePath() const override final { return _path; }

	virtual HRESULT Save (const wchar_t* filePath) override final
	{
		com_ptr<IXMLDOMDocument3> doc;
		HRESULT hr = CoCreateInstance (CLSID_DOMDocument60, nullptr, CLSCTX_INPROC_SERVER, __uuidof(doc), (void**) &doc);
		if (FAILED(hr))
			return hr;

		auto project_element = serialize (doc, this, true);

		hr = doc->appendChild (project_element, nullptr);
		if (FAILED(hr))
			return hr;

		hr = FormatAndSaveToFile (doc, filePath);
		if (FAILED(hr))
			return hr;

		_path = filePath;
		return S_OK;
	}

	HRESULT FormatAndSaveToFile (IXMLDOMDocument3* doc, const wchar_t* path) const
	{
		com_ptr<IStream> stream;
		auto hr = SHCreateStreamOnFileEx (path, STGM_WRITE | STGM_SHARE_DENY_WRITE | STGM_CREATE, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &stream);
		if (FAILED(hr))
			return hr;

		com_ptr<IMXWriter> writer;
		hr = CoCreateInstance (CLSID_MXXMLWriter60, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&writer));
		if (FAILED(hr))
			return hr;

		hr = writer->put_encoding (_bstr_t("utf-8"));
		if (FAILED(hr))
			return hr;

		hr = writer->put_indent (_variant_t(true));
		if (FAILED(hr))
			return hr;

		hr = writer->put_standalone (_variant_t(true));
		if (FAILED(hr))
			return hr;

		hr = writer->put_output (_variant_t(stream));
		if (FAILED(hr))
			return hr;

		com_ptr<ISAXXMLReader> saxReader;
		hr = CoCreateInstance (CLSID_SAXXMLReader60, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&saxReader));
		if (FAILED(hr))
			return hr;

		hr = saxReader->putContentHandler(com_ptr<ISAXContentHandler>(writer));
		if (FAILED(hr))
			return hr;

		hr = saxReader->putProperty(L"http://xml.org/sax/properties/lexical-handler", _variant_t(writer));
		if (FAILED(hr))
			return hr;

		hr = saxReader->parse(_variant_t(doc));
		if (FAILED(hr))
			return hr;

		hr = stream->Commit(STGC_DEFAULT);
		return hr;
	}

	virtual void Load (const wchar_t* filePath) override final
	{
		com_ptr<IXMLDOMDocument3> doc;
		HRESULT hr = CoCreateInstance (CLSID_DOMDocument60, nullptr, CLSCTX_INPROC_SERVER, __uuidof(doc), (void**) &doc); assert(SUCCEEDED(hr));

		VARIANT_BOOL isSuccessful;
		hr = doc->load(_variant_t(filePath), &isSuccessful); assert(SUCCEEDED(hr));
		if (isSuccessful != VARIANT_TRUE)
			throw runtime_error("Load failed.");

		com_ptr<IXMLDOMNode> xmlDeclarationNode;
		hr = doc->get_firstChild(&xmlDeclarationNode); assert(SUCCEEDED(hr));
		_bstr_t nodeName;
		hr = xmlDeclarationNode->get_nodeName(nodeName.GetAddress()); assert(SUCCEEDED(hr));
		if (_wcsicmp (nodeName.GetBSTR(), L"xml") != 0)
			throw runtime_error("Missing XML declaration.");

		com_ptr<IXMLDOMNode> projectNode;
		hr = xmlDeclarationNode->get_nextSibling(&projectNode); assert(SUCCEEDED(hr));
		hr = projectNode->get_nodeName(nodeName.GetAddress()); assert(SUCCEEDED(hr));
		if (_wcsicmp (nodeName.GetBSTR(), L"Project") != 0)
			throw runtime_error("Missing \"Project\" element in the XML.");
		com_ptr<IXMLDOMElement> projectElement = projectNode;

		deserialize_to (projectElement, this);
		/*
		_variant_t value;
		hr = projectElement->getAttribute (NextMacAddressString, &value);
		if (SUCCEEDED(hr) && (value.vt == VT_BSTR))
			mac_address_from_string<wchar_t>(value.bstrVal, _next_mac_address);

		{
			com_ptr<IXMLDOMNodeList> bridgeNodes;
			hr = doc->selectNodes(_bstr_t("Project/Bridges/bridge"), &bridgeNodes); assert(SUCCEEDED(hr));
			long bridgeCount;
			hr = bridgeNodes->get_length(&bridgeCount); assert(SUCCEEDED(hr));
			for (long i = 0; i < bridgeCount; i++)
			{
				com_ptr<IXMLDOMNode> bridgeNode;
				hr = bridgeNodes->get_item(i, &bridgeNode); assert(SUCCEEDED(hr));
				com_ptr<IXMLDOMElement> bridgeElement;
				hr = bridgeNode->QueryInterface(&bridgeElement); assert(SUCCEEDED(hr));
				auto bridge = bridge::Deserialize(this, bridgeElement);
				this->insert_bridge(_bridges.size(), move(bridge));
			}
		}

		{
			com_ptr<IXMLDOMNodeList> wireNodes;
			hr = doc->selectNodes(_bstr_t("Project/Wires/Wire"), &wireNodes); assert(SUCCEEDED(hr));
			long wireCount;
			hr = wireNodes->get_length(&wireCount); assert(SUCCEEDED(hr));
			for (long i = 0; i < wireCount; i++)
			{
				com_ptr<IXMLDOMNode> wireNode;
				hr = wireNodes->get_item(i, &wireNode); assert(SUCCEEDED(hr));
				com_ptr<IXMLDOMElement> wireElement;
				hr = wireNode->QueryInterface(&wireElement); assert(SUCCEEDED(hr));
				auto wire = wire::Deserialize (this, wireElement);
				this->insert_wire(_wires.size(), move(wire));
			}
		}
		*/
		_path = filePath;
		this->event_invoker<LoadedEvent>()(this);
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

	static inline mac_address_p next_mac_address_property = {
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
	static const property* _properties[];
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

const property* project::_properties[] = { &next_mac_address_property, &bridges_property, &wires_property };

const xtype<project> project::_type = { "Project", &base::_type, _properties, [] { return new project(); } };

extern const project_factory_t project_factory = []() -> std::shared_ptr<project_i> { return std::make_shared<project>(); };
