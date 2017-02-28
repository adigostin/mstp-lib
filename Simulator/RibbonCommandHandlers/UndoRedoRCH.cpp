
#include "pch.h"
#include "RCHBase.h"
#include "Ribbon/RibbonIds.h"

class UndoRedoRCH : public RCHBase
{
	typedef RCHBase base;

	HRESULT Update_cmdUndo (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
	{
		if (key == UI_PKEY_Enabled)
			return InitPropVariantFromBoolean (FALSE, newValue);

		return E_NOTIMPL;
	}

	HRESULT Update_cmdRedo (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
	{
		if (key == UI_PKEY_Enabled)
			return InitPropVariantFromBoolean (FALSE, newValue);

		return E_NOTIMPL;
	}

	static const RCHInfo _info;
	virtual const RCHInfo& GetInfo() const override final { return _info; }
};

const RCHInfo UndoRedoRCH::_info (
	{
		{ cmdUndo,  { static_cast<RCHUpdate>(&Update_cmdUndo), nullptr } },
		{ cmdRedo,  { static_cast<RCHUpdate>(&Update_cmdRedo), nullptr } },
	},
	[] { return ComPtr<RCHBase>(new UndoRedoRCH(), false); }
);
