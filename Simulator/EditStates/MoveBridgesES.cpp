
#include "pch.h"
#include "EditState.h"
#include "Bridge.h"
#include "win32/utility_functions.h"

using namespace std;

class MoveBridgeES : public EditState
{
	using base = EditState;

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

	virtual void OnMouseDown (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		auto firstBridge = static_cast<Bridge*>(_selection->GetObjects()[0]); assert (firstBridge != nullptr);
		_firstBridgeInitialLocation = firstBridge->GetLocation();

		for (auto o : _selection->GetObjects())
		{
			auto b = dynamic_cast<Bridge*>(o); assert (b != nullptr);
			_infos.push_back ({ b, b->GetLocation() - firstBridge->GetLocation() });
		}

		_offsetFirstBridge = location.w - firstBridge->GetLocation();
	}

	virtual void OnMouseMove (const MouseLocation& location) override final
	{
		auto firstBridgeLocation = location.w - _offsetFirstBridge;
		_infos[0].b->SetLocation(firstBridgeLocation);
		for (size_t i = 1; i < _infos.size(); i++)
			_infos[i].b->SetLocation (firstBridgeLocation + _infos[i].offsetFromFirst);
	}

	virtual std::optional<LRESULT> OnKeyDown (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_infos[0].b->SetLocation (_firstBridgeInitialLocation);
			for (size_t i = 1; i < _infos.size(); i++)
				_infos[i].b->SetLocation (_firstBridgeInitialLocation + _infos[i].offsetFromFirst);

			_completed = true;
			::InvalidateRect (_editArea->hwnd(), nullptr, FALSE);
			return 0;
		}

		return nullopt;
	}

	virtual void OnMouseUp (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		_project->SetChangedFlag(true);
		_completed = true;
	}

	virtual bool Completed() const override final { return _completed; }
};

unique_ptr<EditState> CreateStateMoveBridges (const EditStateDeps& deps) { return unique_ptr<EditState>(new MoveBridgeES(deps)); }
