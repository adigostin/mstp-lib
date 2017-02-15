
#include "pch.h"
#include "RCHBase.h"
#include "../Ribbon/RibbonIds.h"
#include "../PhysicalBridge.h"

class BridgePropertiesRCH : public RCHBase
{
	typedef RCHBase base;

public:
	using base::base;
	/*
	template<typename... Args>
	BridgePropertiesRCH (Args... args)
		: base (args...)
	{
	}

	virtual ~BridgePropertiesRCH()
	{
	}
	*/
	template<typename... Args>
	static ComPtr<IUICommandHandler> Create(Args... args) { return ComPtr<IUICommandHandler>(new BridgePropertiesRCH(args...), false); }

	virtual HRESULT __stdcall Execute(UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties) override final
	{
		return E_NOTIMPL;
	}

	virtual HRESULT __stdcall UpdateProperty(UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue) override final
	{
		if (commandId == cmdBridgeAddress)
		{
			if (key == UI_PKEY_Enabled)
			{
				bool enable = (_selection->GetObjects().size() == 1);
				return UIInitPropertyFromBoolean (key, enable ? TRUE : FALSE, newValue);
			}
			else
				return E_NOTIMPL;
		}
		else
			return E_NOTIMPL;
	}

	virtual void OnSelectionChanged()
	{
		base::OnSelectionChanged();
		_rf->InvalidateUICommand (cmdBridgeAddress, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
		_rf->InvalidateUICommand (cmdBridgeAddress, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
		_rf->InvalidateUICommand (cmdBridgeAddress, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_StringValue);
	}

	static const RCHInfo _info;
	virtual const RCHInfo& GetInfo() const override final { return _info; }
};

const RCHInfo BridgePropertiesRCH::_info ({ cmdBridgeAddress }, &Create);
