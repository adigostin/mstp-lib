
#include "pch.h"
#include "EditState.h"

using namespace std;

class BeginningDragES : public EditState
{
	using base = EditState;

	MouseLocation const _location;
	MouseButton const _button;
	HCURSOR const _cursor;
	unique_ptr<EditState> _stateMoveThreshold;
	unique_ptr<EditState> _stateButtonUp;
	bool _completed = false;

public:
	BeginningDragES (const EditStateDeps& deps,
					 const MouseLocation& location,
					 MouseButton button,
					 HCURSOR cursor,
					 unique_ptr<EditState>&& stateMoveThreshold,
					 unique_ptr<EditState>&& stateButtonUp)
		: base(deps)
		, _location(location)
		, _button(button)
		, _cursor(cursor)
		, _stateMoveThreshold(move(stateMoveThreshold))
		, _stateButtonUp(move(stateButtonUp))
	{ }

	virtual bool Completed() const override final { return _completed; }

	virtual HCURSOR GetCursor() const { return _cursor; }

	virtual void OnMouseDown (const MouseLocation& location, MouseButton button) override final
	{
		// User began dragging with a mouse button, now he pressed a second button. Nothing to do here.
		base::OnMouseDown (location, button);
	}

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
				_stateMoveThreshold->OnMouseDown (_location, _button);
				assert (!_stateMoveThreshold->Completed());
				_stateMoveThreshold->OnMouseMove (location);
				if (!_stateMoveThreshold->Completed())
					_editArea->EnterState(move(_stateMoveThreshold));
			}
		}
	}

	virtual void OnMouseUp (const MouseLocation& location, MouseButton button) override final
	{
		if (button != _button)
			return;

		_completed = true;

		if (_stateButtonUp != nullptr)
		{
			_stateButtonUp->OnMouseDown (_location, _button);
			if (!_stateButtonUp->Completed())
			{
				if ((location.pt.x != _location.pt.x) || (location.pt.y != _location.pt.y))
					_stateButtonUp->OnMouseMove (_location);

				_stateButtonUp->OnMouseUp (location, button);
				if (!_stateButtonUp->Completed())
					_editArea->EnterState(move(_stateButtonUp));
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

std::unique_ptr<EditState> CreateStateBeginningDrag (const EditStateDeps& deps,
													 const MouseLocation& location,
													 MouseButton button,
													 HCURSOR cursor,
													 std::unique_ptr<EditState>&& stateMoveThreshold,
													 std::unique_ptr<EditState>&& stateButtonUp)
{
	return unique_ptr<EditState>(new BeginningDragES(deps, location, button, cursor, move(stateMoveThreshold), move(stateButtonUp)));
}
