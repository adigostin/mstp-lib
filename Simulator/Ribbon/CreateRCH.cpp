
#include "pch.h"
#include "RCHBase.h"
#include "../Ribbon/RibbonIds.h"
#include "../Bridge.h"
#include "../EditStates/EditState.h"

class CreateRCH : public RCHBase
{
	typedef RCHBase base;

	using base::base;

	virtual HRESULT __stdcall Execute(UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties) override final
	{
		_area->EnterState (CreateStateCreateBridge(_area->MakeEditStateDeps()));
		return S_OK;
	}

	virtual HRESULT __stdcall UpdateProperty(UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue) override final
	{
		return E_NOTIMPL;
	}

	static const RCHInfo _info;
	virtual const RCHInfo& GetInfo() const override final { return _info; }
};

const RCHInfo CreateRCH::_info (
	{ cmdCreateBridge, },
	[](const RCHDeps& deps) { return ComPtr<IUICommandHandler>(new CreateRCH(deps), false); });
