#pragma once
#include "Win32/D2DWindow.h"
#include "Object.h"

class PropertyGrid : public D2DWindow
{
	using base = D2DWindow;

	IDWriteTextFormatPtr _textFormat;
	IDWriteTextFormatPtr _boldTextFormat;
	IDWriteTextFormatPtr _wingdings;
	float _nameColumnSize = 0.5f;
	ID2D1SolidColorBrushPtr _windowBrush;
	ID2D1SolidColorBrushPtr _windowTextBrush;
	ID2D1SolidColorBrushPtr _grayTextBrush;
	std::vector<Object*> _selectedObjects;
	std::unique_ptr<IPropertyEditor> _customEditor;

	struct Item
	{
		PropertyGrid* const _pg;
		const PropertyOrGroup* const pd;
		TextLayout labelTL;
		TextLayout valueTL;

		Item (PropertyGrid* pg, const PropertyOrGroup* pd) : _pg(pg), pd(pd) { }

		void CreateValueTextLayout();
	};
	std::vector<std::unique_ptr<Item>> _items;

	using VSF = std::function<void(const std::wstring& newStr)>;
	struct EditorInfo
	{
		const Property* const _property;
		Item* const _item;
		std::wstring _initialString;
		VSF          _validateAndSetFunction;
		HWND         _popupHWnd;
		HWND         _editHWnd;
		HFONT_unique_ptr _font;
		bool         _validating = false;

		EditorInfo (const Property* property, Item* item)
			: _property(property), _item(item)
		{ }
	};
	std::unique_ptr<EditorInfo> _editorInfo;

	IWindowWithWorkQueue* const _iwwwq;

public:
	PropertyGrid (HINSTANCE hInstance, const RECT& rect, HWND hWndParent, IDWriteFactory* dWriteFactory, IWindowWithWorkQueue* iwwwq);
	~PropertyGrid();

	void DiscardEditor();
	void SelectObjects (Object* const* objects, size_t count);
	const std::vector<Object*>& GetSelectedObjects() const { return _selectedObjects; }
	LONG GetGridHeightPixels() const;
	void ReloadProperties();

	struct PropertyChangedByUserEvent : public Event<PropertyChangedByUserEvent, void(const Property* property)> { };
	PropertyChangedByUserEvent::Subscriber GetPropertyChangedByUserEvent() { return PropertyChangedByUserEvent::Subscriber(this); }

	struct GridHeightChangedEvent : public Event<GridHeightChangedEvent, void()> { };
	GridHeightChangedEvent::Subscriber GetGridHeightChangedEvent() { return GridHeightChangedEvent::Subscriber(this); }

private:
	virtual void Render(ID2D1RenderTarget* rt) const override final;
	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override final;
	void ProcessWmSetCursor (POINT pt);
	Item* GetItemAt (D2D1_POINT_2F location);
	void CreateLabelTextLayouts();
	std::wstring GetValueText(const Property* pd) const;
	Item* EnumItems (std::function<void(float textY, float lineY, float lineWidth, Item* item, bool& stopEnum)> func) const;
	float GetNameColumnWidth() const;
	void ProcessLButtonUp (DWORD modifierKeys, POINT pt);
	int ShowEditor (POINT ptScreen, const NVP* nameValuePairs);
	void ShowStringEditor (const Property* property, Item* item, POINT ptScreen, const wchar_t* str, VSF validateAndSetFunction);
	static LRESULT CALLBACK EditSubclassProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	static void OnSelectedObjectPropertyChanged (void* callbackArg, Object* o, const Property* property);
};

