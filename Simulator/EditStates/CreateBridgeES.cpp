
#include "pch.h"
#include "EditState.h"
#include "Bridge.h"

using namespace std;

class CreateBridgeES : public EditState
{
	typedef EditState base;
	bool _completed = false;
	ComPtr<Bridge> _bridge;

public:
	using base::base;

	virtual void OnMouseMove (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation) override final
	{
		if (_bridge == nullptr)
			_bridge = ComPtr<Bridge>(new Bridge (3, _project->AllocNextMacAddress()), false);

		_bridge->SetLocation (wLocation.x - _bridge->GetWidth() / 2, wLocation.y - _bridge->GetHeight() / 2);
		::InvalidateRect (_area->GetHWnd(), nullptr, FALSE);
	}

	virtual void OnMouseUp (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation, MouseButton button) override final
	{
		if (_bridge != nullptr)
		{
			_project->AddBridge(_bridge);
			_selection->Select(_bridge);
		}

		_completed = true;
	}

	virtual void Render (ID2D1DeviceContext* dc) override final
	{
		if (_bridge != nullptr)
			_bridge->Render (dc, _area->GetDrawingObjects(), _area->GetDWriteFactory(), _area->GetSelectedVlanNumber());
	}

	virtual bool Completed() const override final { return _completed; }
};

unique_ptr<EditState> CreateStateCreateBridge (const EditStateDeps& deps) { return unique_ptr<EditState>(new CreateBridgeES(deps)); }
