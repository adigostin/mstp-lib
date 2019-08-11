
#pragma once
#include "object.h"
#include "win32/com_ptr.h"
#include "win32/zoomable_window.h"

struct drawing_resources
{
	edge::com_ptr<IDWriteFactory> _dWriteFactory;
	edge::com_ptr<ID2D1SolidColorBrush> _poweredFillBrush;
	edge::com_ptr<ID2D1SolidColorBrush> _unpoweredBrush;
	edge::com_ptr<ID2D1SolidColorBrush> _brushWindowText;
	edge::com_ptr<ID2D1SolidColorBrush> _brushWindow;
	edge::com_ptr<ID2D1SolidColorBrush> _brushHighlight;
	edge::com_ptr<ID2D1SolidColorBrush> _brushDiscardingPort;
	edge::com_ptr<ID2D1SolidColorBrush> _brushLearningPort;
	edge::com_ptr<ID2D1SolidColorBrush> _brushForwarding;
	edge::com_ptr<ID2D1SolidColorBrush> _brushNoForwardingWire;
	edge::com_ptr<ID2D1SolidColorBrush> _brushLoop;
	edge::com_ptr<ID2D1SolidColorBrush> _brushTempWire;
	edge::com_ptr<ID2D1StrokeStyle> _strokeStyleForwardingWire;
	edge::com_ptr<ID2D1StrokeStyle> _strokeStyleNoForwardingWire;
	edge::com_ptr<IDWriteTextFormat> _regularTextFormat;
	edge::com_ptr<IDWriteTextFormat> _smallTextFormat;
	edge::com_ptr<IDWriteTextFormat> _smallBoldTextFormat;
	edge::com_ptr<ID2D1StrokeStyle> _strokeStyleSelectionRect;
};

class renderable_object : public edge::object
{
public:
	struct ht_result
	{
		renderable_object* object;
		int code;
		bool operator==(const ht_result& other) const { return (this->object == other.object) && (this->code == other.code); }
		bool operator!=(const ht_result& other) const { return (this->object != other.object) || (this->code != other.code); }
	};

	struct invalidate_e : public edge::event<invalidate_e, renderable_object*> { };
	invalidate_e::subscriber invalidated() { return invalidate_e::subscriber(this); }

	virtual void render_selection (const edge::zoomable_i* zoomable, ID2D1RenderTarget* rt, const drawing_resources& dos) const = 0;
	virtual ht_result hit_test (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance) = 0;
	virtual D2D1_RECT_F extent() const = 0;

protected:
	template<typename tpd_>
	void set_and_invalidate (const tpd_* pd, typename tpd_::value_t& field, typename tpd_::param_t value)
	{
		this->on_property_changing(pd);
		field = value;
		this->on_property_changed(pd);
		this->event_invoker<invalidate_e>()(this);
	}
};

struct project_i;

class project_child : public renderable_object
{
	friend class project;

	project_i* _project = nullptr;

protected:
	virtual void on_added_to_project(project_i* project)
	{
		assert (_project == nullptr);
		_project = project;
	}

	virtual void on_removing_from_project(project_i* project)
	{
		assert (_project == project);
		_project = nullptr;
	}

public:
	project_i* project() const { return _project; }
};
