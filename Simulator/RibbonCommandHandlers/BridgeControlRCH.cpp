
#include "pch.h"
#include "RCHBase.h"
#include "../Ribbon/RibbonIds.h"
#include "../PhysicalBridge.h"

class BridgeControlRCH : public RCHBase
{
	typedef RCHBase base;

	using base::base;

	template<typename... Args>
	static ComPtr<IUICommandHandler> Create(Args... args) { return ComPtr<IUICommandHandler>(new BridgeControlRCH(args...), false); }

	virtual HRESULT __stdcall Execute(UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties) override final
	{
		if (verb != UI_EXECUTIONVERB_EXECUTE)
			return E_NOTIMPL;

		if ((commandId == cmdStartSTP) || (commandId == cmdStopSTP))
		{
			bool start = (commandId == cmdStartSTP);

			for (auto o : _selection->GetObjects())
			{
				if (auto b = dynamic_cast<PhysicalBridge*>(o))
				{
					bool started = b->IsStpBridgeStarted();

					if (start && !started)
						b->StartStpBridge(GetTimestampMilliseconds());
					else if (!start && started)
						b->StopStpBridge(GetTimestampMilliseconds());
				}
			}

			return S_OK;
		}
		else if (commandId == cmdStopSTP)
		{
			return S_OK;
		}
		else
			return E_NOTIMPL;
	}

	virtual HRESULT __stdcall UpdateProperty(UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue) override final
	{
		if ((commandId == cmdStartSTP) || (commandId == cmdStopSTP))
		{
			if (key == UI_PKEY_Enabled)
			{
				BOOL enable = FALSE;
				for (auto o : _selection->GetObjects())
				{
					if (auto b = dynamic_cast<PhysicalBridge*>(o))
					{
						if (((commandId == cmdStartSTP) && !b->IsStpBridgeStarted())
							|| ((commandId == cmdStopSTP) && b->IsStpBridgeStarted()))
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
		_rf->InvalidateUICommand (cmdStartSTP, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
		_rf->InvalidateUICommand (cmdStopSTP, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
	}

	virtual void OnAddedToSelection (Object* o) override final
	{
		base::OnAddedToSelection(o);
		if (auto b = dynamic_cast<PhysicalBridge*>(o))
		{
			b->GetBridgeStartedEvent().AddHandler (&OnSelectedBridgeStartedOrStopping, this);
			b->GetBridgeStoppingEvent().AddHandler (&OnSelectedBridgeStartedOrStopping, this);
		}
	}

	virtual void OnRemovingFromSelection (Object* o) override final
	{
		if (auto b = dynamic_cast<PhysicalBridge*>(o))
		{
			b->GetBridgeStartedEvent().RemoveHandler (&OnSelectedBridgeStartedOrStopping, this);
			b->GetBridgeStoppingEvent().RemoveHandler (&OnSelectedBridgeStartedOrStopping, this);
		}
		base::OnRemovingFromSelection(o);
	}

	static void OnSelectedBridgeStartedOrStopping (void* callbackArg, PhysicalBridge* b)
	{
		auto rch = static_cast<BridgeControlRCH*>(callbackArg);
		rch->_rf->InvalidateUICommand(cmdStartSTP, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
		rch->_rf->InvalidateUICommand(cmdStopSTP, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
	}
	/*
	virtual void OnSelectedBridgeStarted (PhysicalBridge* b) override final
	{
		base::OnSelectedBridgeStarted(b);
		_rf->InvalidateUICommand(cmdStartSTP, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
		_rf->InvalidateUICommand(cmdStopSTP, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
	}

	virtual void OnSelectedBridgeStopping(PhysicalBridge* b) override final
	{
		base::OnSelectedBridgeStopping(b);
		_rf->InvalidateUICommand(cmdStartSTP, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
		_rf->InvalidateUICommand(cmdStopSTP, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
	}
	*/
	static const RCHInfo _info;
	virtual const RCHInfo& GetInfo() const override final { return _info; }
};

const RCHInfo BridgeControlRCH::_info({ cmdStartSTP, cmdStopSTP }, &Create);
