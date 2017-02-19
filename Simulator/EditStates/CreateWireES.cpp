
#include "pch.h"
#include "EditState.h"
#include "Bridge.h"
#include "Wire.h"
#include "Port.h"

using namespace std;

class CreateWireES : public EditState
{
	typedef EditState base;

	ComPtr<Wire> _wire;
	bool _completed = false;

public:
	CreateWireES (const EditStateDeps& deps, Port* fromPort)
		: base(deps)
	{
		if (fromPort != nullptr)
		{
			_wire = ComPtr<Wire>(new Wire(), false);
			_wire->SetP0 (fromPort);
			_wire->SetP1 (fromPort->GetConnectionPointLocation());
		}
	}

	virtual void OnMouseDown (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation, MouseButton button) override final
	{
		if (_wire == nullptr)
		{
			// Entered state by clicking a button on the ribbon. Now the user is placing the starting point of the wire.
			throw not_implemented_exception();
		}
	}

	virtual void OnMouseMove (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation) override final
	{
		if (_wire == nullptr)
		{
			// Placing the starting point of the wire.
			throw not_implemented_exception();
		}
		else
		{
			// Placing second point.
			auto port = _area->GetCPAt (dLocation, SnapDistance);
			if (port != nullptr)
				_wire->SetP1(port);
			else
				_wire->SetP1(wLocation);

			::InvalidateRect (_area->GetHWnd(), nullptr, FALSE);
		}
	}

	virtual void OnMouseUp (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation, MouseButton button) override final
	{
		assert (_wire != nullptr);
		if (holds_alternative<ConnectedWireEnd>(_wire->GetP1()))
		{
			_project->Add(_wire);
			_selection->Select(_wire);
			_completed = true;
		}
	}

	virtual void Render (ID2D1RenderTarget* rt) override final
	{
		D2D1_MATRIX_3X2_F oldtr;
		rt->GetTransform(&oldtr);
		rt->SetTransform(_area->GetZoomTransform());
		_wire->Render(rt, _area->GetDrawingObjects(), _area->GetDWriteFactory(), _area->GetSelectedVlanNumber());
		rt->SetTransform(&oldtr);

		if (holds_alternative<ConnectedWireEnd>(_wire->GetP1()))
			_area->RenderHoverCP (rt, get<ConnectedWireEnd>(_wire->GetP1()));
	}

	virtual bool Completed() const override final
	{
		return _completed;
	}

	virtual HCURSOR GetCursor() const override final { return LoadCursor(nullptr, IDC_CROSS); }
};

std::unique_ptr<EditState> CreateStateCreateWire (const EditStateDeps& deps, Port* fromPort)  { return unique_ptr<EditState>(new CreateWireES(deps, fromPort)); }
