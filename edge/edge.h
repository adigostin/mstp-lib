
#pragma once
#include "reflection.h"

namespace edge
{
	enum class side { left, top, right, bottom };
	extern const char side_type_name[];
	extern const edge::NVP side_nvps[];
	using side_p = edge::enum_property<enum side, side_type_name, side_nvps>;

}
