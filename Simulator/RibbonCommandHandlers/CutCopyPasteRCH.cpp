
#include "pch.h"
#include "RCHBase.h"
#include "Ribbon/RibbonIds.h"

class CutCopyPasteRCH : public RCHBase
{
	typedef RCHBase base;
public:
	using RCHBase::RCHBase;

	HRESULT Update_cmdCutCopy (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
	{
		if (key == UI_PKEY_Enabled)
			return InitPropVariantFromBoolean (FALSE, newValue);

		return E_NOTIMPL;
	}

	HRESULT Update_cmdPaste (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
	{
		if (key == UI_PKEY_Enabled)
			return InitPropVariantFromBoolean (FALSE, newValue);

		return E_NOTIMPL;
	}

	static const RCHInfo _info;
	virtual const RCHInfo& GetInfo() const override final { return _info; }
};

const RCHInfo CutCopyPasteRCH::_info (
	{
		{ cmdCut,   { static_cast<RCHUpdate>(&Update_cmdCutCopy), nullptr } },
		{ cmdCopy,  { static_cast<RCHUpdate>(&Update_cmdCutCopy), nullptr } },
		{ cmdPaste, { static_cast<RCHUpdate>(&Update_cmdPaste), nullptr } },
	},
	[](const RCHDeps& deps) { return ComPtr<RCHBase>(new CutCopyPasteRCH(deps), false); }
);
