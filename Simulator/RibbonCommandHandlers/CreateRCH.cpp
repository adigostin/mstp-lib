
#include "pch.h"
#include "RCHBase.h"
#include "../Ribbon/RibbonIds.h"
#include "../PhysicalBridge.h"

class CreateRCH : public RCHBase
{
public:
	using RCHBase::RCHBase;

	template<typename... Args>
	static ComPtr<IUICommandHandler> Create (Args... args) { return ComPtr<IUICommandHandler>(new CreateRCH(args...), false); }

	virtual HRESULT __stdcall Execute(UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties) override final
	{
		auto bridge = ComPtr<PhysicalBridge>(new PhysicalBridge(4), false);
		bridge->SetLocation (100, 100);
		_project->AddBridge (bridge);

		return E_NOTIMPL;
	}

	virtual HRESULT __stdcall UpdateProperty(UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue) override final
	{
		return E_NOTIMPL;
	}

	static const RCHInfo _info;
	virtual const RCHInfo& GetInfo() const override final { return _info; }
};

const RCHInfo CreateRCH::_info (
	{
		{ cmdCreateBridge, { &UI_PKEY_Enabled } },
	},
	&Create);
