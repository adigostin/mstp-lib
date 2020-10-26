
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "text_layout.h"
#include "rassert.h"

namespace edge
{
	text_layout::text_layout (IDWriteFactory* dwrite_factory, IDWriteTextFormat* format, std::string_view str, float maxWidth)
	{
		rassert (maxWidth >= 0);
		if (maxWidth == 0)
			maxWidth = 100'000;

		HRESULT hr;
		if (str.empty())
		{
			hr = dwrite_factory->CreateTextLayout (L"", 0, format, maxWidth, 100'000, &_layout);
			rassert(SUCCEEDED(hr));
		}
		else
		{
			int utf16_char_count = MultiByteToWideChar (CP_UTF8, 0, str.data(), (int)str.length(), nullptr, 0);
			rassert(utf16_char_count > 0);

			auto buffer = std::make_unique<wchar_t[]>(utf16_char_count);
			MultiByteToWideChar (CP_UTF8, 0, str.data(), (int)str.length(), buffer.get(), utf16_char_count);

			hr = dwrite_factory->CreateTextLayout (buffer.get(), (UINT32) utf16_char_count, format, std::min(maxWidth, 100'000.0f), 100'000, &_layout);
			rassert(SUCCEEDED(hr));
		}
	}

	text_layout::text_layout (IDWriteFactory* dwrite_factory, IDWriteTextFormat* format, std::wstring_view str, float maxWidth)
	{
		rassert (maxWidth >= 0);
		if (maxWidth == 0)
			maxWidth = 100'000;

		HRESULT hr;
		if (str.empty())
		{
			hr = dwrite_factory->CreateTextLayout (L"", 0, format, maxWidth, 100'000, &_layout);
			rassert(SUCCEEDED(hr));
		}
		else
		{
			hr = dwrite_factory->CreateTextLayout (str.data(), (UINT32) str.size(), format, std::min(maxWidth, 100'000.0f), 100'000, &_layout);
			rassert(SUCCEEDED(hr));
		}
	}

	text_layout_with_metrics::text_layout_with_metrics (IDWriteFactory* dwrite_factory, IDWriteTextFormat* format, std::string_view str, float maxWidth)
		: text_layout (dwrite_factory, format, str, maxWidth)
	{
		auto hr = this->operator->()->GetMetrics(&_metrics); rassert(SUCCEEDED(hr));
	}

	text_layout_with_metrics::text_layout_with_metrics (IDWriteFactory* dwrite_factory, IDWriteTextFormat* format, std::wstring_view str, float maxWidth)
		: text_layout (dwrite_factory, format, str, maxWidth)
	{
		auto hr = this->operator->()->GetMetrics(&_metrics); rassert(SUCCEEDED(hr));
	}

	float text_layout_with_metrics::left() const
	{
		rassert (layout());
		return _metrics.left;
	}

	float text_layout_with_metrics::width() const
	{
		rassert(layout());
		return _metrics.widthIncludingTrailingWhitespace;
	}

	float text_layout_with_metrics::height() const
	{
		rassert(layout());
		return _metrics.height;
	}

	float text_layout_with_metrics::layout_width() const
	{
		rassert(layout());
		return _metrics.layoutWidth;
	}

	text_layout_with_line_metrics::text_layout_with_line_metrics (IDWriteFactory* dwrite_factory, IDWriteTextFormat* format, std::string_view str, float maxWidth)
		: text_layout_with_metrics (dwrite_factory, format, str, maxWidth)
	{
		UINT32 actual_line_count;
		auto hr = this->operator->()->GetLineMetrics (&_line_metrics, 1, &actual_line_count); rassert(SUCCEEDED(hr));
	}

	text_layout_with_line_metrics::text_layout_with_line_metrics (IDWriteFactory* dwrite_factory, IDWriteTextFormat* format, std::wstring_view str, float maxWidth)
		: text_layout_with_metrics (dwrite_factory, format, str, maxWidth)
	{
		UINT32 actual_line_count;
		auto hr = this->operator->()->GetLineMetrics (&_line_metrics, 1, &actual_line_count); rassert(SUCCEEDED(hr));
	}
}
