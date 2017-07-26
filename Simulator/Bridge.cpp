#include "pch.h"
#include "Bridge.h"
#include "Port.h"
#include "Wire.h"
#include "stp_md5.h"
#include "Win32/D2DWindow.h"
#include "Win32/UtilityFunctions.h"

using namespace std;
using namespace D2D1;

static constexpr UINT WM_ONE_SECOND_TIMER  = WM_APP + 1;
static constexpr UINT WM_LINK_PULSE_TIMER  = WM_APP + 2;
static constexpr UINT WM_PACKET_RECEIVED   = WM_APP + 3;

static constexpr uint8_t BpduDestAddress[6] = { 1, 0x80, 0xC2, 0, 0, 0 };

Bridge::HelperWindow Bridge::_helperWindow;

Bridge::HelperWindow::HelperWindow()
{
	HINSTANCE hInstance;
	BOOL bRes = ::GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR) &SubclassProc, &hInstance);
	if (!bRes)
		throw win32_exception(GetLastError());

	_hwnd = ::CreateWindowExW (0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, hInstance, 0);
	if (_hwnd == nullptr)
		throw win32_exception(GetLastError());

	bRes = ::SetWindowSubclass (_hwnd, SubclassProc, 0, (DWORD_PTR) this);
	if (!bRes)
		throw win32_exception(GetLastError());

	auto callback = [](void* lpParameter, BOOLEAN TimerOrWaitFired) { ::PostMessage (_helperWindow._hwnd, WM_LINK_PULSE_TIMER, 0, 0); };
	bRes = ::CreateTimerQueueTimer (&_linkPulseTimerHandle, nullptr, callback, this, 16, 16, 0); ThrowWin32IfFailed(bRes);
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
		LinkPulseEvent::InvokeHandlers(hw);
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

Bridge::Bridge (unsigned int portCount, unsigned int mstiCount, const unsigned char macAddress[6])
{
	for (unsigned int i = 0; i < 1 + mstiCount; i++)
		_trees.push_back (make_unique<BridgeTree>(this, i));

	float offset = 0;
	for (unsigned int i = 0; i < portCount; i++)
	{
		offset += (Port::PortToPortSpacing / 2 + Port::InteriorWidth / 2);
		auto port = unique_ptr<Port>(new Port(this, i, Side::Bottom, offset));
		_ports.push_back (move(port));
		offset += (Port::InteriorWidth / 2 + Port::PortToPortSpacing / 2);
	}

	_x = 0;
	_y = 0;
	_width = max (offset, MinWidth);
	_height = DefaultHeight;

	DWORD period = 950 + (std::random_device()() % 100);
	HANDLE handle;
	BOOL bRes = ::CreateTimerQueueTimer (&handle, nullptr, OneSecondTimerCallback, this, period, period, 0); ThrowWin32IfFailed(bRes);
	_oneSecondTimerHandle.reset(handle);

	_stpBridge = STP_CreateBridge (portCount, mstiCount, MaxVlanNumber, &StpCallbacks, macAddress, 256);
	STP_EnableLogging (_stpBridge, true);
	STP_SetApplicationContext (_stpBridge, this);

	_helperWindow.GetLinkPulseEvent().AddHandler (&OnLinkPulseTick, this);

	for (auto& port : _ports)
		port->GetInvalidateEvent().AddHandler(&OnPortInvalidate, this);
}

Bridge::~Bridge()
{
	for (auto& port : _ports)
		port->GetInvalidateEvent().RemoveHandler(&OnPortInvalidate, this);

	_helperWindow.GetLinkPulseEvent().RemoveHandler (&OnLinkPulseTick, this);

	// First stop the timers, to be sure the mutex won't be acquired in a background thread (when we'll have background threads).
	_oneSecondTimerHandle = nullptr;

	STP_DestroyBridge (_stpBridge);
}

//static
void Bridge::OnPortInvalidate (void* callbackArg, Object* object)
{
	auto bridge = static_cast<Bridge*>(callbackArg);
	InvalidateEvent::InvokeHandlers (bridge, bridge);
}

//static
void CALLBACK Bridge::OneSecondTimerCallback (void* lpParameter, BOOLEAN TimerOrWaitFired)
{
	auto bridge = static_cast<Bridge*>(lpParameter);
	// We're on a worker thread. Let's post this message and continue processing on the GUI thread.
	::PostMessage (_helperWindow._hwnd, WM_ONE_SECOND_TIMER, (WPARAM) bridge, 0);
}

