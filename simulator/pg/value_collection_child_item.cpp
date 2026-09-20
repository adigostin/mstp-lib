
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pg_internal.h"

using namespace pg;
using namespace edge;

class value_collection_existing_child_item : public value_collection_child_item_i
{
	ULONG _refCount = 0;
	ULONG _sig = 0xAA55000E;
	value_collection_item_i* const _parent;
	//edge::text_layout_with_metrics _name;
	uint32_t _property_setting_semaphore = 0;
	ULONG _performLayoutCount = 0;

public:
	value_collection_existing_child_item (value_collection_item_i* parent)
		: _parent(parent)
	{
		auto* objs = _parent->parent()->parent()->objects();
//		objs->objects_change().add_handler<&value_collection_existing_child_item::on_selected_objects_change>(this);
	}

	~value_collection_existing_child_item()
	{
		auto* objs = _parent->parent()->parent()->objects();
//		objs->objects_change().remove_handler<&value_collection_existing_child_item::on_selected_objects_change>(this);
	}

	IUnknown* AsUnknown() { return static_cast<value_collection_child_item_i*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IItem>(this, riid, ppvObject)
			)
			return S_OK;

		//if (riid == __uuidof(IWeakRef))
		//	return _weakRefToThis.QueryIWeakRef(ppvObject);

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	bool multiple_selection (size_t value_index) const
	{
		auto* objs = parent()->parent()->parent()->objects();
		for (size_t obj_index = 1; obj_index < objs->size(); obj_index++)
		{
			_ASSERT(false);
			//if (!parent()->property()->equal(value_index, objs[0], objs[obj_index]))
			//	return true;
		}

		return false;
	}

	bool changed(size_t value_index) const
	{
		_ASSERT (!multiple_selection(value_index));
		auto* objs = parent()->parent()->parent()->objects();
		auto prop = parent()->property();
		_ASSERT(false); return { };
		//if (prop->can_insert_remove())
		//	return true;
		//
		//return objs.any ([prop, value_index](edge::object* o) { return prop->changed(o, value_index); });
	}

	HRESULT PerformLayout (size_t value_index)
	{
		RETURN_HR(E_NOTIMPL);
		/*
		auto grid = root()->grid();
		uint32_t dpi = edge::dpi(grid->HWnd());
		float line_width = grid->line_width(dpi);
		auto dwf = grid->renderer()->dwrite_factory();
		auto prop = parent()->property();
		
		float name_layout_width = grid->value_column_left(dpi) - grid->name_column_left(indent()) - line_width - 2 * text_lr_padding;
		if (name_layout_width <= 0)
			_name = { };
		else
			_name = text_layout_with_metrics(dwf, grid->text_format(), std::string("[") + std::to_string(value_index) + "]", name_layout_width);

		float value_layout_width = grid->value_column_right(dpi) - grid->value_column_left(dpi) - line_width - 2 * text_lr_padding;
		//try
		//{
			if (multiple_selection(value_index))
			{
				_value = {
					read_state::multiple_values,
					text_layout_with_metrics(dwf, grid->bold_text_format(), "(multiple selection)", value_layout_width)
				};
			}
			else
			{
				auto* objs = parent()->parent()->parent()->objects();
				_ASSERT(false);
				//auto value_str = prop->get_to_string(objs[0], value_index, root()->app_context());
				//auto format = changed(value_index) ? grid->bold_text_format() : grid->text_format();
				//_value = { text_layout_with_metrics(dwf, format, value_str, value_layout_width), value_layout_t::read_state::ok };
			}
		//}
		//catch (const std::exception& ex)
		//{
		//	_value = { text_layout_with_metrics(dwf, grid->bold_text_format(), ex.what(), value_layout_width), value_layout_t::read_state::read_exception };
		//}

		grid->InvalidateItem(this);

		return S_OK;
		*/
	}

	#pragma region IItem
	virtual ULONG PerformLayoutCount() const noexcept override
	{
		return _performLayoutCount;
	}

	virtual void ResetPerformLayoutCount() noexcept override
	{
		_performLayoutCount = 0;
	}

