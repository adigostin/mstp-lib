
#include "pch.h"
#include "Simulator.h"
#include "Wire.h"
#include "Bridge.h"
#include "Port.h"

using namespace std;
using namespace edge;

static const _bstr_t NextMacAddressString = "NextMacAddress";

class Project : public event_manager, public IProject
{
	ULONG _refCount = 1;
	wstring _path;
	vector<unique_ptr<Bridge>> _bridges;
	vector<unique_ptr<Wire>> _wires;
	STP_BRIDGE_ADDRESS _nextMacAddress = { 0x00, 0xAA, 0x55, 0xAA, 0x55, 0x80 };
	bool _simulationPaused = false;
	bool _changedFlag = false;

public:
	virtual const vector<unique_ptr<Bridge>>& GetBridges() const override final { return _bridges; }

	virtual void InsertBridge (size_t index, unique_ptr<Bridge>&& bridge) override final
	{
		if (index > _bridges.size())
			throw invalid_argument("index");

		Bridge* b = bridge.get();
		auto it = _bridges.insert (_bridges.begin() + index, (move(bridge)));
		b->GetInvalidateEvent().add_handler (&OnObjectInvalidate, this);
		b->GetPacketTransmitEvent().add_handler (&OnPacketTransmit, this);
		b->GetLinkPulseEvent().add_handler (&OnLinkPulse, this);
		this->event_invoker<BridgeInsertedEvent>()(this, index, b);
		this->event_invoker<invalidate_e>()(this);
	}

	virtual unique_ptr<Bridge> RemoveBridge(size_t index) override final
	{
		if (index >= _bridges.size())
			throw invalid_argument("index");

		Bridge* b = _bridges[index].get();

		if (any_of (_wires.begin(), _wires.end(), [b](const unique_ptr<Wire>& w) {
			return any_of (w->GetPoints().begin(), w->GetPoints().end(), [b] (WireEnd p) {
				return std::holds_alternative<ConnectedWireEnd>(p) && (std::get<ConnectedWireEnd>(p)->GetBridge() == b);
			});
		}))
			throw invalid_argument("cannot remove a connected bridge");

		this->event_invoker<BridgeRemovingEvent>()(this, index, b);
		_bridges[index]->GetLinkPulseEvent().remove_handler (&OnLinkPulse, this);
		_bridges[index]->GetPacketTransmitEvent().remove_handler (&OnPacketTransmit, this);
		_bridges[index]->GetInvalidateEvent().remove_handler (&OnObjectInvalidate, this);
		auto result = move(_bridges[index]);
		_bridges.erase (_bridges.begin() + index);
		this->event_invoker<invalidate_e>()(this);
		return result;
	}

	virtual const vector<unique_ptr<Wire>>& GetWires() const override final { return _wires; }

	virtual void InsertWire (size_t index, unique_ptr<Wire>&& wire) override final
	{
		if (index > _wires.size())
			throw invalid_argument("index");

		Wire* w = wire.get();
		auto it = _wires.insert (_wires.begin() + index, move(wire));
		w->GetInvalidateEvent().add_handler (&OnObjectInvalidate, this);
		this->event_invoker<WireInsertedEvent>()(this, index, w);
		this->event_invoker<invalidate_e>()(this);
	}

	virtual unique_ptr<Wire> RemoveWire (size_t index) override final
	{
		if (index >= _wires.size())
			throw invalid_argument("index");

		this->event_invoker<WireRemovingEvent>()(this, index, _wires[index].get());
		_wires[index]->GetInvalidateEvent().remove_handler (&OnObjectInvalidate, this);
		auto result = move(_wires[index]);
		_wires.erase(_wires.begin() + index);
		this->event_invoker<invalidate_e>()(this);
		return result;
	}

	static void OnPacketTransmit (void* callbackArg, Bridge* bridge, size_t txPortIndex, PacketInfo&& pi)
	{
		auto project = static_cast<Project*>(callbackArg);
		auto txPort = bridge->GetPorts().at(txPortIndex).get();
		auto rxPort = project->FindConnectedPort(txPort);
		if (rxPort != nullptr)
		{
			pi.txPortPath.push_back (bridge->GetPortAddress(txPortIndex));
			rxPort->GetBridge()->EnqueuePacket(move(pi), rxPort->GetPortIndex());
		}
	}

	static void OnLinkPulse (void* callbackArg, Bridge* bridge, size_t txPortIndex, unsigned int timestamp)
	{
		auto project = static_cast<Project*>(callbackArg);
		auto txPort = bridge->GetPorts().at(txPortIndex).get();
		auto rxPort = project->FindConnectedPort(txPort);
		if (rxPort != nullptr)
			rxPort->GetBridge()->ProcessLinkPulse(rxPort->GetPortIndex(), timestamp);
	}