//static
void Bridge::OnWmOneSecondTimer (WPARAM wParam, LPARAM lParam)
{
	auto bridge = static_cast<Bridge*>((void*)wParam);
	if (!bridge->_simulationPaused)
		STP_OnOneSecondTick (bridge->_stpBridge, GetTimestampMilliseconds());
}

// Checks the wires and computes macOperational for each port on this bridge.
//static
void Bridge::OnLinkPulseTick (void* callbackArg)
{
	auto b = static_cast<Bridge*>(callbackArg);
	if (b->_simulationPaused)
		return;

	auto timestamp = GetTimestampMilliseconds();

	bool invalidate = false;
	for (size_t portIndex = 0; portIndex < b->_ports.size(); portIndex++)
	{
		Port* port = b->_ports[portIndex].get();
		if (port->_missedLinkPulseCounter < Port::MissedLinkPulseCounterMax)
		{
			port->_missedLinkPulseCounter++;
			if (port->_missedLinkPulseCounter == Port::MissedLinkPulseCounterMax)
			{
				STP_OnPortDisabled (b->_stpBridge, (unsigned int) portIndex, timestamp);
				invalidate = true;
			}
		}

		LinkPulseEvent::InvokeHandlers(b, b, portIndex, timestamp);
	}

	if (invalidate)
		InvalidateEvent::InvokeHandlers(b, b);
}

void Bridge::ProcessLinkPulse (size_t rxPortIndex, unsigned int timestamp)
{
	auto port = _ports.at(rxPortIndex).get();
	bool oldMacOperational = port->_missedLinkPulseCounter < Port::MissedLinkPulseCounterMax;
	port->_missedLinkPulseCounter = 0;
	if (oldMacOperational == false)
	{
		STP_OnPortEnabled (_stpBridge, (unsigned int) rxPortIndex, 100, true, timestamp);
		InvalidateEvent::InvokeHandlers(this, this);
	}
}

void Bridge::EnqueuePacket (PacketInfo&& packet, size_t rxPortIndex)
{
	_rxQueue.push ({ rxPortIndex, move(packet) });
	::PostMessage (_helperWindow._hwnd, WM_PACKET_RECEIVED, (WPARAM)(void*)this, 0);
}

// WM_PACKET_RECEIVED
//static
void Bridge::OnWmPacketReceived (WPARAM wParam, LPARAM lParam)
{
	auto bridge = static_cast<Bridge*>((void*)wParam);
	if (!bridge->_simulationPaused)
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
			if (STP_IsBridgeStarted(_stpBridge))
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
						// We don't do anything here; we have code in Wire.cpp that shows loops to the user - as thick red wires.
					}
					else
					{
						PacketTransmitEvent::InvokeHandlers (this, this, txPortIndex, PacketInfo(rp));
					}
				}
			}
		}
		else
			throw not_implemented_exception();
	}

	if (invalidate)
		InvalidateEvent::InvokeHandlers(this, this);
}

void Bridge::SetLocation(float x, float y)
{
	if ((_x != x) || (_y != y))
	{
		_x = x;
		_y = y;
		InvalidateEvent::InvokeHandlers(this, this);
	}
}

