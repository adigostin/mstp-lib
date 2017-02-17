#include "pch.h"
#include "Bridge.h"
#include "Win32Defs.h"

using namespace std;
using namespace D2D1;

Bridge::Bridge (unsigned int portCount, const std::array<uint8_t, 6>& macAddress)
	: _macAddress(macAddress), _guiThreadId(this_thread::get_id())
{
	float offset = 0;

	for (size_t i = 0; i < portCount; i++)
	{
		offset += (PortSpacing / 2 + PortLongSize / 2);
		auto port = make_unique<Port>(this, i, Side::Bottom, offset);
		_ports.push_back (move(port));
		offset += (PortLongSize / 2 + PortSpacing / 2);
	}

	_x = 0;
	_y = 0;
	_width = max (offset, MinBridgeWidth);
	_height = BridgeDefaultHeight;
}

Bridge::~Bridge()
{
	assert (this_thread::get_id() == _guiThreadId);
	if (_stpBridge != nullptr)
		STP_DestroyBridge (_stpBridge);
}

void Bridge::EnableStp (STP_VERSION stpVersion, unsigned int treeCount, uint32_t timestamp)
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

	BridgeInvalidateEvent::InvokeHandlers(_em, this);
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

	BridgeInvalidateEvent::InvokeHandlers(_em, this);
}

void Bridge::SetLocation(float x, float y)
{
	if ((_x != x) || (_y != y))
	{
		BridgeInvalidateEvent::InvokeHandlers(_em, this);
		_x = x;
		_y = y;
		BridgeInvalidateEvent::InvokeHandlers(_em, this);
	}
}

unsigned int Bridge::GetTreeCount() const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetTreeCount(_stpBridge);
}

STP_PORT_ROLE Bridge::GetStpPortRole (unsigned int portIndex, unsigned int treeIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetPortRole (_stpBridge, portIndex, treeIndex);
}

bool Bridge::GetStpPortLearning (unsigned int portIndex, unsigned int treeIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetPortLearning (_stpBridge, portIndex, treeIndex);
}

bool Bridge::GetStpPortForwarding (unsigned int portIndex, unsigned int treeIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetPortForwarding (_stpBridge, portIndex, treeIndex);
}

bool Bridge::GetStpPortOperEdge (unsigned int portIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetPortOperEdge (_stpBridge, portIndex);
}

unsigned short Bridge::GetStpBridgePriority (unsigned int treeIndex) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	return STP_GetBridgePriority(_stpBridge, treeIndex);
}

unsigned int Bridge::GetStpTreeIndexFromVlanNumber (unsigned short vlanNumber) const
{
	if (_stpBridge == nullptr)
		throw runtime_error ("STP was not enabled on this bridge.");

	if ((vlanNumber == 0) || (vlanNumber > 4094))
		throw invalid_argument ("The VLAN number must be >=1 and <=4094.");

	return STP_GetTreeIndexFromVlanNumber(_stpBridge, vlanNumber);
}

// static
void Bridge::RenderExteriorNonStpPort (ID2D1DeviceContext* dc, const DrawingObjects& dos, bool macOperational)
{
	auto brush = macOperational ? dos._brushForwarding : dos._brushDiscardingPort;
	dc->DrawLine (Point2F (0, 0), Point2F (0, PortExteriorHeight), brush);
}

