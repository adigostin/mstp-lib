
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#pragma once
#include "..\Simulator_.h"

using edge::handled;
using edge::mouse_button;

struct edit_state_deps
{
	ISimulatorApp* app;
	IProjectWindow* pw;
	IEditWindow*    ew;
	IStpProject*    project;
	IVlanSelection* vlanSelection;
	ISelection*     selection;
};

class edit_state abstract
{
protected:
	ISimulatorApp* const _app;
	IProjectWindow* const _pw;
	IEditWindow*    const _ew;
	IStpProject*    const _project;
	com_ptr<IVlanSelection> const _vlanSelection;
	com_ptr<ISelection> const _selection;

public:
	edit_state (const edit_state_deps& deps)
		: _app(deps.app), _pw(deps.pw), _ew(deps.ew), _vlanSelection(deps.vlanSelection), _selection(deps.selection), _project(deps.project)
	{ }

	virtual ~edit_state() { }
	virtual handled OnMouseButtonDown (mouse_button button, UINT mks, const mouse_location& ml) { return handled(false); }
	virtual handled OnMouseButtonUp   (mouse_button button, UINT mks, const mouse_location& ml) { return handled(false); }
	virtual void OnMouseMove (const mouse_location& location) { }
	virtual handled process_key_or_syskey_down (uint32_t vkey, UINT mks) { return handled(false); }
	virtual handled process_key_or_syskey_up   (uint32_t vkey, UINT mks) { return handled(false); }
	virtual void render (ID2D1DeviceContext* dc) { }
	virtual bool completed() const = 0;
	virtual HCURSOR cursor() const { return LoadCursor(nullptr, IDC_ARROW); }
};

std::unique_ptr<edit_state> create_state_move_bridges (const edit_state_deps& deps);
std::unique_ptr<edit_state> create_state_move_port (const edit_state_deps& deps);
std::unique_ptr<edit_state> create_state_create_bridge (const edit_state_deps& deps);
std::unique_ptr<edit_state> create_state_create_wire (const edit_state_deps& deps);
std::unique_ptr<edit_state> CreateStateMoveWirePoint (const edit_state_deps& deps, IWire* wire, size_t pointIndex);
std::unique_ptr<edit_state> CreateStateBeginningDrag (const edit_state_deps& deps,
	ISelectableObject* clickedObject,
	mouse_button button,
	UINT mks,
	const mouse_location& ml,
	HCURSOR cursor,
	std::unique_ptr<edit_state> stateMoveThreshold,
	std::unique_ptr<edit_state> stateButtonUp);