void Bridge::Render (ID2D1RenderTarget* dc, const DrawingObjects& dos, unsigned int vlanNumber, const D2D1_COLOR_F& configIdColor) const
{
	auto treeIndex = STP_GetTreeIndexFromVlanNumber (_stpBridge, vlanNumber);

	wstringstream text;
	float bridgeOutlineWidth = OutlineWidth;
	if (STP_IsBridgeStarted(_stpBridge))
	{
		auto stpVersion = STP_GetStpVersion(_stpBridge);
		auto treeIndex = STP_GetTreeIndexFromVlanNumber(_stpBridge, vlanNumber);
		bool isCistRoot = STP_IsCistRoot(_stpBridge);
		bool isRegionalRoot = (treeIndex > 0) && STP_IsRegionalRoot(_stpBridge, treeIndex);

		if ((treeIndex == 0) ? isCistRoot : isRegionalRoot)
			bridgeOutlineWidth *= 2;

		text << uppercase << setfill(L'0') << setw(4) << hex << STP_GetBridgePriority(_stpBridge, treeIndex) << L'.' << GetBridgeAddressAsWString() << endl;
		text << L"STP enabled (" << STP_GetVersionString(stpVersion) << L")" << endl;
		text << (isCistRoot ? L"CIST Root Bridge\r\n" : L"");
		if (stpVersion >= STP_VERSION_MSTP)
		{
			text << L"VLAN " << dec << vlanNumber << L". Spanning tree: " << ((treeIndex == 0) ? L"CIST(0)" : (wstring(L"MSTI") + to_wstring(treeIndex)).c_str()) << endl;
			text << (isRegionalRoot ? L"Regional Root\r\n" : L"");
		}
	}
	else
	{
		text << uppercase << setfill(L'0') << hex << GetBridgeAddressAsWString() << endl << L"STP disabled\r\n(right-click to enable)";
	}

	// Draw bridge outline.
	D2D1_ROUNDED_RECT rr = RoundedRect (GetBounds(), RoundRadius, RoundRadius);
	InflateRoundedRect (&rr, -bridgeOutlineWidth / 2);
	ID2D1SolidColorBrushPtr brush;
	dc->CreateSolidColorBrush (configIdColor, &brush);
	dc->FillRoundedRectangle (&rr, brush/*_powered ? dos._poweredFillBrush : dos._unpoweredBrush*/);
	dc->DrawRoundedRectangle (&rr, dos._brushWindowText, bridgeOutlineWidth);

	// Draw bridge text.
	auto tl = TextLayout::Create (dos._dWriteFactory, dos._regularTextFormat, text.str().c_str());
	dc->DrawTextLayout ({ _x + OutlineWidth * 2 + 3, _y + OutlineWidth * 2 + 3}, tl.layout, dos._brushWindowText);

	for (auto& port : _ports)
		port->Render (dc, dos, vlanNumber);
}

void Bridge::RenderSelection (const IZoomable* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const
{
	auto oldaa = rt->GetAntialiasMode();
	rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

	auto tl = zoomable->GetDLocationFromWLocation ({ _x - OutlineWidth / 2, _y - OutlineWidth / 2 });
	auto br = zoomable->GetDLocationFromWLocation ({ _x + _width + OutlineWidth / 2, _y + _height + OutlineWidth / 2 });
	rt->DrawRectangle ({ tl.x - 10, tl.y - 10, br.x + 10, br.y + 10 }, dos._brushHighlight, 2, dos._strokeStyleSelectionRect);

	rt->SetAntialiasMode(oldaa);
}

RenderableObject::HTResult Bridge::HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance)
{
	for (auto& p : _ports)
	{
		auto ht = p->HitTest (zoomable, dLocation, tolerance);
		if (ht.object != nullptr)
			return ht;
	}

	auto tl = zoomable->GetDLocationFromWLocation ({ _x, _y });
	auto br = zoomable->GetDLocationFromWLocation ({ _x + _width, _y + _height });

	if ((dLocation.x >= tl.x) && (dLocation.y >= tl.y) && (dLocation.x < br.x) && (dLocation.y < br.y))
		return { this, HTCodeInner };

	return {};
}

std::string Bridge::GetBridgeAddressAsString() const
{
	auto address = STP_GetBridgeAddress(_stpBridge);
	return ConvertBridgeAddressToString (address->bytes);
}

std::wstring Bridge::GetBridgeAddressAsWString() const
{
	auto address = STP_GetBridgeAddress(_stpBridge);
	return ConvertBridgeAddressToWString(address->bytes);
}

void Bridge::SetBridgeAddressFromWString (std::wstring str, unsigned int timestamp)
{
	auto newAddress = ConvertStringToBridgeAddress(str.c_str());
	STP_SetBridgeAddress (_stpBridge, newAddress.bytes, timestamp);
}

std::array<uint8_t, 6> Bridge::GetPortAddress (size_t portIndex) const
{
	std::array<uint8_t, 6> pa;
	memcpy (pa.data(), STP_GetBridgeAddress(_stpBridge)->bytes, 6);
	pa[5]++;
	if (pa[5] == 0)
	{
		pa[4]++;
		if (pa[4] == 0)
		{
			pa[3]++;
			if (pa[3] == 0)
				throw not_implemented_exception();
		}
	}

	return pa;
}

