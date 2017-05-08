#include "pch.h"
#include "Bridge.h"
#include "Win32Defs.h"
#include "Port.h"
#include "Wire.h"
#include "UtilityFunctions.h"
#include "mstp-lib/stp_md5.h"

using namespace std;
using namespace D2D1;

static constexpr UINT WM_ONE_SECOND_TIMER = WM_APP + 1;
static constexpr UINT WM_MAC_OPERATIONAL_TIMER = WM_APP + 2;
static constexpr UINT WM_PACKET_RECEIVED = WM_APP + 3;

static constexpr uint8_t BpduDestAddress[6] = { 1, 0x80, 0xC2, 0, 0, 0 };

HWND     Bridge::_helperWindow;
uint32_t Bridge::_helperWindowRefCount;

Bridge::Bridge (IProject* project, unsigned int portCount, unsigned int mstiCount, const std::array<uint8_t, 6>& macAddress)
	: _project(project)
{
	float offset = 0;

	for (unsigned int i = 0; i < portCount; i++)
	{
		offset += (Port::PortToPortSpacing / 2 + Port::InteriorWidth / 2);
		auto port = ComPtr<Port>(new Port(this, i, Side::Bottom, offset), false);
		_ports.push_back (move(port));
		offset += (Port::InteriorWidth / 2 + Port::PortToPortSpacing / 2);
	}

	_x = 0;
	_y = 0;
	_width = max (offset, MinWidth);
	_height = DefaultHeight;

	if (_helperWindow == nullptr)
	{
		HINSTANCE hInstance;
		BOOL bRes = GetModuleHandleEx (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR) &StpCallbacks, &hInstance);
		if (!bRes)
			throw win32_exception(GetLastError());

		_helperWindow = CreateWindow (L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, hInstance, 0);
		if (_helperWindow == nullptr)
			throw win32_exception(GetLastError());

		bRes = SetWindowSubclass (_helperWindow, HelperWindowProc, 0, 0);
		if (!bRes)
			throw win32_exception(GetLastError());
	}

	_helperWindowRefCount++;

	DWORD period = 950 + (std::random_device()() % 100);
	HANDLE handle;
	BOOL bRes = ::CreateTimerQueueTimer (&handle, nullptr, OneSecondTimerCallback, this, period, period, 0);
	if (!bRes)
		throw win32_exception(GetLastError());
	_oneSecondTimerHandle.reset(handle);

	period = 45 + (std::random_device()() % 10);
	bRes = ::CreateTimerQueueTimer (&handle, nullptr, MacOperationalTimerCallback, this, period, period, 0);
	if (!bRes)
		throw win32_exception(GetLastError());
	_macOperationalTimerHandle.reset(handle);

	_stpBridge = STP_CreateBridge (portCount, mstiCount, MaxVlanNumber, &StpCallbacks, &macAddress[0], 256);
	STP_EnableLogging (_stpBridge, true);
	STP_SetApplicationContext (_stpBridge, this);
}

Bridge::~Bridge()
{
	// First stop the timers, to be sure the mutex won't be acquired in a background thread (when we'll have background threads).
	_macOperationalTimerHandle = nullptr;
	_oneSecondTimerHandle = nullptr;

	_helperWindowRefCount--;
	if (_helperWindowRefCount == 0)
	{
		::RemoveWindowSubclass (_helperWindow, HelperWindowProc, 0);
		::DestroyWindow (_helperWindow);
	}

	STP_DestroyBridge (_stpBridge);
}

//static
void CALLBACK Bridge::OneSecondTimerCallback (void* lpParameter, BOOLEAN TimerOrWaitFired)
{
	auto bridge = static_cast<Bridge*>(lpParameter);
	::PostMessage (bridge->_helperWindow, WM_ONE_SECOND_TIMER, (WPARAM) bridge, 0);
}

//static
void CALLBACK Bridge::MacOperationalTimerCallback (void* lpParameter, BOOLEAN TimerOrWaitFired)
{
	auto bridge = static_cast<Bridge*>(lpParameter);
	::PostMessage (bridge->_helperWindow, WM_MAC_OPERATIONAL_TIMER, (WPARAM) bridge, 0);
}

