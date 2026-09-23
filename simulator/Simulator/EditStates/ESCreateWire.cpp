
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "EditState.h"

class create_wire_es : public edit_state
{
	using base = edit_state;

	IWire* _wire = nullptr;

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

	virtual handled OnMouseButtonDown (mouse_button button, UINT mks, const mouse_location& ml) override final
	{
		if (button != edge::mouse_button::left)
			return handled(true); // discard it

		if (_substate == waiting_first_down)
		{
			auto fromPort = _ew->GetCPAt(ml.d, SnapDistance);
			if (fromPort != nullptr)
			{
				com_ptr<IWire> newWire;
				auto hr = MakeWire(&newWire); LOG_IF_FAILED(hr);
				newWire->set_p0 (fromPort);
				newWire->set_p1 (fromPort->GetCPLocation());
				_wire = newWire.get();
				_project->AddWire(std::move(newWire));
				_substate  = waiting_first_up;
			}
		}

		return handled(true);
	}

	virtual void OnMouseMove (const mouse_location& location) override final
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
		::InvalidateRect(_ew->hWnd(), 0, 0);

		if (_substate == waiting_first_up)
			_substate = waiting_second_up;
	}

	virtual handled OnMouseButtonUp (mouse_button button, UINT mks, const mouse_location& ml) override final
	{
		if (button != edge::mouse_button::left)
			return handled(true); // discard it
		
		if (_substate == waiting_second_up)
		{
			if (std::holds_alternative<connected_wire_end>(_wire->p1()))
			{
				_project->SetChangedFlag(true);
				_selection->Select(wil::try_com_query_nothrow<IDispatch>(_wire));
				_substate = down;
			}
		}

		return handled(true);
	}

	virtual handled process_key_or_syskey_down (uint32_t virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			if (_wire != nullptr)
			{
				_project->RemoveWire((ULONG)_project->WireCount() - 1);
				_wire = nullptr;
			}

			_substate = down;
			::InvalidateRect(_ew->hWnd(), 0, 0);
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