// static
void Bridge::RenderExteriorStpPort (ID2D1DeviceContext* dc, const DrawingObjects& dos, STP_PORT_ROLE role, bool learning, bool forwarding, bool operEdge)
{
	static constexpr float circleDiameter = min (PortExteriorHeight / 2, PortExteriorWidth);

	static constexpr float edw = PortExteriorWidth;
	static constexpr float edh = PortExteriorHeight;

	static constexpr float discardingFirstHorizontalLineY = circleDiameter + (edh - circleDiameter) / 3;
	static constexpr float discardingSecondHorizontalLineY = circleDiameter + (edh - circleDiameter) * 2 / 3;
	static constexpr float learningHorizontalLineY = circleDiameter + (edh - circleDiameter) / 2;

	static constexpr float dfhly = discardingFirstHorizontalLineY;
	static constexpr float dshly = discardingSecondHorizontalLineY;

	static const D2D1_ELLIPSE ellipse = { Point2F (0, circleDiameter / 2), circleDiameter / 2, circleDiameter / 2};

	if (role == STP_PORT_ROLE_DISABLED)
	{
		// disabled
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, edh / 3), Point2F (edw / 2, edh * 2 / 3), dos._brushDiscardingPort);
	}
	else if ((role == STP_PORT_ROLE_DESIGNATED) && !learning && !forwarding)
	{
		// designated discarding
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushDiscardingPort);
		dc->FillEllipse (&ellipse, dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), dos._brushDiscardingPort);
	}
	else if ((role == STP_PORT_ROLE_DESIGNATED) && learning && !forwarding)
	{
		// designated learning
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushLearningPort);
		dc->FillEllipse (&ellipse, dos._brushLearningPort);
		dc->DrawLine (Point2F (-edw / 2, learningHorizontalLineY), Point2F (edw / 2, learningHorizontalLineY), dos._brushLearningPort);
	}
	else if ((role == STP_PORT_ROLE_DESIGNATED) && learning && forwarding && !operEdge)
	{
		// designated forwarding
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushForwarding);
		dc->FillEllipse (&ellipse, dos._brushForwarding);
	}
	else if ((role == STP_PORT_ROLE_DESIGNATED) && learning && forwarding && operEdge)
	{
		// designated forwarding operEdge
		dc->FillEllipse (&ellipse, dos._brushForwarding);
		static constexpr D2D1_POINT_2F points[] = 
		{
			{ 0, circleDiameter },
			{ -edw / 2 + 1, circleDiameter + (edh - circleDiameter) / 2 },
			{ 0, edh },
			{ edw / 2 - 1, circleDiameter + (edh - circleDiameter) / 2 },
		};

		dc->DrawLine (points[0], points[1], dos._brushForwarding);
		dc->DrawLine (points[1], points[2], dos._brushForwarding);
		dc->DrawLine (points[2], points[3], dos._brushForwarding);
		dc->DrawLine (points[3], points[0], dos._brushForwarding);
	}
	else if (((role == STP_PORT_ROLE_ROOT) || (role == STP_PORT_ROLE_MASTER)) && !learning && !forwarding)
	{
		// root or master discarding
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushDiscardingPort);
		dc->DrawEllipse (&ellipse, dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), dos._brushDiscardingPort);
	}
	else if (((role == STP_PORT_ROLE_ROOT) || (role == STP_PORT_ROLE_MASTER)) && learning && !forwarding)
	{
		// root or master learning
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushLearningPort);
		dc->DrawEllipse (&ellipse, dos._brushLearningPort);
		dc->DrawLine (Point2F (-edw / 2, learningHorizontalLineY), Point2F (edw / 2, learningHorizontalLineY), dos._brushLearningPort);
	}
	else if (((role == STP_PORT_ROLE_ROOT) || (role == STP_PORT_ROLE_MASTER)) && learning && forwarding)
	{
		// root or master forwarding
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushForwarding);
		dc->DrawEllipse (&ellipse, dos._brushForwarding);
	}
	else if ((role == STP_PORT_ROLE_ALTERNATE) && !learning && !forwarding)
	{
		// Alternate discarding
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), dos._brushDiscardingPort);
	}
	else if ((role == STP_PORT_ROLE_ALTERNATE) && learning && !forwarding)
	{
		// Alternate learning
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushLearningPort);
		dc->DrawLine (Point2F (-edw / 2, learningHorizontalLineY), Point2F (edw / 2, learningHorizontalLineY), dos._brushLearningPort);
	}
	else if ((role == STP_PORT_ROLE_BACKUP) && !learning && !forwarding)
	{
		// Backup discarding
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dfhly / 2), Point2F (edw / 2, dfhly / 2), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), dos._brushDiscardingPort);
	}
	else if (role == STP_PORT_ROLE_UNKNOWN)
	{
		// Undefined
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushDiscardingPort);

		D2D1_RECT_F rect = { 2, 0, 20, 20 };
		dc->DrawText (L"?", 1, dos._regularTextFormat, &rect, dos._brushDiscardingPort, D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
	}
	else
		throw exception("Not implemented.");
}

