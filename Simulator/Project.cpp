
#include "pch.h"
#include "Simulator.h"
#include "Wire.h"
#include "Bridge.h"
#include "Port.h"

using namespace std;

class Project : public EventManager, public IProject
{
	ULONG _refCount = 1;
	wstring _path;
	vector<unique_ptr<Bridge>> _bridges;
	vector<unique_ptr<Wire>> _wires;
	STP_BRIDGE_ADDRESS _nextMacAddress = { 0x00, 0xAA, 0x55, 0xAA, 0x55, 0x80 };

public:
	virtual const vector<unique_ptr<Bridge>>& GetBridges() const override final { return _bridges; }

	virtual void InsertBridge (size_t index, unique_ptr<Bridge>&& bridge, std::vector<ConvertedWirePoint>* convertedWirePoints) override final
	{
		if (index > _bridges.size())
			throw invalid_argument("index");

		Bridge* b = bridge.get();
		auto it = _bridges.insert (_bridges.begin() + index, (move(bridge)));
		b->GetInvalidateEvent().AddHandler (&OnObjectInvalidate, this);
		BridgeInsertedEvent::InvokeHandlers (*this, this, index, b);
		InvalidateEvent::InvokeHandlers (*this, this);

		if (convertedWirePoints != nullptr)
		{
			for (size_t i = convertedWirePoints->size() - 1; i != -1; i--)
			{
				auto& cwp = convertedWirePoints->at(i);
				if (cwp.port->GetBridge() == b)
				{
					cwp.wire->SetPoint(cwp.pointIndex, cwp.port);
					convertedWirePoints->erase (convertedWirePoints->begin() + i);
				}
			}
		}
	}

	virtual unique_ptr<Bridge> RemoveBridge(size_t index, vector<ConvertedWirePoint>* convertedWirePointsToAddTo) override final
	{
		if (index >= _bridges.size())
			throw invalid_argument("index");

		Bridge* b = _bridges[index].get();

		size_t wireIndex = 0;
		while (wireIndex < _wires.size())
		{
			Wire* w = _wires[wireIndex].get();
			for (size_t i = 0; i < w->GetPoints().size(); i++)
			{
				auto& point = w->GetPoints()[i];
				if (holds_alternative<ConnectedWireEnd>(point))
				{
					auto port = get<ConnectedWireEnd>(point);
					if (port->GetBridge() == b)
					{
						assert (convertedWirePointsToAddTo != nullptr);
						convertedWirePointsToAddTo->push_back(ConvertedWirePoint { w, i, port });
						w->SetPoint(i, w->GetPointCoords(i));
					}
				}

				wireIndex++;
			}
		}

		BridgeRemovingEvent::InvokeHandlers(*this, this, index, b);
		_bridges[index]->GetInvalidateEvent().RemoveHandler (&OnObjectInvalidate, this);
		auto result = move(_bridges[index]);
		_bridges.erase (_bridges.begin() + index);
		InvalidateEvent::InvokeHandlers (*this, this);
		return result;
	}

	virtual const vector<unique_ptr<Wire>>& GetWires() const override final { return _wires; }

	virtual void InsertWire (size_t index, unique_ptr<Wire>&& wire) override final
	{
		if (index > _wires.size())
			throw invalid_argument("index");

		Wire* w = wire.get();
		auto it = _wires.insert (_wires.begin() + index, move(wire));
		w->GetInvalidateEvent().AddHandler (&OnObjectInvalidate, this);
		WireInsertedEvent::InvokeHandlers (*this, this, index, w);
		InvalidateEvent::InvokeHandlers (*this, this);
	}

	virtual unique_ptr<Wire> RemoveWire (size_t index) override final
	{
		if (index >= _wires.size())
			throw invalid_argument("index");

		WireRemovingEvent::InvokeHandlers(*this, this, index, _wires[index].get());
		_wires[index]->GetInvalidateEvent().RemoveHandler (&OnObjectInvalidate, this);
		auto result = move(_wires[index]);
		_wires.erase(_wires.begin() + index);
		InvalidateEvent::InvokeHandlers (*this, this);
		return result;
	}

	static void OnObjectInvalidate (void* callbackArg, Object* object)
	{
		auto project = static_cast<Project*>(callbackArg);
		InvalidateEvent::InvokeHandlers (*project, project);
	}

