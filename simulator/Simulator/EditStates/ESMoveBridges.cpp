
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "EditState.h"

class move_bridges_es : public edit_state
{
	using base = edit_state;

	POINT _first_bridge_initial_location;
	SIZE _offset_first_bridge;

	struct info
	{
		IBridge* b;
		SIZE offset_from_first;
	};

	std::vector<info> _infos;
	bool _completed = false;

public:
	using base::base;

	virtual handled OnMouseButtonDown (mouse_button button, UINT mks, const mouse_location& ml) override final
	{
		if (button != mouse_button::left)
			return handled(true); // discard it

		auto firstBridge = wil::try_com_query_nothrow<IBridge>(_selection->front());
		_first_bridge_initial_location = firstBridge->location();

		for (auto o : *_selection)
		{
			auto b = wil::try_com_query_nothrow<IBridge>(o);
			_infos.push_back ({ b, b->location() - firstBridge->location() });
		}

		_offset_first_bridge = ml.w - firstBridge->location();

		return handled(true);
	}

	virtual void OnMouseMove (const mouse_location& location) override final
	{
		auto firstBridgeLocation = location.w - _offset_first_bridge;
		_infos[0].b->set_location(firstBridgeLocation);
		for (size_t i = 1; i < _infos.size(); i++)
			_infos[i].b->set_location (firstBridgeLocation + _infos[i].offset_from_first);
	}

	virtual handled process_key_or_syskey_down (uint32_t virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_infos[0].b->set_location (_first_bridge_initial_location);
			for (size_t i = 1; i < _infos.size(); i++)
				_infos[i].b->set_location (_first_bridge_initial_location + _infos[i].offset_from_first);

			_completed = true;
			::InvalidateRect(_ew->hWnd(), 0, 0);
			return handled(true);
		}

		return handled(false);
	}

	virtual handled OnMouseButtonUp (mouse_button button, UINT mks, const mouse_location& ml) override final
	{
		if (button != mouse_button::left)
			return handled(true); // discard it
		
		_project->SetChangedFlag(true);
		_completed = true;
		return handled(true);
	}

	virtual bool completed() const override final { return _completed; }
};

std::unique_ptr<edit_state> create_state_move_bridges (const edit_state_deps& deps) { return std::make_unique<move_bridges_es>(deps); }
