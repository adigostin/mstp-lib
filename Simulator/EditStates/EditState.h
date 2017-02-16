#pragma once
#include "..\SimulatorDefs.h"

class EditState abstract
{
protected:
	IEditArea* const _area;
	ISelection* const _selection;

public:
	EditState (IEditArea* area, ISelection* selection)
		: _area(area), _selection(selection)
	{ }

	virtual ~EditState() { }
	virtual void OnMouseDown (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation, MouseButton button, Object* clickedObject) { }
	virtual void OnMouseMove (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation) { }
	virtual void OnMouseUp   (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation, MouseButton button) { }
	virtual bool Completed() const = 0;
};

std::unique_ptr<EditState> CreateStateMoveBridges (IEditArea* area, ISelection* selection);
