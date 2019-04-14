#include "pch.h"
#include "Bridge.h"
#include "wire.h"
#include "simulator.h"
#include "win32/d2d_window.h"

using namespace std;
using namespace D2D1;
using namespace edge;

static constexpr UINT WM_ONE_SECOND_TIMER  = WM_APP + 1;
static constexpr UINT WM_LINK_PULSE_TIMER  = WM_APP + 2;
static constexpr UINT WM_PACKET_RECEIVED   = WM_APP + 3;

static constexpr uint8_t BpduDestAddress[6] = { 1, 0x80, 0xC2, 0, 0, 0 };

std::string mac_address_to_string (mac_address address)
{
	stringstream ss;
	ss << uppercase << setfill('0') << hex
		<< setw(2) << (int) address[0] << setw(2) << (int) address[1] << setw(2) << (int) address[2]
		<< setw(2) << (int) address[3] << setw(2) << (int) address[4] << setw(2) << (int) address[5];
	return ss.str();
}

template<typename char_type> bool mac_address_from_string (std::basic_string_view<char_type> str, mac_address& to)
{
//	static constexpr char FormatErrorMessage[] = u8"Invalid address format. The Bridge Address must have the format XX:XX:XX:XX:XX:XX or XXXXXXXXXXXX (6 hex bytes).";

	int offsetMultiplier;
	if (str.size() == 12)
	{
		offsetMultiplier = 2;
	}
	else if (str.size() == 17)
	{
		if ((str[2] != ':') || (str[5] != ':') || (str[8] != ':') || (str[11] != ':') || (str[14] != ':'))
			return false;

		offsetMultiplier = 3;
	}
	else
		return false;

	for (size_t i = 0; i < 6; i++)
	{
		wchar_t ch0 = str[i * offsetMultiplier];
		wchar_t ch1 = str[i * offsetMultiplier + 1];

		if (!iswxdigit(ch0) || !iswxdigit(ch1))
			return false;

		auto hn = (ch0 <= '9') ? (ch0 - '0') : ((ch0 >= 'a') ? (ch0 - 'a' + 10) : (ch0 - 'A' + 10));
		auto ln = (ch1 <= '9') ? (ch1 - '0') : ((ch1 >= 'a') ? (ch1 - 'a' + 10) : (ch1 - 'A' + 10));
		to[i] = (hn << 4) | ln;
	}

	return true;
}

template bool mac_address_from_string (std::basic_string_view<char> from, mac_address& to);
template bool mac_address_from_string (std::basic_string_view<wchar_t> from, mac_address& to);

#pragma region Bridge::HelperWindow
Bridge::HelperWindow::HelperWindow()
{
	HINSTANCE hInstance;
	BOOL bRes = ::GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR) &SubclassProc, &hInstance); assert(bRes);

	_hwnd = ::CreateWindowExW (0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, hInstance, 0); assert (_hwnd != nullptr);

	bRes = ::SetWindowSubclass (_hwnd, SubclassProc, 0, (DWORD_PTR) this); assert (bRes);

	auto callback = [](void* lpParameter, BOOLEAN TimerOrWaitFired)
	{
		auto helperWindow = static_cast<HelperWindow*>(lpParameter);
		::PostMessage (helperWindow->_hwnd, WM_LINK_PULSE_TIMER, 0, 0);
	};

	bRes = ::CreateTimerQueueTimer (&_linkPulseTimerHandle, nullptr, callback, this, 16, 16, 0); assert(bRes);
}

Bridge::HelperWindow::~HelperWindow()
{
	::DeleteTimerQueueTimer (nullptr, _linkPulseTimerHandle, INVALID_HANDLE_VALUE);
	::RemoveWindowSubclass (_hwnd, SubclassProc, 0);
	::DestroyWindow (_hwnd);
}

