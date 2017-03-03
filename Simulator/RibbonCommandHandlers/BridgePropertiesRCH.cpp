
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
				b->EnableStp(GetTimestampMilliseconds());
			else
				b->DisableStp(GetTimestampMilliseconds());
		}

		return S_OK;
	}

	HRESULT Update_cmdStpVersion (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
	{
		if ((key == UI_PKEY_Enabled) || (key == UI_PKEY_StringValue) || (key == UI_PKEY_SelectedItem))
		{
			std::optional<STP_VERSION> stpVersion;
			if (BridgesSelected())
			{
				auto s = dynamic_cast<Bridge*>(_selection->GetObjects()[0].Get())->GetStpVersion();
				if (all_of (_selection->GetObjects().begin(),
							_selection->GetObjects().end(),
							[=](const ComPtr<Object>& o) { return dynamic_cast<Bridge*>(o.Get())->GetStpVersion() == s; }))
					stpVersion = s;
			}

			if (key == UI_PKEY_Enabled)
				return UIInitPropertyFromBoolean (key, stpVersion ? TRUE : FALSE, newValue);

			if (key == UI_PKEY_SelectedItem)
			{
				if (!stpVersion)
					return UIInitPropertyFromUInt32 (key, -1, newValue);
				else if (stpVersion == STP_VERSION_LEGACY_STP)
					return UIInitPropertyFromUInt32 (key, 0, newValue);
				else if (stpVersion == STP_VERSION_RSTP)
					return UIInitPropertyFromUInt32 (key, 1, newValue);
				else if (stpVersion == STP_VERSION_MSTP)
					return UIInitPropertyFromUInt32 (key, 2, newValue);
				else
					return E_NOTIMPL;
			}
		
			if (key == UI_PKEY_StringValue)
			{
				wstring_convert<codecvt_utf8<wchar_t>> converter;
				auto s = stpVersion ? converter.from_bytes(STP_GetVersionString(stpVersion.value())) : wstring();
				return UIInitPropertyFromString (key, s.c_str(), newValue);
			}

			return E_NOTIMPL;
		}

		if (key == UI_PKEY_RepresentativeString)
		{
			wstring_convert<codecvt_utf8<wchar_t>> converter;
			return UIInitPropertyFromString (key, converter.from_bytes(STP_GetVersionString(STP_VERSION_LEGACY_STP)).c_str(), newValue);
		}

		if (key == UI_PKEY_ItemsSource)
		{
			ComPtr<IUICollection> collection;
			auto hr = UIPropertyToInterface (key, *currentValue, &collection);
			if (FAILED(hr))
				return hr;
			collection->Clear();
			wstring_convert<codecvt_utf8<wchar_t>> converter;
			collection->Add (ComPtr<IUISimplePropertySet>(new ItemPropertySet(converter.from_bytes(STP_GetVersionString(STP_VERSION_LEGACY_STP))), false));
			collection->Add (ComPtr<IUISimplePropertySet>(new ItemPropertySet(converter.from_bytes(STP_GetVersionString(STP_VERSION_RSTP))), false));
			collection->Add (ComPtr<IUISimplePropertySet>(new ItemPropertySet(converter.from_bytes(STP_GetVersionString(STP_VERSION_MSTP))), false));
			//return UIInitPropertyFromInterface (key, collection, newValue);
			return S_OK;
		}

		return E_NOTIMPL;
	}

	HRESULT Execute_cmdStpVersion (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties)
	{
		if (verb != UI_EXECUTIONVERB_EXECUTE)
			return E_NOTIMPL;

		UINT32 index;
		auto hr = UIPropertyToUInt32(*key, *currentValue, &index); ThrowIfFailed(hr);
		
		STP_VERSION newVersion;
		if (index == 0)
			newVersion = STP_VERSION_LEGACY_STP;
		else if (index == 1)
			newVersion = STP_VERSION_RSTP;
		else if (index == 2)
			newVersion = STP_VERSION_MSTP;
		else
			assert(false);

		auto timestamp = GetTimestampMilliseconds();

		for (auto& o : _selection->GetObjects())
		{
			auto b = dynamic_cast<Bridge*>(o.Get()); assert (b != nullptr);
			b->SetStpVersion(newVersion, timestamp);
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
					b->EnableStp(GetTimestampMilliseconds());
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
		_rf->InvalidateUICommand (cmdStpVersion, UI_INVALIDATIONS_VALUE | UI_INVALIDATIONS_STATE | UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
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
			b->GetStpVersionChangedEvent().AddHandler (&OnStpVersionChanged, this);
		}
	}

	virtual void OnRemovingFromSelection (Object* o) override final
	{
		if (auto b = dynamic_cast<Bridge*>(o))
		{
			b->GetStpEnabledEvent().RemoveHandler (&OnStpEnabledOrDisabling, this);
			b->GetStpDisablingEvent().RemoveHandler (&OnStpEnabledOrDisabling, this);
			b->GetStpVersionChangedEvent().RemoveHandler (&OnStpVersionChanged, this);
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

	static void OnStpVersionChanged (void* callbackArg, Bridge* b)
	{
		auto rch = static_cast<BridgePropertiesRCH*>(callbackArg);
		rch->_rf->InvalidateUICommand (cmdStpVersion, UI_INVALIDATIONS_VALUE | UI_INVALIDATIONS_STATE | UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
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
		{ cmdStpVersion,     { static_cast<RCHUpdate>(&Update_cmdStpVersion), static_cast<RCHExecute>(&Execute_cmdStpVersion) } },
	},
	[]() { return ComPtr<RCHBase>(new BridgePropertiesRCH(), false); }
);
