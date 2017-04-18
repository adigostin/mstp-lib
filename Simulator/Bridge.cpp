#include "pch.h"
#include "Bridge.h"
#include "Win32Defs.h"
#include "Port.h"
#include "Wire.h"

using namespace std;
using namespace D2D1;

static constexpr UINT WM_ONE_SECOND_TIMER = WM_APP + 1;
static constexpr UINT WM_MAC_OPERATIONAL_TIMER = WM_APP + 2;
static constexpr UINT WM_PACKET_RECEIVED = WM_APP + 3;

static constexpr uint8_t BpduDestAddress[6] = { 1, 0x80, 0xC2, 0, 0, 0 };

Bridge::Bridge (IProject* project, unsigned int portCount, const std::array<uint8_t, 6>& macAddress)
	: _project(project), _config({macAddress}), _guiThreadId(this_thread::get_id())
{
	float offset = 0;

	for (size_t i = 0; i < portCount; i++)
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

	HINSTANCE hInstance;
	BOOL bRes = GetModuleHandleEx (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR) &StpCallbacks, &hInstance);
	if (!bRes)
		throw win32_exception(GetLastError());

	auto hwnd = CreateWindow (L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, hInstance, 0);
	if (hwnd == nullptr)
		throw win32_exception(GetLastError());
	_helperWindow.reset (hwnd);

	bRes = SetWindowSubclass (hwnd, HelperWindowProc, 0, 0);
	if (!bRes)
		throw win32_exception(GetLastError());

	DWORD period = 950 + (std::random_device()() % 100);
	HANDLE handle;
	bRes = CreateTimerQueueTimer (&handle, nullptr, OneSecondTimerCallback, this, period, period, 0);
	if (!bRes)
		throw win32_exception(GetLastError());
	_oneSecondTimerHandle.reset(handle);

	period = 45 + (std::random_device()() % 10);
	bRes = CreateTimerQueueTimer (&handle, nullptr, MacOperationalTimerCallback, this, period, period, 0);
	if (!bRes)
		throw win32_exception(GetLastError());
	_macOperationalTimerHandle.reset(handle);
}

Bridge::~Bridge()
{
	// First stop the timers, to be sure the mutex won't be acquired in a background thread (when we'll have background threads).
	_macOperationalTimerHandle = nullptr;
	_oneSecondTimerHandle = nullptr;

	RemoveWindowSubclass (_helperWindow.get(), HelperWindowProc, 0);

	if (_stpBridge != nullptr)
		STP_DestroyBridge (_stpBridge);
}

//static
void CALLBACK Bridge::OneSecondTimerCallback (void* lpParameter, BOOLEAN TimerOrWaitFired)
{
	auto bridge = static_cast<Bridge*>(lpParameter);
	::PostMessage (bridge->_helperWindow.get(), WM_ONE_SECOND_TIMER, (WPARAM) bridge, 0);
}

//static
void CALLBACK Bridge::MacOperationalTimerCallback (void* lpParameter, BOOLEAN TimerOrWaitFired)
{
	auto bridge = static_cast<Bridge*>(lpParameter);
	::PostMessage (bridge->_helperWindow.get(), WM_MAC_OPERATIONAL_TIMER, (WPARAM) bridge, 0);
}

