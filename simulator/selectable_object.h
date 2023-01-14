
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "edge/om/object.h"
#include "edge/com_ptr.h"
#include "edge/zoomer.h"

using edge::backed_string_p;
using edge::temp_string_p;
using edge::property;
using edge::event;
using edge::object;
using edge::type;
using edge::concrete_type;
using edge::xtype;
using edge::value_property;
using edge::property_change_args;
using edge::side;
using edge::out_sstream_i;
using edge::nvp;
using edge::static_value_property;
using edge::com_ptr;

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

struct __declspec(novtable) selectable_object_i : edge::object
{
	virtual void render_selection (ID2D1DeviceContext* dc, const edge::zoomer* zoomer, const drawing_resources& dos) const = 0;
	virtual uint8_t hit_test (const D2D1::Matrix3x2F& wtr, D2D1_POINT_2F dLocation, float tolerance) = 0;
	virtual D2D1_RECT_F extent() const = 0;
};
