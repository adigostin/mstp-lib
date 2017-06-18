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

	using VSF = std::function<void(const std::wstring& newStr)>;

	struct EditorInfo
	{
		std::wstring _initialString;
		VSF          _validateAndSetFunction;
		HWND         _popupHWnd;
		HWND         _editHWnd;
		HFONT_unique_ptr _font;
		bool         _validating = false;
	};

	std::optional<EditorInfo> _editorInfo;

	struct Item
	{
		const PropertyOrGroup* pd;
		TextLayout labelTL;
		TextLayout valueTL;
	};

	std::vector<Item> _items;
	IWindowWithWorkQueue* const _iwwwq;

public:
	PropertyGrid (HINSTANCE hInstance, const RECT& rect, HWND hWndParent, IDWriteFactory* dWriteFactory, IWindowWithWorkQueue* iwwwq);

	void DiscardEditor();
	void SelectObjects (Object* const* objects, size_t count);
	void ReloadPropertyValues();

	struct PropertyChangedEvent : public Event<PropertyChangedEvent, void(const Property* property)> { };
	PropertyChangedEvent::Subscriber GetPropertyChangedEvent() { return PropertyChangedEvent::Subscriber(this); }

private:
	virtual void Render(ID2D1RenderTarget* rt) const override final;
	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override final;
	void ProcessWmSetCursor (POINT pt) const;
	const Item* GetItemAt (D2D1_POINT_2F location) const;
	void CreateLabelTextLayouts();
	void CreateValueTextLayouts();
	std::wstring GetValueText(const Property* pd) const;
	const Item* EnumItems (std::function<void(float textY, float lineY, float lineWidth, const Item& item, bool& stopEnum)> func) const;
	float GetNameColumnWidth() const;
	void ProcessLButtonUp (DWORD modifierKeys, POINT pt);
	int ShowEditor (POINT ptScreen, const NVP* nameValuePairs);
	void ShowEditor (POINT ptScreen, const wchar_t* str, VSF validateAndSetFunction);
	static LRESULT CALLBACK EditSubclassProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
};

