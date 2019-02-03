
#include "pch.h"
#include "edit_state.h"

using namespace std;

class BeginningDragES : public edit_state
{
	using base = edit_state;

	renderable_object* const _clickedObject;
	edge::mouse_button const _button;
	UINT               const _modifierKeysDown;
	MouseLocation      const _location;
	HCURSOR            const _cursor;
	unique_ptr<edit_state> _stateMoveThreshold;
	unique_ptr<edit_state> _stateButtonUp;
	bool _completed = false;

public:
	BeginningDragES (const edit_state_deps& deps,
					 renderable_object* clickedObject,
					 edge::mouse_button button,
					 UINT modifierKeysDown,
					 const MouseLocation& location,
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

	virtual void process_mouse_button_down (edge::mouse_button button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		// User began dragging with a mouse button, now he pressed a second button. Nothing to do here.
		base::process_mouse_button_down (button, modifierKeysDown, location);
	}

	virtual void process_mouse_move (const MouseLocation& location) override final
	{
		base::process_mouse_move(location);

		RECT rc = { _location.pt.x, _location.pt.y, _location.pt.x, _location.pt.y };
		InflateRect (&rc, GetSystemMetrics(SM_CXDRAG), GetSystemMetrics(SM_CYDRAG));
		if (!PtInRect (&rc, location.pt))
		{
			_completed = true;

			if (_stateMoveThreshold != nullptr)
			{
				_stateMoveThreshold->process_mouse_button_down (_button, _modifierKeysDown, _location);
				assert (!_stateMoveThreshold->completed());
				_stateMoveThreshold->process_mouse_move (location);
				if (!_stateMoveThreshold->completed())
					_ea->EnterState(move(_stateMoveThreshold));
			}
		}
	}

	virtual void process_mouse_button_up (edge::mouse_button button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		if (button != _button)
			return;

		if ((button == edge::mouse_button::left) && ((modifierKeysDown & MK_CONTROL) == 0) && (_clickedObject != nullptr))
			_selection->select(_clickedObject);

		_completed = true;

		if (_stateButtonUp != nullptr)
		{
			_stateButtonUp->process_mouse_button_down (_button, _modifierKeysDown, _location);
			if (!_stateButtonUp->completed())
			{
				if ((location.pt.x != _location.pt.x) || (location.pt.y != _location.pt.y))
					_stateButtonUp->process_mouse_move (_location);

				_stateButtonUp->process_mouse_button_up (button, modifierKeysDown, location);
				if (!_stateButtonUp->completed())
					_ea->EnterState(move(_stateButtonUp));
			}
		}
	}

	virtual std::optional<LRESULT> process_key_or_syskey_down (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_completed = true;
			return 0;
		}

		return nullopt;
	}
};

std::unique_ptr<edit_state> CreateStateBeginningDrag (const edit_state_deps& deps,
													 renderable_object* clickedObject,
													 edge::mouse_button button,
													 UINT modifierKeysDown,
													 const MouseLocation& location,
													 HCURSOR cursor,
													 std::unique_ptr<edit_state>&& stateMoveThreshold,
													 std::unique_ptr<edit_state>&& stateButtonUp)
{
	return unique_ptr<edit_state>(new BeginningDragES(deps, clickedObject, button, modifierKeysDown, location, cursor, move(stateMoveThreshold), move(stateButtonUp)));
}
