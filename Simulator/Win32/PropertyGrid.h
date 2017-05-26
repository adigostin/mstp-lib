#pragma once
#include "D2DWindow.h"

class PropertyGrid;

class PGItem
{
public:
	const PropertyGrid& _pg;

	PGItem (const PropertyGrid& pg) : _pg(pg) { }
	virtual ~PGItem() = default;
	virtual void Draw (ID2D1RenderTarget* rt, _Inout_ float& y) const = 0;
};

class PGTextItem : public PGItem
{
	std::wstring _label;
	std::wstring _value;
	std::function<bool(const wchar_t*)> _setter;

public:
	PGTextItem (const PropertyGrid& pg, const char* label, const char* value, std::function<bool(const wchar_t*)> setter);
	virtual void Draw (ID2D1RenderTarget* rt, _Inout_ float& y) const override final;
};

// ============================================================================

class PropertyGrid : public D2DWindow
{
	using base = D2DWindow;

	friend class PGItem;
	friend class PGTextItem;

	IDWriteTextFormatPtr _textFormat;
	std::vector<std::unique_ptr<PGItem>> _items;
	float _nameColumnSize = 0.5f;
	std::wstring _noItemText;
	ID2D1SolidColorBrushPtr _windowBrush;
	ID2D1SolidColorBrushPtr _windowTextBrush;
	ID2D1SolidColorBrushPtr _grayTextBrush;

public:
	PropertyGrid (HINSTANCE hInstance, const RECT& rect, HWND hWndParent, IDWriteFactory* dWriteFactory);
	~PropertyGrid();

	void SetNoItemText (const wchar_t* text);
	void AddItem (PGItem* item);
	void ClearItems();

	float GetNameColumnWidth() const { return GetClientWidthDips() * _nameColumnSize; }
	float GetValueColumnWidth() const { return GetClientWidthDips() * ( 1 - _nameColumnSize); }

private:
	virtual void Render(ID2D1RenderTarget* rt) const override final;
	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override final;
};
