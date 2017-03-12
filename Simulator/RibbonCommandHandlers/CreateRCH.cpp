
#include "pch.h"
#include "RCHBase.h"
#include "../Ribbon/RibbonIds.h"
#include "../Bridge.h"
#include "../EditStates/EditState.h"

class CreateRCH : public RCHBase
{
	typedef RCHBase base;
public:
	using RCHBase::RCHBase;

	HRESULT Update_cmdCreateBridge(UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
	{
		return E_NOTIMPL;
	}

	HRESULT Execute_cmdCreateBridge(UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties)
	{
		_area->EnterState (CreateStateCreateBridge(_area->MakeEditStateDeps()));
		return S_OK;
	}

	static const RCHInfo _info;
	virtual const RCHInfo& GetInfo() const override final { return _info; }
};

const RCHInfo CreateRCH::_info (
	{
		{ cmdCreateBridge, { static_cast<RCHUpdate>(&Update_cmdCreateBridge), static_cast<RCHExecute>(&Execute_cmdCreateBridge) } },
	},
	[](const RCHDeps& deps) { return ComPtr<RCHBase>(new CreateRCH(deps), false); }
);
