
#pragma once

class Object
{
protected:
	virtual ~Object() { }
};

class PhysicalPort : public Object
{
};

class PhysicalBridge : public Object
{
	D2D1_RECT_F _bounds = { 0, 0, 100, 100 };

public:
	PhysicalBridge();
	~PhysicalBridge();

	void Render (ID2D1DeviceContext* dc) const;
};

