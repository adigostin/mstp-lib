#pragma once
#include "..\simulator.h"

struct EditStateDeps
{
	IProjectWindow* pw;
	edit_area_i*    ea;
	project_i*      project;
	selection_i*    selection;
};

class edit_state abstract
{
protected:
	IProjectWindow* const _pw;
	edit_area_i*    const _ea;
	project_i*      const _project;
	selection_i*    const _selection;

public:
	edit_state (const EditStateDeps& deps)
		: _pw(deps.pw), _ea(deps.ea), _selection(deps.selection), _project(deps.project)
	{ }

	virtual ~edit_state() { }
	virtual void OnMouseDown (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) { }
	virtual void OnMouseMove (const MouseLocation& location) { }
	virtual void OnMouseUp   (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) { }
	virtual std::optional<LRESULT> OnKeyDown (UINT virtualKey, UINT modifierKeys) { return std::nullopt; }
	virtual std::optional<LRESULT> OnKeyUp   (UINT virtualKey, UINT modifierKeys) { return std::nullopt; }
	virtual void render (ID2D1DeviceContext* dc) { }
	virtual bool Completed() const = 0;
	virtual HCURSOR GetCursor() const { return LoadCursor(nullptr, IDC_ARROW); }
};

std::unique_ptr<edit_state> CreateStateMoveBridges (const EditStateDeps& deps);
std::unique_ptr<edit_state> CreateStateMovePort (const EditStateDeps& deps);
std::unique_ptr<edit_state> CreateStateCreateBridge (const EditStateDeps& deps);
std::unique_ptr<edit_state> create_state_create_wire (const EditStateDeps& deps);
std::unique_ptr<edit_state> CreateStateMoveWirePoint (const EditStateDeps& deps, Wire* wire, size_t pointIndex);
std::unique_ptr<edit_state> CreateStateBeginningDrag (const EditStateDeps& deps,
													 renderable_object* clickedObject,
													 MouseButton button,
													 UINT modifierKeysDown,
													 const MouseLocation& location,
													 HCURSOR cursor,
													 std::unique_ptr<edit_state>&& stateMoveThreshold,
													 std::unique_ptr<edit_state>&& stateButtonUp);
