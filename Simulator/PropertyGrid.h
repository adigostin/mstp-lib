#pragma once
#include "Win32/D2DWindow.h"
#include "Simulator.h"

class PropertyGrid : public D2DWindow
{
	using base = D2DWindow;

	ISimulatorApp* const _app;
	IProjectWindow* const _projectWindow;
	IProjectPtr const _project;
	IDWriteTextFormatPtr _textFormat;
	IDWriteTextFormatPtr _wingdings;
	float _nameColumnSize = 0.5f;
	ID2D1SolidColorBrushPtr _windowBrush;
	ID2D1SolidColorBrushPtr _windowTextBrush;
	ID2D1SolidColorBrushPtr _grayTextBrush;
	std::vector<Object*> _selectedObjects;

	struct Item
	{
		const Property* pd;
		TextLayout labelTL;
		TextLayout valueTL;
	};

	std::vector<Item> _items;

public:
	PropertyGrid (ISimulatorApp* app, IProjectWindow* projectWindow, IProject* project, const RECT& rect, HWND hWndParent, IDWriteFactory* dWriteFactory);

	void DiscardEditor();
	void SelectObjects (Object* const* objects, size_t count);
	void ReloadPropertyValues();

private:
	virtual void Render(ID2D1RenderTarget* rt) const override final;
	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override final;
	void ProcessWmSetCursor (POINT pt) const;
	const Item* GetItemAt (D2D1_POINT_2F location) const;
	void CreateLabelTextLayouts();
	void CreateValueTextLayouts();
	const Item* EnumItems (std::function<void(float textY, float lineY, float lineWidth, const Item& item, bool& stopEnum)> func) const;
	float GetNameColumnWidth() const;
	void ProcessLButtonUp (DWORD modifierKeys, POINT pt);
	int ShowEditor (POINT ptScreen, const NVP* nameValuePairs);
};