static const _bstr_t BridgeString = "Bridge";
static const _bstr_t BridgeIndexString = "BridgeIndex"; // saved to ease reading the XML
static const _bstr_t AddressString = "Address";
static const _bstr_t StpEnabledString = "STPEnabled";
static const _bstr_t TrueString = "True";
static const _bstr_t FalseString = "False";
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
static const _bstr_t BridgeTreeString = "BridgeTree";
static const _bstr_t TreeIndexString = "TreeIndex";
static const _bstr_t BridgePriorityString = "BridgePriority";
static const _bstr_t VlanString = "Vlan";
static const _bstr_t TreeString = "Tree";
static const _bstr_t PortsString = "Ports";

IXMLDOMElementPtr Bridge::Serialize (size_t bridgeIndex, IXMLDOMDocument3* doc) const
{
	IXMLDOMElementPtr bridgeElement;
	auto hr = doc->createElement (BridgeString, &bridgeElement); ThrowIfFailed(hr);

	bridgeElement->setAttribute (BridgeIndexString,   _variant_t(bridgeIndex));
	bridgeElement->setAttribute (AddressString,       _variant_t(GetBridgeAddressAsWString().c_str()));
	bridgeElement->setAttribute (StpEnabledString,    _variant_t(STP_IsBridgeStarted(_stpBridge)));
	bridgeElement->setAttribute (StpVersionString,    _variant_t(STP_GetVersionString(STP_GetStpVersion(_stpBridge))));
	bridgeElement->setAttribute (PortCountString,     _variant_t(_ports.size()));
	bridgeElement->setAttribute (MstiCountString,     _variant_t(STP_GetMstiCount(_stpBridge)));
	bridgeElement->setAttribute (MstConfigNameString, _variant_t(STP_GetMstConfigId(_stpBridge)->ConfigurationName));
	bridgeElement->setAttribute (XString,             _variant_t(_x));
	bridgeElement->setAttribute (YString,             _variant_t(_y));
	bridgeElement->setAttribute (WidthString,         _variant_t(_width));
	bridgeElement->setAttribute (HeightString,        _variant_t(_height));

	IXMLDOMElementPtr configTableElement;
	hr = doc->createElement (MstConfigTableString, &configTableElement); ThrowIfFailed(hr);
	hr = bridgeElement->appendChild(configTableElement, nullptr); ThrowIfFailed(hr);
	unsigned int entryCount;
	auto configTable = STP_GetMstConfigTable(_stpBridge, &entryCount);
	for (unsigned int vlan = 0; vlan < entryCount; vlan++)
	{
		IXMLDOMElementPtr entryElement;
		hr = doc->createElement (EntryString, &entryElement); ThrowIfFailed(hr);
		hr = entryElement->setAttribute (VlanString, _variant_t(vlan)); ThrowIfFailed(hr);
		hr = entryElement->setAttribute (TreeString, _variant_t(configTable[vlan].treeIndex)); ThrowIfFailed(hr);
		hr = configTableElement->appendChild(entryElement, nullptr); ThrowIfFailed(hr);
	}

	IXMLDOMElementPtr bridgeTreesElement;
	hr = doc->createElement (BridgeTreesString, &bridgeTreesElement); ThrowIfFailed(hr);
	hr = bridgeElement->appendChild(bridgeTreesElement, nullptr); ThrowIfFailed(hr);
	for (unsigned int treeIndex = 0; treeIndex <= STP_GetMstiCount(_stpBridge); treeIndex++)
	{
		IXMLDOMElementPtr bridgeTreeElement;
		hr = doc->createElement (BridgeTreeString, &bridgeTreeElement); ThrowIfFailed(hr);
		bridgeTreeElement->setAttribute (TreeIndexString, _variant_t(treeIndex));
		bridgeTreeElement->setAttribute (BridgePriorityString, _variant_t(STP_GetBridgePriority(_stpBridge, treeIndex)));
		bridgeTreesElement->appendChild(bridgeTreeElement, nullptr);
	}

	IXMLDOMElementPtr portsElement;
	hr = doc->createElement (PortsString, &portsElement); ThrowIfFailed(hr);
	hr = bridgeElement->appendChild(portsElement, nullptr); ThrowIfFailed(hr);
	for (size_t portIndex = 0; portIndex < _ports.size(); portIndex++)
	{
		auto portElement = _ports[portIndex]->Serialize(doc);
		hr = portsElement->appendChild(portElement, nullptr); ThrowIfFailed(hr);
	}

	return bridgeElement;
}