	static void OnObjectInvalidate (void* callbackArg, renderable_object* object)
	{
		auto project = static_cast<Project*>(callbackArg);
		project->event_invoker<invalidate_e>()(project);
	}

	virtual BridgeInsertedEvent::subscriber GetBridgeInsertedEvent() override final { return BridgeInsertedEvent::subscriber(this); }
	virtual BridgeRemovingEvent::subscriber GetBridgeRemovingEvent() override final { return BridgeRemovingEvent::subscriber(this); }

	virtual WireInsertedEvent::subscriber GetWireInsertedEvent() override final { return WireInsertedEvent::subscriber(this); }
	virtual WireRemovingEvent::subscriber GetWireRemovingEvent() override final { return WireRemovingEvent::subscriber(this); }

	virtual invalidate_e::subscriber GetInvalidateEvent() override final { return invalidate_e::subscriber(this); }
	virtual LoadedEvent::subscriber GetLoadedEvent() override final { return LoadedEvent::subscriber(this); }

	virtual bool IsWireForwarding (Wire* wire, unsigned int vlanNumber, _Out_opt_ bool* hasLoop) const override final
	{
		if (!holds_alternative<ConnectedWireEnd>(wire->GetP0()) || !holds_alternative<ConnectedWireEnd>(wire->GetP1()))
			return false;

		auto portA = get<ConnectedWireEnd>(wire->GetP0());
		auto portB = get<ConnectedWireEnd>(wire->GetP1());
		bool portAFw = portA->IsForwarding(vlanNumber);
		bool portBFw = portB->IsForwarding(vlanNumber);
		if (!portAFw || !portBFw)
			return false;

		if (hasLoop != nullptr)
		{
			unordered_set<Port*> txPorts;

			function<bool(Port* txPort)> transmitsTo = [this, vlanNumber, &txPorts, &transmitsTo, targetPort=portA](Port* txPort) -> bool
			{
				if (txPort->IsForwarding(vlanNumber))
				{
					auto rx = FindConnectedPort(txPort);
					if ((rx != nullptr) && rx->IsForwarding(vlanNumber))
					{
						txPorts.insert(txPort);

						for (unsigned int i = 0; i < (unsigned int) rx->GetBridge()->GetPorts().size(); i++)
						{
							if ((i != rx->GetPortIndex()) && rx->IsForwarding(vlanNumber))
							{
								Port* otherTxPort = rx->GetBridge()->GetPorts()[i].get();
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

	virtual STP_BRIDGE_ADDRESS AllocMacAddressRange (size_t count) override final
	{
		if (count >= 128)
			throw range_error("count must be lower than 128.");

		auto result = _nextMacAddress;
		_nextMacAddress.bytes[5] += (uint8_t)count;
		if (_nextMacAddress.bytes[5] < count)
		{
			_nextMacAddress.bytes[4]++;
			if (_nextMacAddress.bytes[4] == 0)
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

		com_ptr<IXMLDOMElement> projectElement;
		hr = doc->createElement (_bstr_t("Project"), &projectElement); assert(SUCCEEDED(hr));
		projectElement->setAttribute (NextMacAddressString, _variant_t(ConvertBridgeAddressToString(_nextMacAddress.bytes).c_str()));
		hr = doc->appendChild (projectElement, nullptr); assert(SUCCEEDED(hr));

		com_ptr<IXMLDOMElement> bridgesElement;
		hr = doc->createElement (_bstr_t("Bridges"), &bridgesElement); assert(SUCCEEDED(hr));
		hr = projectElement->appendChild (bridgesElement, nullptr); assert(SUCCEEDED(hr));
		for (size_t bridgeIndex = 0; bridgeIndex < _bridges.size(); bridgeIndex++)
		{
			auto b = _bridges.at(bridgeIndex).get();
			auto e = b->Serialize(bridgeIndex, doc);
			hr = bridgesElement->appendChild (e, nullptr); assert(SUCCEEDED(hr));
		}

		com_ptr<IXMLDOMElement> wiresElement;
		hr = doc->createElement (_bstr_t("Wires"), &wiresElement); assert(SUCCEEDED(hr));
		hr = projectElement->appendChild (wiresElement, nullptr); assert(SUCCEEDED(hr));
		for (auto& w : _wires)
		{
			hr = wiresElement->appendChild (w->Serialize(this, doc), nullptr);
			assert(SUCCEEDED(hr));
		}

		hr = FormatAndSaveToFile (doc, filePath);
		if (FAILED(hr))
			return hr;

		_path = filePath;
		return S_OK;
	}

	HRESULT FormatAndSaveToFile (IXMLDOMDocument3* doc, const wchar_t* path) const
	{
		static const char StylesheetText[] =
			"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
			"<xsl:stylesheet xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\" version=\"1.0\">\n"
			"  <xsl:output method=\"xml\" indent=\"yes\" omit-xml-declaration=\"no\" />\n"
			"  <xsl:template match=\"@* | node()\">\n"
			"    <xsl:copy>\n"
			"      <xsl:apply-templates select=\"@* | node()\"/>\n"
			"    </xsl:copy>\n"
			"  </xsl:template>\n"
			"</xsl:stylesheet>\n"
			"";

		com_ptr<IXMLDOMDocument3> loadXML;
		HRESULT hr = CoCreateInstance (CLSID_DOMDocument60, nullptr, CLSCTX_INPROC_SERVER, __uuidof(loadXML), (void**) &loadXML);
		if (FAILED(hr))
			return hr;
		VARIANT_BOOL successful;
		hr = loadXML->loadXML (_bstr_t(StylesheetText), &successful);
		if (FAILED(hr))
			return hr;

		// Create the final document which will be indented properly.
		com_ptr<IXMLDOMDocument3> pXMLFormattedDoc;
		hr = CoCreateInstance(CLSID_DOMDocument60, nullptr, CLSCTX_INPROC_SERVER, __uuidof(pXMLFormattedDoc), (void**) &pXMLFormattedDoc);
		if (FAILED(hr))
			return hr;

		com_ptr<IDispatch> pDispatch;
		hr = pXMLFormattedDoc->QueryInterface(IID_IDispatch, (void**)&pDispatch);
		if (FAILED(hr))
			return hr;

		_variant_t vtOutObject;
		vtOutObject.vt = VT_DISPATCH;
		vtOutObject.pdispVal = pDispatch;
		vtOutObject.pdispVal->AddRef();

		// Apply the transformation to format the final document.
		hr = doc->transformNodeToObject(loadXML,vtOutObject);
		if (FAILED(hr))
			return hr;

		// By default it writes the encoding UTF-16; let's change it to UTF-8.
		com_ptr<IXMLDOMNode> firstChild;
		hr = pXMLFormattedDoc->get_firstChild(&firstChild);
		if (FAILED(hr))
			return hr;
		com_ptr<IXMLDOMNamedNodeMap> pXMLAttributeMap;
		hr = firstChild->get_attributes(&pXMLAttributeMap);
		if (FAILED(hr))
			return hr;
		com_ptr<IXMLDOMNode> encodingNode;
		hr = pXMLAttributeMap->getNamedItem(_bstr_t("encoding"), &encodingNode);
		if (FAILED(hr))
			return hr;
		encodingNode->put_nodeValue (_variant_t("UTF-8"));

		hr = pXMLFormattedDoc->save(_variant_t(path));
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

		_variant_t value;
		hr = projectElement->getAttribute (NextMacAddressString, &value);
		if (SUCCEEDED(hr) && (value.vt == VT_BSTR))
			_nextMacAddress = ConvertStringToBridgeAddress (static_cast<_bstr_t>(value));

		{
			com_ptr<IXMLDOMNodeList> bridgeNodes;
			hr = doc->selectNodes(_bstr_t("Project/Bridges/Bridge"), &bridgeNodes); assert(SUCCEEDED(hr));
			long bridgeCount;
			hr = bridgeNodes->get_length(&bridgeCount); assert(SUCCEEDED(hr));
			for (long i = 0; i < bridgeCount; i++)
			{
				com_ptr<IXMLDOMNode> bridgeNode;
				hr = bridgeNodes->get_item(i, &bridgeNode); assert(SUCCEEDED(hr));
				com_ptr<IXMLDOMElement> bridgeElement;
				hr = bridgeNode->QueryInterface(&bridgeElement); assert(SUCCEEDED(hr));
				auto bridge = Bridge::Deserialize(bridgeElement);
				this->InsertBridge(_bridges.size(), move(bridge));
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
				auto wire = Wire::Deserialize (this, wireElement);
				this->InsertWire(_wires.size(), move(wire));
			}
		}

		_path = filePath;
		this->event_invoker<LoadedEvent>()(this);
	}

	virtual void PauseSimulation() override final
	{
		_simulationPaused = true;
		for (auto& b : _bridges)
			b->PauseSimulation();
		this->event_invoker<invalidate_e>()(this);
	}

	virtual void ResumeSimulation() override final
	{
		_simulationPaused = false;
		for (auto& b : _bridges)
			b->ResumeSimulation();
		this->event_invoker<invalidate_e>()(this);
	}

	virtual bool IsSimulationPaused() const override final { return _simulationPaused; }

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
};

extern const ProjectFactory projectFactory = []() -> std::shared_ptr<IProject> { return std::make_shared<Project>(); };