//static
LRESULT CALLBACK Bridge::HelperWindow::SubclassProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	auto hw = (HelperWindow*) dwRefData;

	if (uMsg == WM_LINK_PULSE_TIMER)
	{
		// We use a timer on a single thread for pulses because we want to avoid links going down due to delays on some threads but not on others.
		hw->event_invoker<LinkPulseEvent>()();
		return 0;
	}
	else if (uMsg == WM_ONE_SECOND_TIMER)
	{
		Bridge::OnWmOneSecondTimer (wParam, lParam);
		return 0;
	}
	else if (uMsg == WM_PACKET_RECEIVED)
	{
		Bridge::OnWmPacketReceived (wParam, lParam);
		return 0;
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
#pragma endregion

Bridge::Bridge (uint32_t portCount, uint32_t mstiCount, mac_address macAddress)
{
	for (unsigned int i = 0; i < 1 + mstiCount; i++)
		_trees.push_back (make_unique<BridgeTree>(this, i));

	float offset = 0;
	for (unsigned int portIndex = 0; portIndex < portCount; portIndex++)
	{
		offset += (Port::PortToPortSpacing / 2 + Port::InteriorWidth / 2);
		auto port = unique_ptr<Port>(new Port(this, portIndex, side::bottom, offset));
		_ports.push_back (move(port));
		offset += (Port::InteriorWidth / 2 + Port::PortToPortSpacing / 2);
	}

	_x = 0;
	_y = 0;
	_width = max (offset, MinWidth);
	_height = DefaultHeight;

	_stpBridge = STP_CreateBridge (portCount, mstiCount, max_vlan_number, &StpCallbacks, macAddress.data(), 256);
	STP_EnableLogging (_stpBridge, true);
	STP_SetApplicationContext (_stpBridge, this);

	for (auto& port : _ports)
		port->GetInvalidateEvent().add_handler(&OnPortInvalidate, this);
}

Bridge::~Bridge()
{
	for (auto& port : _ports)
		port->GetInvalidateEvent().remove_handler(&OnPortInvalidate, this);
	STP_DestroyBridge (_stpBridge);
}

void Bridge::on_added_to_project(project_i* project)
{
	base::on_added_to_project(project);
	assert (_helper_window == nullptr);
	_helper_window = std::make_unique<HelperWindow>();
	_helper_window->GetLinkPulseEvent().add_handler (&OnLinkPulseTick, this);
	DWORD period = 950 + (std::random_device()() % 100);
	BOOL bRes = ::CreateTimerQueueTimer (&_oneSecondTimerHandle, nullptr, OneSecondTimerCallback, this, period, period, 0); assert(bRes);

}

void Bridge::on_removing_from_project(project_i* project)
{
	// First stop the timers, to be sure the mutex won't be acquired in a background thread (when we'll have background threads).
	::DeleteTimerQueueTimer (nullptr, _oneSecondTimerHandle, INVALID_HANDLE_VALUE);
	_helper_window->GetLinkPulseEvent().remove_handler (&OnLinkPulseTick, this);
	_helper_window = nullptr;
	base::on_removing_from_project(project);
}

//static
void Bridge::OnPortInvalidate (void* callbackArg, renderable_object* object)
{
	auto bridge = static_cast<Bridge*>(callbackArg);
	bridge->event_invoker<invalidate_e>()(bridge);
}

//static
void CALLBACK Bridge::OneSecondTimerCallback (void* lpParameter, BOOLEAN TimerOrWaitFired)
{
	auto bridge = static_cast<Bridge*>(lpParameter);
	// We're on a worker thread. Let's post this message and continue processing on the GUI thread.
	::PostMessage (bridge->_helper_window->_hwnd, WM_ONE_SECOND_TIMER, (WPARAM) bridge, 0);
}

//static
void Bridge::OnWmOneSecondTimer (WPARAM wParam, LPARAM lParam)
{
	auto bridge = static_cast<Bridge*>((void*)wParam);
	if (!bridge->project()->simulation_paused())
		STP_OnOneSecondTick (bridge->_stpBridge, GetMessageTime());
}

// Checks the wires and computes macOperational for each port on this bridge.
//static
void Bridge::OnLinkPulseTick (void* callbackArg)
{
	auto b = static_cast<Bridge*>(callbackArg);
	if (b->project()->simulation_paused())
		return;

	bool invalidate = false;
	for (size_t portIndex = 0; portIndex < b->_ports.size(); portIndex++)
	{
		Port* port = b->_ports[portIndex].get();
		if (port->_missedLinkPulseCounter < Port::MissedLinkPulseCounterMax)
		{
			port->_missedLinkPulseCounter++;
			if (port->_missedLinkPulseCounter == Port::MissedLinkPulseCounterMax)
			{
				STP_OnPortDisabled (b->_stpBridge, (unsigned int) portIndex, ::GetMessageTime());
				invalidate = true;
			}
		}

		b->event_invoker<LinkPulseEvent>()(b, portIndex, ::GetMessageTime());
	}

	if (invalidate)
		b->event_invoker<invalidate_e>()(b);
}

void Bridge::ProcessLinkPulse (size_t rxPortIndex, unsigned int timestamp)
{
	auto port = _ports.at(rxPortIndex).get();
	bool oldMacOperational = port->_missedLinkPulseCounter < Port::MissedLinkPulseCounterMax;
	port->_missedLinkPulseCounter = 0;
	if (oldMacOperational == false)
	{
		STP_OnPortEnabled (_stpBridge, (unsigned int) rxPortIndex, 100, true, timestamp);
		this->event_invoker<invalidate_e>()(this);
	}
}

void Bridge::EnqueuePacket (PacketInfo&& packet, size_t rxPortIndex)
{
	_rxQueue.push ({ rxPortIndex, move(packet) });
	::PostMessage (_helper_window->_hwnd, WM_PACKET_RECEIVED, (WPARAM)(void*)this, 0);
}

// WM_PACKET_RECEIVED
//static
void Bridge::OnWmPacketReceived (WPARAM wParam, LPARAM lParam)
{
	auto bridge = static_cast<Bridge*>((void*)wParam);
	if (!bridge->project()->simulation_paused())
		bridge->ProcessReceivedPackets();
}

void Bridge::ProcessReceivedPackets()
{
	bool invalidate = false;

	while (!_rxQueue.empty())
	{
		size_t rxPortIndex = _rxQueue.front().first;
		auto port = _ports.at(rxPortIndex).get();
		auto rp = move(_rxQueue.front().second);
		_rxQueue.pop();

		bool oldMacOperational = port->_missedLinkPulseCounter < Port::MissedLinkPulseCounterMax;
		port->_missedLinkPulseCounter = 0;
		if (oldMacOperational == false)
		{
			STP_OnPortEnabled (_stpBridge, (unsigned int) rxPortIndex, 100, true, rp.timestamp);
			invalidate = true;
		}

		if ((rp.data.size() >= 6) && (memcmp (&rp.data[0], BpduDestAddress, 6) == 0))
		{
			// It's a BPDU.
			if (_bpdu_trapping_enabled)
			{
				STP_OnBpduReceived (_stpBridge, (unsigned int) rxPortIndex, &rp.data[21], (unsigned int) (rp.data.size() - 21), rp.timestamp);
			}
			else
			{
				// broadcast it to the other ports.
				for (size_t txPortIndex = 0; txPortIndex < _ports.size(); txPortIndex++)
				{
					if (txPortIndex == rxPortIndex)
						continue;

					auto txPortAddress = GetPortAddress(txPortIndex);

					// If it already went through this port, we have a loop that would hang our UI.
					if (find (rp.txPortPath.begin(), rp.txPortPath.end(), txPortAddress) != rp.txPortPath.end())
					{
						// We don't do anything here; we have code in wire.cpp that shows loops to the user - as thick red wires.
					}
					else
					{
						this->event_invoker<PacketTransmitEvent>()(this, txPortIndex, PacketInfo(rp));
					}
				}
			}
		}
		else
			assert(false); // not implemented
	}

	if (invalidate)
		this->event_invoker<invalidate_e>()(this);
}

void Bridge::SetLocation(float x, float y)
{
	if ((_x != x) || (_y != y))
	{
		_x = x;
		_y = y;
		this->event_invoker<invalidate_e>()(this);
	}
}

void Bridge::Render (ID2D1RenderTarget* dc, const drawing_resources& dos, unsigned int vlanNumber, const D2D1_COLOR_F& configIdColor) const
{
	auto treeIndex = STP_GetTreeIndexFromVlanNumber (_stpBridge, vlanNumber);

	stringstream text;
	float bridgeOutlineWidth = OutlineWidth;
	if (STP_IsBridgeStarted(_stpBridge))
	{
		auto stpVersion = STP_GetStpVersion(_stpBridge);
		auto treeIndex = STP_GetTreeIndexFromVlanNumber(_stpBridge, vlanNumber);
		bool isCistRoot = STP_IsCistRoot(_stpBridge);
		bool isRegionalRoot = (treeIndex > 0) && STP_IsRegionalRoot(_stpBridge, treeIndex);

		if ((treeIndex == 0) ? isCistRoot : isRegionalRoot)
			bridgeOutlineWidth *= 2;

		text << uppercase << setfill('0') << setw(4) << hex << STP_GetBridgePriority(_stpBridge, treeIndex) << '.' << mac_address_to_string(bridge_address()) << endl;
		text << "STP enabled (" << STP_GetVersionString(stpVersion) << ")" << endl;
		text << (isCistRoot ? "CIST Root Bridge\r\n" : "");
		if (stpVersion >= STP_VERSION_MSTP)
		{
			text << "VLAN " << dec << vlanNumber << ". Spanning tree: " << ((treeIndex == 0) ? "CIST(0)" : (string("MSTI") + to_string(treeIndex)).c_str()) << endl;
			text << (isRegionalRoot ? "Regional Root\r\n" : "");
		}
	}
	else
	{
		text << uppercase << setfill('0') << hex << mac_address_to_string(bridge_address()) << endl << "STP disabled\r\n(right-click to enable)";
	}

	// Draw bridge outline.
	D2D1_ROUNDED_RECT rr = RoundedRect (GetBounds(), RoundRadius, RoundRadius);
	InflateRoundedRect (&rr, -bridgeOutlineWidth / 2);
	com_ptr<ID2D1SolidColorBrush> brush;
	dc->CreateSolidColorBrush (configIdColor, &brush);
	dc->FillRoundedRectangle (&rr, brush/*_powered ? dos._poweredFillBrush : dos._unpoweredBrush*/);
	dc->DrawRoundedRectangle (&rr, dos._brushWindowText, bridgeOutlineWidth);

	// Draw bridge text.
	auto tl = text_layout::create (dos._dWriteFactory, dos._regularTextFormat, text.str().c_str());
	dc->DrawTextLayout ({ _x + OutlineWidth * 2 + 3, _y + OutlineWidth * 2 + 3}, tl.layout, dos._brushWindowText);

	for (auto& port : _ports)
		port->Render (dc, dos, vlanNumber);
}

void Bridge::render_selection (const edge::zoomable_i* zoomable, ID2D1RenderTarget* rt, const drawing_resources& dos) const
{
	auto oldaa = rt->GetAntialiasMode();
	rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

	auto tl = zoomable->pointw_to_pointd ({ _x - OutlineWidth / 2, _y - OutlineWidth / 2 });
	auto br = zoomable->pointw_to_pointd ({ _x + _width + OutlineWidth / 2, _y + _height + OutlineWidth / 2 });
	rt->DrawRectangle ({ tl.x - 10, tl.y - 10, br.x + 10, br.y + 10 }, dos._brushHighlight, 2, dos._strokeStyleSelectionRect);

	rt->SetAntialiasMode(oldaa);
}

renderable_object::HTResult Bridge::hit_test (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance)
{
	for (auto& p : _ports)
	{
		auto ht = p->hit_test (zoomable, dLocation, tolerance);
		if (ht.object != nullptr)
			return ht;
	}

	auto tl = zoomable->pointw_to_pointd ({ _x, _y });
	auto br = zoomable->pointw_to_pointd ({ _x + _width, _y + _height });

	if ((dLocation.x >= tl.x) && (dLocation.y >= tl.y) && (dLocation.x < br.x) && (dLocation.y < br.y))
		return { this, HTCodeInner };

	return {};
}

std::array<uint8_t, 6> Bridge::GetPortAddress (size_t portIndex) const
{
	std::array<uint8_t, 6> pa = bridge_address();
	pa[5]++;
	if (pa[5] == 0)
	{
		pa[4]++;
		if (pa[4] == 0)
		{
			pa[3]++;
			if (pa[3] == 0)
				assert(false); // not implemented
		}
	}

	return pa;
}

mac_address Bridge::bridge_address() const
{
	mac_address address;
	auto x = sizeof(address);
	memcpy (address.data(), STP_GetBridgeAddress(_stpBridge)->bytes, 6);
	return address;
}

void Bridge::set_bridge_address (mac_address address)
{
	if (memcmp(STP_GetBridgeAddress(_stpBridge)->bytes, address.data(), 6) != 0)
	{
		this->on_property_changing(&bridge_address_property);
		STP_SetBridgeAddress(_stpBridge, address.data(), GetMessageTime());
		this->on_property_changed(&bridge_address_property);
	}
}

static const _bstr_t StpVersionString = "StpVersion";
static const _bstr_t PortCountString = "PortCount";
static const _bstr_t MstiCountString = "MstiCount";
static const _bstr_t MstConfigNameString = "MstConfigName";
static const _bstr_t MstConfigTableString = "MstConfigTable";
static const _bstr_t EntryString = "Entry";
static const _bstr_t XString = "X";
static const _bstr_t YString = "Y";
static const _bstr_t WidthString = "Width";
static const _bstr_t HeightString = "Height";
static const _bstr_t BridgeTreesString = "BridgeTrees";
static const _bstr_t VlanString = "Vlan";
static const _bstr_t TreeString = "Tree";
static const _bstr_t PortsString = "Ports";
static const _bstr_t BridgeMaxAgeString = L"BridgeMaxAge";
static const _bstr_t BridgeForwardDelayString = L"BridgeForwardDelay";

/*
com_ptr<IXMLDOMElement> Bridge::Serialize (size_t bridgeIndex, IXMLDOMDocument3* doc) const
{
	com_ptr<IXMLDOMElement> bridgeElement;
	auto hr = doc->createElement (BridgeString, &bridgeElement); assert(SUCCEEDED(hr));

	bridgeElement->setAttribute (BridgeIndexString,   _variant_t(bridgeIndex));
	bridgeElement->setAttribute (AddressString,       _variant_t(mac_address_to_string(bridge_address()).c_str()));
	bridgeElement->setAttribute (StpEnabledString,    _variant_t(STP_IsBridgeStarted(_stpBridge)));
	bridgeElement->setAttribute (StpVersionString,    _variant_t(STP_GetVersionString(STP_GetStpVersion(_stpBridge))));
	bridgeElement->setAttribute (PortCountString,     _variant_t(_ports.size()));
	bridgeElement->setAttribute (MstiCountString,     _variant_t(STP_GetMstiCount(_stpBridge)));
	bridgeElement->setAttribute (MstConfigNameString, _variant_t(STP_GetMstConfigId(_stpBridge)->ConfigurationName));
	bridgeElement->setAttribute (XString,             _variant_t(_x));
	bridgeElement->setAttribute (YString,             _variant_t(_y));
	bridgeElement->setAttribute (WidthString,         _variant_t(_width));
	bridgeElement->setAttribute (HeightString,        _variant_t(_height));
	bridgeElement->setAttribute (BridgeMaxAgeString,       _variant_t(GetBridgeMaxAge()));
	bridgeElement->setAttribute (BridgeForwardDelayString, _variant_t(GetBridgeForwardDelay()));

	com_ptr<IXMLDOMElement> configTableElement;
	hr = doc->createElement (MstConfigTableString, &configTableElement); assert(SUCCEEDED(hr));
	hr = bridgeElement->appendChild(configTableElement, nullptr); assert(SUCCEEDED(hr));
	unsigned int entryCount;
	auto configTable = STP_GetMstConfigTable(_stpBridge, &entryCount);
	for (unsigned int vlan = 0; vlan < entryCount; vlan++)
	{
		com_ptr<IXMLDOMElement> entryElement;
		hr = doc->createElement (EntryString, &entryElement); assert(SUCCEEDED(hr));
		hr = entryElement->setAttribute (VlanString, _variant_t(vlan)); assert(SUCCEEDED(hr));
		hr = entryElement->setAttribute (TreeString, _variant_t(configTable[vlan].treeIndex)); assert(SUCCEEDED(hr));
		hr = configTableElement->appendChild(entryElement, nullptr); assert(SUCCEEDED(hr));
	}

	com_ptr<IXMLDOMElement> bridgeTreesElement;
	hr = doc->createElement (BridgeTreesString, &bridgeTreesElement); assert(SUCCEEDED(hr));
	hr = bridgeElement->appendChild(bridgeTreesElement, nullptr); assert(SUCCEEDED(hr));
	for (size_t treeIndex = 0; treeIndex < _trees.size(); treeIndex++)
	{
		com_ptr<IXMLDOMElement> bridgeTreeElement;
		hr = _trees.at(treeIndex)->Serialize (doc, bridgeTreeElement); assert(SUCCEEDED(hr));
		hr = bridgeTreesElement->appendChild (bridgeTreeElement, nullptr); assert(SUCCEEDED(hr));
	}

	com_ptr<IXMLDOMElement> portsElement;
	hr = doc->createElement (PortsString, &portsElement); assert(SUCCEEDED(hr));
	hr = bridgeElement->appendChild(portsElement, nullptr); assert(SUCCEEDED(hr));
	for (size_t portIndex = 0; portIndex < _ports.size(); portIndex++)
	{
		auto portElement = _ports.at(portIndex)->Serialize(doc);
		hr = portsElement->appendChild(portElement, nullptr); assert(SUCCEEDED(hr));
	}

	return bridgeElement;
}

//static
unique_ptr<Bridge> Bridge::Deserialize (project_i* project, IXMLDOMElement* element)
{
	auto getAttribute = [element](const _bstr_t& name) -> _variant_t
	{
		_variant_t value;
		auto hr = element->getAttribute (name, &value);
		assert (SUCCEEDED(hr) && (value.vt == VT_BSTR));
		return value;
	};

	HRESULT hr;
	_variant_t value;

	auto make_string = [](const _variant_t& v)
	{
		assert (v.vt == VT_BSTR);
		size_t len = wcslen(v.bstrVal);
		std::string str (len, (char) 0);
		for (size_t i = 0; i < len; i++)
			str[i] = (char) v.bstrVal[i];
		return str;
	};

	unsigned int portCount = wcstoul(getAttribute(PortCountString).bstrVal, nullptr, 10);
	unsigned int mstiCount = wcstoul(getAttribute(MstiCountString).bstrVal, nullptr, 10);
	mac_address bridgeAddress;
	mac_address_from_string<wchar_t>(getAttribute(AddressString).bstrVal, bridgeAddress);

	auto bridge = unique_ptr<Bridge>(new Bridge(project, portCount, mstiCount, bridgeAddress));

	bridge->_x = wcstof(getAttribute(XString).bstrVal, nullptr);
	bridge->_y = wcstof(getAttribute(YString).bstrVal, nullptr);
	bridge->_width  = wcstof(getAttribute(WidthString).bstrVal, nullptr);
	bridge->_height = wcstof(getAttribute(HeightString).bstrVal, nullptr);

	hr = element->getAttribute(StpVersionString, &value);
	if (SUCCEEDED(hr) && (value.vt == VT_BSTR))
	{
		auto versionString = make_string(value.bstrVal);
		auto version = STP_GetVersionFromString(versionString.c_str());
		STP_SetStpVersion (bridge->_stpBridge, version, GetMessageTime());
	}

	hr = element->getAttribute(MstConfigNameString, &value);
	if (SUCCEEDED(hr) && (value.vt == VT_BSTR))
	{
		auto name = make_string(value.bstrVal);
		STP_SetMstConfigName (bridge->_stpBridge, name.c_str(), GetMessageTime());
	}

	hr = element->getAttribute(StpEnabledString, &value);
	if (SUCCEEDED(hr) && (value.vt == VT_BSTR))
	{
		if (wcstoul(value.bstrVal, nullptr, 10) != 0)
			STP_StartBridge (bridge->_stpBridge, GetMessageTime());
		else
			STP_StopBridge (bridge->_stpBridge, GetMessageTime());
	}

	hr = element->getAttribute(BridgeMaxAgeString, &value);
	if (SUCCEEDED(hr) && (value.vt == VT_BSTR))
		bridge->SetBridgeMaxAge (wcstoul (value.bstrVal, nullptr, 10));

	hr = element->getAttribute(BridgeForwardDelayString, &value);
	if (SUCCEEDED(hr) && (value.vt == VT_BSTR))
		bridge->SetBridgeForwardDelay (wcstoul (value.bstrVal, nullptr, 10));

	com_ptr<IXMLDOMNode> configTableNode;
	hr = element->selectSingleNode(MstConfigTableString, &configTableNode);
	if (SUCCEEDED(hr) && (configTableNode != nullptr))
	{
		com_ptr<IXMLDOMNodeList> nodes;
		hr = configTableNode->get_childNodes(&nodes); assert(SUCCEEDED(hr));
		long entryCount;
		hr = nodes->get_length(&entryCount); assert(SUCCEEDED(hr));
		vector<STP_CONFIG_TABLE_ENTRY> configTable;
		for (unsigned int vlan = 0; vlan < (unsigned int) entryCount; vlan++)
		{
			com_ptr<IXMLDOMNode> entryNode;
			hr = nodes->get_item(vlan, &entryNode); assert(SUCCEEDED(hr));
			com_ptr<IXMLDOMElement> entryElement (entryNode);

			hr = entryElement->getAttribute(TreeString, &value); assert(SUCCEEDED(hr) && (value.vt == VT_BSTR));
			auto treeIndex = wcstoul(value.bstrVal, nullptr, 10);
			configTable.push_back (STP_CONFIG_TABLE_ENTRY { 0, (unsigned char) treeIndex });
		}

		STP_SetMstConfigTable(bridge->_stpBridge, &configTable[0], (unsigned int) configTable.size(), GetMessageTime());
	}

	com_ptr<IXMLDOMNode> bridgeTreesNode;
	hr = element->selectSingleNode(BridgeTreesString, &bridgeTreesNode);
	if (SUCCEEDED(hr) && (bridgeTreesNode != nullptr))
	{
		com_ptr<IXMLDOMNodeList> bridgeTreeNodes;
		hr = bridgeTreesNode->get_childNodes(&bridgeTreeNodes); assert(SUCCEEDED(hr));

		for (size_t treeIndex = 0; treeIndex < 1 + mstiCount; treeIndex++)
		{
			com_ptr<IXMLDOMNode> bridgeTreeNode;
			hr = bridgeTreeNodes->get_item((long) treeIndex, &bridgeTreeNode); assert(SUCCEEDED(hr));
			com_ptr<IXMLDOMElement> bridgeTreeElement = bridgeTreeNode;
			hr = bridge->_trees.at(treeIndex)->Deserialize(bridgeTreeElement); assert(SUCCEEDED(hr));
		}
	}

	com_ptr<IXMLDOMNode> portsNode;
	hr = element->selectSingleNode (PortsString, &portsNode);
	if (SUCCEEDED(hr) && (portsNode != nullptr))
	{
		com_ptr<IXMLDOMNodeList> portNodes;
		hr = portsNode->get_childNodes (&portNodes); assert(SUCCEEDED(hr));

		for (size_t portIndex = 0; portIndex < portCount; portIndex++)
		{
			com_ptr<IXMLDOMNode> portNode;
			hr = portNodes->get_item((long) portIndex, &portNode); assert(SUCCEEDED(hr));
			com_ptr<IXMLDOMElement> portElement = portNode;
			hr = bridge->_ports[portIndex]->Deserialize(portElement); assert(SUCCEEDED(hr));
		}
	}

	return bridge;
}
*/
void Bridge::SetCoordsForInteriorPort (Port* _port, D2D1_POINT_2F proposedLocation)
{
	float mouseX = proposedLocation.x - _x;
	float mouseY = proposedLocation.y - _y;

	float wh = _width / _height;

	// top side
	if ((mouseX > mouseY * wh) && (_width - mouseX) > mouseY * wh)
	{
		_port->_side = side::top;

		if (mouseX < Port::InteriorWidth / 2)
			_port->_offset = Port::InteriorWidth / 2;
		else if (mouseX > _width - Port::InteriorWidth / 2)
			_port->_offset = _width - Port::InteriorWidth / 2;
		else
			_port->_offset = mouseX;
	}

	// bottom side
	else if ((mouseX <= mouseY * wh) && (_width - mouseX) <= mouseY * wh)
	{
		_port->_side = side::bottom;

		if (mouseX < Port::InteriorWidth / 2 + 1)
			_port->_offset = Port::InteriorWidth / 2 + 1;
		else if (mouseX > _width - Port::InteriorWidth / 2)
			_port->_offset = _width - Port::InteriorWidth / 2;
		else
			_port->_offset = mouseX;
	}

	// left side
	if ((mouseX <= mouseY * wh) && (_width - mouseX) > mouseY * wh)
	{
		_port->_side = side::left;

		if (mouseY < Port::InteriorWidth / 2)
			_port->_offset = Port::InteriorWidth / 2;
		else if (mouseY > _height - Port::InteriorWidth / 2)
			_port->_offset = _height - Port::InteriorWidth / 2;
		else
			_port->_offset = mouseY;
	}

	// right side
	if ((mouseX > mouseY * wh) && (_width - mouseX) <= mouseY * wh)
	{
		_port->_side = side::right;

		if (mouseY < Port::InteriorWidth / 2)
			_port->_offset = Port::InteriorWidth / 2;
		else if (mouseY > _height - Port::InteriorWidth / 2)
			_port->_offset = _height - Port::InteriorWidth / 2;
		else
			_port->_offset = mouseY;
	}

	this->event_invoker<invalidate_e>()(this);
}

void Bridge::clear_log()
{
	_logLines.clear();
	_currentLogLine.text.clear();
	this->event_invoker<log_cleared_e>()(this);
}

std::string Bridge::mst_config_id_name() const
{
	auto configId = STP_GetMstConfigId(_stpBridge);
	size_t len = strnlen (configId->ConfigurationName, 32);
	return std::string(std::begin(configId->ConfigurationName), std::begin(configId->ConfigurationName) + len);
}

void Bridge::set_mst_config_id_name (std::string_view value)
{
	if (value.length() > 32)
		throw invalid_argument("Invalid MST Config Name: more than 32 characters.");

	char null_terminated[33];
	memcpy (null_terminated, value.data(), value.size());
	null_terminated[value.size()] = 0;

	this->on_property_changing(&mst_config_id_name_property);
	STP_SetMstConfigName (_stpBridge, null_terminated, GetMessageTime());
	this->on_property_changed(&mst_config_id_name_property);
}

uint32_t Bridge::GetMstConfigIdRevLevel() const
{
	auto id = STP_GetMstConfigId(_stpBridge);
	return ((unsigned short) id->RevisionLevelHigh << 8) | (unsigned short) id->RevisionLevelLow;
}

void Bridge::SetMstConfigIdRevLevel (uint32_t revLevel)
{
	if (GetMstConfigIdRevLevel() != revLevel)
	{
		this->on_property_changing(&MstConfigIdRevLevel);
		STP_SetMstConfigRevisionLevel (_stpBridge, revLevel, GetMessageTime());
		this->on_property_changed(&MstConfigIdRevLevel);
	}
}

std::string Bridge::GetMstConfigIdDigest() const
{
	const unsigned char* digest = STP_GetMstConfigId(_stpBridge)->ConfigurationDigest;
	stringstream ss;
	ss << uppercase << setfill('0') << hex
		<< setw(2) << (int) digest[0]  << setw(2) << (int) digest[1]  << setw(2) << (int) digest[2]  << setw(2) << (int) digest[3]
		<< setw(2) << (int) digest[4]  << setw(2) << (int) digest[5]  << setw(2) << (int) digest[6]  << setw(2) << (int) digest[7]
		<< setw(2) << (int) digest[8]  << setw(2) << (int) digest[9]  << setw(2) << (int) digest[10] << setw(2) << (int) digest[11]
		<< setw(2) << (int) digest[12] << setw(2) << (int) digest[13] << setw(2) << (int) digest[14] << setw(2) << (int) digest[15];
	return ss.str();
}

void Bridge::SetMstConfigTable (const STP_CONFIG_TABLE_ENTRY* entries, size_t entryCount)
{
	this->on_property_changing (&MstConfigIdDigest);
	STP_SetMstConfigTable (_stpBridge, &entries[0], (unsigned int) entryCount, GetMessageTime());
	this->on_property_changed (&MstConfigIdDigest);
}

void Bridge::set_stp_enabled (bool value)
{
	if (value && !STP_IsBridgeStarted(_stpBridge))
	{
		this->on_property_changing(&stp_enabled_property);
		STP_StartBridge (_stpBridge, GetMessageTime());
		this->on_property_changed(&stp_enabled_property);
	}
	else if (!value && STP_IsBridgeStarted(_stpBridge))
	{
		this->on_property_changing(&stp_enabled_property);
		STP_StopBridge (_stpBridge, GetMessageTime());
		this->on_property_changed(&stp_enabled_property);
	}
}

void Bridge::set_stp_version (STP_VERSION stp_version)
{
	if (STP_GetStpVersion(_stpBridge) != stp_version)
	{
		this->on_property_changing(&stp_version_property);
		STP_SetStpVersion(_stpBridge, stp_version, GetMessageTime());
		this->on_property_changed(&stp_version_property);
	}
}

uint32_t Bridge::GetBridgeMaxAge() const
{
	return (uint32_t) STP_GetBridgeMaxAge(_stpBridge);
}

void Bridge::SetBridgeMaxAge (uint32_t maxAge)
{
	this->on_property_changing (&bridge_max_age_property);
	STP_SetBridgeMaxAge (_stpBridge, maxAge, GetMessageTime());
	this->on_property_changed (&bridge_max_age_property);
}

uint32_t Bridge::GetBridgeForwardDelay() const
{
	return (uint32_t) STP_GetBridgeForwardDelay(_stpBridge);
}

void Bridge::SetBridgeForwardDelay (uint32_t forwardDelay)
{
	this->on_property_changing (&bridge_forward_delay_property);
	STP_SetBridgeForwardDelay (_stpBridge, forwardDelay, GetMessageTime());
	this->on_property_changed (&bridge_forward_delay_property);
}

#pragma region properties

size_t Bridge::mst_config_table_get_value_count() const
{
	unsigned int entry_count;
	STP_GetMstConfigTable(_stpBridge, &entry_count);
	return entry_count;
}

uint32_t Bridge::mst_config_table_get_value(size_t i) const
{
	unsigned int entry_count;
	auto entries = STP_GetMstConfigTable(_stpBridge, &entry_count);
	return entries[i].treeIndex;
}

void Bridge::mst_config_table_set_value(size_t i, uint32_t value)
{
	unsigned int entry_count;
	auto table = STP_GetMstConfigTable (_stpBridge, &entry_count);
	assert (i < entry_count);
	if (table->treeIndex != value)
	{
		property_change_args args = { &mst_config_table_property, i, list_property_change_type::set };
		this->on_property_changing(args);
		STP_SetMstConfigTableEntry (_stpBridge, (unsigned int)i, value, ::GetMessageTime());
		this->on_property_changed(args);
	}
}

static const edge::property_group bridge_times_group = { 5, "Timer Params (Table 13-5)" };
static const edge::property_group mst_group = { 10, "MST Config Id" };

// kept for compatibility with old versions of the simulator
const edge::uint32_p Bridge::bridge_index_property {
	"BridgeIndex", nullptr, nullptr, ui_visible::no,
	nullptr,
	static_cast<uint32_p::static_setter_t>([](object*, uint32_t) { }), // setter
	std::nullopt
};

const mac_address_p Bridge::bridge_address_property {
	"Address", nullptr, nullptr, ui_visible::yes,
	static_cast<mac_address_p::member_getter_t>(&bridge_address),
	static_cast<mac_address_p::member_setter_t>(&set_bridge_address),
	std::nullopt,
};

const bool_p Bridge::stp_enabled_property {
	"STPEnabled", nullptr, nullptr, ui_visible::yes,
	static_cast<bool_p::member_getter_t>(&stp_enabled),
	static_cast<bool_p::member_setter_t>(&set_stp_enabled),
	true,
};

const stp_version_p Bridge::stp_version_property {
	"StpVersion", nullptr, nullptr, ui_visible::yes,
	static_cast<stp_version_p::member_getter_t>(&stp_version),
	static_cast<stp_version_p::member_setter_t>(&set_stp_version),
	STP_VERSION_RSTP,
};

const uint32_p Bridge::port_count_property {
	"PortCount", nullptr, nullptr, ui_visible::yes,
	static_cast<uint32_p::member_getter_t>(&port_count),
	nullptr,
	std::nullopt,
};

const edge::uint32_p Bridge::msti_count_property {
	"MstiCount", nullptr, nullptr, ui_visible::yes,
	static_cast<uint32_p::member_getter_t>(&msti_count),
	nullptr,
	std::nullopt,
};

const temp_string_p Bridge::mst_config_id_name_property {
	"MstConfigName", &mst_group, nullptr, ui_visible::yes,
	static_cast<temp_string_p::member_getter_t>(&mst_config_id_name),
	static_cast<temp_string_p::member_setter_t>(&set_mst_config_id_name),
	std::nullopt,
};

const typed_value_collection_property<Bridge, uint32_property_traits> Bridge::mst_config_table_property {
	"MstConfigTable", nullptr, nullptr, ui_visible::no,
	"Vlan", "Tree",
	&mst_config_table_get_value_count,
	&mst_config_table_get_value,
	&mst_config_table_set_value,
	nullptr,
	nullptr,
};

const edge::uint32_p Bridge::MstConfigIdRevLevel {
	"Revision Level", &mst_group, nullptr, ui_visible::yes,
	static_cast<uint32_p::member_getter_t>(&Bridge::GetMstConfigIdRevLevel),
	static_cast<uint32_p::member_setter_t>(&Bridge::SetMstConfigIdRevLevel),
	0,
};

const config_id_digest_p Bridge::MstConfigIdDigest {
	"Digest", &mst_group, nullptr, ui_visible::yes,
	static_cast<temp_string_p::member_getter_t>(&Bridge::GetMstConfigIdDigest),
	nullptr,
	std::nullopt,
};

#pragma region Timer and related parameters from Table 13-5
const edge::uint32_p Bridge::migrate_time_property {
	"MigrateTime", &bridge_times_group, nullptr, ui_visible::yes,
	[](const object* o) { return 3u; },
	nullptr,
	3,
};

const edge::uint32_p Bridge::bridge_hello_time_property {
	"BridgeHelloTime", &bridge_times_group, nullptr, ui_visible::yes,
	[](const object* o) { return 2u; },
	nullptr,
	2,
};

const edge::uint32_p Bridge::bridge_max_age_property {
	"BridgeMaxAge", &bridge_times_group, nullptr, ui_visible::yes,
	static_cast<edge::uint32_p::member_getter_t>(&Bridge::GetBridgeMaxAge),
	static_cast<edge::uint32_p::member_setter_t>(&Bridge::SetBridgeMaxAge),
	20,
};

const edge::uint32_p Bridge::bridge_forward_delay_property {
	"BridgeForwardDelay", &bridge_times_group, nullptr, ui_visible::yes,
	static_cast<edge::uint32_p::member_getter_t>(&Bridge::GetBridgeForwardDelay),
	static_cast<edge::uint32_p::member_setter_t>(&Bridge::SetBridgeForwardDelay),
	15,
};

const edge::uint32_p Bridge::tx_hold_count_property {
	"TxHoldCount", &bridge_times_group, nullptr, ui_visible::yes,
	[](const object* o) { return STP_GetTxHoldCount(static_cast<const Bridge*>(o)->stp_bridge()); },
	[](object* o, uint32_t value) { STP_SetTxHoldCount(static_cast<Bridge*>(o)->stp_bridge(), value, ::GetMessageTime()); },
	6
};

const edge::uint32_p Bridge::max_hops_property {
	"MaxHops",
	&bridge_times_group,
	"Setting this is not yet implemented in the library",
	ui_visible::yes,
	[](const object* o) { return 20u; },
	nullptr,
	20
};

const float_p Bridge::x_property {
	"X", nullptr, nullptr, ui_visible::no,
	static_cast<float_p::member_getter_t>(&x),
	static_cast<float_p::member_setter_t>(&set_x),
	std::nullopt
};

const float_p Bridge::y_property {
	"Y", nullptr, nullptr, ui_visible::no,
	static_cast<float_p::member_getter_t>(&y),
	static_cast<float_p::member_setter_t>(&set_y),
	std::nullopt
};

const float_p Bridge::width_property {
	"Width", nullptr, nullptr, ui_visible::no,
	static_cast<float_p::member_getter_t>(&width),
	static_cast<float_p::member_setter_t>(&set_width),
	std::nullopt
};

const float_p Bridge::height_property {
	"Height", nullptr, nullptr, ui_visible::no,
	static_cast<float_p::member_getter_t>(&height),
	static_cast<float_p::member_setter_t>(&set_height),
	std::nullopt
};

#pragma endregion

const edge::property* const Bridge::_properties[] = {
	&bridge_index_property,
	&bridge_address_property,
	&stp_enabled_property,
	&stp_version_property,
	&port_count_property,
	&msti_count_property,
	&mst_config_id_name_property,
	&MstConfigIdRevLevel,
	&mst_config_table_property,
	&MstConfigIdDigest,
	&migrate_time_property,
	&bridge_hello_time_property,
	&bridge_max_age_property,
	&bridge_forward_delay_property,
	&tx_hold_count_property,
	&max_hops_property,
	&x_property, &y_property, &width_property, &height_property
};

const xtype<Bridge, uint32_p, uint32_p, mac_address_p>  Bridge::_type = {
	"Bridge",
	&base::_type,
	_properties,
	[](uint32_t port_count, uint32_t msti_count, mac_address address) { return new Bridge(port_count, msti_count, address); },
	&port_count_property,
	&msti_count_property,
	&bridge_address_property,
};
#pragma endregion

#pragma region STP Callbacks
const STP_CALLBACKS Bridge::StpCallbacks =
{
	&StpCallback_EnableBpduTrapping,
	&StpCallback_EnableLearning,
	&StpCallback_EnableForwarding,
	&StpCallback_TransmitGetBuffer,
	&StpCallback_TransmitReleaseBuffer,
	&StpCallback_FlushFdb,
	&StpCallback_DebugStrOut,
	&StpCallback_OnTopologyChange,
	&StpCallback_OnNotifiedTopologyChange,
	&StpCallback_OnPortRoleChanged,
	&StpCallback_AllocAndZeroMemory,
	&StpCallback_FreeMemory,
};

void* Bridge::StpCallback_AllocAndZeroMemory(unsigned int size)
{
	void* p = malloc(size);
	memset (p, 0, size);
	return p;
}

void Bridge::StpCallback_FreeMemory(void* p)
{
	free(p);
}

void* Bridge::StpCallback_TransmitGetBuffer (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp)
{
	Bridge* b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	Port* txPort = b->_ports[portIndex].get();

	b->_txPacketData.resize (bpduSize + 21);
	memcpy (&b->_txPacketData[0], BpduDestAddress, 6);
	memcpy (&b->_txPacketData[6], &b->GetPortAddress(portIndex)[0], 6);
	b->_txTransmittingPort = txPort;
	b->_txTimestamp = timestamp;
	return &b->_txPacketData[21];
}

void Bridge::StpCallback_TransmitReleaseBuffer (const STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));

	PacketInfo info;
	info.data = move(b->_txPacketData);
	info.timestamp = b->_txTimestamp;
	b->event_invoker<PacketTransmitEvent>()(b, b->_txTransmittingPort->port_index(), move(info));
}

