#pragma once
#include "D2DWindow.h"

class PropertyGrid : public D2DWindow
{
	using base = D2DWindow;

public:
	struct PD abstract
	{
		const wchar_t* const _name;
		PD(const wchar_t* name) : _name(name) { }
		virtual bool IsReadOnly() const = 0;
		virtual std::wstring to_wstring (const void* so) const = 0;
	};

	template<typename TValue>
	struct TypedPD : PD
	{
		using Getter = TValue(*)(const void* object);
		using Setter = void(*)(void* object, TValue newValue, const PropertyGrid* pg);
		Getter const _getter;
		Setter const _setter;

		TypedPD (const wchar_t* name, Getter getter, Setter setter)
			: PD(name), _getter(getter), _setter(setter)
		{ }

		virtual bool IsReadOnly() const override final { return _setter == nullptr; }
		virtual std::wstring to_wstring (const void* so) const override { return std::to_wstring(_getter(so)); }
	};

	struct EnumPD : TypedPD<int>
	{
		using NVP = std::pair<const wchar_t*, int>;
		const NVP* const _nameValuePairs;

		EnumPD (const wchar_t* name, Getter getter, Setter setter, const NVP* nameValuePairs);
		virtual std::wstring to_wstring (const void* so) const override;
	};

	typedef const std::vector<const PD*>& (*PropertyCollectionGetter) (const void* selectedObject);

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

template<> std::wstring PropertyGrid::TypedPD<std::wstring>::to_wstring(const void* so) const { return _getter(so); }
template<> std::wstring PropertyGrid::TypedPD<bool>::to_wstring(const void* so) const { return _getter(so) ? L"True" : L"False"; }
