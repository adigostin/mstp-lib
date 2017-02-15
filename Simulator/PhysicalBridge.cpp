#include "pch.h"
#include "PhysicalBridge.h"

using namespace D2D1;

PhysicalBridge::PhysicalBridge()
{
}

PhysicalBridge::~PhysicalBridge()
{
}

void PhysicalBridge::Render(ID2D1DeviceContext* dc) const
{
	ComPtr<ID2D1SolidColorBrush> brush;
	dc->CreateSolidColorBrush (ColorF(ColorF::Red), &brush);
	dc->DrawRectangle (_bounds, brush, 4.0f);
}
