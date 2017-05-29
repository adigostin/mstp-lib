
#include "pch.h"
#include "PropertyGrid.h"

using namespace std;

static constexpr float CellLRPadding = 3.0f;

PropertyGrid::PropertyGrid (HINSTANCE hInstance, const RECT& rect, HWND hWndParent, IDWriteFactory* dWriteFactory, void* appContext, PropertyCollectionGetter propertyCollectionGetter)
	: base (hInstance, 0, WS_CHILD | WS_VISIBLE, rect, hWndParent, nullptr, dWriteFactory)
	, _appContext(appContext)
	, _propertyCollectionGetter(propertyCollectionGetter)
{
	auto hr = dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
											   DWRITE_FONT_STRETCH_NORMAL, 12, L"en-US", &_textFormat); ThrowIfFailed(hr);

	hr = dWriteFactory->CreateTextFormat (L"Wingdings", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
										  DWRITE_FONT_STRETCH_NORMAL, 14, L"en-US", &_wingdings); ThrowIfFailed(hr);

	GetRenderTarget()->CreateSolidColorBrush (GetD2DSystemColor(COLOR_WINDOW), &_windowBrush);
	GetRenderTarget()->CreateSolidColorBrush (GetD2DSystemColor(COLOR_WINDOWTEXT), &_windowTextBrush);
	GetRenderTarget()->CreateSolidColorBrush (GetD2DSystemColor(COLOR_GRAYTEXT), &_grayTextBrush);
}

PropertyGrid::~PropertyGrid()
{ }

PropertyGrid::EnumPD::EnumPD (const wchar_t* name, Getter getter, Setter setter, const NVP* nameValuePairs)
	: TypedPD(name, getter, setter), _nameValuePairs(nameValuePairs)
{ }

std::wstring PropertyGrid::EnumPD::to_wstring (const PropertyGrid* pg, const void* so) const
{
	auto value = _getter(pg, so);
	for (auto nvp = _nameValuePairs; nvp->first != nullptr; nvp++)
	{
		if (nvp->second == value)
			return nvp->first;
	}

	return L"??";
}

std::optional<LRESULT> PropertyGrid::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	auto resultBaseClass = base::WindowProc (hwnd, msg, wParam, lParam);

	return resultBaseClass;
}

void PropertyGrid::Render (ID2D1RenderTarget* rt) const
{
	rt->Clear(GetD2DSystemColor(COLOR_WINDOW));

	if (_selectedObjects.empty())
	{
		auto tl = TextLayout::Create (GetDWriteFactory(), _textFormat, L"(no selection)", GetClientWidthDips());
		D2D1_POINT_2F p = { GetClientWidthDips() / 2 - tl.metrics.width / 2, GetClientHeightDips() / 2 - tl.metrics.height / 2};
		rt->DrawTextLayout (p, tl.layout, _windowTextBrush);
		return;
	}

	float pixelWidthDips = GetDipSizeFromPixelSize ({ 1, 0 }).width;
	float lineWidthDips = roundf(1.0f / pixelWidthDips) * pixelWidthDips;
	float y = 0;
	for (auto ppd = _propertyCollectionGetter(_selectedObjects[0]); *ppd != nullptr; ppd++)
	{
		auto pd = *ppd;
		TextLayout labelTL;
		if (pd->_labelGetter != nullptr)
			labelTL = TextLayout::Create (GetDWriteFactory(), _textFormat, pd->_labelGetter(this).c_str(), GetNameColumnWidth());
		else
			labelTL = TextLayout::Create (GetDWriteFactory(), _textFormat, pd->_name, GetNameColumnWidth());
		rt->DrawTextLayout ({ CellLRPadding, y }, labelTL.layout, pd->IsReadOnly() ? _grayTextBrush : _windowTextBrush);

		auto str = pd->to_wstring(this, _selectedObjects[0]);
		auto valueTL = TextLayout::Create (GetDWriteFactory(), _textFormat, str.c_str());
		rt->DrawTextLayout ({ GetNameColumnWidth() + CellLRPadding, y }, valueTL.layout, pd->IsReadOnly() ? _grayTextBrush : _windowTextBrush);

		y += std::max (labelTL.metrics.height, valueTL.metrics.height);

		y = roundf (y / pixelWidthDips) * pixelWidthDips + lineWidthDips / 2;
		rt->DrawLine ({ 0, y }, { GetClientWidthDips(), y }, _grayTextBrush, lineWidthDips);
		y += lineWidthDips;
	}

	float x = roundf ((GetClientWidthDips() * _nameColumnSize) / pixelWidthDips) * pixelWidthDips + lineWidthDips / 2;

	rt->DrawLine ({ x, 0 }, { x, y - lineWidthDips }, _grayTextBrush, lineWidthDips);
}

void PropertyGrid::DiscardEditor()
{
}

void PropertyGrid::SelectObjects (void* const* objects, size_t count)
{
	bool sameSelection = false;
	if (_selectedObjects.size() == count)
	{
		sameSelection = true;
		for (size_t i = 0; i < count; i++)
		{
			if (_selectedObjects[i] != objects[i])
			{
				sameSelection = false;
				break;
			}
		}
	}

	if (sameSelection)
		return;

	DiscardEditor();

	_selectedObjects.assign (objects, objects + count);
	::InvalidateRect (GetHWnd(), NULL, FALSE);
}

void PropertyGrid::ReloadPropertyValues()
{
	DiscardEditor();

	//throw not_implemented_exception();
	::InvalidateRect (GetHWnd(), NULL, FALSE);
}