//static
unique_ptr<Bridge> Bridge::Deserialize (IXMLDOMElement* element)
{
	auto getAttribute = [element](const _bstr_t& name) -> _bstr_t
	{
		_variant_t value;
		auto hr = element->getAttribute (name, &value); ThrowIfFailed(hr);
		return (_bstr_t) value;
	};

	HRESULT hr;
	_variant_t value;
	setlocale (LC_NUMERIC, "C");
	wstring_convert<codecvt_utf8<wchar_t>> converter;
	auto timestamp = GetTimestampMilliseconds();

	unsigned int portCount = wcstoul(getAttribute(PortCountString), nullptr, 10);
	unsigned int mstiCount = wcstoul(getAttribute(MstiCountString), nullptr, 10);
	auto bridgeAddress = ConvertStringToBridgeAddress(getAttribute(AddressString));

	auto bridge = unique_ptr<Bridge>(new Bridge(portCount, mstiCount, bridgeAddress.bytes));

	bridge->_x = wcstof(getAttribute(XString), nullptr);
	bridge->_y = wcstof(getAttribute(YString), nullptr);
	bridge->_width  = wcstof(getAttribute(WidthString), nullptr);
	bridge->_height = wcstof(getAttribute(HeightString), nullptr);

	hr = element->getAttribute(StpVersionString, &value);
	if (SUCCEEDED(hr) && (value.vt != VT_NULL))
	{
		auto versionString = converter.to_bytes(value.bstrVal);
		auto version = STP_GetVersionFromString(versionString.c_str());
		STP_SetStpVersion (bridge->_stpBridge, version, timestamp);
	}

	hr = element->getAttribute(MstConfigNameString, &value);
	if (SUCCEEDED(hr) && (value.vt != VT_NULL))
	{
		auto name = converter.to_bytes(value.bstrVal);
		STP_SetMstConfigName (bridge->_stpBridge, name.c_str(), timestamp);
	}

	hr = element->getAttribute(StpEnabledString, &value);
	if (SUCCEEDED(hr) && (value.vt != VT_NULL))
	{
		if (wcstoul(value.bstrVal, nullptr, 10) != 0)
			STP_StartBridge (bridge->_stpBridge, timestamp);
		else
			STP_StopBridge (bridge->_stpBridge, timestamp);
	}

	IXMLDOMNodePtr configTableNode;
	hr = element->selectSingleNode(MstConfigTableString, &configTableNode);
	if (SUCCEEDED(hr) && (configTableNode != nullptr))
	{
		IXMLDOMNodeListPtr nodes;
		hr = configTableNode->get_childNodes(&nodes); ThrowIfFailed(hr);
		long entryCount;
		hr = nodes->get_length(&entryCount); ThrowIfFailed(hr);
		vector<STP_CONFIG_TABLE_ENTRY> configTable;
		for (unsigned int vlan = 0; vlan < (unsigned int) entryCount; vlan++)
		{
			IXMLDOMNodePtr entryNode;
			hr = nodes->get_item(vlan, &entryNode); ThrowIfFailed(hr);
			IXMLDOMElementPtr entryElement (entryNode);

			hr = entryElement->getAttribute(TreeString, &value); ThrowIfFailed(hr);
			auto treeIndex = wcstoul(value.bstrVal, nullptr, 10);
			configTable.push_back (STP_CONFIG_TABLE_ENTRY { 0, (unsigned char) treeIndex });
		}

		STP_SetMstConfigTable(bridge->_stpBridge, &configTable[0], (unsigned int) configTable.size(), timestamp);
	}

	IXMLDOMNodePtr bridgeTreesNode;
	hr = element->selectSingleNode(BridgeTreesString, &bridgeTreesNode);
	if (SUCCEEDED(hr) && (bridgeTreesNode != nullptr))
	{
		IXMLDOMNodeListPtr bridgeTreeNodes;
		hr = bridgeTreesNode->get_childNodes(&bridgeTreeNodes); ThrowIfFailed(hr);

		long treeCount;
		hr = bridgeTreeNodes->get_length(&treeCount); ThrowIfFailed(hr);
		for (long treeIndex = 0; treeIndex < treeCount; treeIndex++)
		{
			IXMLDOMNodePtr bridgeTreeNode;
			hr = bridgeTreeNodes->get_item(treeIndex, &bridgeTreeNode); ThrowIfFailed(hr);
			IXMLDOMElementPtr bridgeTreeElement = bridgeTreeNode;

			hr = bridgeTreeElement->getAttribute(BridgePriorityString, &value);
			if (SUCCEEDED(hr) && (value.vt != VT_NULL))
			{
				auto prio = wcstoul (value.bstrVal, nullptr, 10);
				STP_SetBridgePriority (bridge->_stpBridge, (unsigned int) treeIndex, (unsigned short) prio, timestamp);
			}
		}
	}

	IXMLDOMNodePtr portsNode;
	hr = element->selectSingleNode (PortsString, &portsNode);
	if (SUCCEEDED(hr) && (portsNode != nullptr))
	{
		IXMLDOMNodeListPtr portNodes;
		hr = portsNode->get_childNodes (&portNodes); ThrowIfFailed(hr);

		for (size_t portIndex = 0; portIndex < portCount; portIndex++)
		{
			IXMLDOMNodePtr portNode;
			hr = portNodes->get_item(portIndex, &portNode); ThrowIfFailed(hr);
			IXMLDOMElementPtr portElement = portNode;
			bridge->_ports[portIndex]->Deserialize(portElement);
		}
	}

	return bridge;
}

