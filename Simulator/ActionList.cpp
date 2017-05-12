
#include "pch.h"
#include "Simulator.h"

using namespace std;

class ActionList : public EventManager, public IActionList
{
	vector<pair<wstring, unique_ptr<EditAction>>> _actions;
	size_t _savePointIndex = 0;
	size_t _editPointIndex = 0;

	virtual ChangedEvent::Subscriber GetChangedEvent() override final { return ChangedEvent::Subscriber(*this); }

	virtual void AddPerformedUserAction (wstring&& name, unique_ptr<EditAction>&& action) override final
	{
		_actions.erase (_actions.begin() + _editPointIndex, _actions.end());
		_actions.push_back ({ move(name), move(action) });
		_editPointIndex++;
		ChangedEvent::InvokeHandlers(*this, this);
	}

	virtual void PerformAndAddUserAction (std::wstring&& actionName, std::unique_ptr<EditAction>&& action) override final
	{
		action->Redo();
		AddPerformedUserAction(move(actionName), move(action));
	}

	virtual size_t GetSavePointIndex() const override final { return _savePointIndex; }

	virtual size_t GetEditPointIndex() const override final { return _editPointIndex; }
};

template<typename... Args>
static unique_ptr<IActionList> Create (Args... args)
{
	return unique_ptr<IActionList>(new ActionList (std::forward<Args>(args)...));
}

const ActionListFactory actionListFactory = &Create;
