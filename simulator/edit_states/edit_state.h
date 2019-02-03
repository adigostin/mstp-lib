#pragma once
#include "..\simulator.h"
#include "win32\window.h"

struct edit_state_deps
{
	project_window_i* pw;
	edit_area_i*    ea;
	project_i*      project;
	selection_i*    selection;
};

class edit_state abstract
{
protected:
	project_window_i* const _pw;
	edit_area_i*    const _ea;
	project_i*      const _project;
	selection_i*    const _selection;

public:
	edit_state (const edit_state_deps& deps)
		: _pw(deps.pw), _ea(deps.ea), _selection(deps.selection), _project(deps.project)
	{ }

	virtual ~edit_state() { }
	virtual void process_mouse_button_down (edge::mouse_button button, UINT modifierKeysDown, const MouseLocation& location) { }
	virtual void process_mouse_button_up   (edge::mouse_button button, UINT modifierKeysDown, const MouseLocation& location) { }
	virtual void process_mouse_move (const MouseLocation& location) { }
	virtual std::optional<LRESULT> process_key_or_syskey_down (UINT virtualKey, UINT modifierKeys) { return std::nullopt; }
	virtual std::optional<LRESULT> process_key_or_syskey_up   (UINT virtualKey, UINT modifierKeys) { return std::nullopt; }
	virtual void render (ID2D1DeviceContext* dc) { }
	virtual bool completed() const = 0;
	virtual HCURSOR cursor() const { return LoadCursor(nullptr, IDC_ARROW); }
};

std::unique_ptr<edit_state> create_state_move_bridges (const edit_state_deps& deps);
std::unique_ptr<edit_state> create_state_move_port (const edit_state_deps& deps);
std::unique_ptr<edit_state> create_state_create_bridge (const edit_state_deps& deps);
std::unique_ptr<edit_state> create_state_create_wire (const edit_state_deps& deps);
std::unique_ptr<edit_state> CreateStateMoveWirePoint (const edit_state_deps& deps, Wire* wire, size_t pointIndex);
std::unique_ptr<edit_state> CreateStateBeginningDrag (const edit_state_deps& deps,
													 renderable_object* clickedObject,
													 edge::mouse_button button,
													 UINT modifierKeysDown,
													 const MouseLocation& location,
													 HCURSOR cursor,
													 std::unique_ptr<edit_state>&& stateMoveThreshold,
													 std::unique_ptr<edit_state>&& stateButtonUp);
