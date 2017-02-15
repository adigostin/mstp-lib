
#include "pch.h"
#include "RCHBase.h"
#include "../Ribbon/RibbonIds.h"
#include "../PhysicalBridge.h"
#include "../mstp-lib/stp.h"

class BridgeControlRCH : public RCHBase
{
	typedef RCHBase base;

	using base::base;

	template<typename... Args>
	static ComPtr<IUICommandHandler> Create(Args... args) { return ComPtr<IUICommandHandler>(new BridgeControlRCH(args...), false); }

	virtual HRESULT __stdcall Execute(UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties) override final
	{
		return E_NOTIMPL;
	}

	virtual HRESULT __stdcall UpdateProperty(UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue) override final
	{
		if (commandId == cmdStartSTP)
		{
			if (key == UI_PKEY_Enabled)
			{
				InitPropVariantFromBoolean(FALSE, newValue);

				for (auto o : _selection->GetObjects())
				{
					if (auto b = dynamic_cast<PhysicalBridge*>(o))
					{
						if (!STP_IsBridgeStarted(b->GetStpBridge()))
						{
							InitPropVariantFromBoolean(TRUE, newValue);
							break;
						}
					}
				}

				return S_OK;
			}
			else
				return E_NOTIMPL;
		}
		else if (commandId == cmdStopSTP)
		{
			if (key == UI_PKEY_Enabled)
			{
				InitPropVariantFromBoolean(FALSE, newValue);

				for (auto o : _selection->GetObjects())
				{
					if (auto b = dynamic_cast<PhysicalBridge*>(o))
					{
						if (STP_IsBridgeStarted(b->GetStpBridge()))
						{
							InitPropVariantFromBoolean(TRUE, newValue);
							break;
						}
					}
				}

				return S_OK;
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
		_rf->InvalidateUICommand (cmdStartSTP, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
		_rf->InvalidateUICommand (cmdStopSTP, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
	}

	static const RCHInfo _info;
	virtual const RCHInfo& GetInfo() const override final { return _info; }
};

const RCHInfo BridgeControlRCH::_info({ cmdStartSTP, cmdStopSTP }, &Create);