	virtual BridgeInsertedEvent::Subscriber GetBridgeInsertedEvent() override final { return BridgeInsertedEvent::Subscriber(this); }
	virtual BridgeRemovingEvent::Subscriber GetBridgeRemovingEvent() override final { return BridgeRemovingEvent::Subscriber(this); }

	virtual WireInsertedEvent::Subscriber GetWireInsertedEvent() override final { return WireInsertedEvent::Subscriber(this); }
	virtual WireRemovingEvent::Subscriber GetWireRemovingEvent() override final { return WireRemovingEvent::Subscriber(this); }

	virtual InvalidateEvent::Subscriber GetInvalidateEvent() override final { return InvalidateEvent::Subscriber(this); }
	virtual LoadedEvent::Subscriber GetLoadedEvent() override final { return LoadedEvent::Subscriber(this); }

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
				throw not_implemented_exception();
		}

		return result;
	}

	virtual const std::wstring& GetFilePath() const override final { return _path; }

	virtual HRESULT Save (const wchar_t* filePath) override final
	{
		IXMLDOMDocument3Ptr doc;
		HRESULT hr = CoCreateInstance (CLSID_DOMDocument60, nullptr, CLSCTX_INPROC_SERVER, __uuidof(doc), (void**) &doc);
		if (FAILED(hr))
			return hr;

		IXMLDOMElementPtr projectElement;
		hr = doc->createElement (_bstr_t("Project"), &projectElement); ThrowIfFailed(hr);
		hr = doc->appendChild (projectElement, nullptr); ThrowIfFailed(hr);

		IXMLDOMElementPtr bridgesElement;
		hr = doc->createElement (_bstr_t("Bridges"), &bridgesElement); ThrowIfFailed(hr);
		hr = projectElement->appendChild (bridgesElement, nullptr); ThrowIfFailed(hr);
		for (auto& b : _bridges)
		{
			auto e = b->Serialize(doc);
			hr = bridgesElement->appendChild (e, nullptr); ThrowIfFailed(hr);
		}

		IXMLDOMElementPtr wiresElement;
		hr = doc->createElement (_bstr_t("Wires"), &wiresElement); ThrowIfFailed(hr);
		hr = projectElement->appendChild (wiresElement, nullptr); ThrowIfFailed(hr);
		for (auto& w : _wires)
		{
			hr = wiresElement->appendChild (w->Serialize(doc), nullptr);
			ThrowIfFailed(hr);
		}

		FormatAndSaveToFile (doc, filePath);
		_path = filePath;

		return S_OK;
	}

	void FormatAndSaveToFile (IXMLDOMDocument3* doc, const wchar_t* path) const
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

		IXMLDOMDocument3Ptr loadXML;
		HRESULT hr = CoCreateInstance (CLSID_DOMDocument60, nullptr, CLSCTX_INPROC_SERVER, __uuidof(loadXML), (void**) &loadXML); ThrowIfFailed(hr);
		VARIANT_BOOL successful;
		hr = loadXML->loadXML (_bstr_t(StylesheetText), &successful); ThrowIfFailed(hr);

		//Create the final document which will be indented properly
		IXMLDOMDocument3Ptr pXMLFormattedDoc;
		hr = CoCreateInstance(CLSID_DOMDocument60, nullptr, CLSCTX_INPROC_SERVER, __uuidof(pXMLFormattedDoc), (void**) &pXMLFormattedDoc); ThrowIfFailed(hr);

		IDispatchPtr pDispatch;
		hr = pXMLFormattedDoc->QueryInterface(IID_IDispatch, (void**)&pDispatch); ThrowIfFailed(hr);

		_variant_t vtOutObject;
		vtOutObject.vt = VT_DISPATCH;
		vtOutObject.pdispVal = pDispatch;
		vtOutObject.pdispVal->AddRef();

		//Apply the transformation to format the final document
		hr = doc->transformNodeToObject(loadXML,vtOutObject);

		// By default it is writing the encoding = UTF-16. Let us change the encoding to UTF-8
		IXMLDOMNodePtr firstChild;
		hr = pXMLFormattedDoc->get_firstChild(&firstChild); ThrowIfFailed(hr);
		IXMLDOMNamedNodeMapPtr pXMLAttributeMap;
		hr = firstChild->get_attributes(&pXMLAttributeMap); ThrowIfFailed(hr);
		IXMLDOMNodePtr encodingNode;
		hr = pXMLAttributeMap->getNamedItem(_bstr_t("encoding"), &encodingNode); ThrowIfFailed(hr);
		encodingNode->put_nodeValue (_variant_t("UTF-8"));

		hr = pXMLFormattedDoc->save(_variant_t(path)); ThrowIfFailed(hr);
	}

	virtual void Load (const wchar_t* filePath) override final
	{
		IXMLDOMDocument3Ptr doc;
		HRESULT hr = CoCreateInstance (CLSID_DOMDocument60, nullptr, CLSCTX_INPROC_SERVER, __uuidof(doc), (void**) &doc); ThrowIfFailed(hr);

		VARIANT_BOOL isSuccessful;
		hr = doc->load(_variant_t(filePath), &isSuccessful); ThrowIfFailed(hr);
		if (isSuccessful != VARIANT_TRUE)
			throw runtime_error("Load failed.");

		IXMLDOMNodePtr xmlDeclarationNode;
		hr = doc->get_firstChild(&xmlDeclarationNode); ThrowIfFailed(hr);
		_bstr_t nodeName;
		hr = xmlDeclarationNode->get_nodeName(nodeName.GetAddress()); ThrowIfFailed(hr);
		if (_wcsicmp (nodeName.GetBSTR(), L"xml") != 0)
			throw runtime_error("Missing XML declaration.");

		IXMLDOMNodePtr projectNode;
		hr = xmlDeclarationNode->get_nextSibling(&projectNode); ThrowIfFailed(hr);
		hr = projectNode->get_nodeName(nodeName.GetAddress()); ThrowIfFailed(hr);
		if (_wcsicmp (nodeName.GetBSTR(), L"Project") != 0)
			throw runtime_error("Missing \"Project\" element in the XML.");

		{
			IXMLDOMNodeListPtr bridgeNodes;
			hr = doc->selectNodes(_bstr_t("Project/Bridges/Bridge"), &bridgeNodes); ThrowIfFailed(hr);
			long bridgeCount;
			hr = bridgeNodes->get_length(&bridgeCount); ThrowIfFailed(hr);
			for (long i = 0; i < bridgeCount; i++)
			{
				IXMLDOMNodePtr bridgeNode;
				hr = bridgeNodes->get_item(i, &bridgeNode); ThrowIfFailed(hr);
				IXMLDOMElementPtr bridgeElement;
				hr = bridgeNode->QueryInterface(&bridgeElement); ThrowIfFailed(hr);
				auto bridge = Bridge::Deserialize(this, bridgeElement);
				this->InsertBridge(_bridges.size(), move(bridge), nullptr);
			}
		}

		{
			IXMLDOMNodeListPtr wireNodes;
			hr = doc->selectNodes(_bstr_t("Project/Wires/Wire"), &wireNodes); ThrowIfFailed(hr);
			long wireCount;
			hr = wireNodes->get_length(&wireCount); ThrowIfFailed(hr);
			for (long i = 0; i < wireCount; i++)
			{
				IXMLDOMNodePtr wireNode;
				hr = wireNodes->get_item(i, &wireNode); ThrowIfFailed(hr);
				IXMLDOMElementPtr wireElement;
				hr = wireNode->QueryInterface(&wireElement); ThrowIfFailed(hr);
				auto wire = Wire::Deserialize(wireElement);
				this->InsertWire(_wires.size(), move(wire));
			}
		}

		LoadedEvent::InvokeHandlers(*this, this);
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override { return E_NOTIMPL; }

	virtual ULONG STDMETHODCALLTYPE AddRef() override final
	{
		return InterlockedIncrement(&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release() override final
	{
		assert (_refCount > 0);
		ULONG newRefCount = InterlockedDecrement(&_refCount);
		if (newRefCount == 0)
			delete this;
		return newRefCount;
	}
};

template<typename... Args>
static IProjectPtr Create (Args... args)
{
	return IProjectPtr(new Project(std::forward<Args>(args)...), false);
}

extern const ProjectFactory projectFactory = &Create;
