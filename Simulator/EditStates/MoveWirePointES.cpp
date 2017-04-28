
#include "pch.h"
#include "EditState.h"
#include "Wire.h"

using namespace std;

class MoveWirePointES : public EditState
{
	typedef EditState base;

	Wire* const _wire;
	size_t const _pointIndex;
	WireEnd const _initialPoint;
	bool _completed = false;

public:
	MoveWirePointES (const EditStateDeps& deps, Wire* wire, size_t pointIndex)
		: base(deps), _wire(wire), _pointIndex(pointIndex), _initialPoint(wire->GetPoints()[pointIndex])
	{ }

	virtual bool Completed() const override final { return _completed; }

	virtual void OnMouseMove (const MouseLocation& location) override
	{
		auto port = _area->GetCPAt (location.d, SnapDistance);
		if (port != nullptr)
			_wire->SetP1(port);
		else
			_wire->SetP1(location.w);
		::InvalidateRect (_area->GetHWnd(), nullptr, FALSE);

	}

	virtual std::optional<LRESULT> OnKeyDown (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_wire->SetPoint(_pointIndex, _initialPoint);
			_completed = true;
			::InvalidateRect (_area->GetHWnd(), nullptr, FALSE);
			return 0;
		}

		return nullopt;
	}

};

unique_ptr<EditState> CreateStateMoveWirePoint (const EditStateDeps& deps, Wire* wire, size_t pointIndex)
{
	return unique_ptr<EditState>(new MoveWirePointES(deps, wire, pointIndex));
}
