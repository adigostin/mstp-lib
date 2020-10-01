
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "edit_state.h"
#include "bridge.h"
#include "port.h"

class move_port_es : public edit_state
{
	typedef edit_state base;
	port* _port;
	side _initialSide;
	float _initialOffset;
	bool _completed = false;

public:
	using base::base;

	handled process_mouse_button_down (mouse_button button, modifier_key mks, const mouse_location& ml) final
	{
		if (button != mouse_button::left)
			return handled(true); // discard it
		
		rassert (_selection->objects().size() == 1);
		_port = dynamic_cast<port*>(_selection->objects().front());
		rassert (_port);
		_initialSide = _port->side();
		_initialOffset = _port->offset();
		return handled(true);
	}

	void process_mouse_move (const mouse_location& ml) final
	{
		_port->bridge()->move_port(_port, ml.w);
	}

	handled process_key_or_syskey_down (uint32_t vkey, modifier_key mks) final
	{
		if (vkey == VK_ESCAPE)
		{
			_port->SetSideAndOffset (_initialSide, _initialOffset);
			_completed = true;
			return handled(true);
		}

		return handled(false);
	}

	handled process_mouse_button_up (mouse_button button, modifier_key mks, const mouse_location& mk) final
	{
		if (button != mouse_button::left)
			return handled(true); // discard it
		
		_project->SetChangedFlag(true);
		_completed = true;
		return handled(true);
	}

	bool completed() const final { return _completed; }
};

std::unique_ptr<edit_state> create_state_move_port (const edit_state_deps& deps) { return std::make_unique<move_port_es>(deps); }
