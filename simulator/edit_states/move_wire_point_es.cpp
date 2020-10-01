
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "edit_state.h"
#include "wire.h"
#include "port.h"

class move_wire_point_es : public edit_state
{
	typedef edit_state base;

	wire* const _wire;
	size_t const _pointIndex;
	wire_end const _initialPoint;
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
	move_wire_point_es (const edit_state_deps& deps, wire* wire, size_t pointIndex)
		: base(deps), _wire(wire), _pointIndex(pointIndex), _initialPoint(wire->points()[pointIndex])
	{ }

	virtual bool completed() const override final { return _substate == down; }

	virtual handled process_mouse_button_down (mouse_button button, modifier_key mks, const mouse_location& ml) override final
	{
		if (button != mouse_button::left)
			return handled(true); // discard it

		if (_substate == waiting_first_down)
		{
			_first_down_location = ml.pt;
			_substate  = waiting_first_up;
		}

		return handled(true);
	}

	virtual void process_mouse_move (const mouse_location& ml) override
	{
		auto port = _ew->GetCPAt (ml.d, SnapDistance);
		if (port != nullptr)
		{
			auto alreadyConnectedWire = _project->GetWireConnectedToPort(port);
			if (alreadyConnectedWire.first == nullptr)
				_wire->set_point(_pointIndex, port);
		}
		else
			_wire->set_point(_pointIndex, ml.w);

		if (_substate == waiting_first_up)
		{
			RECT rc = { _first_down_location.x, _first_down_location.y, _first_down_location.x, _first_down_location.y };
			InflateRect (&rc, GetSystemMetrics(SM_CXDRAG), GetSystemMetrics(SM_CYDRAG));
			if (!PtInRect(&rc, ml.pt))
				_substate = waiting_second_up;
		}
	}

	virtual handled process_mouse_button_up (mouse_button button, modifier_key mks, const mouse_location& ml) override final
	{
		if (button != mouse_button::left)
			return handled(true); // discard it
		
		if (_substate == waiting_second_up)
		{
			_project->SetChangedFlag(true);
			_substate = down;
		}

		return handled(false);
	}

	virtual handled process_key_or_syskey_down (uint32_t virtualKey, modifier_key modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_wire->set_point(_pointIndex, _initialPoint);
			_substate = down;
			return handled(true);
		}

		return handled(false);
	}

	virtual void render (ID2D1DeviceContext* rt) override final
	{
		auto& point = _wire->points()[_pointIndex];
		if (std::holds_alternative<connected_wire_end>(point))
			_ew->RenderSnapRect (rt, std::get<connected_wire_end>(point)->GetCPLocation());
	}

	virtual HCURSOR cursor() const override final { return LoadCursor(nullptr, IDC_CROSS); }
};

std::unique_ptr<edit_state> CreateStateMoveWirePoint (const edit_state_deps& deps, wire* wire, size_t pointIndex)
{
	return std::unique_ptr<edit_state>(new move_wire_point_es(deps, wire, pointIndex));
}
