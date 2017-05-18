
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

	virtual void OnMouseUp (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		struct Action : public EditAction
		{
			Port* const _port;
			Side  const _initialSide;
			float const _initialOffset;
			Side  const _finalSide;
			float const _finalOffset;

			Action (Port* port, Side initialSide, float initialOffset, Side finalSide, float finalOffset)
				: _port(port), _initialSide(initialSide), _initialOffset(initialOffset), _finalSide(finalSide), _finalOffset(finalOffset)
			{ }

			virtual void Redo() override final { _port->SetSideAndOffset (_finalSide, _finalOffset); }
			virtual void Undo() override final { _port->SetSideAndOffset (_initialSide, _initialOffset); }
			virtual string GetName() const override final { return "Move Port"; }
		};

		auto action = unique_ptr<EditAction>(new Action(_port, _initialSide, _initialOffset, _port->GetSide(), _port->GetOffset()));
		_actionList->AddPerformedUserAction (move(action));
		_completed = true;
	}

	virtual bool Completed() const override final { return _completed; }
};

unique_ptr<EditState> CreateStateMovePort (const EditStateDeps& deps) { return unique_ptr<EditState>(new MovePortES(deps)); }
