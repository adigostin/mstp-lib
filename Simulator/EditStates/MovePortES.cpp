
#include "pch.h"
#include "EditState.h"
#include "Bridge.h"
#include "Port.h"

using namespace std;

class MovePortES : public edit_state
{
	typedef edit_state base;
	Port* _port;
	Side _initialSide;
	float _initialOffset;
	bool _completed = false;

public:
	using base::base;

	virtual void OnMouseDown (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		assert (_selection->GetObjects().size() == 1);
		_port = dynamic_cast<Port*>(_selection->GetObjects().front());
		assert (_port != nullptr);
		_initialSide = _port->GetSide();
		_initialOffset = _port->GetOffset();
	}

	virtual void OnMouseMove (const MouseLocation& location) override final
	{
		_port->bridge()->SetCoordsForInteriorPort (_port, location.w);
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

	virtual void OnMouseUp (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		_project->SetChangedFlag(true);
		_completed = true;
	}

	virtual bool Completed() const override final { return _completed; }
};

unique_ptr<edit_state> CreateStateMovePort (const EditStateDeps& deps) { return unique_ptr<edit_state>(new MovePortES(deps)); }
