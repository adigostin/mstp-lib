
#include "pch.h"
#include "RCHBase.h"
#include "../Ribbon/RibbonIds.h"
#include "../Bridge.h"
#include "../Port.h"

using namespace std;

class PortPropertiesRCH : public RCHBase
{
	typedef RCHBase base;
public:
	using RCHBase::RCHBase;

	void InvalidateAll()
	{
		_rf->InvalidateUICommand (cmdPortTabGroup, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ContextAvailable);
		_rf->InvalidateUICommand (cmdPortAdminEdge, UI_INVALIDATIONS_VALUE | UI_INVALIDATIONS_STATE, nullptr);
	}

	virtual void OnSelectionChanged() override final
	{
		base::OnSelectionChanged();
		InvalidateAll();
	}

	bool OnlyPortsSelected() const
	{
		return (_selection != nullptr)
			&& !_selection->GetObjects().empty()
			&& all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(),
					   [](const ComPtr<Object>& o) { return dynamic_cast<Port*>(o.Get()) != nullptr; });
	}

	template<typename T>
	optional<T> AllPortsSameValue (function<T(Port*)> getter)
	{
		if (OnlyPortsSelected())
		{
			auto val = getter(dynamic_cast<Port*>(_selection->GetObjects()[0].Get()));
			if (all_of (_selection->GetObjects().begin(), _selection->GetObjects().end(),
						[&getter,&val](const ComPtr<Object>& o) { return getter(dynamic_cast<Port*>(o.Get())) == val; }))
				return val;
		}

		return nullopt;
	}

	HRESULT Update_cmdPortTabGroup (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
	{
		if (key == UI_PKEY_ContextAvailable)
			return UIInitPropertyFromUInt32 (key, OnlyPortsSelected() ? UI_CONTEXTAVAILABILITY_ACTIVE : UI_CONTEXTAVAILABILITY_NOTAVAILABLE, newValue);

		return E_NOTIMPL;
	}

	// ========================================================================

	HRESULT Update_cmdPortAdminAutoEdge (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
	{
		if (key == UI_PKEY_BooleanValue)
		{
			for (auto& o : _selection->GetObjects())
			{
				Port* p = dynamic_cast<Port*>(o.Get());
				if (p != nullptr)
				{
					BOOL value = false;
					if (commandId == cmdPortAdminEdge)
					{
						if (p->GetBridge()->GetPortAdminEdge(p->GetPortIndex()))
							return UIInitPropertyFromBoolean (key, TRUE, newValue);
					}
					else if (commandId == cmdPortAutoEdge)
					{
						if (p->GetBridge()->GetPortAutoEdge(p->GetPortIndex()))
							return UIInitPropertyFromBoolean (key, TRUE, newValue);
					}
					else
						return E_NOTIMPL;
				}
			}

			return UIInitPropertyFromBoolean (key, FALSE, newValue);
		}

		return E_NOTIMPL;
	}

	HRESULT Execute_cmdPortAdminAutoEdge (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties)
	{
		BOOL value;
		auto hr = UIPropertyToBoolean (UI_PKEY_BooleanValue, *currentValue, &value); ThrowIfFailed(hr);
		assert (!_selection->GetObjects().empty());
		for (auto& o : _selection->GetObjects())
		{
			auto p = dynamic_cast<Port*>(o.Get());
			if (p != nullptr)
			{
				if (commandId == cmdPortAdminEdge)
					p->GetBridge()->SetPortAdminEdge (p->GetPortIndex(), value != 0);
				else if (commandId == cmdPortAutoEdge)
					p->GetBridge()->SetPortAutoEdge (p->GetPortIndex(), value != 0);
				else
					return E_NOTIMPL;
			}
		}

		return S_OK;
	}

	// ========================================================================

	static const RCHInfo _info;
	virtual const RCHInfo& GetInfo() const override final { return _info; }
};

const RCHInfo PortPropertiesRCH::_info (
{
	{ cmdPortTabGroup, { static_cast<RCHUpdate>(&Update_cmdPortTabGroup), nullptr } },
	{ cmdPortAdminEdge, { static_cast<RCHUpdate>(&Update_cmdPortAdminAutoEdge), static_cast<RCHExecute>(&Execute_cmdPortAdminAutoEdge) } },
	{ cmdPortAutoEdge, { static_cast<RCHUpdate>(&Update_cmdPortAdminAutoEdge), static_cast<RCHExecute>(&Execute_cmdPortAdminAutoEdge) } },
},
[](const RCHDeps& deps) { return ComPtr<RCHBase>(new PortPropertiesRCH(deps), false); }
);