void Bridge::SetCoordsForInteriorPort (Port* _port, D2D1_POINT_2F proposedLocation)
{
	float mouseX = proposedLocation.x - _x;
	float mouseY = proposedLocation.y - _y;

	float wh = _width / _height;

	// top side
	if ((mouseX > mouseY * wh) && (_width - mouseX) > mouseY * wh)
	{
		_port->_side = Side::Top;

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
		_port->_side = Side::Bottom;

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
		_port->_side = Side::Left;

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
		_port->_side = Side::Right;

		if (mouseY < Port::InteriorWidth / 2)
			_port->_offset = Port::InteriorWidth / 2;
		else if (mouseY > _height - Port::InteriorWidth / 2)
			_port->_offset = _height - Port::InteriorWidth / 2;
		else
			_port->_offset = mouseY;
	}

	InvalidateEvent::InvokeHandlers (this, this);
}

void Bridge::PauseSimulation()
{
	_simulationPaused = true;
}

void Bridge::ResumeSimulation()
{
	_simulationPaused = false;
	ProcessReceivedPackets();
}

std::wstring Bridge::GetMstConfigIdName() const
{
	auto configId = STP_GetMstConfigId(_stpBridge);
	std::string ascii (configId->ConfigurationName, configId->ConfigurationName + strnlen (configId->ConfigurationName, 32));
	wstring_convert<codecvt_utf8<wchar_t>> converter;
	std::wstring utf16 = converter.from_bytes(ascii);
	return utf16;
}

void Bridge::SetMstConfigIdName (std::wstring value, unsigned int timestamp)
{
	auto len = wcslen(value.c_str());
	if (len > 32)
		throw invalid_argument("Invalid MST Config Name: more than 32 characters.");

	string ascii;
	for (auto p = value.c_str(); *p != 0; p++)
	{
		if (*p >= 128)
			throw invalid_argument("Invalid MST Config Name: non-ASCII characters.");

		ascii.push_back((char) *p);
	}
	ascii.resize(32);

	STP_SetMstConfigName (_stpBridge, ascii.c_str(), timestamp);
}

unsigned short Bridge::GetMstConfigIdRevLevel() const
{
	auto id = STP_GetMstConfigId(_stpBridge);
	return ((unsigned short) id->RevisionLevelHigh << 8) | (unsigned short) id->RevisionLevelLow;
}

void Bridge::SetMstConfigIdRevLevel (unsigned short revLevel, unsigned int timestamp)
{
	STP_SetMstConfigRevisionLevel (_stpBridge, revLevel, timestamp);
}

std::wstring Bridge::GetMstConfigIdDigest() const
{
	const unsigned char* digest = STP_GetMstConfigId(_stpBridge)->ConfigurationDigest;
	wstringstream ss;
	ss << uppercase << setfill(L'0') << hex
		<< setw(2) << digest[0]  << setw(2) << digest[1]  << setw(2) << digest[2]  << setw(2) << digest[3]
		<< setw(2) << digest[4]  << setw(2) << digest[5]  << setw(2) << digest[6]  << setw(2) << digest[7]
		<< setw(2) << digest[8]  << setw(2) << digest[9]  << setw(2) << digest[10] << setw(2) << digest[11]
		<< setw(2) << digest[12] << setw(2) << digest[13] << setw(2) << digest[14] << setw(2) << digest[15];
	return ss.str();
}

