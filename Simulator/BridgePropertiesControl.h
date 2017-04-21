
#pragma once
#include "Simulator.h"

struct IBridgePropertiesControl
{
	virtual HWND GetHWnd() const = 0;
};

typedef std::unique_ptr<IBridgePropertiesControl> (*BridgePropertiesControlFactory)(HWND hwndParent, const RECT& rect, ISelection* selection);
extern const BridgePropertiesControlFactory bridgePropertiesControlFactory;
