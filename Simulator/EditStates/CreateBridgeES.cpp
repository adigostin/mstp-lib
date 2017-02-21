
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

	virtual void OnMouseMove (const MouseLocation& location) override final
	{
		if (_bridge == nullptr)
		{
			unsigned int portCount = 3;
			_bridge = ComPtr<Bridge>(new Bridge (_project, portCount, _project->AllocMacAddressRange(1 + portCount)), false);
		}

		_bridge->SetLocation (location.w.x - _bridge->GetWidth() / 2, location.w.y - _bridge->GetHeight() / 2);
		::InvalidateRect (_area->GetHWnd(), nullptr, FALSE);
	}

	virtual void OnMouseUp (const MouseLocation& location, MouseButton button) override final
	{
		if (_bridge != nullptr)
		{
			_project->Add(_bridge);
			_selection->Select(_bridge);
		}

		_completed = true;
	}

	virtual void Render (ID2D1RenderTarget* rt) override final
	{
		if (_bridge != nullptr)
		{
			D2D1_MATRIX_3X2_F oldtr;
			rt->GetTransform(&oldtr);
			rt->SetTransform(_area->GetZoomTransform());
			_bridge->Render (rt, _area->GetDrawingObjects(), _area->GetDWriteFactory(), _area->GetSelectedVlanNumber());
			rt->SetTransform(&oldtr);
		}
	}

	virtual bool Completed() const override final { return _completed; }
};

unique_ptr<EditState> CreateStateCreateBridge (const EditStateDeps& deps) { return unique_ptr<EditState>(new CreateBridgeES(deps)); }
