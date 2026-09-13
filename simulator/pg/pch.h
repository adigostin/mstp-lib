
#ifndef PCH_H
#define PCH_H

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _COM_NO_STANDARD_GUIDS_
#define _HAS_EXCEPTIONS 0 // Visual C++ headers look at this
#undef __EXCEPTIONS // WIL and Intellisense look at this

// C/C++
#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <typeindex>
#include <vector>

// Windows
//#include <comdef.h>
#include <Windows.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include <OCIdl.h>
#include <shtypes.h>
#include <propvarutil.h>
#include <Uxtheme.h>
#undef DrawText

// WIL
#include <wil/com.h>

#endif
