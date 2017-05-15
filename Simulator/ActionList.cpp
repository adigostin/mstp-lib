
#include "pch.h"
#include "Simulator.h"

using namespace std;

class ActionList : public EventManager, public IActionList
{
	ULONG _refCount = 1;
	vector<pair<wstring, unique_ptr<EditAction>>> _actions;
	size_t _savePointIndex = 0;
	size_t _editPointIndex = 0;

	virtual ChangedEvent::Subscriber GetChangedEvent() override final { return ChangedEvent::Subscriber(this); }

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

	virtual size_t GetCount() const override final { return _actions.size(); }

	virtual void SetSavePoint() override final
	{
		_savePointIndex = _editPointIndex;
		ChangedEvent::InvokeHandlers (*this, this);
	}

	virtual void Undo() override final
	{
		assert (_editPointIndex > 0);
		_editPointIndex--;
		_actions[_editPointIndex].second->Undo();
		ChangedEvent::InvokeHandlers (*this, this);
	}

	virtual void Redo() override final
	{
		_actions[_editPointIndex].second->Redo();
		_editPointIndex++;
		ChangedEvent::InvokeHandlers (*this, this);
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override { return E_NOTIMPL; }

	virtual ULONG STDMETHODCALLTYPE AddRef() override final
	{
		return InterlockedIncrement(&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release() override final
	{
		assert (_refCount > 0);
		ULONG newRefCount = InterlockedDecrement(&_refCount);
		if (newRefCount == 0)
			delete this;
		return newRefCount;
	}
};

template<typename... Args>
static IActionListPtr Create (Args... args)
{
	return IActionListPtr(new ActionList (std::forward<Args>(args)...), false);
}

const ActionListFactory actionListFactory = &Create;