// static
LRESULT CALLBACK Bridge::HelperWindowProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	if (uMsg == WM_ONE_SECOND_TIMER)
	{
		auto bridge = static_cast<Bridge*>((void*)wParam);

		if (STP_IsBridgeStarted(bridge->_stpBridge))
			STP_OnOneSecondTick (bridge->_stpBridge, GetTimestampMilliseconds());

		return 0;
	}
	else if (uMsg == WM_MAC_OPERATIONAL_TIMER)
	{
		auto bridge = static_cast<Bridge*>((void*)wParam);
		bridge->ComputeMacOperational();
		return 0;
	}
	else if (uMsg == WM_PACKET_RECEIVED)
	{
		auto bridge = static_cast<Bridge*>((void*)wParam);
		bridge->ProcessReceivedPacket();
		return 0;
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// Checks the wires and computes macOperational for each port on this bridge.
void Bridge::ComputeMacOperational()
{
	auto timestamp = GetTimestampMilliseconds();

	bool invalidate = false;
	for (unsigned int portIndex = 0; portIndex < _ports.size(); portIndex++)
	{
		auto& port = _ports[portIndex];

		bool newMacOperational = (_project->FindConnectedPort(port) != nullptr);
		if (port->_macOperational != newMacOperational)
		{
			if (port->_macOperational)
			{
				// port just disconnected
				STP_OnPortDisabled (_stpBridge, portIndex, timestamp);
			}

			port->_macOperational = newMacOperational;

			if (port->_macOperational)
			{
				// port just connected
				STP_OnPortEnabled (_stpBridge, portIndex, 100, true, timestamp);
			}

			invalidate = true;
		}
	}

	if (invalidate)
		InvalidateEvent::InvokeHandlers(_em, this);
}

void Bridge::ProcessReceivedPacket()
{
	auto rp = move(_rxQueue.front());
	_rxQueue.pop();

	bool invalidate = false;
	if (!_ports[rp.portIndex]->_macOperational)
	{
		_ports[rp.portIndex]->_macOperational = true;
		invalidate = true;
	}

	if (memcmp (&rp.data[0], BpduDestAddress, 6) == 0)
	{
		// It's a BPDU.
		if (STP_IsBridgeStarted(_stpBridge))
		{
			if (!STP_GetPortEnabled(_stpBridge, rp.portIndex))
			{
				STP_OnPortEnabled (_stpBridge, rp.portIndex, 100, true, rp.timestamp);
				invalidate = true;
			}

			STP_OnBpduReceived (_stpBridge, rp.portIndex, &rp.data[21], (unsigned int) (rp.data.size() - 21), rp.timestamp);
		}
		else
		{
			// broadcast it to the other ports.
			for (size_t i = 0; i < _ports.size(); i++)
			{
				if (i != rp.portIndex)
				{
					auto txPortAddress = GetPortAddress(i);

					// If it already went through this port, we have a loop that would hang our UI.
					if (find (rp.txPortPath.begin(), rp.txPortPath.end(), txPortAddress) != rp.txPortPath.end())
					{
						// We don't do anything here; we have code in Wire.cpp that shows loops to the user - as thick red wires.
					}
					else
					{
						auto rxPort = _project->FindConnectedPort(_ports[i]);
						if (rxPort != nullptr)
						{
							RxPacketInfo info = rp;
							info.portIndex = rxPort->_portIndex;
							info.txPortPath.push_back(txPortAddress);
							rxPort->_bridge->_rxQueue.push(move(info));
							::PostMessage (rxPort->_bridge->_helperWindow, WM_PACKET_RECEIVED, (WPARAM)(void*)rxPort->_bridge, 0);
						}
					}
				}
			}
		}
	}

	if (invalidate)
		InvalidateEvent::InvokeHandlers(_em, this);
}

STP_VERSION Bridge::GetStpVersion() const
{
	return STP_GetStpVersion(_stpBridge);
}

void Bridge::SetLocation(float x, float y)
{
	if ((_x != x) || (_y != y))
	{
		InvalidateEvent::InvokeHandlers(_em, this);
		_x = x;
		_y = y;
		InvalidateEvent::InvokeHandlers(_em, this);
	}
}

void Bridge::Render (ID2D1RenderTarget* dc, const DrawingObjects& dos, unsigned int vlanNumber) const
{
	bool isRootBridge = STP_IsBridgeStarted(_stpBridge) && STP_IsRootBridge(_stpBridge);
	// Draw bridge outline.
	D2D1_ROUNDED_RECT rr = RoundedRect (GetBounds(), RoundRadius, RoundRadius);
	float ow = OutlineWidth * (isRootBridge ? 2 : 1);
	InflateRoundedRect (&rr, -ow / 2);
	dc->FillRoundedRectangle (&rr, _powered ? dos._poweredFillBrush : dos._unpoweredBrush);
	dc->DrawRoundedRectangle (&rr, dos._brushWindowText, ow);

	auto stpVersion = STP_GetStpVersion(_stpBridge);

	array<uint8_t, 6> macAddress;
	STP_GetBridgeAddress(_stpBridge, macAddress.data());

	// Draw bridge name.
	wstringstream ss;
	if (STP_IsBridgeStarted(_stpBridge))
	{
		auto treeIndex = STP_GetTreeIndexFromVlanNumber(_stpBridge, vlanNumber);
		ss << uppercase << setfill(L'0') << setw(4) << hex << STP_GetBridgePriority(_stpBridge, treeIndex) << L'.'
			<< setw(2) << macAddress[0] << setw(2) << macAddress[1] << setw(2) << macAddress[2]
			<< setw(2) << macAddress[3] << setw(2) << macAddress[4] << setw(2) << macAddress[5] << endl
			<< L"STP enabled (" << STP_GetVersionString(stpVersion) << L")" << endl
			<< L"VLAN " << dec << vlanNumber << L" (spanning tree " << treeIndex << L")" << endl
			<< (isRootBridge ? L"Root Bridge\r\n" : L"");
	}
	else
	{
		ss << uppercase << setfill(L'0') << hex
			<< setw(2) << macAddress[0] << setw(2) << macAddress[1] << setw(2) << macAddress[2]
			<< setw(2) << macAddress[3] << setw(2) << macAddress[4] << setw(2) << macAddress[5] << endl
			<< L"STP disabled\r\n(right-click to enable)";
	}

	auto tl = TextLayout::Make (dos._dWriteFactory, dos._regularTextFormat, ss.str().c_str());
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

HTResult Bridge::HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance)
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

bool Bridge::IsPortForwardingOnVlan (unsigned int portIndex, unsigned int vlanNumber) const
{
	if (!STP_IsBridgeStarted(_stpBridge))
		return true;

	auto treeIndex = STP_GetTreeIndexFromVlanNumber(_stpBridge, vlanNumber);
	return STP_GetPortForwarding(_stpBridge, portIndex, treeIndex);
}

bool Bridge::IsStpRootBridge() const
{
	if (!STP_IsBridgeStarted(_stpBridge))
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_IsRootBridge(_stpBridge);
}

//static
wstring Bridge::GetStpVersionString (STP_VERSION stpVersion)
{
	static wstring_convert<codecvt_utf8<wchar_t>> converter;
	return converter.from_bytes (STP_GetVersionString(stpVersion));
}

wstring Bridge::GetStpVersionString() const
{
	auto stpVersion = STP_GetStpVersion(_stpBridge);
	return GetStpVersionString(stpVersion);
}

std::array<uint8_t, 6> Bridge::GetBridgeAddress() const
{
	std::array<uint8_t, 6> address;
	STP_GetBridgeAddress (_stpBridge, address.data());
	return address;
}

std::wstring Bridge::GetBridgeAddressAsString() const
{
	auto address = GetBridgeAddress();
	std::wstring str;
	str.resize(18);
	swprintf_s (str.data(), str.size(), L"%02X:%02X:%02X:%02X:%02X:%02X", address[0], address[1], address[2], address[3], address[4], address[5]);
	return str;
}

array<uint8_t, 16> Bridge::GetMstConfigDigest()
{
	unsigned int digestLength;
	auto digest = STP_GetMstConfigTableDigest(_stpBridge, &digestLength);
	assert (digestLength == 16);
	array<uint8_t, 16> result;
	memcpy (result.data(), digest, 16);
	return result;
}

std::array<uint8_t, 6> Bridge::GetPortAddress (size_t portIndex) const
{
	std::array<uint8_t, 6> pa = GetBridgeAddress();
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

void* Bridge::StpCallback_TransmitGetBuffer (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	auto txPort = b->_ports[portIndex].Get();
	auto rxPort = b->_project->FindConnectedPort(txPort);
	if (rxPort == nullptr)
		return nullptr; // The port was disconnected and our port polling code hasn't reacted yet. This is what happens in a real system too.

	b->_txPacketData.resize (bpduSize + 21);
	memcpy (&b->_txPacketData[0], BpduDestAddress, 6);
	memcpy (&b->_txPacketData[6], &b->GetPortAddress(portIndex)[0], 6);
	b->_txTransmittingPort = txPort;
	b->_txReceivingPort = rxPort;
	b->_txTimestamp = timestamp;
	return &b->_txPacketData[21];
}

void Bridge::StpCallback_TransmitReleaseBuffer (STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));

	RxPacketInfo info;
	info.data = move(b->_txPacketData);
	info.portIndex = b->_txReceivingPort->_portIndex;
	info.timestamp = b->_txTimestamp;
	info.txPortPath.push_back (b->GetPortAddress(b->_txTransmittingPort->GetPortIndex()));
	Bridge* receivingBridge = b->_txReceivingPort->_bridge;
	receivingBridge->_rxQueue.push (move(info));
	::PostMessage (receivingBridge->_helperWindow, WM_PACKET_RECEIVED, (WPARAM)(void*)receivingBridge, 0);
}

