
#pragma once

#define NOMINMAX
#define _USE_MATH_DEFINES
#define WIN32_LEAN_AND_MEAN   // Exclude rarely-used stuff from Windows headers
#define _WIN7_PLATFORM_UPDATE // Needed by wincodec.h

#include "targetver.h"

// C/C++
#include <array>
#include <cstdint>
#include <cmath>
#include <exception>
#include <functional>
#include <intrin.h>
#include <iomanip>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <variant>

#include <comdef.h>
#include <CommCtrl.h>
#include <d2d1_1helper.h>
#include <d3d11_1.h>
#include <dwrite.h>
#include <MsXml6.h>
#include <ShlObj_core.h>
#include <Shlwapi.h>
#include <ShObjIdl_core.h>
#include <Unknwn.h>
#include <VersionHelpers.h>
#include <wincodec.h>
#include <Windows.h>
#include <windowsx.h>

#include "..\assert.h"

#define TCB_SPAN_NAMESPACE_NAME std
#define TCB_SPAN_NO_CONTRACT_CHECKING
#include "../tcb/span.hpp"