void Bridge::SetStpEnabled (bool value, unsigned int timestamp)
{
	if (value && !STP_IsBridgeStarted(_stpBridge))
		STP_StartBridge (_stpBridge, timestamp);
	else if (!value && STP_IsBridgeStarted(_stpBridge))
		STP_StopBridge (_stpBridge, timestamp);
}

void Bridge::SetStpVersionFromInt (int value, unsigned int timestamp)
{
	auto newVersion = (STP_VERSION) value;
	if (STP_GetStpVersion(_stpBridge) != newVersion)
		STP_SetStpVersion(_stpBridge, newVersion, timestamp);
}

static const PropertyGroup CommonPropGroup
{
	L"Common",
	nullptr,
};

static const TypedProperty<wstring> AddressProperty
(
	L"Bridge Address",
	nullptr,
	static_cast<TypedProperty<wstring>::Getter>(&Bridge::GetBridgeAddressAsWString),
	static_cast<TypedProperty<wstring>::Setter>(&Bridge::SetBridgeAddressFromWString)
);

static const TypedProperty<bool> BridgePropStpEnabled
(
	L"STP Enabled",
	nullptr,
	static_cast<TypedProperty<bool>::Getter>(&Bridge::GetStpEnabled),
	static_cast<TypedProperty<bool>::Setter>(&Bridge::SetStpEnabled)
);

static const NVP StpVersionNVPs[] = { { L"LegacySTP", STP_VERSION_LEGACY_STP }, { L"RSTP", STP_VERSION_RSTP }, { L"MSTP", STP_VERSION_MSTP }, { 0, 0 } };

static const EnumProperty BridgePropStpVersion
{
	L"STP Version",
	nullptr,
	static_cast<EnumProperty::Getter>(&Bridge::GetStpVersionAsInt),
	static_cast<EnumProperty::Setter>(&Bridge::SetStpVersionFromInt),
	StpVersionNVPs
};

static const TypedProperty<unsigned int> BridgePropPortCount
{
	L"Port Count",
	nullptr,
	static_cast<TypedProperty<unsigned int>::Getter>(&Bridge::GetPortCount),
	nullptr
};

static const TypedProperty<unsigned int> BridgePropMstiCount
{
	L"MSTI Count",
	nullptr,
	static_cast<TypedProperty<unsigned int>::Getter>(&Bridge::GetMstiCount),
	nullptr
};

static const PropertyGroup MstConfigIdGroup
{
	L"MST Config ID",
	nullptr,
};

static const TypedProperty<wstring> MstConfigIdName
(
	L"Name",
	nullptr,
	static_cast<TypedProperty<wstring>::Getter>(&Bridge::GetMstConfigIdName),
	static_cast<TypedProperty<wstring>::Setter>(&Bridge::SetMstConfigIdName)
);

static const TypedProperty<unsigned short> MstConfigIdRevLevel
(
	L"Revision Level",
	nullptr,
	static_cast<TypedProperty<unsigned short>::Getter>(&Bridge::GetMstConfigIdRevLevel),
	static_cast<TypedProperty<unsigned short>::Setter>(&Bridge::SetMstConfigIdRevLevel)
);

static const TypedProperty<wstring> MstConfigIdDigest
(
	L"Digest",
	nullptr,
	static_cast<TypedProperty<wstring>::Getter>(&Bridge::GetMstConfigIdDigest),
	nullptr,
	mstConfigIdDialogFactory
);

static const PropertyOrGroup* const BridgeProperties[] =
{
	&CommonPropGroup,
	&AddressProperty,
	&BridgePropStpEnabled,
	&BridgePropStpVersion,
	&BridgePropPortCount,
	&BridgePropMstiCount,
	&MstConfigIdGroup,
	&MstConfigIdName,
	&MstConfigIdRevLevel,
	&MstConfigIdDigest,
	nullptr,
};

const PropertyOrGroup* const* Bridge::GetProperties() const
{
	return BridgeProperties;
}

#pragma region STP Callbacks
const STP_CALLBACKS Bridge::StpCallbacks =
{
	StpCallback_EnableLearning,
	StpCallback_EnableForwarding,
	StpCallback_TransmitGetBuffer,
	StpCallback_TransmitReleaseBuffer,
	StpCallback_FlushFdb,
	StpCallback_DebugStrOut,
	StpCallback_OnTopologyChange,
	StpCallback_OnNotifiedTopologyChange,
	StpCallback_OnPortRoleChanged,
	StpCallback_OnConfigChanged,
	StpCallback_AllocAndZeroMemory,
	StpCallback_FreeMemory,
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
	PacketTransmitEvent::InvokeHandlers (b, b, b->_txTransmittingPort->GetPortIndex(), move(info));
}

