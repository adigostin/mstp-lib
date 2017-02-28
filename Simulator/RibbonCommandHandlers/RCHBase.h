
#pragma once
#include "../Simulator.h"

struct RCHDeps
{
	IProjectWindow* pw;
	IUIFramework* rf;
	IProject* project;
	IEditArea* area;
	ISelection* selection;
};

class RCHBase;
typedef ComPtr<RCHBase> (*RCHFactory)();

using RCHUpdate  = HRESULT(RCHBase::*)(UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue);
using RCHExecute = HRESULT(RCHBase::*)(UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties);

struct RCHInfo
{
	struct Callbacks
	{
		RCHUpdate  update;
		RCHExecute execute;
	};


	std::unordered_map<UINT32, Callbacks> const _commands;
	RCHFactory const _factory;

	RCHInfo(std::unordered_map<UINT32, Callbacks>&& commands, RCHFactory factory);
	~RCHInfo();
};

const std::unordered_set<const RCHInfo*>& GetRCHInfos();

class RCHBase abstract : public IUICommandHandler
{
	ULONG _refCount = 1;

protected:
	IProjectWindow* _pw;
	IEditArea* _area;
	IUIFramework* _rf;
	ComPtr<IProject> _project;
	ComPtr<ISelection> _selection;

public:
	void InjectDependencies (const RCHDeps& deps);

	virtual HRESULT __stdcall QueryInterface(REFIID riid, void **ppvObject) override final;
	virtual ULONG __stdcall AddRef() override final;
	virtual ULONG __stdcall Release() override final;

protected:
	virtual ~RCHBase();

	// IUICommandHandler
	virtual HRESULT STDMETHODCALLTYPE Execute (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties) override final;
	virtual HRESULT STDMETHODCALLTYPE UpdateProperty (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue) override final;

	virtual const RCHInfo& GetInfo() const = 0;
	virtual void OnAddedToSelection (Object* o) { }
	virtual void OnRemovingFromSelection (Object* o) { }
	virtual void OnSelectionChanged() { }

private:
	static void OnSelectionChangedStatic (void* callbackArg, ISelection* selection) { static_cast<RCHBase*>(callbackArg)->OnSelectionChanged(); }
	static void OnAddedToSelection (void* callbackArg, ISelection* selection, Object* o) { static_cast<RCHBase*>(callbackArg)->OnAddedToSelection(o); }
	static void OnRemovingFromSelection (void* callbackArg, ISelection* selection, Object* o) { static_cast<RCHBase*>(callbackArg)->OnRemovingFromSelection(o); }
};