void Bridge::StpCallback_EnableBpduTrapping (const STP_BRIDGE* bridge, bool enable, unsigned int timestamp)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	b->_bpdu_trapping_enabled = enable;
}

void Bridge::StpCallback_EnableLearning (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable, unsigned int timestamp)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	b->event_invoker<invalidate_e>()(b);
}

void Bridge::StpCallback_EnableForwarding (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable, unsigned int timestamp)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	b->event_invoker<invalidate_e>()(b);
}

void Bridge::StpCallback_FlushFdb (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
}

void Bridge::StpCallback_DebugStrOut (const STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, unsigned int flush)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));

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
				b->_logLines.push_back(make_unique<BridgeLogLine>(move(b->_currentLogLine)));
				b->event_invoker<LogLineGenerated>()(b, b->_logLines.back().get());
			}

			b->_currentLogLine.text.append (nullTerminatedString, (size_t) stringLength);
		}

		if (!b->_currentLogLine.text.empty() && (b->_currentLogLine.text.back() == L'\n'))
		{
			b->_logLines.push_back(make_unique<BridgeLogLine>(move(b->_currentLogLine)));
			b->event_invoker<LogLineGenerated>()(b, b->_logLines.back().get());
		}
	}

	if (flush && !b->_currentLogLine.text.empty())
	{
		b->_logLines.push_back(make_unique<BridgeLogLine>(move(b->_currentLogLine)));
		b->event_invoker<LogLineGenerated>()(b, b->_logLines.back().get());
	}
}

void Bridge::StpCallback_OnTopologyChange (const STP_BRIDGE* bridge, unsigned int treeIndex, unsigned int timestamp)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	b->_trees[treeIndex]->on_topology_change(timestamp);
}

void Bridge::StpCallback_OnNotifiedTopologyChange (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int timestamp)
{
}

void Bridge::StpCallback_OnPortRoleChanged (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, STP_PORT_ROLE role, unsigned int timestamp)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	b->event_invoker<invalidate_e>()(b);
}
#pragma endregion
