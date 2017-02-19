#include "pch.h"
#include "Bridge.h"
#include "Win32Defs.h"
#include "Port.h"

using namespace std;
using namespace D2D1;

Bridge::Bridge (unsigned int portCount, const std::array<uint8_t, 6>& macAddress)
	: _macAddress(macAddress), _guiThreadId(this_thread::get_id())
{
	float offset = 0;

	for (size_t i = 0; i < portCount; i++)
	{
		offset += (Port::PortToPortSpacing / 2 + Port::InteriorLongSize / 2);
		auto port = ComPtr<Port>(new Port(this, i, Side::Bottom, offset), false);
		_ports.push_back (move(port));
		offset += (Port::InteriorLongSize / 2 + Port::PortToPortSpacing / 2);
	}

	_x = 0;
	_y = 0;
	_width = max (offset, MinWidth);
	_height = DefaultHeight;
}

Bridge::~Bridge()
{
	assert (this_thread::get_id() == _guiThreadId);
	if (_stpBridge != nullptr)
		STP_DestroyBridge (_stpBridge);
}

void Bridge::EnableStp (STP_VERSION stpVersion, uint16_t treeCount, uint32_t timestamp)
{
	if (this_thread::get_id() != _guiThreadId)
		throw runtime_error ("This function may be called only on the main thread.");

	if (_stpBridge != nullptr)
		throw runtime_error ("STP is already enabled on this bridge.");

	_stpBridge = STP_CreateBridge ((unsigned int) _ports.size(), treeCount, &StpCallbacks, stpVersion, &_macAddress[0], 256);
	STP_SetApplicationContext (_stpBridge, this);
	STP_EnableLogging (_stpBridge, true);
	STP_StartBridge (_stpBridge, timestamp);
	BridgeStartedEvent::InvokeHandlers (_em, this);

	InvalidateEvent::InvokeHandlers(_em, this);
}

void Bridge::DisableStp (uint32_t timestamp)
{
	if (this_thread::get_id() != _guiThreadId)
		throw runtime_error ("This function may be called only on the main thread.");

	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	BridgeStoppingEvent::InvokeHandlers(_em, this);
	STP_StopBridge(_stpBridge, timestamp);
	STP_DestroyBridge (_stpBridge);
	_stpBridge = nullptr;

	InvalidateEvent::InvokeHandlers(_em, this);
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

uint16_t Bridge::GetTreeCount() const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetTreeCount(_stpBridge);
}

STP_PORT_ROLE Bridge::GetStpPortRole (uint16_t portIndex, uint16_t treeIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetPortRole (_stpBridge, portIndex, treeIndex);
}

bool Bridge::GetStpPortLearning (uint16_t portIndex, uint16_t treeIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetPortLearning (_stpBridge, portIndex, treeIndex);
}

bool Bridge::GetStpPortForwarding (uint16_t portIndex, uint16_t treeIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetPortForwarding (_stpBridge, portIndex, treeIndex);
}

bool Bridge::GetStpPortOperEdge (uint16_t portIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetPortOperEdge (_stpBridge, portIndex);
}

unsigned short Bridge::GetStpBridgePriority (uint16_t treeIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetBridgePriority(_stpBridge, treeIndex);
}

uint16_t Bridge::GetStpTreeIndexFromVlanNumber (uint16_t vlanNumber) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	if ((vlanNumber == 0) || (vlanNumber > 4094))
		throw invalid_argument ("The VLAN number must be >=1 and <=4094.");

	return STP_GetTreeIndexFromVlanNumber(_stpBridge, vlanNumber);
}

void Bridge::Render (ID2D1RenderTarget* dc, const DrawingObjects& dos, IDWriteFactory* dWriteFactory, uint16_t vlanNumber) const
{
	optional<unsigned int> treeIndex;
	if (IsStpEnabled())
		treeIndex = GetStpTreeIndexFromVlanNumber(vlanNumber);

	// Draw bridge outline.
	D2D1_ROUNDED_RECT rr = RoundedRect (GetBounds(), RoundRadius, RoundRadius);
	dc->FillRoundedRectangle (&rr, _powered ? dos._poweredFillBrush : dos._unpoweredBrush);
	dc->DrawRoundedRectangle (&rr, _powered ? dos._poweredOutlineBrush : dos._unpoweredBrush, 2.0f);

	// Draw bridge name.
	wchar_t str[128];
	int strlen;
	if (IsStpEnabled())
	{
		unsigned short prio = GetStpBridgePriority(treeIndex.value());
		strlen = swprintf_s (str, L"%04x.%02x%02x%02x%02x%02x%02x\r\nSTP enabled", prio,
			_macAddress[0], _macAddress[1], _macAddress[2], _macAddress[3], _macAddress[4], _macAddress[5]);
	}
	else
	{
		strlen = swprintf_s (str, L"%02x%02x%02x%02x%02x%02x\r\nSTP disabled (right-click to enable)",
			_macAddress[0], _macAddress[1], _macAddress[2], _macAddress[3], _macAddress[4], _macAddress[5]);
	}
	ComPtr<IDWriteTextLayout> tl;
	HRESULT hr = dWriteFactory->CreateTextLayout (str, strlen, dos._regularTextFormat, 10000, 10000, &tl); ThrowIfFailed(hr);
	dc->DrawTextLayout ({ _x + OutlineWidth / 2 + 3, _y + OutlineWidth / 2 + 3}, tl, dos._brushWindowText);

	Matrix3x2F oldTransform;
	dc->GetTransform (&oldTransform);

	for (auto& port : _ports)
		port->Render (dc, dos, dWriteFactory, vlanNumber);
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

#pragma region STP Callbacks
const STP_CALLBACKS Bridge::StpCallbacks =
{
	StpCallback_EnableLearning,
	StpCallback_EnableForwarding,
	nullptr, // transmitGetBuffer;
	nullptr, // transmitReleaseBuffer;
	StpCallback_FlushFdb,
	StpCallback_DebugStrOut,
	nullptr, // onTopologyChange;
	nullptr, // onNotifiedTopologyChange;
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

#pragma endregion
