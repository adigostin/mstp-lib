
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "edit_state.h"

class move_port_es : public edit_state
{
	typedef edit_state base;
	IPort* _port;
	PortSide _initialSide;
	LONG _initialOffset;
	bool _completed = false;

public:
	using base::base;

	handled OnMouseButtonDown (mouse_button button, UINT mks, const mouse_location& ml) final
	{
		if (button != mouse_button::left)
			return handled(true); // discard it
		
		_ASSERT(_selection->size() == 1);
		_port = wil::try_com_query_nothrow<IPort>(_selection->front());
		_ASSERT(_port);
		wil::com_query_failfast<IPortProperties>(_port)->get_Side(&_initialSide);
		wil::com_query_failfast<IPortProperties>(_port)->get_Offset(&_initialOffset);
		return handled(true);
	}

	void OnMouseMove (const mouse_location& ml) final
	{
		_port->Move(ml.w);
	}

	handled process_key_or_syskey_down (uint32_t vkey, UINT mks) final
	{
		if (vkey == VK_ESCAPE)
		{
			wil::com_query_failfast<IPortProperties>(_port)->put_Side(_initialSide);
			wil::com_query_failfast<IPortProperties>(_port)->put_Offset(_initialOffset);
			_completed = true;
			return handled(true);
		}

		return handled(false);
	}

	handled OnMouseButtonUp (mouse_button button, UINT mks, const mouse_location& mk) final
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
