
#include "pch.h"
#include "edit_state.h"

using namespace std;

class BeginningDragES : public edit_state
{
	using base = edit_state;

	renderable_object* const _clickedObject;
	MouseButton       const _button;
	UINT              const _modifierKeysDown;
	MouseLocation     const _location;
	HCURSOR           const _cursor;
	unique_ptr<edit_state> _stateMoveThreshold;
	unique_ptr<edit_state> _stateButtonUp;
	bool _completed = false;

public:
	BeginningDragES (const EditStateDeps& deps,
					 renderable_object* clickedObject,
					 MouseButton button,
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

	virtual bool Completed() const override final { return _completed; }

	virtual HCURSOR GetCursor() const { return _cursor; }

	// User began dragging with a mouse button, now he pressed a second button. Nothing to do here.
	//virtual void OnMouseDown (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	//{
	//	base::OnMouseDown (button, modifierKeysDown, location);
	//}

	virtual void OnMouseMove (const MouseLocation& location) override final
	{
		base::OnMouseMove(location);

		RECT rc = { _location.pt.x, _location.pt.y, _location.pt.x, _location.pt.y };
		InflateRect (&rc, GetSystemMetrics(SM_CXDRAG), GetSystemMetrics(SM_CYDRAG));
		if (!PtInRect (&rc, location.pt))
		{
			_completed = true;

			if (_stateMoveThreshold != nullptr)
			{
				_stateMoveThreshold->OnMouseDown (_button, _modifierKeysDown, _location);
				assert (!_stateMoveThreshold->Completed());
				_stateMoveThreshold->OnMouseMove (location);
				if (!_stateMoveThreshold->Completed())
					_ea->EnterState(move(_stateMoveThreshold));
			}
		}
	}

	virtual void OnMouseUp (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		if (button != _button)
			return;

		if ((button == MouseButton::Left) && ((modifierKeysDown & MK_CONTROL) == 0) && (_clickedObject != nullptr))
			_selection->select(_clickedObject);

		_completed = true;

		if (_stateButtonUp != nullptr)
		{
			_stateButtonUp->OnMouseDown (_button, _modifierKeysDown, _location);
			if (!_stateButtonUp->Completed())
			{
				if ((location.pt.x != _location.pt.x) || (location.pt.y != _location.pt.y))
					_stateButtonUp->OnMouseMove (_location);

				_stateButtonUp->OnMouseUp (button, modifierKeysDown, location);
				if (!_stateButtonUp->Completed())
					_ea->EnterState(move(_stateButtonUp));
			}
		}
	}

	virtual std::optional<LRESULT> OnKeyDown (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_completed = true;
			return 0;
		}

		return nullopt;
	}
};

std::unique_ptr<edit_state> CreateStateBeginningDrag (const EditStateDeps& deps,
													 renderable_object* clickedObject,
													 MouseButton button,
													 UINT modifierKeysDown,
													 const MouseLocation& location,
													 HCURSOR cursor,
													 std::unique_ptr<edit_state>&& stateMoveThreshold,
													 std::unique_ptr<edit_state>&& stateButtonUp)
{
	return unique_ptr<edit_state>(new BeginningDragES(deps, clickedObject, button, modifierKeysDown, location, cursor, move(stateMoveThreshold), move(stateButtonUp)));
}
