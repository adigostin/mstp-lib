
#include "pch.h"
#include "RCHBase.h"
#include "../Ribbon/RibbonIds.h"
#include "../Bridge.h"

using namespace std;

class VlanRCH : public RCHBase
{
	typedef RCHBase base;
public:
	using RCHBase::RCHBase;

	HRESULT Update_cmdSelectedVlan (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
	{
		if (key == UI_PKEY_RepresentativeString)
			return UIInitPropertyFromString (key, L"9999", newValue);

		if (key == UI_PKEY_ItemsSource)
		{
			ComPtr<IUICollection> collection;
			auto hr = UIPropertyToInterface (key, *currentValue, &collection);
			if (FAILED(hr))
				return hr;
			collection->Clear();
			for (int i = 1; i <= 8; i++)
				collection->Add (ItemPropertySet::Make (to_wstring(i)));
			return S_OK;
		}

		if (key == UI_PKEY_SelectedItem)
		{
			if ((_pw != nullptr) && (_pw->GetSelectedVlanNumber() <= 8))
				return UIInitPropertyFromUInt32(key, _pw->GetSelectedVlanNumber() - 1, newValue);

			return UIInitPropertyFromUInt32(key, -1, newValue);
		}

		if (key == UI_PKEY_StringValue)
			return UIInitPropertyFromString (key, (_pw != nullptr) ? to_wstring(_pw->GetSelectedVlanNumber()).c_str() : L"", newValue);


		return E_NOTIMPL;
	}

	HRESULT Execute_cmdSelectedVlan (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties)
	{
		return E_NOTIMPL;
	}

	static const RCHInfo _info;
	virtual const RCHInfo& GetInfo() const override final { return _info; }
};

const RCHInfo VlanRCH::_info (
{
	{ cmdSelectedVlan,     { static_cast<RCHUpdate>(&Update_cmdSelectedVlan), static_cast<RCHExecute>(&Execute_cmdSelectedVlan) } },
},
[](const RCHDeps& deps) { return ComPtr<RCHBase>(new VlanRCH(deps), false); });
