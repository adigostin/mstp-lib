
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "edge_win32.h"
#include "simulator.h"

using edge::handled;
using edge::modifier_key;
using edge::mouse_button;

struct edit_state_deps
{
	project_window_i* pw;
	edit_window_i*    ew;
	project_i*      project;
	selection_i*    selection;
};

class edit_state abstract
{
protected:
	project_window_i* const _pw;
	edit_window_i*    const _ew;
	project_i*        const _project;
	selection_i*      const _selection;

public:
	edit_state (const edit_state_deps& deps)
		: _pw(deps.pw), _ew(deps.ew), _selection(deps.selection), _project(deps.project)
	{ }

	virtual ~edit_state() { }
	virtual handled process_mouse_button_down (mouse_button button, modifier_key mks, const mouse_location& ml) { return handled(false); }
	virtual handled process_mouse_button_up   (mouse_button button, modifier_key mks, const mouse_location& ml) { return handled(false); }
	virtual void process_mouse_move (const mouse_location& location) { }
	virtual handled process_key_or_syskey_down (uint32_t vkey, modifier_key mks) { return handled(false); }
	virtual handled process_key_or_syskey_up   (uint32_t vkey, modifier_key mks) { return handled(false); }
	virtual void render (ID2D1DeviceContext* dc) { }
	virtual bool completed() const = 0;
	virtual HCURSOR cursor() const { return LoadCursor(nullptr, IDC_ARROW); }
};

std::unique_ptr<edit_state> create_state_move_bridges (const edit_state_deps& deps);
std::unique_ptr<edit_state> create_state_move_port (const edit_state_deps& deps);
std::unique_ptr<edit_state> create_state_create_bridge (const edit_state_deps& deps);
std::unique_ptr<edit_state> create_state_create_wire (const edit_state_deps& deps);
std::unique_ptr<edit_state> CreateStateMoveWirePoint (const edit_state_deps& deps, wire* wire, size_t pointIndex);
std::unique_ptr<edit_state> CreateStateBeginningDrag (const edit_state_deps& deps,
	renderable_object* clickedObject,
	mouse_button button,
	modifier_key mks,
	const mouse_location& ml,
	HCURSOR cursor,
	std::unique_ptr<edit_state>&& stateMoveThreshold,
	std::unique_ptr<edit_state>&& stateButtonUp);
