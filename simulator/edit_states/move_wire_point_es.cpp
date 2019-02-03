
#include "pch.h"
#include "edit_state.h"
#include "Wire.h"
#include "Port.h"

using namespace std;

class MoveWirePointES : public edit_state
{
	typedef edit_state base;

	Wire* const _wire;
	size_t const _pointIndex;
	WireEnd const _initialPoint;
	POINT _first_down_location;

	enum substate
	{
		waiting_first_down,
		waiting_first_up,
		waiting_second_down,
		waiting_second_up,
		down,
	};

	substate _substate = waiting_first_down;

public:
	MoveWirePointES (const EditStateDeps& deps, Wire* wire, size_t pointIndex)
		: base(deps), _wire(wire), _pointIndex(pointIndex), _initialPoint(wire->GetPoints()[pointIndex])
	{ }

	virtual bool Completed() const override final { return _substate == down; }

	virtual void OnMouseDown (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		base::OnMouseDown (button, modifierKeysDown, location);

		if (button != MouseButton::Left)
			return;

		if (_substate == waiting_first_down)
		{
			_first_down_location = location.pt;
			_substate  = waiting_first_up;
		}
	}

	virtual void OnMouseMove (const MouseLocation& location) override
	{
		base::OnMouseMove(location);

		auto port = _pw->GetEditArea()->GetCPAt (location.d, SnapDistance);
		if (port != nullptr)
		{
			auto alreadyConnectedWire = _project->GetWireConnectedToPort(port);
			if (alreadyConnectedWire.first == nullptr)
				_wire->SetPoint(_pointIndex, port);
		}
		else
			_wire->SetPoint(_pointIndex, location.w);

		if (_substate == waiting_first_up)
		{
			RECT rc = { _first_down_location.x, _first_down_location.y, _first_down_location.x, _first_down_location.y };
			InflateRect (&rc, GetSystemMetrics(SM_CXDRAG), GetSystemMetrics(SM_CYDRAG));
			if (!PtInRect(&rc, location.pt))
				_substate = waiting_second_up;
		}
	}

	virtual void OnMouseUp (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		if (_substate == waiting_second_up)
		{
			_project->SetChangedFlag(true);
			_substate = down;
		}
	}

	virtual std::optional<LRESULT> OnKeyDown (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_wire->SetPoint(_pointIndex, _initialPoint);
			_substate = down;
			return 0;
		}

		return nullopt;
	}

	virtual void render (ID2D1DeviceContext* rt) override final
	{
		auto& point = _wire->GetPoints()[_pointIndex];
		if (holds_alternative<ConnectedWireEnd>(point))
			_pw->GetEditArea()->RenderSnapRect (rt, get<ConnectedWireEnd>(point)->GetCPLocation());
	}

	virtual HCURSOR GetCursor() const override final { return LoadCursor(nullptr, IDC_CROSS); }
};

unique_ptr<edit_state> CreateStateMoveWirePoint (const EditStateDeps& deps, Wire* wire, size_t pointIndex)
{
	return unique_ptr<edit_state>(new MoveWirePointES(deps, wire, pointIndex));
}
