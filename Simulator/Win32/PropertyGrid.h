#pragma once
#include "D2DWindow.h"

class PropertyGrid : public D2DWindow
{
	using base = D2DWindow;

public:
	struct PD abstract
	{
		using LabelGetter = std::wstring(*)(const PropertyGrid* pg);

		const wchar_t* const _name;
		LabelGetter const _labelGetter;

		PD (const wchar_t* name, LabelGetter labelGetter) : _name(name), _labelGetter(labelGetter) { }
		virtual bool IsReadOnly() const = 0;
		virtual std::wstring to_wstring (const PropertyGrid* pg, const void* so) const = 0;
	};

	template<typename TValue>
	struct TypedPD : PD
	{
		using Getter = TValue(*)(const PropertyGrid* pg, const void* object);
		using Setter = void(*)(const PropertyGrid* pg, void* object, TValue newValue);
		Getter const _getter;
		Setter const _setter;

		TypedPD (const wchar_t* name, Getter getter, Setter setter, LabelGetter labelGetter = nullptr)
			: PD(name, labelGetter), _getter(getter), _setter(setter)
		{ }

		virtual bool IsReadOnly() const override final { return _setter == nullptr; }
		virtual std::wstring to_wstring (const PropertyGrid* pg, const void* so) const override { return std::to_wstring(_getter(pg, so)); }
	};

	struct EnumPD : TypedPD<int>
	{
		using NVP = std::pair<const wchar_t*, int>;
		const NVP* const _nameValuePairs;

		EnumPD (const wchar_t* name, Getter getter, Setter setter, const NVP* nameValuePairs);
		virtual std::wstring to_wstring (const PropertyGrid* pg, const void* so) const override;
	};

	typedef const PD* const* (*PropertyCollectionGetter) (const void* selectedObject);

private:
	void* const _appContext;
	PropertyCollectionGetter const _propertyCollectionGetter;
	IDWriteTextFormatPtr _textFormat;
	IDWriteTextFormatPtr _wingdings;
	float _nameColumnSize = 0.5f;
	ID2D1SolidColorBrushPtr _windowBrush;
	ID2D1SolidColorBrushPtr _windowTextBrush;
	ID2D1SolidColorBrushPtr _grayTextBrush;
	std::vector<void*> _selectedObjects;

public:
	PropertyGrid (HINSTANCE hInstance, const RECT& rect, HWND hWndParent, IDWriteFactory* dWriteFactory, void* appContext, PropertyCollectionGetter propertyCollectionGetter);
	~PropertyGrid();

	void DiscardEditor();
	void SelectObjects (void* const* objects, size_t count);
	void ReloadPropertyValues();

	float GetNameColumnWidth() const { return GetClientWidthDips() * _nameColumnSize; }
	float GetValueColumnWidth() const { return GetClientWidthDips() * ( 1 - _nameColumnSize); }

	void* GetAppContext() const { return _appContext; }

private:
	virtual void Render(ID2D1RenderTarget* rt) const override final;
	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override final;
};

template<>
std::wstring PropertyGrid::TypedPD<std::wstring>::to_wstring(const PropertyGrid* pg, const void* so) const
{
	return _getter(pg, so);
}

template<>
std::wstring PropertyGrid::TypedPD<bool>::to_wstring(const PropertyGrid* pg, const void* so) const
{
	return _getter(pg, so) ? L"True" : L"False";
}
