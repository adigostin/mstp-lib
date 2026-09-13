
#pragma once
#include "pg_internal.h"

namespace pg
{
	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("28CD2BDD-0770-41F0-86CA-412000220F99") IObjectItemChildManager : IUnknown
	{
		virtual ULONG ChildCount() = 0;
		virtual IGroupItem* ChildAt(ULONG i) = 0;
		virtual edge::IObjectList* selected_objects() const = 0;
	};

	HRESULT MakeObjectItemChildManager (IObjectItem* owner, edge::IObjectList* selected_objects, IObjectItemChildManager** ppMan);
}
