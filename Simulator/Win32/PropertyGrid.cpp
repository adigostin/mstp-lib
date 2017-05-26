
#include "pch.h"
#include "PropertyGrid.h"

using namespace std;

static constexpr float CellLRPadding = 3.0f;

PGTextItem::PGTextItem (const PropertyGrid& pg, const char* label, const char* value, std::function<bool(const wchar_t*)> setter)
	: PGItem(pg), _setter(setter)
{
	wstring_convert<codecvt_utf8<wchar_t>> converter;
	_label = converter.from_bytes(label);
	_value = converter.from_bytes(value);
}

void PGTextItem::Draw (ID2D1RenderTarget* rt, _Inout_ float& y) const
{
	auto labelTL = TextLayout::Create (_pg.GetDWriteFactory(), _pg._textFormat, _label.c_str(), _pg.GetNameColumnWidth());
	auto valueTL = TextLayout::Create (_pg.GetDWriteFactory(), _pg._textFormat, _value.c_str());

	rt->DrawTextLayout ({ CellLRPadding, y }, labelTL.layout, _pg._windowTextBrush);
	rt->DrawTextLayout ({ _pg.GetNameColumnWidth() + CellLRPadding, y }, valueTL.layout, _pg._windowTextBrush);
	y += std::max (labelTL.metrics.height, valueTL.metrics.height);
}

// ============================================================================

PropertyGrid::PropertyGrid (HINSTANCE hInstance, const RECT& rect, HWND hWndParent, IDWriteFactory* dWriteFactory)
	: base (hInstance, 0, WS_CHILD | WS_VISIBLE, rect, hWndParent, nullptr, dWriteFactory)
{
	auto hr = dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
											   DWRITE_FONT_STRETCH_NORMAL, 12, L"en-US", &_textFormat); ThrowIfFailed(hr);

	GetRenderTarget()->CreateSolidColorBrush (GetD2DSystemColor(COLOR_WINDOW), &_windowBrush);
	GetRenderTarget()->CreateSolidColorBrush (GetD2DSystemColor(COLOR_WINDOWTEXT), &_windowTextBrush);
	GetRenderTarget()->CreateSolidColorBrush (GetD2DSystemColor(COLOR_GRAYTEXT), &_grayTextBrush);
}

PropertyGrid::~PropertyGrid()
{ }

void PropertyGrid::AddItem (PGItem* item)
{
	// todo cancel edit
	_items.push_back (unique_ptr<PGItem>(item));
	::InvalidateRect (GetHWnd(), nullptr, FALSE);
}

void PropertyGrid::ClearItems()
{
	// todo cancel edit
	_items.clear();
	::InvalidateRect (GetHWnd(), nullptr, FALSE);
}

std::optional<LRESULT> PropertyGrid::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	auto resultBaseClass = base::WindowProc (hwnd, msg, wParam, lParam);

	return resultBaseClass;
}

void PropertyGrid::Render (ID2D1RenderTarget* rt) const
{
	rt->Clear(GetD2DSystemColor(COLOR_WINDOW));

	if (_items.empty())
	{
		auto tl = TextLayout::Create (GetDWriteFactory(), _textFormat, _noItemText.c_str(), GetClientWidthDips());
		D2D1_POINT_2F p = { GetClientWidthDips() / 2 - tl.metrics.width / 2, GetClientHeightDips() / 2 - tl.metrics.height / 2};
		rt->DrawTextLayout (p, tl.layout, _windowTextBrush);
		return;
	}

	float pixelWidthDips = GetDipSizeFromPixelSize ({ 1, 0 }).width;
	float lineWidthDips = roundf(1.0f / pixelWidthDips) * pixelWidthDips;

	float y = 0;
	for (auto& item : _items)
	{
		item->Draw (rt, y);
		y = roundf (y / pixelWidthDips) * pixelWidthDips + lineWidthDips / 2;
		rt->DrawLine ({ 0, y }, { GetClientWidthDips(), y }, _grayTextBrush, lineWidthDips);
		y += lineWidthDips;
	}

	float x = roundf ((GetClientWidthDips() * _nameColumnSize) / pixelWidthDips) * pixelWidthDips + lineWidthDips / 2;

	rt->DrawLine ({ x, 0 }, { x, y - lineWidthDips }, _grayTextBrush, lineWidthDips);
}

void PropertyGrid::SetNoItemText (const wchar_t* text)
{
	if (_noItemText != text)
	{
		_noItemText = text;
		if (_items.empty())
			::InvalidateRect (GetHWnd(), nullptr, FALSE);
	}
}

