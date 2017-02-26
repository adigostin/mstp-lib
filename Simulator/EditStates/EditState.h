#pragma once
#include "..\Simulator.h"

struct EditStateDeps
{
	IProject* project;
	IProjectWindow* pw;
	IEditArea* area;
	ISelection* selection;
};

class EditState abstract
{
protected:
	IProject* const _project;
	IProjectWindow* const _pw;
	IEditArea* const _area;
	ISelection* const _selection;

public:
	EditState (const EditStateDeps& deps)
		: _project(deps.project), _pw(deps.pw), _area(deps.area), _selection(deps.selection)
	{ }

	virtual ~EditState() { }
	virtual void OnMouseDown (const MouseLocation& location, MouseButton button) { }
	virtual void OnMouseMove (const MouseLocation& location) { }
	virtual void OnMouseUp   (const MouseLocation& location, MouseButton button) { }
	virtual std::optional<LRESULT> OnKeyDown (UINT virtualKey, UINT modifierKeys) { return std::nullopt; }
	virtual std::optional<LRESULT> OnKeyUp   (UINT virtualKey, UINT modifierKeys) { return std::nullopt; }
	virtual void Render (ID2D1RenderTarget* dc) { }
	virtual bool Completed() const = 0;
	virtual HCURSOR GetCursor() const { return LoadCursor(nullptr, IDC_ARROW); }
};

std::unique_ptr<EditState> CreateStateMoveBridges (const EditStateDeps& deps);
std::unique_ptr<EditState> CreateStateCreateBridge (const EditStateDeps& deps);
std::unique_ptr<EditState> CreateStateCreateWire (const EditStateDeps& deps, Port* fromPort);
std::unique_ptr<EditState> CreateStateMoveWirePoint (const EditStateDeps& deps, Wire* wire, size_t pointIndex);
