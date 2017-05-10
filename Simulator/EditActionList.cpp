
#include "pch.h"
#include "Simulator.h"

using namespace std;

class ActionList : public IActionList
{
	vector<function<void(UndoOrRedo which)>> _actions;
	size_t _savePointIndex = 0;
	size_t _editPointIndex = 0;

	virtual void AddAndPerformEditAction (function<void(UndoOrRedo which)>&& action) override final
	{
		throw not_implemented_exception();
	}
};

template<typename... Args>
static IActionList* Create (Args... args)
{
	return new ActionList (std::forward<Args>(args)...);
}

const ActionListFactory actionListFactory = &Create;
