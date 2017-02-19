
#include "pch.h"
#include "RCHBase.h"
#include "Ribbon/RibbonIds.h"

class CutCopyPasteRCH : public RCHBase
{
	typedef RCHBase base;

	using base::base;

	virtual HRESULT __stdcall Execute(UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties) override final
	{
		return E_NOTIMPL;
	}

	virtual HRESULT __stdcall UpdateProperty(UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue) override final
	{
		if (key == UI_PKEY_Enabled)
			return InitPropVariantFromBoolean (FALSE, newValue);

		return E_NOTIMPL;
	}

	static const RCHInfo _info;
	virtual const RCHInfo& GetInfo() const override final { return _info; }
};

const RCHInfo CutCopyPasteRCH::_info (
{ cmdCut, cmdCopy, cmdPaste },
[](const RCHDeps& deps) { return ComPtr<IUICommandHandler>(new CutCopyPasteRCH(deps), false); });