void Bridge::StpCallback_EnableLearning (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int enable, unsigned int timestamp)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	InvalidateEvent::InvokeHandlers (b, b);
}

void Bridge::StpCallback_EnableForwarding (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int enable, unsigned int timestamp)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	InvalidateEvent::InvokeHandlers(b, b);
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
				LogLineGenerated::InvokeHandlers (b, b, b->_logLines.back().get());
			}

			b->_currentLogLine.text.append (nullTerminatedString, (size_t) stringLength);
		}

		if (!b->_currentLogLine.text.empty() && (b->_currentLogLine.text.back() == L'\n'))
		{
			b->_logLines.push_back(make_unique<BridgeLogLine>(move(b->_currentLogLine)));
			LogLineGenerated::InvokeHandlers (b, b, b->_logLines.back().get());
		}
	}

	if (flush && !b->_currentLogLine.text.empty())
	{
		b->_logLines.push_back(make_unique<BridgeLogLine>(move(b->_currentLogLine)));
		LogLineGenerated::InvokeHandlers (b, b, b->_logLines.back().get());
	}
}

void Bridge::StpCallback_OnTopologyChange (const STP_BRIDGE* bridgetimestamp)
{
}

void Bridge::StpCallback_OnNotifiedTopologyChange (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int timestamp)
{
}

void Bridge::StpCallback_OnPortRoleChanged (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, STP_PORT_ROLE role, unsigned int timestamp)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	InvalidateEvent::InvokeHandlers(b, b);
}

void Bridge::StpCallback_OnConfigChanged (const STP_BRIDGE* bridge, unsigned int timestamp)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	ConfigChangedEvent::InvokeHandlers (b, b);
	InvalidateEvent::InvokeHandlers(b, b);
}
#pragma endregion


STP_BRIDGE_ADDRESS ConvertStringToBridgeAddress (const wchar_t* str)
{
	static constexpr char FormatErrorMessage[] = u8"Invalid address format. The Bridge Address must have the format XX:XX:XX:XX:XX:XX or XXXXXXXXXXXX (6 hex bytes).";

	int offsetMultiplier;
	if (wcslen(str) == 12)
	{
		offsetMultiplier = 2;
	}
	else if (wcslen(str) == 17)
	{
		if ((str[2] != ':') || (str[5] != ':') || (str[8] != ':') || (str[11] != ':') || (str[14] != ':'))
			throw invalid_argument(FormatErrorMessage);

		offsetMultiplier = 3;
	}
	else
		throw invalid_argument(FormatErrorMessage);

	STP_BRIDGE_ADDRESS newAddress;
	for (size_t i = 0; i < 6; i++)
	{
		wchar_t ch0 = str[i * offsetMultiplier];
		wchar_t ch1 = str[i * offsetMultiplier + 1];

		if (!iswxdigit(ch0) || !iswxdigit(ch1))
			throw invalid_argument(FormatErrorMessage);

		auto hn = (ch0 <= '9') ? (ch0 - '0') : ((ch0 >= 'a') ? (ch0 - 'a' + 10) : (ch0 - 'A' + 10));
		auto ln = (ch1 <= '9') ? (ch1 - '0') : ((ch1 >= 'a') ? (ch1 - 'a' + 10) : (ch1 - 'A' + 10));
		newAddress.bytes[i] = (hn << 4) | ln;
	}

	return newAddress;
}

std::string ConvertBridgeAddressToString (const unsigned char address[6])
{
	stringstream ss;
	ss << uppercase << setfill('0') << hex
		<< setw(2) << (int) address[0] << setw(2) << (int) address[1] << setw(2) << (int) address[2]
		<< setw(2) << (int) address[3] << setw(2) << (int) address[4] << setw(2) << (int) address[5];
	return ss.str();
}

std::wstring ConvertBridgeAddressToWString (const unsigned char address[6])
{
	wstringstream ss;
	ss << uppercase << setfill(L'0') << hex
		<< setw(2) << address[0] << setw(2) << address[1] << setw(2) << address[2]
		<< setw(2) << address[3] << setw(2) << address[4] << setw(2) << address[5];
	return ss.str();
}

