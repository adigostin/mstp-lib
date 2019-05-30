
#pragma once
#include "CppUnitTest.h"
#include "stp.h"
#include "port.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Microsoft::VisualStudio::CppUnitTestFramework
{
	template<>
	static std::wstring ToString(port* p)
	{
		return L"port";
	}

	template<>
	static std::wstring ToString (const STP_PORT_ROLE& role)
	{
		std::string_view str = STP_GetPortRoleString(role);
		return std::wstring (str.begin(), str.end());
	}
}

