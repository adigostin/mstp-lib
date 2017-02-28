
#include "pch.h"
#include "RCHBase.h"
#include "../Ribbon/RibbonIds.h"
#include "../Bridge.h"

using namespace std;

class BridgePropertiesRCH : public RCHBase
{
	typedef RCHBase base;

	bool SingleBridgeSelected() const
	{
		return (_selection != nullptr) && (_selection->GetObjects().size() == 1) && (dynamic_cast<Bridge*>(_selection->GetObjects()[0].Get()) != nullptr);
	}

	bool BridgesSelected() const
	{
		return (_selection != nullptr)
			&& !_selection->GetObjects().empty()
			&& all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(),
					   [](const ComPtr<Object>& o) { return dynamic_cast<Bridge*>(o.Get()) != nullptr; });

	}

	// ========================================================================

	HRESULT Update_cmdStpEnabled (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
	{
		if (key == UI_PKEY_Enabled)
		{
			BOOL enabled = (_selection != nullptr) && all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(),
															  [](const ComPtr<Object>& o) { return dynamic_cast<Bridge*>(o.Get()) != nullptr; });
			return UIInitPropertyFromBoolean (key, enabled, newValue);
		}

		if (key == UI_PKEY_BooleanValue)
		{
			BOOL checked = FALSE;
			if (_selection != nullptr)
			{
				for (auto& o : _selection->GetObjects())
				{
					auto b = dynamic_cast<Bridge*>(o.Get());
					if (b == nullptr)
					{
						checked = FALSE;
						break;
					}

					if (b->IsStpEnabled())
						checked = TRUE;
				}
			}

			return UIInitPropertyFromBoolean (key, checked, newValue);
		}

		return E_NOTIMPL;
	}

	HRESULT Execute_cmdStpEnabled (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties)
	{
		BOOL enable;
		auto hr = UIPropertyToBoolean(*key, *currentValue, &enable); ThrowIfFailed(hr);

		for (auto& o : _selection->GetObjects())
		{
			auto b = dynamic_cast<Bridge*>(o.Get()); assert (b != nullptr);
			if (enable)
				b->EnableStp(STP_VERSION_RSTP, 1, GetTimestampMilliseconds());
			else
				b->DisableStp(GetTimestampMilliseconds());
		}

		return S_OK;
	}

	// ========================================================================

	HRESULT Update_cmdBridgeAddress (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
	{
		if (key == UI_PKEY_Enabled)
			return UIInitPropertyFromBoolean (key, SingleBridgeSelected(), newValue);

		if (key == UI_PKEY_RepresentativeString)
			return UIInitPropertyFromString (key, L"MM:MM:MM:MM:MM:MM", newValue);

		if (key == UI_PKEY_StringValue)
		{
			if (!SingleBridgeSelected())
				return UIInitPropertyFromString (key, L"", newValue);

			auto bridge = dynamic_cast<Bridge*>(_selection->GetObjects()[0].Get());
			const auto& addr = bridge->GetMacAddress();
			wchar_t str[32];
			swprintf_s (str, L"%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
			return UIInitPropertyFromString (key, str, newValue);
		}

		if (key == UI_PKEY_ItemsSource)
		{
			ComPtr<IUICollection> collection;
			auto hr = UIPropertyToInterface (key, *currentValue, &collection);
			if (FAILED(hr))
				return hr;
			return UIInitPropertyFromInterface (key, collection, newValue);
		}

		if (key == UI_PKEY_SelectedItem)
		{
			// Workaround for what seems like a framework bug: the first this command is displayed,
			// the framework doesn't request UI_PKEY_StringValue and uses an empty string instead.
			_rf->InvalidateUICommand (cmdBridgeAddress, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_StringValue);
			return UIInitPropertyFromUInt32 (key, -1, newValue);
		}

		return E_NOTIMPL;
	}

	HRESULT Execute_cmdBridgeAddress (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties)
	{
		return E_NOTIMPL;
	}
	
	// ========================================================================

	HRESULT Update_cmdEnableDisableSTP (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
	{
		if (key == UI_PKEY_Enabled)
		{
			BOOL enable = FALSE;
			for (auto& o : _selection->GetObjects())
			{
				auto b = dynamic_cast<Bridge*>(o.Get());
				if (b == nullptr)
					break;

				if (((commandId == cmdEnableSTP) && !b->IsStpEnabled())
					|| ((commandId == cmdDisableSTP) && b->IsStpEnabled()))
				{
					enable = TRUE;
					break;
				}
			}

			return UIInitPropertyFromBoolean (key, enable, newValue);
		}
		else
			return E_NOTIMPL;
	}

	HRESULT Execute_cmdEnableDisableSTP (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties)
	{
		bool enable = (commandId == cmdEnableSTP);

		for (auto& o : _selection->GetObjects())
		{
			if (auto b = dynamic_cast<Bridge*>(o.Get()))
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

	// ========================================================================

	HRESULT Update_cmdBridgeTabGroup (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
	{
		if (key == UI_PKEY_ContextAvailable)
			return UIInitPropertyFromUInt32 (key, BridgesSelected() ? UI_CONTEXTAVAILABILITY_ACTIVE : UI_CONTEXTAVAILABILITY_NOTAVAILABLE, newValue);

		return E_NOTIMPL;
	}

	// ========================================================================

	virtual void OnSelectionChanged() override final
	{
		base::OnSelectionChanged();
		_rf->InvalidateUICommand (cmdBridgeAddress, UI_INVALIDATIONS_VALUE | UI_INVALIDATIONS_STATE, nullptr);
		_rf->InvalidateUICommand (cmdStpEnabled, UI_INVALIDATIONS_VALUE | UI_INVALIDATIONS_STATE, nullptr);
		_rf->InvalidateUICommand (cmdEnableSTP, UI_INVALIDATIONS_STATE, nullptr);
		_rf->InvalidateUICommand (cmdDisableSTP, UI_INVALIDATIONS_STATE, nullptr);
		_rf->InvalidateUICommand (cmdBridgeTabGroup, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ContextAvailable);
	}

	virtual void OnAddedToSelection (Object* o) override final
	{
		base::OnAddedToSelection(o);
		if (auto b = dynamic_cast<Bridge*>(o))
		{
			b->GetStpEnabledEvent().AddHandler (&OnStpEnabledOrDisabling, this);
			b->GetStpDisablingEvent().AddHandler (&OnStpEnabledOrDisabling, this);
		}
	}

	virtual void OnRemovingFromSelection (Object* o) override final
	{
		if (auto b = dynamic_cast<Bridge*>(o))
		{
			b->GetStpEnabledEvent().RemoveHandler (&OnStpEnabledOrDisabling, this);
			b->GetStpDisablingEvent().RemoveHandler (&OnStpEnabledOrDisabling, this);
		}
		base::OnRemovingFromSelection(o);
	}

	static void OnStpEnabledOrDisabling (void* callbackArg, Bridge* b)
	{
		auto rch = static_cast<BridgePropertiesRCH*>(callbackArg);
		rch->_rf->InvalidateUICommand (cmdStpEnabled, UI_INVALIDATIONS_STATE | UI_INVALIDATIONS_VALUE, nullptr);
		rch->_rf->InvalidateUICommand (cmdEnableSTP, UI_INVALIDATIONS_STATE, nullptr);
		rch->_rf->InvalidateUICommand (cmdDisableSTP, UI_INVALIDATIONS_STATE, nullptr);
	}

	static const RCHInfo _info;
	virtual const RCHInfo& GetInfo() const override final { return _info; }
};

const RCHInfo BridgePropertiesRCH::_info (
	{
		{ cmdBridgeAddress,  { static_cast<RCHUpdate>(&Update_cmdBridgeAddress), static_cast<RCHExecute>(&Execute_cmdBridgeAddress) } },
		{ cmdBridgeTabGroup, { static_cast<RCHUpdate>(&Update_cmdBridgeTabGroup), nullptr } },
		{ cmdEnableSTP,      { static_cast<RCHUpdate>(&Update_cmdEnableDisableSTP), static_cast<RCHExecute>(&Execute_cmdEnableDisableSTP) } },
		{ cmdDisableSTP,     { static_cast<RCHUpdate>(&Update_cmdEnableDisableSTP), static_cast<RCHExecute>(&Execute_cmdEnableDisableSTP) } },
		{ cmdStpEnabled,     { static_cast<RCHUpdate>(&Update_cmdStpEnabled), static_cast<RCHExecute>(&Execute_cmdStpEnabled) } },
	},
	[]() { return ComPtr<RCHBase>(new BridgePropertiesRCH(), false); }
);
