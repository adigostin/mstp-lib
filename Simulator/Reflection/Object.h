
#pragma once
#include "Win32/EventManager.h"

struct IZoomable;
class Object;

struct PropertyOrGroup
{
	using LabelGetter = std::wstring(*)(const std::vector<Object*>& objects);

	const wchar_t* _name;
	LabelGetter _labelGetter;

	PropertyOrGroup (const wchar_t* name, LabelGetter labelGetter)
		: _name(name), _labelGetter(labelGetter)
	{ }

	virtual ~PropertyOrGroup() = default;
};

struct PropertyGroup : PropertyOrGroup
{
	using PropertyOrGroup::PropertyOrGroup;
};

struct IPropertyEditor
{
	virtual ~IPropertyEditor() { }
	virtual UINT ShowModal (HWND hWndParent) = 0; // return IDOK, IDCANCEL, -1 (some error), 0 (hWndParent invalid or closed)
	virtual void Cancel() = 0;
};

using PropertyEditorFactory = std::unique_ptr<IPropertyEditor>(*const)(const std::vector<Object*>& objects);

struct Property abstract : PropertyOrGroup
{
	using base = PropertyOrGroup;

	PropertyEditorFactory const _customEditor;

	Property (const wchar_t* name, LabelGetter labelGetter, PropertyEditorFactory customEditor = nullptr)
		: base (name, labelGetter), _customEditor(customEditor)
	{ }

	virtual bool HasSetter() const = 0;
	virtual std::wstring to_wstring (const Object* so) const = 0;
};

template<typename TValue>
struct TypedProperty : Property
{
	using Getter = TValue(Object::*)() const;
	using Setter = void(Object::*)(TValue newValue);
	Getter const _getter;
	Setter const _setter;

	TypedProperty (const wchar_t* name, LabelGetter labelGetter, Getter getter, Setter setter, PropertyEditorFactory customEditor = nullptr)
		: Property(name, labelGetter, customEditor), _getter(getter), _setter(setter)
	{ }

	virtual bool HasSetter() const override final { return _setter != nullptr; }
	virtual std::wstring to_wstring (const Object* obj) const override { return std::to_wstring((obj->*_getter)()); }
};

template<>
std::wstring TypedProperty<std::wstring>::to_wstring(const Object* obj) const
{
	return (obj->*_getter)();
}

template<>
std::wstring TypedProperty<bool>::to_wstring(const Object* obj) const
{
	return (obj->*_getter)() ? L"True" : L"False";
}

using NVP = std::pair<const wchar_t*, int>;

const wchar_t* GetEnumName (const NVP* nvps, int value);
int GetEnumValue (const NVP* nvps, const wchar_t* name);

struct EnumProperty : TypedProperty<int>
{
	using base = TypedProperty<int>;

	const NVP* const _nameValuePairs;

	EnumProperty (const wchar_t* name, LabelGetter labelGetter, Getter getter, Setter setter, const NVP* nameValuePairs)
		: base(name, labelGetter, getter, setter), _nameValuePairs(nameValuePairs)
	{ }

	virtual std::wstring to_wstring (const Object* obj) const override;
};

struct DrawingObjects
{
	IDWriteFactoryPtr _dWriteFactory;
	ID2D1SolidColorBrushPtr _poweredFillBrush;
	ID2D1SolidColorBrushPtr _unpoweredBrush;
	ID2D1SolidColorBrushPtr _brushWindowText;
	ID2D1SolidColorBrushPtr _brushWindow;
	ID2D1SolidColorBrushPtr _brushHighlight;
	ID2D1SolidColorBrushPtr _brushDiscardingPort;
	ID2D1SolidColorBrushPtr _brushLearningPort;
	ID2D1SolidColorBrushPtr _brushForwarding;
	ID2D1SolidColorBrushPtr _brushNoForwardingWire;
	ID2D1SolidColorBrushPtr _brushLoop;
	ID2D1SolidColorBrushPtr _brushTempWire;
	ID2D1StrokeStylePtr _strokeStyleForwardingWire;
	ID2D1StrokeStylePtr _strokeStyleNoForwardingWire;
	IDWriteTextFormatPtr _regularTextFormat;
	IDWriteTextFormatPtr _smallTextFormat;
	ID2D1StrokeStylePtr _strokeStyleSelectionRect;
};

class Object : public EventManager
{
public:
	virtual ~Object() = default;

	template<typename T>
	bool Is() const { return dynamic_cast<const T*>(this) != nullptr; }

	struct PropertyChangedEvent : public Event<PropertyChangedEvent, void(Object* o, const Property* property)> { };
	PropertyChangedEvent::Subscriber GetPropertyChangedEvent() { return PropertyChangedEvent::Subscriber(this); }

	virtual const PropertyOrGroup* const* GetProperties() const = 0;
};

class RenderableObject : public Object
{
public:
	struct HTResult
	{
		RenderableObject* object;
		int code;
		bool operator==(const HTResult& other) const { return (this->object == other.object) && (this->code == other.code); }
		bool operator!=(const HTResult& other) const { return (this->object != other.object) || (this->code != other.code); }
	};

	struct InvalidateEvent : public Event<InvalidateEvent, void(Object*)> { };
	InvalidateEvent::Subscriber GetInvalidateEvent() { return InvalidateEvent::Subscriber(this); }

	virtual void RenderSelection (const IZoomable* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const = 0;
	virtual HTResult HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) = 0;
};
