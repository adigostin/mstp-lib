
#include "pch.h"
#include "EditState.h"
#include "Wire.h"
#include "Port.h"

using namespace std;

class MoveWirePointES : public EditState
{
	typedef EditState base;

	Wire* const _wire;
	size_t const _pointIndex;
	WireEnd const _initialPoint;
	POINT _firstDownLocation;

	enum SubState
	{
		WaitingFirstDown,
		WaitingFirstUp,
		WaitingSecondDown,
		WaitingSecondUp,
		Done,
	};

	SubState _subState = WaitingFirstDown;

public:
	MoveWirePointES (const EditStateDeps& deps, Wire* wire, size_t pointIndex)
		: base(deps), _wire(wire), _pointIndex(pointIndex), _initialPoint(wire->GetPoints()[pointIndex])
	{ }

	virtual bool Completed() const override final { return _subState == Done; }

	virtual void OnMouseDown (const MouseLocation& location, MouseButton button) override final
	{
		base::OnMouseDown(location, button);

		if (button != MouseButton::Left)
			return;

		if (_subState == WaitingFirstDown)
		{
			_firstDownLocation = location.pt;
			_subState  = WaitingFirstUp;
		}
	}

	virtual void OnMouseMove (const MouseLocation& location) override
	{
		base::OnMouseMove(location);

		auto port = _pw->GetEditArea()->GetCPAt (location.d, SnapDistance);
		if (port != nullptr)
			_wire->SetPoint(_pointIndex, port);
		else
			_wire->SetPoint(_pointIndex, location.w);

		if (_subState == WaitingFirstUp)
		{
			RECT rc = { _firstDownLocation.x, _firstDownLocation.y, _firstDownLocation.x, _firstDownLocation.y };
			InflateRect (&rc, GetSystemMetrics(SM_CXDRAG), GetSystemMetrics(SM_CYDRAG));
			if (!PtInRect(&rc, location.pt))
				_subState = WaitingSecondUp;
		}
	}

	virtual void OnMouseUp (const MouseLocation& location, MouseButton button) override final
	{
		if (_subState == WaitingSecondUp)
		{
			//if (holds_alternative<ConnectedWireEnd>(_wire->GetPoints()[_point Index]))
			{
				_subState = Done;
			}
		}
	}

	virtual std::optional<LRESULT> OnKeyDown (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_wire->SetPoint(_pointIndex, _initialPoint);
			_subState = Done;
			return 0;
		}

		return nullopt;
	}

	virtual void Render (ID2D1RenderTarget* rt) override final
	{
		auto& point = _wire->GetPoints()[_pointIndex];
		if (holds_alternative<ConnectedWireEnd>(point))
			_pw->GetEditArea()->RenderSnapRect (rt, get<ConnectedWireEnd>(point)->GetCPLocation());
	}

	virtual HCURSOR GetCursor() const override final { return LoadCursor(nullptr, IDC_CROSS); }
};

unique_ptr<EditState> CreateStateMoveWirePoint (const EditStateDeps& deps, Wire* wire, size_t pointIndex)
{
	return unique_ptr<EditState>(new MoveWirePointES(deps, wire, pointIndex));
}
