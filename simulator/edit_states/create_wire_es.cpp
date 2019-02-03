
#include "pch.h"
#include "edit_state.h"
#include "Bridge.h"
#include "Wire.h"
#include "Port.h"

class create_wire_es : public edit_state
{
	using base = edit_state;

	Wire* _wire = nullptr;

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

	virtual void process_mouse_button_down (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		if (button != MouseButton::Left)
			return;

		if (_substate == waiting_first_down)
		{
			auto fromPort = _ea->GetCPAt(location.d, SnapDistance);
			if (fromPort != nullptr)
			{
				auto newWire = std::make_unique<Wire>();
				newWire->SetP0 (fromPort);
				newWire->SetP1 (fromPort->GetCPLocation());
				_wire = newWire.get();
				_project->insert_wire(_project->wires().size(), move(newWire));
				_substate  = waiting_first_up;
			}
		}
	}

	virtual void process_mouse_move (const MouseLocation& location) override final
	{
		if (_substate == waiting_first_down)
			return;

		auto port = _ea->GetCPAt (location.d, SnapDistance);
		if (port != nullptr)
		{
			if (port != std::get<ConnectedWireEnd>(_wire->GetP0()))
			{
				auto alreadyConnectedWire = _project->GetWireConnectedToPort(port);
				if (alreadyConnectedWire.first == nullptr)
					_wire->SetP1(port);
			}
		}
		else
			_wire->SetP1(location.w);
		::InvalidateRect (_ea->hwnd(), nullptr, FALSE);

		if (_substate == waiting_first_up)
			_substate = waiting_second_up;
	}

	virtual void process_mouse_button_up (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		if (_substate == waiting_second_up)
		{
			if (std::holds_alternative<ConnectedWireEnd>(_wire->GetP1()))
			{
				_project->SetChangedFlag(true);
				_selection->select(_wire);
				_substate = down;
			}
		}
	}

	virtual std::optional<LRESULT> process_key_or_syskey_down (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			if (_wire != nullptr)
			{
				_project->remove_wire(_project->wires().size() - 1);
				_wire = nullptr;
			}

			_substate = down;
			::InvalidateRect (_ea->hwnd(), nullptr, FALSE);
			return 0;
		}

		return std::nullopt;
	}

	virtual void render (ID2D1DeviceContext* rt) override final
	{
		base::render(rt);
		if ((_wire != nullptr) && std::holds_alternative<ConnectedWireEnd>(_wire->GetP1()))
			_ea->RenderSnapRect (rt, std::get<ConnectedWireEnd>(_wire->GetP1())->GetCPLocation());
	}

	virtual bool completed() const override final
	{
		return _substate == down;
	}

	virtual HCURSOR cursor() const override final { return LoadCursor(nullptr, IDC_CROSS); }
};

std::unique_ptr<edit_state> create_state_create_wire (const edit_state_deps& deps)  { return std::unique_ptr<edit_state>(new create_wire_es(deps)); }
