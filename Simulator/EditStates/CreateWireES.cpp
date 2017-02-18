
#include "pch.h"
#include "EditState.h"
#include "Bridge.h"
#include "Wire.h"

using namespace std;

class CreateWireES : public EditState
{
	using EditState::EditState;

	ComPtr<Wire> _wire;
	bool _completed = false;

	virtual void OnMouseDown (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation, MouseButton button, HTCodeAndObject ht) override final
	{
		assert (ht.code == HTCode::PortConnectionPoint);
		auto fromPort = dynamic_cast<Port*>(ht.object); assert (fromPort != nullptr);
		_wire = ComPtr<Wire>(new Wire(), false);
		_wire->SetP0 (fromPort);
		_wire->SetP1 (fromPort->GetConnectionPointLocation());
	}

	virtual void OnMouseMove (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation) override final
	{
		_wire->SetP1(wLocation);
		::InvalidateRect (_area->GetHWnd(), nullptr, FALSE);
	}

	virtual void OnMouseUp (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation, MouseButton button) override final
	{
		_completed = true;
	}

	virtual void Render (ID2D1RenderTarget* dc) override final
	{
		_wire->Render(dc, _area->GetDrawingObjects());
	}

	virtual bool Completed() const override final
	{
		return _completed;
	}

	virtual HCURSOR GetCursor() const override final { return LoadCursor(nullptr, IDC_CROSS); }
};

std::unique_ptr<EditState> CreateStateCreateWire (const EditStateDeps& deps)  { return unique_ptr<EditState>(new CreateWireES(deps)); }
