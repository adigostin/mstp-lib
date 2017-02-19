
#include "pch.h"
#include "EditState.h"

using namespace std;

class MoveWirePointES : public EditState
{
	typedef EditState base;

	Wire* const _wire;
	size_t const _pointIndex;
	bool _completed = false;

public:
	MoveWirePointES (const EditStateDeps& deps, Wire* wire, size_t pointIndex)
		: base(deps), _wire(wire), _pointIndex(pointIndex)
	{ }

	virtual bool Completed() const override final { return _completed; }
};

unique_ptr<EditState> CreateStateMoveWirePoint (const EditStateDeps& deps, Wire* wire, size_t pointIndex)
{
	return unique_ptr<EditState>(new MoveWirePointES(deps, wire, pointIndex));
}
