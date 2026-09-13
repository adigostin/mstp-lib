
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#pragma once
#include "edge/edge_d2d.h"

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

struct DECLSPEC_NOVTABLE DECLSPEC_UUID("9F626A71-4C9E-4F25-A821-FBBE33748563") ISelectableObject : IUnknown
{
	virtual void render_selection (ID2D1DeviceContext* dc, const edge::IZoomer* zoomer, const drawing_resources& dos) const = 0;
	virtual int32_t hit_test (const D2D1::Matrix3x2F& wtr, D2D1_POINT_2F dLocation, float tolerance) = 0;
	virtual RECT extent() const noexcept = 0;
	
	D2D1_RECT_F extentf() const noexcept 
	{
		auto r = extent();
		return { (float)r.left, (float)r.top, (float)r.right, (float)r.bottom };
	}
};
