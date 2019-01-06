#pragma once
#include "..\Simulator.h"

struct EditStateDeps
{
	IProjectWindow* pw;
	IEditArea*      editArea;
	IProject*       project;
	ISelection*     selection;
};

class EditState abstract
{
protected:
	IProjectWindow* const _pw;
	IEditArea* const _editArea;
	IProject* const _project;
	ISelection* const _selection;

public:
	EditState (const EditStateDeps& deps)
		: _pw(deps.pw), _editArea(deps.editArea), _selection(deps.selection), _project(deps.project)
	{ }

	virtual ~EditState() { }
	virtual void OnMouseDown (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) { }
	virtual void OnMouseMove (const MouseLocation& location) { }
	virtual void OnMouseUp   (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) { }
	virtual std::optional<LRESULT> OnKeyDown (UINT virtualKey, UINT modifierKeys) { return std::nullopt; }
	virtual std::optional<LRESULT> OnKeyUp   (UINT virtualKey, UINT modifierKeys) { return std::nullopt; }
	virtual void Render (ID2D1RenderTarget* dc) { }
	virtual bool Completed() const = 0;
	virtual HCURSOR GetCursor() const { return LoadCursor(nullptr, IDC_ARROW); }
};

std::unique_ptr<EditState> CreateStateMoveBridges (const EditStateDeps& deps);
std::unique_ptr<EditState> CreateStateMovePort (const EditStateDeps& deps);
std::unique_ptr<EditState> CreateStateCreateBridge (const EditStateDeps& deps);
std::unique_ptr<EditState> CreateStateCreateWire (const EditStateDeps& deps);
std::unique_ptr<EditState> CreateStateMoveWirePoint (const EditStateDeps& deps, Wire* wire, size_t pointIndex);
std::unique_ptr<EditState> CreateStateBeginningDrag (const EditStateDeps& deps,
													 renderable_object* clickedObject,
													 MouseButton button,
													 UINT modifierKeysDown,
													 const MouseLocation& location,
													 HCURSOR cursor,
													 std::unique_ptr<EditState>&& stateMoveThreshold,
													 std::unique_ptr<EditState>&& stateButtonUp);
