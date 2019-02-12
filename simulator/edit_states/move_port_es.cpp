
#include "pch.h"
#include "edit_state.h"
#include "Bridge.h"
#include "Port.h"

class move_port_es : public edit_state
{
	typedef edit_state base;
	Port* _port;
	side _initialSide;
	float _initialOffset;
	bool _completed = false;

public:
	using base::base;

	virtual void process_mouse_button_down (edge::mouse_button button, UINT modifierKeysDown, const mouse_location& location) override final
	{
		assert (_selection->objects().size() == 1);
		_port = dynamic_cast<Port*>(_selection->objects().front());
		assert (_port != nullptr);
		_initialSide = _port->GetSide();
		_initialOffset = _port->GetOffset();
	}

	virtual void process_mouse_move (const mouse_location& location) override final
	{
		_port->bridge()->SetCoordsForInteriorPort (_port, location.w);
	}

	virtual std::optional<LRESULT> process_key_or_syskey_down (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_port->SetSideAndOffset (_initialSide, _initialOffset);
			_completed = true;
			return 0;
		}

		return std::nullopt;
	}

	virtual void process_mouse_button_up (edge::mouse_button button, UINT modifierKeysDown, const mouse_location& location) override final
	{
		_project->SetChangedFlag(true);
		_completed = true;
	}

	virtual bool completed() const override final { return _completed; }
};

std::unique_ptr<edit_state> create_state_move_port (const edit_state_deps& deps) { return std::make_unique<move_port_es>(deps); }
