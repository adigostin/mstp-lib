
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "edit_state.h"
#include "bridge.h"
#include "wire.h"
#include "port.h"

class create_wire_es : public edit_state
{
	using base = edit_state;

	wire* _wire = nullptr;

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
	using base::base;

	virtual void process_mouse_button_down (edge::mouse_button button, UINT modifierKeysDown, const mouse_location& location) override final
	{
		if (button != edge::mouse_button::left)
			return;

		if (_substate == waiting_first_down)
		{
			auto fromPort = _ew->GetCPAt(location.d, SnapDistance);
			if (fromPort != nullptr)
			{
				auto newWire = std::make_unique<wire>();
				newWire->set_p0 (fromPort);
				newWire->set_p1 (fromPort->GetCPLocation());
				_wire = newWire.get();
				_project->wire_collection_i::insert(_project->wires().size(), std::move(newWire));
				_substate  = waiting_first_up;
			}
		}
	}

	virtual void process_mouse_move (const mouse_location& location) override final
	{
		if (_substate == waiting_first_down)
			return;

		auto port = _ew->GetCPAt (location.d, SnapDistance);
		if (port != nullptr)
		{
			if (port != std::get<connected_wire_end>(_wire->p0()))
			{
				auto alreadyConnectedWire = _project->GetWireConnectedToPort(port);
				if (alreadyConnectedWire.first == nullptr)
					_wire->set_p1(port);
			}
		}
		else
			_wire->set_p1(location.w);
		::InvalidateRect (_ew->hwnd(), nullptr, FALSE);

		if (_substate == waiting_first_up)
			_substate = waiting_second_up;
	}

	virtual void process_mouse_button_up (edge::mouse_button button, UINT modifierKeysDown, const mouse_location& location) override final
	{
		if (_substate == waiting_second_up)
		{
			if (std::holds_alternative<connected_wire_end>(_wire->p1()))
			{
				_project->SetChangedFlag(true);
				_selection->select(_wire);
				_substate = down;
			}
		}
	}

	virtual handled process_key_or_syskey_down (uint32_t virtualKey, modifier_key modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			if (_wire != nullptr)
			{
				_project->wire_collection_i::remove(_project->wires().size() - 1);
				_wire = nullptr;
			}

			_substate = down;
			::InvalidateRect (_ew->hwnd(), nullptr, FALSE);
			return handled(true);
		}

		return handled(false);
	}

	virtual void render (ID2D1DeviceContext* rt) override final
	{
		base::render(rt);
		if ((_wire != nullptr) && std::holds_alternative<connected_wire_end>(_wire->p1()))
			_ew->RenderSnapRect (rt, std::get<connected_wire_end>(_wire->p1())->GetCPLocation());
	}

	virtual bool completed() const override final
	{
		return _substate == down;
	}

	virtual HCURSOR cursor() const override final { return LoadCursor(nullptr, IDC_CROSS); }
};

std::unique_ptr<edit_state> create_state_create_wire (const edit_state_deps& deps)  { return std::unique_ptr<edit_state>(new create_wire_es(deps)); }
