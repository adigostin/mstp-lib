
#include "pch.h"
#include "RCHBase.h"
#include "../Ribbon/RibbonIds.h"

using namespace std;

class ItemPropertySet : public IUISimplePropertySet
{
	volatile ULONG _refCount = 1;
	wstring _text;

public:
	ItemPropertySet (wstring&& text)
		: _text(move(text))
	{ }

	// IUnknown
	virtual ULONG STDMETHODCALLTYPE AddRef() override final
	{
		return InterlockedIncrement (&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release() override final
	{
		auto newRefCount = InterlockedDecrement(&_refCount);
		if (newRefCount == 0)
			delete this;
		return newRefCount;
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppv) override
	{
		if (!ppv)
			return E_POINTER;

		if (iid == __uuidof(IUnknown))
		{
			*ppv = static_cast<IUnknown*>(this);
			AddRef();
			return S_OK;
		}

		if (iid == __uuidof(IUISimplePropertySet))
		{
			*ppv = static_cast<IUISimplePropertySet*>(this);
			AddRef();
			return S_OK;
		}

		*ppv = NULL;
		return E_NOINTERFACE;
	}

	// IUISimplePropertySet
	virtual HRESULT STDMETHODCALLTYPE GetValue (REFPROPERTYKEY key, PROPVARIANT *ppropvar) override
	{
		/*
		if (key == UI_PKEY_Enabled)
			return UIInitPropertyFromBoolean (UI_PKEY_Enabled, _enabled, ppropvar);

		if (key == UI_PKEY_ItemImage)
		{
			if (_image)
				return UIInitPropertyFromImage (UI_PKEY_ItemImage, _image, ppropvar);

			return S_FALSE;
		}
		*/
		if (key == UI_PKEY_Label)
			return UIInitPropertyFromString (UI_PKEY_Label, _text.c_str(), ppropvar);

		//if (key == UI_PKEY_CategoryId)
		//	return UIInitPropertyFromUInt32 (UI_PKEY_CategoryId, _categoryId, ppropvar);

		return E_NOTIMPL;
	}
};

class OptionsRCH : public RCHBase
{
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
[]() { return ComPtr<RCHBase>(new OptionsRCH(), false); });
