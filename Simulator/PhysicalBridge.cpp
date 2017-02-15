#include "pch.h"
#include "PhysicalBridge.h"
#include "Win32Defs.h"

using namespace std;
using namespace D2D1;

static constexpr float PortLongSize = 30;
static constexpr float PortShortSize = 15;
static constexpr float PortSpacing = 20;
static constexpr float BridgeDefaultHeight = 100;

void PhysicalPort::Render(ID2D1DeviceContext* dc) const
{
	ComPtr<ID2D1SolidColorBrush> brush;
	dc->CreateSolidColorBrush(ColorF(ColorF::DeepPink), &brush);

	D2D1_RECT_F rect;
	switch (_side)
	{
		case Side::Bottom:
			rect.left = _bridge->GetLeft() + _offset - PortLongSize / 2;
			rect.top = _bridge->GetBottom() - PortShortSize;
			rect.right = _bridge->GetLeft() + _offset + PortLongSize / 2;
			rect.bottom = _bridge->GetBottom();
			dc->DrawRectangle(rect, brush, 2.0f);
			break;

		default:
			throw NotImplementedException();
	}
}

PhysicalBridge::PhysicalBridge (size_t portCount)
{
	float offset = 0;

	for (size_t i = 0; i < portCount; i++)
	{
		offset += (PortSpacing / 2 + PortLongSize / 2);
		auto port = make_unique<PhysicalPort>(this, i, Side::Bottom, offset);
		_ports.push_back (move(port));
		offset += (PortLongSize / 2 + PortSpacing / 2);
	}

	_x = 0;
	_y = 0;
	_width = offset;
	_height = BridgeDefaultHeight;
}

PhysicalBridge::~PhysicalBridge()
{
}

void PhysicalBridge::Render(ID2D1DeviceContext* dc) const
{
	ComPtr<ID2D1SolidColorBrush> brush;
	dc->CreateSolidColorBrush (ColorF(ColorF::Red), &brush);
	dc->DrawRectangle ({ _x, _y, _x + _width, _y + _height }, brush, 4.0f);

	for (auto& port : _ports)
		port->Render(dc);
}

void PhysicalBridge::SetLocation(float x, float y)
{
	if ((_x != x) || (_y != y))
	{
		BridgeInvalidateEvent::InvokeHandlers(_em, this);
		_x = x;
		_y = y;
		BridgeInvalidateEvent::InvokeHandlers(_em, this);
	}
}

#pragma region PhysicalBridge::IUnknown
HRESULT STDMETHODCALLTYPE PhysicalBridge::QueryInterface(REFIID riid, void** ppvObject)
{
	throw NotImplementedException();
}

ULONG STDMETHODCALLTYPE PhysicalBridge::AddRef()
{
	return InterlockedIncrement(&_refCount);
}

ULONG STDMETHODCALLTYPE PhysicalBridge::Release()
{
	ULONG newRefCount = InterlockedDecrement(&_refCount);
	if (newRefCount == 0)
		delete this;
	return newRefCount;
}
#pragma endregion
