
#include "pch.h"
#include "UtilityFunctions.h"
#include "Win32/Win32Defs.h"

using namespace std;
using namespace D2D1;

unsigned int GetTimestampMilliseconds()
{
	SYSTEMTIME currentUtcTime;
	GetSystemTime(&currentUtcTime);

	FILETIME currentUtcFileTime;
	SystemTimeToFileTime(&currentUtcTime, &currentUtcFileTime);

	FILETIME creationTime;
	FILETIME exitTime;
	FILETIME kernelTime;
	FILETIME userTime;
	GetProcessTimes (GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime);

	uint64_t start = ((uint64_t) creationTime.dwHighDateTime << 32) | creationTime.dwLowDateTime;
	uint64_t now = ((uint64_t) currentUtcFileTime.dwHighDateTime << 32) | currentUtcFileTime.dwLowDateTime;
	uint64_t milliseconds = (now - start) / 10000;
	return (unsigned int)milliseconds;
}

ColorF GetD2DSystemColor (int sysColorIndex)
{
	DWORD brg = GetSysColor (sysColorIndex);
	DWORD rgb = ((brg & 0xff0000) >> 16) | (brg & 0xff00) | ((brg & 0xff) << 16);
	return ColorF (rgb);
}

bool HitTestLine (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance, D2D1_POINT_2F p0w, D2D1_POINT_2F p1w, float lineWidth)
{
	auto fd = zoomable->GetDLocationFromWLocation(p0w);
	auto td = zoomable->GetDLocationFromWLocation(p1w);

	float halfw = zoomable->GetDLengthFromWLength(lineWidth) / 2.0f;
	if (halfw < tolerance)
		halfw = tolerance;

	float angle = atan2(td.y - fd.y, td.x - fd.x);
	float s = sin(angle);
	float c = cos(angle);

	array<D2D1_POINT_2F, 4> vertices =
	{
		D2D1_POINT_2F { fd.x + s * halfw, fd.y - c * halfw },
		D2D1_POINT_2F { fd.x - s * halfw, fd.y + c * halfw },
		D2D1_POINT_2F { td.x - s * halfw, td.y + c * halfw },
		D2D1_POINT_2F { td.x + s * halfw, td.y - c * halfw }
	};

	return PointInPolygon (&vertices[0], 4, dLocation);
}

bool PointInRect (const D2D1_RECT_F& rect, D2D1_POINT_2F location)
{
	return (location.x >= rect.left) && (location.x < rect.right) && (location.y >= rect.top) && (location.y < rect.bottom);
}

bool PointInPolygon (const D2D1_POINT_2F* vertices, size_t vertexCount, D2D1_POINT_2F point)
{
	// Taken from http://stackoverflow.com/a/2922778/451036
	bool c = false;
	for (size_t i = 0, j = vertexCount - 1; i < vertexCount; j = i++)
	{
		if (((vertices[i].y > point.y) != (vertices[j].y > point.y)) &&
			(point.x < (vertices[j].x - vertices[i].x) * (point.y - vertices[i].y) / (vertices[j].y - vertices[i].y) + vertices[i].x))
			c = !c;
	}

	return c;
}

D2D1_RECT_F InflateRect (const D2D1_RECT_F& rect, float distance)
{
	auto result = rect;
	InflateRect (&result, distance);
	return result;
}

void InflateRect (D2D1_RECT_F* rect, float distance)
{
	rect->left -= distance;
	rect->top -= distance;
	rect->right += distance;
	rect->bottom += distance;
}

D2D1_ROUNDED_RECT InflateRoundedRect (const D2D1_ROUNDED_RECT& rr, float distance)
{
	D2D1_ROUNDED_RECT result = rr;
	InflateRoundedRect (&result, distance);
	return result;
}

void InflateRoundedRect (D2D1_ROUNDED_RECT* rr, float distance)
{
	InflateRect (&rr->rect, distance);

	rr->radiusX += distance;
	if (rr->radiusX < 0)
		rr->radiusX = 0;

	rr->radiusY += distance;
	if (rr->radiusY < 0)
		rr->radiusY = 0;
}

TextLayout TextLayout::Create (IDWriteFactory* dWriteFactory, IDWriteTextFormat* format, const wchar_t* str)
{
	IDWriteTextLayoutPtr tl;
	auto hr = dWriteFactory->CreateTextLayout(str, (UINT32) wcslen(str), format, 10000, 10000, &tl); ThrowIfFailed(hr);
	DWRITE_TEXT_METRICS metrics;
	hr = tl->GetMetrics(&metrics); ThrowIfFailed(hr);
	return TextLayout { move(tl), metrics };
}

STP_BRIDGE_ADDRESS ConvertStringToBridgeAddress (const wchar_t* str)
{
	static constexpr char FormatErrorMessage[] = u8"Invalid address format. The Bridge Address must have the format XX:XX:XX:XX:XX:XX or XXXXXXXXXXXX (6 hex bytes).";

	int offsetMultiplier;
	if (wcslen(str) == 12)
	{
		offsetMultiplier = 2;
	}
	else if (wcslen(str) == 17)
	{
		if ((str[2] != ':') || (str[5] != ':') || (str[8] != ':') || (str[11] != ':') || (str[14] != ':'))
			throw invalid_argument(FormatErrorMessage);

		offsetMultiplier = 3;
	}
	else
		throw invalid_argument(FormatErrorMessage);

	STP_BRIDGE_ADDRESS newAddress;
	for (size_t i = 0; i < 6; i++)
	{
		wchar_t ch0 = str[i * offsetMultiplier];
		wchar_t ch1 = str[i * offsetMultiplier + 1];

		if (!iswxdigit(ch0) || !iswxdigit(ch1))
			throw invalid_argument(FormatErrorMessage);

		auto hn = (ch0 <= '9') ? (ch0 - '0') : ((ch0 >= 'a') ? (ch0 - 'a' + 10) : (ch0 - 'A' + 10));
		auto ln = (ch1 <= '9') ? (ch1 - '0') : ((ch1 >= 'a') ? (ch1 - 'a' + 10) : (ch1 - 'A' + 10));
		newAddress.bytes[i] = (hn << 4) | ln;
	}

	return newAddress;
}
