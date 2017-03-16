
#include "pch.h"
#include "RCHBase.h"
#include "../Ribbon/RibbonIds.h"

using namespace std;

class OptionsRCH : public RCHBase
{
	typedef RCHBase base;
public:
	using RCHBase::RCHBase;

	HRESULT Update_NewBridgePortCount (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
	{
		if (key == UI_PKEY_RepresentativeString)
			return UIInitPropertyFromString (key, L"99", newValue);

		if (key == UI_PKEY_ItemsSource)
		{
			ComPtr<IUICollection> collection;
			auto hr = UIPropertyToInterface (key, *currentValue, &collection);
			if (FAILED(hr))
				return hr;
			collection->Clear();
			for (int i = 2; i <= 16; i++)
				collection->Add (ComPtr<IUISimplePropertySet>(new ItemPropertySet(std::to_wstring(i)), false));
			return UIInitPropertyFromInterface (key, collection, newValue);
		}

		if (key == UI_PKEY_SelectedItem)
		{
			// Workaround for what seems like a framework bug: the first this command is displayed,
			// the framework doesn't request UI_PKEY_StringValue and uses an empty string instead.
			//_rf->InvalidateUICommand (cmdBridgeAddress, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_StringValue);
			return UIInitPropertyFromUInt32 (key, 0, newValue);
		}

		return E_NOTIMPL;
	}

	HRESULT Execute_NewBridgePortCount (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties)
	{
		return E_NOTIMPL;
	}

	// ========================================================================

	static const RCHInfo _info;
	virtual const RCHInfo& GetInfo() const override final { return _info; }
};

const RCHInfo OptionsRCH::_info (
{
	{ cmdNewBridgePortCount,  { static_cast<RCHUpdate>(&Update_NewBridgePortCount), static_cast<RCHExecute>(&Execute_NewBridgePortCount) } },
},
[](const RCHDeps& deps) { return ComPtr<RCHBase>(new OptionsRCH(deps), false); });