// static
LRESULT CALLBACK Bridge::HelperWindowProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	if (uMsg == WM_ONE_SECOND_TIMER)
	{
		auto bridge = static_cast<Bridge*>((void*)wParam);

		if (bridge->_stpBridge != nullptr)
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

void Bridge::ComputeMacOperational()
{
	assert (this_thread::get_id() == _guiThreadId);

	auto timestamp = GetTimestampMilliseconds();

	bool invalidate = false;
	for (size_t portIndex = 0; portIndex < _ports.size(); portIndex++)
	{
		auto& port = _ports[portIndex];

		bool newMacOperational = (_project->FindReceivingPort(port) != nullptr);
		if (port->_macOperational != newMacOperational)
		{
			if (port->_macOperational)
			{
				// port just disconnected
				if (_stpBridge)
					STP_OnPortDisabled (_stpBridge, portIndex, timestamp);
			}

			port->_macOperational = newMacOperational;

			if (port->_macOperational)
			{
				// port just connected
				if (_stpBridge)
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

	if (memcmp (&rp.data[0], BpduDestAddress, 6) == 0)
	{
		// It's a BPDU.
		if (_stpBridge != nullptr)
		{
			if (!_ports[rp.portIndex]->_macOperational)
			{
				_ports[rp.portIndex]->_macOperational = true;
				STP_OnPortEnabled (_stpBridge, rp.portIndex, 100, true, rp.timestamp);
				InvalidateEvent::InvokeHandlers (_em, this);
			}

			STP_OnBpduReceived (_stpBridge, rp.portIndex, &rp.data[21], (unsigned int) (rp.data.size() - 21), rp.timestamp);
		}
		else
		{
			// broadcast it to the other ports.
			for (auto& txPort : _ports)
			{
				if (txPort->_portIndex != rp.portIndex)
				{
					auto rxPort = _project->FindReceivingPort(txPort);
					if (rxPort != nullptr)
					{
						RxPacketInfo info = { rp.data, rxPort->_portIndex, rp.timestamp };
						rxPort->_bridge->_rxQueue.push(move(info));
						::PostMessage (rxPort->_bridge->_helperWindow.get(), WM_PACKET_RECEIVED, (WPARAM)(void*)rxPort->_bridge, 0);
					}
				}
			}
		}
	}
}

void Bridge::EnableStp (uint32_t timestamp)
{
	if (this_thread::get_id() != _guiThreadId)
		throw runtime_error ("This function may be called only on the main thread.");

	if (_stpBridge != nullptr)
		throw runtime_error ("STP is already enabled on this bridge.");

	if ((_config._treeCount != 1) && (_config._stpVersion != STP_VERSION_MSTP))
		throw runtime_error ("The Tree Count must be set to 1 when not using MSTP.");

	_stpBridge = STP_CreateBridge ((unsigned int) _ports.size(), _config._treeCount, &StpCallbacks, _config._stpVersion, &_config._macAddress[0], 256);
	STP_SetApplicationContext (_stpBridge, this);
	STP_EnableLogging (_stpBridge, true);

	for (size_t portIndex = 0; portIndex < _ports.size(); portIndex++)
	{
		auto& port = _ports[portIndex];
		STP_SetPortAdminEdge (_stpBridge, (unsigned int) portIndex, port->_config.adminEdge, timestamp);
		STP_SetPortAutoEdge (_stpBridge, (unsigned int) portIndex, port->_config.autoEdge, timestamp);
	}

	STP_StartBridge (_stpBridge, timestamp);
	StpEnabledEvent::InvokeHandlers (_em, this);

	for (size_t portIndex = 0; portIndex < _ports.size(); portIndex++)
	{
		auto& port = _ports[portIndex];
		if (_project->FindReceivingPort(port) != nullptr)
			STP_OnPortEnabled (_stpBridge, portIndex, 100, true, timestamp);
	}

	InvalidateEvent::InvokeHandlers(_em, this);
}

void Bridge::DisableStp (uint32_t timestamp)
{
	if (this_thread::get_id() != _guiThreadId)
		throw runtime_error ("This function may be called only on the main thread.");

	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	StpDisablingEvent::InvokeHandlers(_em, this);
	STP_StopBridge(_stpBridge, timestamp);
	STP_DestroyBridge (_stpBridge);
	_stpBridge = nullptr;

	InvalidateEvent::InvokeHandlers(_em, this);
}

void Bridge::SetStpVersion (STP_VERSION stpVersion, uint32_t timestamp)
{
	if (this_thread::get_id() != _guiThreadId)
		throw runtime_error ("This function may be called only on the main thread.");

	if (_stpBridge != nullptr)
		throw runtime_error ("Setting the protocol version while STP is enabled is not supported by the library.");

	if (_config._stpVersion != stpVersion)
	{
		_config._stpVersion = stpVersion;
		StpVersionChangedEvent::InvokeHandlers(_em, this);
	}
}

void Bridge::SetStpTreeCount (size_t treeCount)
{
	if (this_thread::get_id() != _guiThreadId)
		throw runtime_error ("This function may be called only on the main thread.");

	if (_stpBridge != nullptr)
		throw runtime_error ("Setting the tree count while STP is enabled is not supported by the library.");

	if (_config._treeCount != treeCount)
	{
		_config._treeCount = treeCount;
	}
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

STP_PORT_ROLE Bridge::GetStpPortRole (size_t portIndex, size_t treeIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetPortRole (_stpBridge, portIndex, treeIndex);
}

bool Bridge::GetStpPortLearning (size_t portIndex, size_t treeIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetPortLearning (_stpBridge, portIndex, treeIndex);
}

bool Bridge::GetStpPortForwarding (size_t portIndex, size_t treeIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetPortForwarding (_stpBridge, portIndex, treeIndex);
}

bool Bridge::GetStpPortOperEdge (size_t portIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetPortOperEdge (_stpBridge, portIndex);
}

bool Bridge::GetPortAdminEdge (size_t portIndex) const
{
	return _ports[portIndex]->_config.adminEdge;
}

bool Bridge::GetPortAutoEdge (size_t portIndex) const
{
	return _ports[portIndex]->_config.autoEdge;
}

void Bridge::SetPortAdminEdge (size_t portIndex, bool adminEdge)
{
	if (_ports[portIndex]->_config.adminEdge != adminEdge)
	{
		_ports[portIndex]->_config.adminEdge = adminEdge;
		
		if (_stpBridge != nullptr)
			STP_SetPortAdminEdge (_stpBridge, portIndex, adminEdge, GetTimestampMilliseconds());
	}
}

void Bridge::SetPortAutoEdge (size_t portIndex, bool autoEdge)
{
	if (_ports[portIndex]->_config.autoEdge != autoEdge)
	{
		_ports[portIndex]->_config.autoEdge = autoEdge;

		if (_stpBridge != nullptr)
			STP_SetPortAutoEdge (_stpBridge, portIndex, autoEdge, GetTimestampMilliseconds());
	}
}

unsigned short Bridge::GetStpBridgePriority (size_t treeIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetBridgePriority(_stpBridge, treeIndex);
}

size_t Bridge::GetStpTreeIndexFromVlanNumber (uint16_t vlanNumber) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	if ((vlanNumber == 0) || (vlanNumber > 4094))
		throw invalid_argument ("The VLAN number must be >=1 and <=4094.");

	return STP_GetTreeIndexFromVlanNumber(_stpBridge, vlanNumber);
}

void Bridge::Render (ID2D1RenderTarget* dc, const DrawingObjects& dos, uint16_t vlanNumber) const
{
	bool isRootBridge = ((_stpBridge != nullptr) && STP_IsRootBridge(_stpBridge));
	// Draw bridge outline.
	D2D1_ROUNDED_RECT rr = RoundedRect (GetBounds(), RoundRadius, RoundRadius);
	float ow = OutlineWidth * (isRootBridge ? 2 : 1);
	InflateRoundedRect (&rr, -ow / 2);
	dc->FillRoundedRectangle (&rr, _powered ? dos._poweredFillBrush : dos._unpoweredBrush);
	dc->DrawRoundedRectangle (&rr, dos._brushWindowText, ow);

	// Draw bridge name.
	wstringstream ss;
	if (IsStpEnabled())
	{
		auto treeIndex = STP_GetTreeIndexFromVlanNumber(_stpBridge, vlanNumber);
		ss << uppercase << setfill(L'0') << setw(4) << hex << STP_GetBridgePriority(_stpBridge, treeIndex) << L'.'
			<< setw(2) << _config._macAddress[0] << setw(2) << _config._macAddress[1] << setw(2) << _config._macAddress[2]
			<< setw(2) << _config._macAddress[3] << setw(2) << _config._macAddress[4] << setw(2) << _config._macAddress[5] << endl
			<< L"STP enabled (" << STP_GetVersionString(_config._stpVersion) << L")" << endl
			<< L"VLAN " << dec << vlanNumber << L" (spanning tree " << treeIndex << L")" << endl
			<< (isRootBridge ? L"Root Bridge\r\n" : L"");
	}
	else
	{
		ss << uppercase << setfill(L'0') << hex
			<< setw(2) << _config._macAddress[0] << setw(2) << _config._macAddress[1] << setw(2) << _config._macAddress[2]
			<< setw(2) << _config._macAddress[3] << setw(2) << _config._macAddress[4] << setw(2) << _config._macAddress[5] << endl
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

bool Bridge::IsPortForwardingOnVlan (unsigned int portIndex, uint16_t vlanNumber) const
{
	if (_stpBridge == nullptr)
		return true;

	auto treeIndex = STP_GetTreeIndexFromVlanNumber(_stpBridge, vlanNumber);
	return STP_GetPortForwarding(_stpBridge, portIndex, treeIndex);
}

bool Bridge::IsStpRootBridge() const
{
	if (_stpBridge == nullptr)
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
	return GetStpVersionString(_config._stpVersion);
}

std::wstring Bridge::GetMacAddressAsString() const
{
	std::wstring str;
	str.resize(18);
	swprintf_s (str.data(), str.size(), L"%02X:%02X:%02X:%02X:%02X:%02X",
				_config._macAddress[0], _config._macAddress[1], _config._macAddress[2],
				_config._macAddress[3], _config._macAddress[4], _config._macAddress[5]);
	return str;
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
	auto rxPort = b->_project->FindReceivingPort(txPort);
	if (rxPort == nullptr)
		return nullptr; // The port was disconnected and our port polling code hasn't reacted yet. This is what happens in a real system too.

	b->_txPacketData.resize (bpduSize + 21);
	memcpy (&b->_txPacketData[0], BpduDestAddress, 6);
	b->_txReceivingPort = rxPort;
	b->_txTimestamp = timestamp;
	return &b->_txPacketData[21];
}

void Bridge::StpCallback_TransmitReleaseBuffer (STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer)
{
	auto transmittingBridge = static_cast<Bridge*>(STP_GetApplicationContext(bridge));

	RxPacketInfo info;
	info.data = move(transmittingBridge->_txPacketData);
	info.portIndex = transmittingBridge->_txReceivingPort->_portIndex;
	info.timestamp = transmittingBridge->_txTimestamp;

	Bridge* receivingBridge = transmittingBridge->_txReceivingPort->_bridge;
	receivingBridge->_rxQueue.push (move(info));
	::PostMessage (receivingBridge->_helperWindow.get(), WM_PACKET_RECEIVED, (WPARAM)(void*)receivingBridge, 0);
}

void Bridge::StpCallback_EnableLearning(STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	InvalidateEvent::InvokeHandlers (b->_em, b);
}

void Bridge::StpCallback_EnableForwarding(STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	InvalidateEvent::InvokeHandlers(b->_em, b);
}

void Bridge::StpCallback_FlushFdb (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
}

void Bridge::StpCallback_DebugStrOut (STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, bool flush)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));

	if (this_thread::get_id() != b->_guiThreadId)
		throw std::runtime_error("Logging-related code does not yet support multithreading.");

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

void Bridge::StpCallback_OnTopologyChange (STP_BRIDGE* bridge)
{
}

void Bridge::StpCallback_OnNotifiedTopologyChange (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex)
{
}

void Bridge::StpCallback_OnPortRoleChanged (STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, STP_PORT_ROLE role)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	InvalidateEvent::InvokeHandlers(b->_em, b);
}

#pragma endregion

