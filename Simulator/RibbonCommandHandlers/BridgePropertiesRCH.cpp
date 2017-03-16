
#include "pch.h"
#include "RCHBase.h"
#include "../Ribbon/RibbonIds.h"
#include "../Bridge.h"

using namespace std;

class BridgePropertiesRCH : public RCHBase
{
	typedef RCHBase base;
public:
	using RCHBase::RCHBase;

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

	template<typename T>
	optional<T> AllBridgesSameValue (function<T(Bridge*)> getter)
	{
		if (BridgesSelected())
		{
			auto val = getter(dynamic_cast<Bridge*>(_selection->GetObjects()[0].Get()));
			if (all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(),
						[&getter,&val](const ComPtr<Object>& o) { return getter(dynamic_cast<Bridge*>(o.Get())) == val; }))
				return val;
		}

		return nullopt;
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
				b->EnableStp(GetTimestampMilliseconds());
			else
				b->DisableStp(GetTimestampMilliseconds());
		}

		return S_OK;
	}

	// ========================================================================

	HRESULT Update_cmdStpVersion (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
	{
		if ((key == UI_PKEY_Enabled) || (key == UI_PKEY_StringValue) || (key == UI_PKEY_SelectedItem))
		{
			optional<bool>        stpEnabled = AllBridgesSameValue<bool>       ([](Bridge* b) { return b->IsStpEnabled(); });
			optional<STP_VERSION> stpVersion = AllBridgesSameValue<STP_VERSION>([](Bridge* b) { return b->GetStpVersion(); });

			if (key == UI_PKEY_Enabled)
				// enable the checkbox if all bridges have stp disabled and all have same stp version.
				return UIInitPropertyFromBoolean (key, stpEnabled.has_value() && !stpEnabled.value() && stpVersion.has_value(), newValue);

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
			collection->Add (ItemPropertySet::Make (converter.from_bytes(STP_GetVersionString(STP_VERSION_LEGACY_STP))));
			collection->Add (ItemPropertySet::Make (converter.from_bytes(STP_GetVersionString(STP_VERSION_RSTP))));
			collection->Add (ItemPropertySet::Make (converter.from_bytes(STP_GetVersionString(STP_VERSION_MSTP))));
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
			return UIInitPropertyFromString (key, L"AA:AA:AA:AA:AA:AA", newValue);

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

	HRESULT Update_cmdPortTreeCount (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
	{
		if (key == UI_PKEY_RepresentativeString)
			return UIInitPropertyFromString (key, L"99", newValue);

		if ((key == UI_PKEY_Enabled) || (key == UI_PKEY_StringValue) || (key == UI_PKEY_SelectedItem))
		{
			optional<bool> stpEnabled = AllBridgesSameValue<bool>       ([](Bridge* b) { return b->IsStpEnabled(); });
			optional<size_t> count;
			if (commandId == cmdPortCount)
				count = AllBridgesSameValue<size_t>([](Bridge* b) { return b->GetPorts().size(); });
			else
				count = AllBridgesSameValue<size_t>([](Bridge* b) { return b->GetTreeCount(); });

			if (key == UI_PKEY_Enabled)
				return UIInitPropertyFromBoolean (key, stpEnabled.has_value() && !stpEnabled.value() && count.has_value() ? TRUE : FALSE, newValue);

			if (key == UI_PKEY_StringValue)
				return UIInitPropertyFromString (key, count ? std::to_wstring(count.value()).c_str() : L"", newValue);

			if (key == UI_PKEY_SelectedItem)
				return UIInitPropertyFromUInt32 (key, count ? (count.value() - 1) : -1, newValue);

			return E_NOTIMPL;
		}

		if (key == UI_PKEY_ItemsSource)
		{
			ComPtr<IUICollection> collection;
			auto hr = UIPropertyToInterface (key, *currentValue, &collection);
			if (FAILED(hr))
				return hr;
			collection->Clear();
			for (size_t i = 1; i <= 64; i++)
				collection->Add (ItemPropertySet::Make(std::to_wstring(i)));
			return S_OK;
		}

		return E_NOTIMPL;
	}

	HRESULT Execute_cmdPortCount (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties)
	{
		return E_NOTIMPL;
	}

	HRESULT Execute_cmdTreeCount (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties)
	{
		if (verb != UI_EXECUTIONVERB_EXECUTE)
			return E_NOTIMPL;

		PROPVARIANT pv = {};
		auto hr = _rf->GetUICommandProperty(cmdTreeCount, UI_PKEY_StringValue, &pv); ThrowIfFailed(hr);
		assert ((pv.vt == VT_BSTR) && (pv.bstrVal != nullptr));
		wstring str (pv.bstrVal, SysStringLen(pv.bstrVal));
		PropVariantClear(&pv);
		auto newTreeCount = std::stoul(str);

		auto newStr = to_wstring(newTreeCount);
		if (str != newStr)
		{
			hr = UIInitPropertyFromString (UI_PKEY_StringValue, newStr.c_str(), &pv); ThrowIfFailed(hr);
			hr = _rf->SetUICommandProperty(cmdTreeCount, UI_PKEY_StringValue, pv);
			PropVariantClear(&pv);
			ThrowIfFailed(hr);
		}

		for (auto& o : _selection->GetObjects())
			dynamic_cast<Bridge*>(o.Get())->SetStpTreeCount(newTreeCount);

		return S_OK;
	}

	// ========================================================================

	HRESULT Update_cmdBridgeTabGroup (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
	{
		if (key == UI_PKEY_ContextAvailable)
			return UIInitPropertyFromUInt32 (key, BridgesSelected() ? UI_CONTEXTAVAILABILITY_ACTIVE : UI_CONTEXTAVAILABILITY_NOTAVAILABLE, newValue);

		return E_NOTIMPL;
	}

	HRESULT Update_cmdBridgePropertiesGroup (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
	{
		if (key == UI_PKEY_Label)
		{
			const wchar_t* text = L"Properties";
			if (BridgesSelected())
			{
				bool anyStpEnabled = any_of(_selection->GetObjects().begin(), _selection->GetObjects().end(),
											[](const ComPtr<Object>& o) { return dynamic_cast<Bridge*>(o.Get())->IsStpEnabled(); });
				if (anyStpEnabled)
					text = L"Properties (Disable STP to change)";
			}

			return UIInitPropertyFromString(key, text, newValue);
		}

		return E_NOTIMPL;
	}
	
	// ========================================================================

	void InvalidateAll()
	{
		_rf->InvalidateUICommand (cmdBridgeAddress, UI_INVALIDATIONS_VALUE | UI_INVALIDATIONS_STATE, nullptr);
		_rf->InvalidateUICommand (cmdStpEnabled, UI_INVALIDATIONS_VALUE | UI_INVALIDATIONS_STATE, nullptr);
		_rf->InvalidateUICommand (cmdStpVersion, UI_INVALIDATIONS_VALUE | UI_INVALIDATIONS_STATE | UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
		_rf->InvalidateUICommand (cmdEnableSTP, UI_INVALIDATIONS_STATE, nullptr);
		_rf->InvalidateUICommand (cmdDisableSTP, UI_INVALIDATIONS_STATE, nullptr);
		_rf->InvalidateUICommand (cmdBridgeTabGroup, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ContextAvailable);
		_rf->InvalidateUICommand (cmdPortCount, UI_INVALIDATIONS_VALUE | UI_INVALIDATIONS_STATE | UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
		_rf->InvalidateUICommand (cmdTreeCount, UI_INVALIDATIONS_VALUE | UI_INVALIDATIONS_STATE | UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
		_rf->InvalidateUICommand (cmdBridgePropertiesGroup, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Label);
	}

	virtual void OnSelectionChanged() override final
	{
		base::OnSelectionChanged();
		InvalidateAll();
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
		static_cast<BridgePropertiesRCH*>(callbackArg)->InvalidateAll();
	}

	static void OnStpVersionChanged (void* callbackArg, Bridge* b)
	{
		static_cast<BridgePropertiesRCH*>(callbackArg)->InvalidateAll();
	}

	static const RCHInfo _info;
	virtual const RCHInfo& GetInfo() const override final { return _info; }
};

const RCHInfo BridgePropertiesRCH::_info (
	{
		{ cmdBridgeAddress,  { static_cast<RCHUpdate>(&Update_cmdBridgeAddress), static_cast<RCHExecute>(&Execute_cmdBridgeAddress) } },
		{ cmdBridgeTabGroup, { static_cast<RCHUpdate>(&Update_cmdBridgeTabGroup), nullptr } },
		{ cmdBridgePropertiesGroup, { static_cast<RCHUpdate>(&Update_cmdBridgePropertiesGroup), nullptr } },
		{ cmdEnableSTP,      { static_cast<RCHUpdate>(&Update_cmdEnableDisableSTP), static_cast<RCHExecute>(&Execute_cmdEnableDisableSTP) } },
		{ cmdDisableSTP,     { static_cast<RCHUpdate>(&Update_cmdEnableDisableSTP), static_cast<RCHExecute>(&Execute_cmdEnableDisableSTP) } },
		{ cmdStpEnabled,     { static_cast<RCHUpdate>(&Update_cmdStpEnabled), static_cast<RCHExecute>(&Execute_cmdStpEnabled) } },
		{ cmdStpVersion,     { static_cast<RCHUpdate>(&Update_cmdStpVersion), static_cast<RCHExecute>(&Execute_cmdStpVersion) } },
		{ cmdPortCount,      { static_cast<RCHUpdate>(&Update_cmdPortTreeCount), static_cast<RCHExecute>(&Execute_cmdPortCount) } },
		{ cmdTreeCount,      { static_cast<RCHUpdate>(&Update_cmdPortTreeCount), static_cast<RCHExecute>(&Execute_cmdTreeCount) } },
	},
	[](const RCHDeps& deps) { return ComPtr<RCHBase>(new BridgePropertiesRCH(deps), false); }
);
