#pragma once
#include "Win32/D2DWindow.h"
#include "Simulator.h"

class PropertyGrid : public D2DWindow
{
	using base = D2DWindow;

public:
	struct PD abstract
	{
		using LabelGetter = std::wstring(*)(const PropertyGrid* pg, const std::vector<Object*>& objects);

		const wchar_t* const _name;
		LabelGetter const _labelGetter;

		PD (const wchar_t* name, LabelGetter labelGetter) : _name(name), _labelGetter(labelGetter) { }
		virtual bool IsReadOnly() const = 0;
		virtual std::wstring to_wstring (const PropertyGrid* pg, const Object* so) const = 0;
	};

	template<typename TValue>
	struct TypedPD : PD
	{
		using Getter = TValue(*)(const PropertyGrid* pg, const Object* object);
		using Setter = void(*)(const PropertyGrid* pg, Object* object, TValue newValue, unsigned int timestamp);
		Getter const _getter;
		Setter const _setter;

		TypedPD (const wchar_t* name, Getter getter, Setter setter, LabelGetter labelGetter = nullptr)
			: PD(name, labelGetter), _getter(getter), _setter(setter)
		{ }

		virtual bool IsReadOnly() const override final { return _setter == nullptr; }
		virtual std::wstring to_wstring (const PropertyGrid* pg, const Object* so) const override { return std::to_wstring(_getter(pg, so)); }
	};

	using NVP = std::pair<const wchar_t*, int>;

	struct EnumPD : TypedPD<int>
	{
		const NVP* const _nameValuePairs;

		EnumPD (const wchar_t* name, Getter getter, Setter setter, const NVP* nameValuePairs);
		virtual std::wstring to_wstring (const PropertyGrid* pg, const Object* so) const override;
	};

	typedef const PD* const* (*PropertyCollectionGetter) (const Object* selectedObject);

private:
	ISimulatorApp* const _app;
	IProjectPtr const _project;
	void* const _appContext;
	PropertyCollectionGetter const _propertyCollectionGetter;
	IDWriteTextFormatPtr _textFormat;
	IDWriteTextFormatPtr _wingdings;
	float _nameColumnSize = 0.5f;
	ID2D1SolidColorBrushPtr _windowBrush;
	ID2D1SolidColorBrushPtr _windowTextBrush;
	ID2D1SolidColorBrushPtr _grayTextBrush;
	std::vector<Object*> _selectedObjects;

	struct Item
	{
		const PD* pd;
		TextLayout labelTL;
		TextLayout valueTL;
	};

	std::vector<Item> _items;

public:
	PropertyGrid (ISimulatorApp* app, IProject* project, const RECT& rect, HWND hWndParent, IDWriteFactory* dWriteFactory, void* appContext, PropertyCollectionGetter propertyCollectionGetter);
	~PropertyGrid();

	void DiscardEditor();
	void SelectObjects (Object* const* objects, size_t count);
	void ReloadPropertyValues();
	void* GetAppContext() const { return _appContext; }

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

template<>
std::wstring PropertyGrid::TypedPD<std::wstring>::to_wstring(const PropertyGrid* pg, const Object* so) const
{
	return _getter(pg, so);
}

template<>
std::wstring PropertyGrid::TypedPD<bool>::to_wstring(const PropertyGrid* pg, const Object* so) const
{
	return _getter(pg, so) ? L"True" : L"False";
}
