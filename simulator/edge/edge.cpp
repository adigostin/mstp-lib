
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "edge_d2d.h"
#include "include/edge/PropDefs.h"

namespace edge
{
	// TODO: move to utility functions
	UINT get_modifier_keys()
	{
		UINT keys = 0;

		if (GetKeyState (VK_SHIFT) < 0)
			keys |= MK_SHIFT;

		if (GetKeyState (VK_CONTROL) < 0)
			keys |= MK_CONTROL;

		if (GetKeyState (VK_MENU) < 0)
			keys |= MK_ALT;

		return keys;
	}

	COLORREF IThemeColorProvider::color_win32 (theme_color color) const
	{
		uint32_t argb = this->argb(color);
		return ((argb & 0xFF0000) >> 16) | (argb & 0x00FF00) | ((argb & 0xFF) << 16);
	}

	D2D_COLOR_F ID2DThemeColorProvider::color_d2d (theme_color color) const
	{
		uint32_t argb = this->argb(color);
		D2D_COLOR_F res = {
			((argb >> 16) & 0xff) / 255.0f,
			((argb >> 8) & 0xff) / 255.0f,
			(argb & 0xff) / 255.0f,
			((argb >> 24) & 0xff) / 255.0f
		};
		return res;
	}

	com_ptr<ID2D1SolidColorBrush> ID2DThemeColorProvider::make_brush (ID2D1DeviceContext* dc, theme_color color) const
	{
		com_ptr<ID2D1SolidColorBrush> brush;
		auto hr = dc->CreateSolidColorBrush (color_d2d(color), &brush);
		_ASSERT(SUCCEEDED(hr));
		return brush;
	}

	com_ptr<ID2D1SolidColorBrush> ID2DThemeColorProvider::make_brush (ID2D1DeviceContext* dc, theme_color color, float opacity) const
	{
		auto b = make_brush(dc, color);
		b->SetOpacity(opacity);
		return b;
	}

	HRESULT PropertyHasDefaultValue (IDispatch* obj, DISPID prop, WORD getterFuncIndex)
	{
		HRESULT hr;

		com_ptr<ITypeInfo> ti;
		hr = obj->GetTypeInfo(0, LANG_INVARIANT, &ti); RETURN_IF_FAILED(hr);

		com_ptr<ITypeInfo2> ti2;
		hr = ti->QueryInterface(&ti2); RETURN_IF_FAILED(hr);

		wil::unique_variant defaultValueData;
		hr = ti2->GetFuncCustData (getterFuncIndex, guidPropertyDefaultValue, &defaultValueData);
		if (FAILED(hr) || defaultValueData.vt == VT_EMPTY)
			return S_FALSE;

		DISPPARAMS params = { };
		wil::unique_variant result;
		EXCEPINFO exception;
		UINT uArgErr;
		hr = ti->Invoke(obj, prop, DISPATCH_PROPERTYGET, &params, &result, &exception, &uArgErr); RETURN_IF_FAILED(hr);

		if (defaultValueData.vt != result.vt)
		{
			if (FAILED(VariantChangeTypeEx (&defaultValueData, &defaultValueData, LANG_INVARIANT, 0, result.vt)))
				return S_FALSE;
		}

		if (VariantCompare(result, defaultValueData))
			return S_FALSE;

		return S_OK;
	}

	HRESULT CreateTextLayoutWithMetrics (IDWriteFactory* dwf, IDWriteTextFormat* format,
		const wchar_t* text, int textLen, float maxWidth, TextLayoutWithMetrics& tl)
	{
		wil::com_ptr_nothrow<IDWriteTextLayout> layout;
		UINT len = (UINT)(textLen >= 0 ? textLen : wcslen(text));
		float width = (maxWidth > 0) ? maxWidth : 100'000;
		auto hr = dwf->CreateTextLayout(text, len, format, width, 100'000, &layout); RETURN_IF_FAILED(hr);
		DWRITE_TEXT_METRICS metrics;
		hr = layout->GetMetrics(&metrics); RETURN_IF_FAILED(hr);
		tl.layout = std::move(layout);
		tl.metrics = metrics;
		return S_OK;
	}
}
