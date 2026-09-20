
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pg_internal.h"

using namespace pg;

class collection_new_child_item : collection_new_child_item_i
{
	collection_item_i* const _parent;
	//edge::text_layout_with_metrics _text_layout;

public:
	collection_new_child_item (collection_item_i* parent)
		: _parent(parent)
	{ }

	~collection_new_child_item()
	{
	}

	virtual collection_item_i* parent() const noexcept override { return _parent; }

	virtual ULONG PerformLayoutCount() const noexcept override
	{
		_ASSERT(false); return { };
	}

	virtual void ResetPerformLayoutCount() noexcept override
	{
		_ASSERT(false);
	}
	/*
	virtual HRESULT STDMETHODCALLTYPE PerformLayout (const PaintResources& res) noexcept override
	{
		auto grid = root()->grid();
		_text_layout = { grid->renderer()->dwrite_factory(), grid->text_format(), "(click to add)" };
		grid->InvalidateItem(this);
		return S_OK;
	}
	
	virtual void render (const render_context& rc, float y, bool selected, bool hot, bool focused) const override final
	{
		render_default_background (rc, y, selected, hot, focused);

		auto grid = root()->grid();
		uint32_t dpi = edge::dpi(grid->HWnd());
		ID2D1Brush* brush = grid->read_only() ? rc.disabled_fore.get() : rc.fore.get();
		D2D1_POINT_2F l = { grid->value_column_left(dpi) + grid->line_width(dpi) + text_lr_padding, y };
		rc.dc->DrawTextLayout (l, _text_layout, brush);
	}
	*/
	virtual LONG Height() const noexcept override
	{
		return 35;
	}

	virtual HCURSOR cursor_at (POINT pd, LONG item_y) const override final
	{
		auto grid = root()->grid();
		LPCWSTR cursor = grid->read_only() ? IDC_ARROW : IDC_HAND;
		return ::LoadCursor(nullptr, cursor);
	}

	virtual bool selectable() const override final { return true; }
	virtual HRESULT STDMETHODCALLTYPE ProcessMouseDown (const edge::mouse_ud_args& ma, LONG item_y) noexcept override { RETURN_HR(E_NOTIMPL); }
	virtual HRESULT STDMETHODCALLTYPE ProcessMouseUp (const edge::mouse_ud_args& ma, LONG item_y) noexcept override { RETURN_HR(E_NOTIMPL); }
	virtual wil::unique_process_heap_string description_title() const override final { return { }; }
	virtual wil::unique_process_heap_string description_text() const override final { return { }; }
};

std::unique_ptr<collection_new_child_item_i> make_collection_new_child_item (collection_item_i* parent)
{
	_ASSERT(false); return { };
}


