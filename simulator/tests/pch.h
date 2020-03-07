
#pragma once

#define NOMINMAX
#define _USE_MATH_DEFINES
#define WIN32_LEAN_AND_MEAN   // Exclude rarely-used stuff from Windows headers
#define _WIN7_PLATFORM_UPDATE // Needed by wincodec.h

#include "targetver.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iomanip>
#include <locale>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include <comdef.h>
#include <Commctrl.h>
#include <d2d1_1.h>
#include <d3d11_1.h>
#include <dwrite_1.h>
#include <intrin.h>
#include <MsXml6.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <Unknwn.h>
#include <VersionHelpers.h>
#include <wincodec.h>
#include <Windows.h>
#include <windowsx.h>
