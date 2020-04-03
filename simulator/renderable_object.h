
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "collection.h"
#include "win32/com_ptr.h"
#include "win32/zoomable_window.h"
#include "win32/property_grid.h"

using edge::size_t_p;
using edge::size_t_property_traits;
using edge::uint32_p;
using edge::bool_p;
using edge::float_p;
using edge::backed_string_p;
using edge::temp_string_p;
using edge::side_p;
using edge::property;
using edge::object;
using edge::type;
using edge::concrete_type;
using edge::xtype;
using edge::value_property;
using edge::typed_object_collection_property;
using edge::property_change_args;
using edge::side;
using edge::binary_reader;
using edge::out_stream_i;
using edge::nvp;
using edge::static_value_property;
using edge::com_ptr;
using edge::pg_hidden;
using edge::prop_wrapper;

struct drawing_resources
{
	com_ptr<IDWriteFactory> _dWriteFactory;
	com_ptr<ID2D1SolidColorBrush> _poweredFillBrush;
	com_ptr<ID2D1SolidColorBrush> _unpoweredBrush;
	com_ptr<ID2D1SolidColorBrush> _brushWindowText;
	com_ptr<ID2D1SolidColorBrush> _brushWindow;
	com_ptr<ID2D1SolidColorBrush> _brushHighlight;
	com_ptr<ID2D1SolidColorBrush> _brushDiscardingPort;
	com_ptr<ID2D1SolidColorBrush> _brushLearningPort;
	com_ptr<ID2D1SolidColorBrush> _brushForwarding;
	com_ptr<ID2D1SolidColorBrush> _brushNoForwardingWire;
	com_ptr<ID2D1SolidColorBrush> _brushLoop;
	com_ptr<ID2D1SolidColorBrush> _brushTempWire;
	com_ptr<ID2D1StrokeStyle> _strokeStyleForwardingWire;
	com_ptr<ID2D1StrokeStyle> _strokeStyleNoForwardingWire;
	com_ptr<IDWriteTextFormat> _regularTextFormat;
	com_ptr<IDWriteTextFormat> _smallTextFormat;
	com_ptr<IDWriteTextFormat> _smallBoldTextFormat;
	com_ptr<ID2D1StrokeStyle> _strokeStyleSelectionRect;
};

class renderable_object : public edge::owned_object
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

	virtual void render_selection (const edge::zoomable_window_i* window, ID2D1RenderTarget* rt, const drawing_resources& dos) const = 0;
	virtual ht_result hit_test (const edge::zoomable_window_i* window, D2D1_POINT_2F dLocation, float tolerance) = 0;
	virtual D2D1_RECT_F extent() const = 0;

protected:
	template<typename tpd_>
	void set_and_invalidate (const tpd_* pd, typename tpd_::value_t& field, typename tpd_::value_t value)
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
