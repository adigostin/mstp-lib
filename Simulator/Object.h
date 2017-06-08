
#pragma once
#include "Win32/EventManager.h"

struct IZoomable;
class Object;

struct PropertyOrGroup
{
	using LabelGetter = std::wstring(*)(const std::vector<Object*>& objects, unsigned int vlanNumber);

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

struct Property abstract : PropertyOrGroup
{
	using base = PropertyOrGroup;
	using base::base;
	virtual bool IsReadOnly() const = 0;
	virtual std::wstring to_wstring (const Object* so, unsigned int vlanNumber) const = 0;
};

template<typename TValue>
struct TypedProperty : Property
{
	using Getter = TValue(*)(const Object* object, unsigned int vlanNumber);
	using Setter = void(*)(Object* object, TValue newValue, unsigned int vlanNumber, unsigned int timestamp);
	Getter const _getter;
	Setter const _setter;

	TypedProperty (const wchar_t* name, LabelGetter labelGetter, Getter getter, Setter setter)
		: Property(name, labelGetter), _getter(getter), _setter(setter)
	{ }

	virtual bool IsReadOnly() const override final { return _setter == nullptr; }
	virtual std::wstring to_wstring (const Object* obj, unsigned int vlanNumber) const override { return std::to_wstring(_getter(obj, vlanNumber)); }
};

template<>
std::wstring TypedProperty<std::wstring>::to_wstring(const Object* obj, unsigned int vlanNumber) const
{
	return _getter(obj, vlanNumber);
}

template<>
std::wstring TypedProperty<bool>::to_wstring(const Object* obj, unsigned int vlanNumber) const
{
	return _getter(obj, vlanNumber) ? L"True" : L"False";
}

using NVP = std::pair<const wchar_t*, int>;

struct EnumProperty : TypedProperty<int>
{
	using base = TypedProperty<int>;

	const NVP* const _nameValuePairs;

	EnumProperty (const wchar_t* name, LabelGetter labelGetter, Getter getter, Setter setter, const NVP* nameValuePairs)
		: base(name, labelGetter, getter, setter), _nameValuePairs(nameValuePairs)
	{ }

	virtual std::wstring to_wstring (const Object* obj, unsigned int vlanNumber) const override;
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

struct HTResult
{
	Object* object;
	int code;
	bool operator==(const HTResult& other) const { return (this->object == other.object) && (this->code == other.code); }
	bool operator!=(const HTResult& other) const { return (this->object != other.object) || (this->code != other.code); }
};

class Object : public EventManager
{
public:
	virtual ~Object() = default;

	struct InvalidateEvent : public Event<InvalidateEvent, void(Object*)> { };
	InvalidateEvent::Subscriber GetInvalidateEvent() { return InvalidateEvent::Subscriber(this); }

	virtual void RenderSelection (const IZoomable* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const = 0;
	virtual HTResult HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) = 0;

	template<typename T>
	bool Is() const { return dynamic_cast<const T*>(this) != nullptr; }

	virtual const PropertyOrGroup* const* GetProperties() const = 0;
};