void Bridge::StpCallback_EnableLearning(STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int enable)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	InvalidateEvent::InvokeHandlers (b->_em, b);
}

void Bridge::StpCallback_EnableForwarding(STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int enable)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	InvalidateEvent::InvokeHandlers(b->_em, b);
}

void Bridge::StpCallback_FlushFdb (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
}

void Bridge::StpCallback_DebugStrOut (STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, unsigned int flush)
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
				b->_logLines.push_back(move(b->_currentLogLine));
				BridgeLogLineGenerated::InvokeHandlers (b->_em, b, b->_logLines.back());
			}

			b->_currentLogLine.text.append (nullTerminatedString, (size_t) stringLength);
		}

		if (!b->_currentLogLine.text.empty() && (b->_currentLogLine.text.back() == L'\n'))
		{
			b->_logLines.push_back(move(b->_currentLogLine));
			BridgeLogLineGenerated::InvokeHandlers (b->_em, b, b->_logLines.back());
		}
	}

	if (flush && !b->_currentLogLine.text.empty())
	{
		b->_logLines.push_back(move(b->_currentLogLine));
		BridgeLogLineGenerated::InvokeHandlers (b->_em, b, b->_logLines.back());
	}
}

void Bridge::StpCallback_OnTopologyChange (STP_BRIDGE* bridgetimestamp)
{
}

void Bridge::StpCallback_OnNotifiedTopologyChange (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int timestamp)
{
}

void Bridge::StpCallback_OnPortRoleChanged (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, STP_PORT_ROLE role, unsigned int timestamp)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	InvalidateEvent::InvokeHandlers(b->_em, b);
}

void Bridge::StpCallback_OnConfigChanged (struct STP_BRIDGE* bridge, unsigned int timestamp)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	BridgeConfigChangedEvent::InvokeHandlers (b->_em, b);
}
#pragma endregion
