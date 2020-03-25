
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "edit_state.h"
#include "bridge.h"
#include "win32/utility_functions.h"

class move_bridges_es : public edit_state
{
	using base = edit_state;

	D2D1_POINT_2F _first_bridge_initial_location;
	D2D1_SIZE_F _offset_first_bridge;

	struct info
	{
		bridge* b;
		D2D1_SIZE_F offset_from_first;
	};

	std::vector<info> _infos;
	bool _completed = false;

public:
	using base::base;

	virtual void process_mouse_button_down (edge::mouse_button button, UINT modifierKeysDown, const mouse_location& location) override final
	{
		auto firstBridge = static_cast<bridge*>(_selection->objects()[0]); assert (firstBridge != nullptr);
		_first_bridge_initial_location = firstBridge->location();

		for (auto o : _selection->objects())
		{
			auto b = dynamic_cast<bridge*>(o); assert (b != nullptr);
			_infos.push_back ({ b, b->location() - firstBridge->location() });
		}

		_offset_first_bridge = location.w - firstBridge->location();
	}

	virtual void process_mouse_move (const mouse_location& location) override final
	{
		auto firstBridgeLocation = location.w - _offset_first_bridge;
		_infos[0].b->set_location(firstBridgeLocation);
		for (size_t i = 1; i < _infos.size(); i++)
			_infos[i].b->set_location (firstBridgeLocation + _infos[i].offset_from_first);
	}

	virtual handled process_key_or_syskey_down (uint32_t virtualKey, modifier_key modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_infos[0].b->set_location (_first_bridge_initial_location);
			for (size_t i = 1; i < _infos.size(); i++)
				_infos[i].b->set_location (_first_bridge_initial_location + _infos[i].offset_from_first);

			_completed = true;
			::InvalidateRect (_ew->hwnd(), nullptr, FALSE);
			return handled(true);
		}

		return handled(false);
	}

	virtual void process_mouse_button_up (edge::mouse_button button, UINT modifierKeysDown, const mouse_location& location) override final
	{
		_project->SetChangedFlag(true);
		_completed = true;
	}

	virtual bool completed() const override final { return _completed; }
};

std::unique_ptr<edit_state> create_state_move_bridges (const edit_state_deps& deps) { return std::make_unique<move_bridges_es>(deps); }
