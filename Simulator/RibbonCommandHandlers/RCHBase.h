
#pragma once

// An empty set of property keys means all properties must be invalidated for that command.
typedef std::unordered_map<UINT32, std::vector<const PROPERTYKEY*>> RCHCommandsAndProperties;

typedef ComPtr<IUICommandHandler> (*RCHFactory)();

struct RCHInfo
{
	RCHCommandsAndProperties const _cps;
	RCHFactory const _factory;

	RCHInfo(RCHCommandsAndProperties&& cps, RCHFactory factory);
	~RCHInfo();
};

const std::unordered_set<const RCHInfo*>& GetRCHInfos();

class RCHBase abstract : public IUICommandHandler
{
	ULONG _refCount = 1;

protected:
	//virtual HRESULT __stdcall Execute(UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties) override final;
	//virtual HRESULT __stdcall UpdateProperty(UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue) override final;
	virtual HRESULT __stdcall QueryInterface(REFIID riid, void **ppvObject) override final;
	virtual ULONG __stdcall AddRef() override final;
	virtual ULONG __stdcall Release() override final;

	virtual const RCHInfo& GetInfo() const = 0;
};
