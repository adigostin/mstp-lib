#pragma once
#include "Win32/D2DWindow.h"
#include "Object.h"

class PropertyGrid : public D2DWindow
{
	using base = D2DWindow;

	com_ptr<IDWriteTextFormat> _textFormat;
	com_ptr<IDWriteTextFormat> _boldTextFormat;
	com_ptr<IDWriteTextFormat> _wingdings;
	com_ptr<IDWriteTextFormat> _headingTextFormat;
	com_ptr<ID2D1SolidColorBrush> _windowBrush;
	com_ptr<ID2D1SolidColorBrush> _windowTextBrush;
	com_ptr<ID2D1SolidColorBrush> _grayTextBrush;
	float _nameColumnSize = 0.5f;
	std::unique_ptr<IPropertyEditor> _customEditor;

	class Item abstract
	{
	public:
		PropertyGrid* const _pg;

		Item (PropertyGrid* pg) : _pg(pg) { }
		virtual ~Item() { }

		virtual void RecreateTextLayouts (float nameColumnWidth) = 0;
		virtual float GetHeight() const = 0;
		virtual void Render (ID2D1RenderTarget* rt, float lineY, float textY, float nameColumnWidth, float lineWidth) const = 0;
	};

	class HeadingItem;
	class GroupItem;
	class PropertyItem;

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

	IWindowWithWorkQueue* const _iwwwq; // TODO: delete this and implement the helper window here

public:
	PropertyGrid (HINSTANCE hInstance, const RECT& rect, HWND hWndParent, IDWriteFactory* dWriteFactory, IWindowWithWorkQueue* iwwwq);
	~PropertyGrid();

	void AddProperties (Object* const* objects, size_t count, const wchar_t* heading);
	void ClearProperties();

	struct PropertyChangedByUserEvent : public Event<PropertyChangedByUserEvent, void(const Property* property)> { };
	PropertyChangedByUserEvent::Subscriber GetPropertyChangedByUserEvent() { return PropertyChangedByUserEvent::Subscriber(this); }

private:
	void DiscardEditor();
	virtual void Render(ID2D1RenderTarget* rt) const override final;
	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override final;
	void ProcessWmSetCursor (POINT pt);
	Item* GetItemAt (D2D1_POINT_2F location);
	Item* EnumItems (std::function<void(float textY, float lineY, float lineWidth, Item* item, bool& stopEnum)> func) const;
	float GetNameColumnWidth() const;
	void ProcessLButtonUp (DWORD modifierKeys, POINT pt);
	int ShowEditor (POINT ptScreen, const NVP* nameValuePairs);
	void ShowStringEditor (const Property* property, Item* item, POINT ptScreen, const wchar_t* str, VSF validateAndSetFunction);
	static LRESULT CALLBACK EditSubclassProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	//static void OnSelectedObjectPropertyChanged (void* callbackArg, Object* o, const Property* property);
};

