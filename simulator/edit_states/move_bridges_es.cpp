
#include "pch.h"
#include "edit_state.h"
#include "Bridge.h"
#include "win32/utility_functions.h"

class move_bridges_es : public edit_state
{
	using base = edit_state;

	D2D1_POINT_2F _first_bridge_initial_location;
	D2D1_SIZE_F _offset_first_bridge;

	struct info
	{
		Bridge* b;
		D2D1_SIZE_F offset_from_first;
	};

	std::vector<info> _infos;
	bool _completed = false;

public:
	using base::base;

	virtual void process_mouse_button_down (edge::mouse_button button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		auto firstBridge = static_cast<Bridge*>(_selection->objects()[0]); assert (firstBridge != nullptr);
		_first_bridge_initial_location = firstBridge->GetLocation();

		for (auto o : _selection->objects())
		{
			auto b = dynamic_cast<Bridge*>(o); assert (b != nullptr);
			_infos.push_back ({ b, b->GetLocation() - firstBridge->GetLocation() });
		}

		_offset_first_bridge = location.w - firstBridge->GetLocation();
	}

	virtual void process_mouse_move (const MouseLocation& location) override final
	{
		auto firstBridgeLocation = location.w - _offset_first_bridge;
		_infos[0].b->SetLocation(firstBridgeLocation);
		for (size_t i = 1; i < _infos.size(); i++)
			_infos[i].b->SetLocation (firstBridgeLocation + _infos[i].offset_from_first);
	}

	virtual std::optional<LRESULT> process_key_or_syskey_down (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_infos[0].b->SetLocation (_first_bridge_initial_location);
			for (size_t i = 1; i < _infos.size(); i++)
				_infos[i].b->SetLocation (_first_bridge_initial_location + _infos[i].offset_from_first);

			_completed = true;
			::InvalidateRect (_ea->hwnd(), nullptr, FALSE);
			return 0;
		}

		return std::nullopt;
	}

	virtual void process_mouse_button_up (edge::mouse_button button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		_project->SetChangedFlag(true);
		_completed = true;
	}

	virtual bool completed() const override final { return _completed; }
};

std::unique_ptr<edit_state> create_state_move_bridges (const edit_state_deps& deps) { return std::make_unique<move_bridges_es>(deps); }
