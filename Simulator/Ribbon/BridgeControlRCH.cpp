
#include "pch.h"
#include "RCHBase.h"
#include "../Ribbon/RibbonIds.h"
#include "../Bridge.h"

class BridgeControlRCH : public RCHBase
{
	typedef RCHBase base;

	using base::base;

	virtual HRESULT __stdcall Execute(UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties) override final
	{
		if (verb != UI_EXECUTIONVERB_EXECUTE)
			return E_NOTIMPL;

		if ((commandId == cmdEnableSTP) || (commandId == cmdDisableSTP))
		{
			bool enable = (commandId == cmdEnableSTP);

			for (auto o : _selection->GetObjects())
			{
				if (auto b = dynamic_cast<Bridge*>(o))
				{
					bool enabled = b->IsStpEnabled();

					if (enable && !enabled)
						b->EnableStp(STP_VERSION_RSTP, 1, GetTimestampMilliseconds());
					else if (!enable && enabled)
						b->DisableStp(GetTimestampMilliseconds());
				}
			}

			return S_OK;
		}
		else if (commandId == cmdDisableSTP)
		{
			return S_OK;
		}
		else
			return E_NOTIMPL;
	}

	virtual HRESULT __stdcall UpdateProperty(UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue) override final
	{
		if ((commandId == cmdEnableSTP) || (commandId == cmdDisableSTP))
		{
			if (key == UI_PKEY_Enabled)
			{
				BOOL enable = FALSE;
				for (auto o : _selection->GetObjects())
				{
					if (auto b = dynamic_cast<Bridge*>(o))
					{
						if (((commandId == cmdEnableSTP) && !b->IsStpEnabled())
							|| ((commandId == cmdDisableSTP) && b->IsStpEnabled()))
						{
							enable = TRUE;
							break;
						}
					}
				}

				return InitPropVariantFromBoolean (enable, newValue);
			}
			else
				return E_NOTIMPL;
		}
		else
			return E_NOTIMPL;
	}

	virtual void OnSelectionChanged() override final
	{
		base::OnSelectionChanged();
		_rf->InvalidateUICommand (cmdEnableSTP, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
		_rf->InvalidateUICommand (cmdDisableSTP, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
	}

	virtual void OnAddedToSelection (Object* o) override final
	{
		base::OnAddedToSelection(o);
		if (auto b = dynamic_cast<Bridge*>(o))
		{
			b->GetBridgeStartedEvent().AddHandler (&OnSelectedBridgeStartedOrStopping, this);
			b->GetBridgeStoppingEvent().AddHandler (&OnSelectedBridgeStartedOrStopping, this);
		}
	}

	virtual void OnRemovingFromSelection (Object* o) override final
	{
		if (auto b = dynamic_cast<Bridge*>(o))
		{
			b->GetBridgeStartedEvent().RemoveHandler (&OnSelectedBridgeStartedOrStopping, this);
			b->GetBridgeStoppingEvent().RemoveHandler (&OnSelectedBridgeStartedOrStopping, this);
		}
		base::OnRemovingFromSelection(o);
	}

	static void OnSelectedBridgeStartedOrStopping (void* callbackArg, Bridge* b)
	{
		auto rch = static_cast<BridgeControlRCH*>(callbackArg);
		rch->_rf->InvalidateUICommand(cmdEnableSTP, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
		rch->_rf->InvalidateUICommand(cmdDisableSTP, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
	}

	static const RCHInfo _info;
	virtual const RCHInfo& GetInfo() const override final { return _info; }
};

const RCHInfo BridgeControlRCH::_info (
	{ cmdEnableSTP, cmdDisableSTP },
	[](const RCHDeps& deps) { return ComPtr<IUICommandHandler>(new BridgeControlRCH(deps), false); });