	/*
	virtual HRESULT STDMETHODCALLTYPE PerformLayout (const PaintResources& res) noexcept override
	{
		_performLayoutCount++;
		return PerformLayout(parent()->index_of(this));
	}
	
	virtual void render (const render_context& rc, float y, bool selected, bool hot, bool focused) const override final
	{
		auto grid = root()->grid();
		uint32_t dpi = edge::dpi(grid->HWnd());
		float pw = edge::pixel_width(dpi);
		auto lt = grid->line_width(dpi);
		float height = std::ceil(this->Height() / pw) * pw;

		render_default_background(rc, y, selected, hot, focused);

		float name_line_x = grid->expand_column_left(dpi) + indent() * grid->indent_width();
		rc.dc->DrawLine ({ name_line_x + lt / 2, y }, { name_line_x + lt / 2, y + height }, rc.disabled_fore, lt);
		rc.dc->DrawTextLayout ({ grid->expand_column_left(dpi) + indent() * grid->indent_width() + lt + text_lr_padding, y }, _name, rc.fore);

		float linex = grid->value_column_left(dpi) + lt / 2;
		rc.dc->DrawLine ({ linex, y }, { linex, y + height }, rc.disabled_fore, lt);

		if (auto& tl = _value.tl)
		{
			ID2D1Brush* brush = root()->grid()->read_only() ? rc.disabled_fore.get() : rc.fore.get();
			D2D1_POINT_2F l = { grid->value_column_left(dpi) + lt + text_lr_padding, y };
			rc.dc->DrawTextLayout (l, _value.tl, brush);
		}
	}
	*/
	virtual LONG Height() const noexcept override
	{
		_ASSERT(false); return { };
		//return (LONG)std::max (_name ? _name.height() : 0, _value.tl ? _value.tl.height() : 0);
	}

	virtual HCURSOR cursor_at (POINT pd, LONG item_y) const override final
	{
		return ::LoadCursor(nullptr, IDC_ARROW);
	}

	virtual bool selectable() const override final
	{
		return true;
	}

	virtual HRESULT STDMETHODCALLTYPE ProcessMouseDown (const edge::mouse_ud_args& ma, LONG item_y) noexcept override
	{
		RETURN_HR(E_NOTIMPL);
		/*
		size_t value_index = parent()->index_of(this);
		auto* objs = parent()->parent()->parent()->objects();
		auto prop = parent()->property();

		bool bold;
		std::wstring initial_text;
		if (multiple_selection(value_index))
		{
			bold = true;
		}
		else
		{
			bold = changed(value_index);
			_ASSERT(false);
			//initial_text = prop->get_to_string(objs[0], value_index, root()->app_context());
		}

		auto hr = root()->grid()->ShowTextEditorOnSelectedItem (bold, initial_text.c_str());
		*/
	}

	virtual HRESULT STDMETHODCALLTYPE ProcessMouseUp (const edge::mouse_ud_args& ma, LONG item_y) noexcept override
	{
		return S_OK;
	}

	virtual wil::unique_process_heap_string description_title() const override final
	{
		_ASSERT(false); return { };
		//return std::string(parent()->property()->name()) + "[" + std::to_string(parent()->index_of(this)) + "]";
	}

	virtual wil::unique_process_heap_string description_text() const override final
	{
		return { };
	}
	#pragma endregion

	#pragma region collection_existing_child_item_i
	virtual size_t collection_entry_count() const override final
	{
		_ASSERT(false); return { };
	}

	virtual collection_existing_child_item_i* collection_entry_at (size_t index) const override final
	{
		_ASSERT(false); return { };
	}

	virtual collection_new_child_item_i* collection_new_entry() const override final
	{
		_ASSERT(false); return { };
	}

	virtual void on_property_setting (size_t object_index) override final
	{
		_property_setting_semaphore++;
	}

	virtual void on_property_set     (size_t object_index) override final
	{
		_property_setting_semaphore--;
		if (!_property_setting_semaphore)
		{
			_ASSERT(false);
			//this->PerformLayout(); // TODO: reconcile layout instead of unconditionally recreating it.
			//root()->grid()->invalidate();
		}
	}
	#pragma endregion

	void on_selected_objects_inserting (std::span<IDispatch* const> objects_to_insert)
	{
	}

//	void on_selected_objects_change (const IObjectList::change_args& args)
//	{
//		if (IObjectList::is_changed_event(args))
//			PerformLayout();
//	}

	#pragma region value_collection_child_item_i
	virtual value_collection_item_i* parent() const noexcept override
	{
		return _parent;
	}
	#pragma endregion
};

extern std::unique_ptr<value_collection_child_item_i> make_value_collection_child_item (value_collection_item_i* parent)
{
	return std::make_unique<value_collection_existing_child_item>(parent);
}
