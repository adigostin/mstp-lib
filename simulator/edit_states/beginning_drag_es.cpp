
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "edit_state.h"

using namespace std;

class BeginningDragES : public edit_state
{
	using base = edit_state;

	renderable_object* const _clickedObject;
	mouse_button       const _button;
	modifier_key       const _modifierKeysDown;
	mouse_location     const _location;
	HCURSOR            const _cursor;
	unique_ptr<edit_state> _stateMoveThreshold;
	unique_ptr<edit_state> _stateButtonUp;
	bool _completed = false;

public:
	BeginningDragES (const edit_state_deps& deps,
					 renderable_object* clickedObject,
					 mouse_button button,
					 modifier_key modifierKeysDown,
					 const mouse_location& location,
					 HCURSOR cursor,
					 unique_ptr<edit_state>&& stateMoveThreshold,
					 unique_ptr<edit_state>&& stateButtonUp)
		: base(deps)
		, _clickedObject(clickedObject)
		, _button(button)
		, _modifierKeysDown(modifierKeysDown)
		, _location(location)
		, _cursor(cursor)
		, _stateMoveThreshold(move(stateMoveThreshold))
		, _stateButtonUp(move(stateButtonUp))
	{ }

	virtual bool completed() const override final { return _completed; }

	virtual HCURSOR cursor() const { return _cursor; }

	virtual handled process_mouse_button_down (mouse_button button, modifier_key mks, const mouse_location& ml) override final
	{
		return handled(true); // discard it
	}

	virtual void process_mouse_move (const mouse_location& ml) override final
	{
		base::process_mouse_move(ml);

		RECT rc = { _location.pt.x, _location.pt.y, _location.pt.x, _location.pt.y };
		InflateRect (&rc, GetSystemMetrics(SM_CXDRAG), GetSystemMetrics(SM_CYDRAG));
		if (!PtInRect (&rc, ml.pt))
		{
			_completed = true;

			if (_stateMoveThreshold != nullptr)
			{
				_stateMoveThreshold->process_mouse_button_down (_button, _modifierKeysDown, _location);
				assert (!_stateMoveThreshold->completed());
				_stateMoveThreshold->process_mouse_move (ml);
				if (!_stateMoveThreshold->completed())
					_ew->EnterState(move(_stateMoveThreshold));
			}
		}
	}

	virtual handled process_mouse_button_up (mouse_button button, modifier_key mks, const mouse_location& ml) override final
	{
		if (button != _button)
			return handled(true); // discard it

		if ((button == edge::mouse_button::left) && ((mks & modifier_key::control) == 0) && (_clickedObject != nullptr))
			_selection->select(_clickedObject);

		_completed = true;

		if (_stateButtonUp != nullptr)
		{
			_stateButtonUp->process_mouse_button_down (_button, _modifierKeysDown, _location);
			if (!_stateButtonUp->completed())
			{
				if ((ml.pt.x != _location.pt.x) || (ml.pt.y != _location.pt.y))
					_stateButtonUp->process_mouse_move (_location);

				_stateButtonUp->process_mouse_button_up (button, mks, ml);
				if (!_stateButtonUp->completed())
					_ew->EnterState(move(_stateButtonUp));
			}
		}

		return handled(true);
	}

	virtual handled process_key_or_syskey_down (uint32_t virtualKey, modifier_key modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_completed = true;
			return handled(true);
		}

		return handled(false);
	}
};

std::unique_ptr<edit_state> CreateStateBeginningDrag (const edit_state_deps& deps,
													 renderable_object* clickedObject,
													 mouse_button button,
													 modifier_key mks,
													 const mouse_location& ml,
													 HCURSOR cursor,
													 std::unique_ptr<edit_state>&& stateMoveThreshold,
													 std::unique_ptr<edit_state>&& stateButtonUp)
{
	return unique_ptr<edit_state>(new BeginningDragES(deps, clickedObject, button, mks, ml, cursor, move(stateMoveThreshold), move(stateButtonUp)));
}