void Bridge::Render (ID2D1DeviceContext* dc, const DrawingObjects& dos, IDWriteFactory* dWriteFactory, uint16_t vlanNumber) const
{
	optional<unsigned int> treeIndex;
	if (IsStpEnabled())
		treeIndex = GetStpTreeIndexFromVlanNumber(vlanNumber);

	// Draw bridge outline.
	D2D1_ROUNDED_RECT rr = RoundedRect (GetBounds(), BridgeRoundRadius, BridgeRoundRadius);
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
	dc->DrawTextLayout ({ _x + BridgeOutlineWidth / 2 + 3, _y + BridgeOutlineWidth / 2 + 3}, tl, dos._brushWindowText);

	Matrix3x2F oldTransform;
	dc->GetTransform (&oldTransform);

	for (size_t portIndex = 0; portIndex < _ports.size(); portIndex++)
	{
		Port* port = _ports[portIndex].get();

		Matrix3x2F portTransform;
		if (port->GetSide() == Side::Left)
		{
			//portTransform = Matrix3x2F::Rotation (90, Point2F (0, 0)) * Matrix3x2F::Translation (bridgeRect.left, bridgeRect.top + port->GetOffset ());
			// The above calculation is correct but slow. Let's assign the matrix members directly.
			portTransform._11 = 0;
			portTransform._12 = 1;
			portTransform._21 = -1;
			portTransform._22 = 0;
			portTransform._31 = _x;
			portTransform._32 = _y + port->GetOffset();
		}
		else if (port->GetSide() == Side::Right)
		{
			//portTransform = Matrix3x2F::Rotation (270, Point2F (0, 0)) * Matrix3x2F::Translation (bridgeRect.right, bridgeRect.top + port->GetOffset ());
			portTransform._11 = 0;
			portTransform._12 = -1;
			portTransform._21 = 1;
			portTransform._22 = 0;
			portTransform._31 = _x + _width;
			portTransform._32 = _y + port->GetOffset();
		}
		else if (port->GetSide() == Side::Top)
		{
			//portTransform = Matrix3x2F::Rotation (180, Point2F (0, 0)) * Matrix3x2F::Translation (bridgeRect.left + port->GetOffset (), bridgeRect.top);
			portTransform._11 = -1;
			portTransform._12 = 0;
			portTransform._21 = 0;
			portTransform._22 = -1;
			portTransform._31 = _x + port->GetOffset();
			portTransform._32 = _y;
		}
		else if (port->GetSide() == Side::Bottom)
		{
			//portTransform = Matrix3x2F::Translation (bridgeRect.left + port->GetOffset (), bridgeRect.bottom);
			portTransform._11 = portTransform._22 = 1;
			portTransform._12 = portTransform._21 = 0;
			portTransform._31 = _x + port->GetOffset();
			portTransform._32 = _y + _height;
		}
		else
			throw exception("Not implemented.");

		portTransform.SetProduct (portTransform, oldTransform);
		dc->SetTransform (&portTransform);

		// Draw the interior of the port.
		D2D1_RECT_F portRect = RectF (
			-PortInteriorLongSize / 2,
			-PortInteriorShortSize,
			-PortInteriorLongSize / 2 + PortInteriorLongSize,
			-PortInteriorShortSize + PortInteriorShortSize);
		dc->FillRectangle (&portRect, port->GetMacOperational() ? dos._poweredFillBrush : dos._unpoweredBrush);
		dc->DrawRectangle (&portRect, port->GetMacOperational() ? dos._poweredOutlineBrush : dos._unpoweredBrush);

		// Draw the exterior of the port.
		if (IsStpEnabled())
		{
			STP_PORT_ROLE role = STP_GetPortRole (_stpBridge, (unsigned int) portIndex, treeIndex.value());
			bool learning      = STP_GetPortLearning (_stpBridge, (unsigned int) portIndex, treeIndex.value());
			bool forwarding    = STP_GetPortForwarding (_stpBridge, (unsigned int) portIndex, treeIndex.value());
			bool operEdge      = STP_GetPortOperEdge (_stpBridge, (unsigned int) portIndex);
			RenderExteriorStpPort (dc, dos, role, learning, forwarding, operEdge);
		}
		else
			RenderExteriorNonStpPort(dc, dos, port->GetMacOperational());

		// fill the gray/green circle representing the operational state of the port.
		float radius = 4;
		D2D1_POINT_2F circleCenter = Point2F (-PortInteriorLongSize / 2 + 2 + radius, -PortInteriorShortSize + 2 + radius);
		D2D1_ELLIPSE circle = Ellipse (circleCenter, radius, radius);
		dc->FillEllipse (&circle, port->GetMacOperational() ? dos._poweredFillBrush : dos._unpoweredBrush);

		dc->SetTransform (&oldTransform);
	}
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
	BridgeInvalidateEvent::InvokeHandlers (b->_em, b);
}

void Bridge::StpCallback_EnableForwarding(STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable)
{
	auto b = static_cast<Bridge*>(STP_GetApplicationContext(bridge));
	BridgeInvalidateEvent::InvokeHandlers(b->_em, b);
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

#pragma region Bridge::IUnknown
HRESULT STDMETHODCALLTYPE Bridge::QueryInterface(REFIID riid, void** ppvObject)
{
	throw exception ("Not implemented.");
}

ULONG STDMETHODCALLTYPE Bridge::AddRef()
{
	return InterlockedIncrement(&_refCount);
}

ULONG STDMETHODCALLTYPE Bridge::Release()
{
	ULONG newRefCount = InterlockedDecrement(&_refCount);
	if (newRefCount == 0)
		delete this;
	return newRefCount;
}
#pragma endregion
