
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "com_ptr.h"

namespace edge
{
	class text_layout
	{
		com_ptr<IDWriteTextLayout> _layout;

	public:
		text_layout() = default;
		text_layout (IDWriteFactory* dwrite_factory, IDWriteTextFormat* format, std::string_view str, float maxWidth = 0);
		text_layout (IDWriteFactory* dwrite_factory, IDWriteTextFormat* format, std::wstring_view str, float maxWidth = 0);
		operator IDWriteTextLayout*() const { return _layout.get(); }
		IDWriteTextLayout* operator->() const { return _layout.get(); }
		void clear() { _layout = nullptr; }
		text_layout& operator= (nullptr_t) { _layout = nullptr; return *this; }
		IDWriteTextLayout* layout() const { return _layout; }
		operator bool() const { return _layout != nullptr; }
	};

	class text_layout_with_metrics : public text_layout
	{
		DWRITE_TEXT_METRICS _metrics;

	public:
		text_layout_with_metrics() = default;
		text_layout_with_metrics (IDWriteFactory* dwrite_factory, IDWriteTextFormat* format, std::string_view str, float maxWidth = 0);
		text_layout_with_metrics (IDWriteFactory* dwrite_factory, IDWriteTextFormat* format, std::wstring_view str, float maxWidth = 0);
		text_layout_with_metrics& operator= (nullptr_t np) { text_layout::operator=(np); return *this; }
		float left() const;
		float width() const;
		float height() const;
		float layout_width() const;
	};

	class text_layout_with_line_metrics : public text_layout_with_metrics
	{
		DWRITE_LINE_METRICS _line_metrics;

	public:
		text_layout_with_line_metrics() = default;
		text_layout_with_line_metrics (IDWriteFactory* dwrite_factory, IDWriteTextFormat* format, std::string_view str, float maxWidth = 0);
		text_layout_with_line_metrics (IDWriteFactory* dwrite_factory, IDWriteTextFormat* format, std::wstring_view str, float maxWidth = 0);
		text_layout_with_line_metrics& operator= (nullptr_t np) { text_layout_with_metrics::operator=(np); return *this; }
		float baseline() const { return _line_metrics.baseline; }
	};
}
