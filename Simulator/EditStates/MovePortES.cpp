
#include "pch.h"
#include "EditState.h"
#include "Bridge.h"
#include "Port.h"

using namespace std;

class MovePortES : public EditState
{
	typedef EditState base;
	Port* _port;
	Side _initialSide;
	float _initialOffset;
	bool _completed = false;

public:
	MovePortES (const EditStateDeps& deps)
		: base(deps)
	{
		assert (deps.selection->GetObjects().size() == 1);
		_port = dynamic_cast<Port*>(deps.selection->GetObjects().front());
		assert (_port != nullptr);
		_initialSide = _port->GetSide();
		_initialOffset = _port->GetOffset();
	}

	virtual void OnMouseMove (const MouseLocation& location) override final
	{
		_port->GetBridge()->SetCoordsForInteriorPort (_port, location.w);
	}

	virtual std::optional<LRESULT> OnKeyDown (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_port->SetSideAndOffset (_initialSide, _initialOffset);
			_completed = true;
			return 0;
		}

		return nullopt;
	}

	virtual void OnMouseUp (const MouseLocation& location, MouseButton button) override final
	{
		_completed = true;
	}

	virtual bool Completed() const override final { return _completed; }
};

unique_ptr<EditState> CreateStateMovePort (const EditStateDeps& deps) { return unique_ptr<EditState>(new MovePortES(deps)); }
