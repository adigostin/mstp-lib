
#include "pch.h"
#include "EditState.h"
#include "Wire.h"
#include "Port.h"

using namespace std;

class MoveWirePointES : public EditState
{
	typedef EditState base;

	Wire* const _wire;
	size_t const _pointIndex;
	WireEnd const _initialPoint;
	POINT _firstDownLocation;

	enum SubState
	{
		WaitingFirstDown,
		WaitingFirstUp,
		WaitingSecondDown,
		WaitingSecondUp,
		Done,
	};

	SubState _subState = WaitingFirstDown;

public:
	MoveWirePointES (const EditStateDeps& deps, Wire* wire, size_t pointIndex)
		: base(deps), _wire(wire), _pointIndex(pointIndex), _initialPoint(wire->GetPoints()[pointIndex])
	{ }

	virtual bool Completed() const override final { return _subState == Done; }

	virtual void OnMouseDown (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		base::OnMouseDown (button, modifierKeysDown, location);

		if (button != MouseButton::Left)
			return;

		if (_subState == WaitingFirstDown)
		{
			_firstDownLocation = location.pt;
			_subState  = WaitingFirstUp;
		}
	}

	virtual void OnMouseMove (const MouseLocation& location) override
	{
		base::OnMouseMove(location);

		auto port = _pw->GetEditArea()->GetCPAt (location.d, SnapDistance);
		if (port != nullptr)
		{
			auto alreadyConnectedWire = _project->GetWireConnectedToPort(port);
			if (alreadyConnectedWire.first == nullptr)
				_wire->SetPoint(_pointIndex, port);
		}
		else
			_wire->SetPoint(_pointIndex, location.w);

		if (_subState == WaitingFirstUp)
		{
			RECT rc = { _firstDownLocation.x, _firstDownLocation.y, _firstDownLocation.x, _firstDownLocation.y };
			InflateRect (&rc, GetSystemMetrics(SM_CXDRAG), GetSystemMetrics(SM_CYDRAG));
			if (!PtInRect(&rc, location.pt))
				_subState = WaitingSecondUp;
		}
	}

	virtual void OnMouseUp (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		if (_subState == WaitingSecondUp)
		{
			struct MoveAction : public EditAction
			{
				Wire* const _wire;
				size_t const _pointIndex;
				WireEnd const _initial;
				WireEnd const _final;

				MoveAction (Wire* wire, size_t pointIndex, WireEnd initial, WireEnd final)
					: _wire(wire), _pointIndex(pointIndex), _initial(initial), _final(final)
				{ }

				virtual void Redo() override final { _wire->SetPoint(_pointIndex, _final); }
				virtual void Undo() override final { _wire->SetPoint(_pointIndex, _initial); }
				virtual std::string GetName() const override final { return "Move wire end"; }
			};

			_actionList->AddPerformedUserAction(unique_ptr<EditAction>(new MoveAction(_wire, _pointIndex, _initialPoint, _wire->GetPoints()[_pointIndex])));
			_subState = Done;
		}
	}

	virtual std::optional<LRESULT> OnKeyDown (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_wire->SetPoint(_pointIndex, _initialPoint);
			_subState = Done;
			return 0;
		}

		return nullopt;
	}

	virtual void Render (ID2D1RenderTarget* rt) override final
	{
		auto& point = _wire->GetPoints()[_pointIndex];
		if (holds_alternative<ConnectedWireEnd>(point))
			_pw->GetEditArea()->RenderSnapRect (rt, get<ConnectedWireEnd>(point)->GetCPLocation());
	}

	virtual HCURSOR GetCursor() const override final { return LoadCursor(nullptr, IDC_CROSS); }
};

unique_ptr<EditState> CreateStateMoveWirePoint (const EditStateDeps& deps, Wire* wire, size_t pointIndex)
{
	return unique_ptr<EditState>(new MoveWirePointES(deps, wire, pointIndex));
}
