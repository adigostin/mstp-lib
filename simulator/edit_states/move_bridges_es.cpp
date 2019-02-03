
#include "pch.h"
#include "edit_state.h"
#include "Bridge.h"
#include "win32/utility_functions.h"

using namespace std;

class MoveBridgeES : public edit_state
{
	using base = edit_state;

	D2D1_POINT_2F _firstBridgeInitialLocation;
	D2D1_SIZE_F _offsetFirstBridge;

	struct Info
	{
		Bridge* b;
		D2D1_SIZE_F offsetFromFirst;
	};

	std::vector<Info> _infos;
	bool _completed = false;

public:
	using base::base;

	virtual void process_mouse_button_down (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		auto firstBridge = static_cast<Bridge*>(_selection->objects()[0]); assert (firstBridge != nullptr);
		_firstBridgeInitialLocation = firstBridge->GetLocation();

		for (auto o : _selection->objects())
		{
			auto b = dynamic_cast<Bridge*>(o); assert (b != nullptr);
			_infos.push_back ({ b, b->GetLocation() - firstBridge->GetLocation() });
		}

		_offsetFirstBridge = location.w - firstBridge->GetLocation();
	}

	virtual void process_mouse_move (const MouseLocation& location) override final
	{
		auto firstBridgeLocation = location.w - _offsetFirstBridge;
		_infos[0].b->SetLocation(firstBridgeLocation);
		for (size_t i = 1; i < _infos.size(); i++)
			_infos[i].b->SetLocation (firstBridgeLocation + _infos[i].offsetFromFirst);
	}

	virtual std::optional<LRESULT> process_key_or_syskey_down (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_infos[0].b->SetLocation (_firstBridgeInitialLocation);
			for (size_t i = 1; i < _infos.size(); i++)
				_infos[i].b->SetLocation (_firstBridgeInitialLocation + _infos[i].offsetFromFirst);

			_completed = true;
			::InvalidateRect (_ea->hwnd(), nullptr, FALSE);
			return 0;
		}

		return nullopt;
	}

	virtual void process_mouse_button_up (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		_project->SetChangedFlag(true);
		_completed = true;
	}

	virtual bool completed() const override final { return _completed; }
};

unique_ptr<edit_state> CreateStateMoveBridges (const edit_state_deps& deps) { return unique_ptr<edit_state>(new MoveBridgeES(deps)); }
