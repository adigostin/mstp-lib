
#include "pch.h"
#include "EditState.h"
#include "Bridge.h"
#include "Wire.h"
#include "Port.h"

using namespace std;

static size_t nextWireIndex = 1;

class CreateWireES : public EditState
{
	typedef EditState base;

	Wire* _wire = nullptr;
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
	using base::base;

	virtual void OnMouseDown (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		if (button != MouseButton::Left)
			return;

		if (_subState == WaitingFirstDown)
		{
			auto fromPort = _pw->GetEditArea()->GetCPAt(location.d, SnapDistance);
			if (fromPort != nullptr)
			{
				_firstDownLocation = location.pt;
				auto newWire = make_unique<Wire>();
				newWire->SetP0 (fromPort);
				newWire->SetP1 (fromPort->GetCPLocation());
				newWire->SetDebugName ((string("Wire") + to_string(nextWireIndex++)).c_str());
				_wire = newWire.get();
				_project->InsertWire(_project->GetWires().size(), move(newWire));
				_subState  = WaitingFirstUp;
			}
		}
		else if (_subState == WaitingSecondDown)
		{
			throw not_implemented_exception();
		}
	}

	virtual void OnMouseMove (const MouseLocation& location) override final
	{
		if (_subState == WaitingFirstDown)
			return;

		auto port = _pw->GetEditArea()->GetCPAt (location.d, SnapDistance);
		if (port != nullptr)
		{
			if (port != get<ConnectedWireEnd>(_wire->GetP0()))
			{
				auto alreadyConnectedWire = _project->GetWireConnectedToPort(port);
				if (alreadyConnectedWire.first == nullptr)
					_wire->SetP1(port);
			}
		}
		else
			_wire->SetP1(location.w);
		::InvalidateRect (_pw->GetEditArea()->GetHWnd(), nullptr, FALSE);

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
			if (holds_alternative<ConnectedWireEnd>(_wire->GetP1()))
			{
				_project->SetModified(true);
				_selection->Select(_wire);
				_subState = Done;
			}
		}
	}

	virtual std::optional<LRESULT> OnKeyDown (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			if (_wire != nullptr)
			{
				_project->RemoveWire(_project->GetWires().size() - 1);
				_wire = nullptr;
			}

			_subState = Done;
			::InvalidateRect (_pw->GetEditArea()->GetHWnd(), nullptr, FALSE);
			return 0;
		}

		return nullopt;
	}

	virtual void Render (ID2D1RenderTarget* rt) override final
	{
		base::Render(rt);
		if ((_wire != nullptr) && holds_alternative<ConnectedWireEnd>(_wire->GetP1()))
			_pw->GetEditArea()->RenderSnapRect (rt, get<ConnectedWireEnd>(_wire->GetP1())->GetCPLocation());
	}

	virtual bool Completed() const override final
	{
		return _subState == Done;
	}

	virtual HCURSOR GetCursor() const override final { return LoadCursor(nullptr, IDC_CROSS); }
};

std::unique_ptr<EditState> CreateStateCreateWire (const EditStateDeps& deps)  { return unique_ptr<EditState>(new CreateWireES(deps)); }
