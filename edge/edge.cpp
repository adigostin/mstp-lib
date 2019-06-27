
#include "edge.h"

namespace edge
{
	const char side_type_name[] = "side";

	const edge::NVP side_nvps[] =
	{
		{ "Left",   (int) side::left },
		{ "Top",    (int) side::top },
		{ "Right",  (int) side::right },
		{ "Bottom", (int) side::bottom },
		{ 0, 0 },
	};
}
