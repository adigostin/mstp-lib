#pragma once
#include "..\SimulatorDefs.h"

struct EditStateDeps
{
	IProject* project;
	IEditArea* area;
	ISelection* selection;
};

class EditState abstract
{
protected:
	IProject* const _project;
	IEditArea* const _area;
	ISelection* const _selection;

public:
	EditState (const EditStateDeps& deps)
		: _project(deps.project), _area(deps.area), _selection(deps.selection)
	{ }

	virtual ~EditState() { }
	virtual void OnMouseDown (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation, MouseButton button, Object* clickedObject) { }
	virtual void OnMouseMove (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation) { }
	virtual void OnMouseUp   (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation, MouseButton button) { }
	virtual void Render (ID2D1DeviceContext* dc) { }
	virtual bool Completed() const = 0;
	virtual HCURSOR GetCursor() const { return LoadCursor(nullptr, IDC_ARROW); }
};

std::unique_ptr<EditState> CreateStateMoveBridges (const EditStateDeps& deps);
std::unique_ptr<EditState> CreateStateCreateBridge (const EditStateDeps& deps);
