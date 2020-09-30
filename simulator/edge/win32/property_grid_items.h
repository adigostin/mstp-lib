
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "..\object.h"
#include "edge_win32.h"
#include "text_layout.h"

namespace edge
{
	struct property_grid_i;
	class root_item;
	class expandable_item;

	struct render_context
	{
		ID2D1DeviceContext* dc;
		com_ptr<ID2D1SolidColorBrush> back_brush;
		com_ptr<ID2D1SolidColorBrush> fore_brush;
		com_ptr<ID2D1SolidColorBrush> selected_back_brush_focused;
		com_ptr<ID2D1SolidColorBrush> selected_back_brush_not_focused;
		com_ptr<ID2D1SolidColorBrush> selected_fore_brush;
		com_ptr<ID2D1SolidColorBrush> disabled_fore_brush;
		com_ptr<ID2D1SolidColorBrush> data_bind_fore_brush;
		com_ptr<ID2D1LinearGradientBrush> item_gradient_brush;
		com_ptr<ID2D1PathGeometry> triangle_geo;
	};

	class pgitem
	{
		pgitem (const pgitem&) = delete;
		pgitem& operator= (const pgitem&) = delete;

	public:
		pgitem() = default;
		virtual ~pgitem() = default;

		static constexpr float text_lr_padding = 3;
		static constexpr float title_lr_padding = 4;
		static constexpr float title_ud_padding = 2;
		static constexpr float font_size = 12;
		static constexpr float indent_step = 10;

		virtual size_t indent() const;
		virtual root_item* root();
		const root_item* root() const { return const_cast<pgitem*>(this)->root(); }

		virtual expandable_item* parent() const = 0;
		virtual void perform_layout() = 0;
		virtual void render (const render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused) const = 0;
		virtual float content_height() const = 0;
		float content_height_aligned() const;
		virtual HCURSOR cursor() const { return nullptr; }
		virtual bool selectable() const = 0;
		virtual void on_mouse_down (mouse_button button, modifier_key mks, POINT pt, D2D1_POINT_2F dip, float item_y) { }
		virtual void on_mouse_up   (mouse_button button, modifier_key mks, POINT pt, D2D1_POINT_2F dip, float item_y) { }
		virtual std::string description_title() const = 0;
		virtual std::string description_text() const = 0;
	};

	class expandable_item : public pgitem
	{
		using base = pgitem;
		using base::base;

		bool _expanded = false;
		std::vector<std::unique_ptr<pgitem>> _children;

	public:
		const std::vector<std::unique_ptr<pgitem>>& children() const { return _children; }
		void expand();
		void collapse();
		bool expanded() const { return _expanded; }

	protected:
		virtual std::vector<std::unique_ptr<pgitem>> create_children() = 0;
	};

	class object_item abstract : public expandable_item
	{
		using base = expandable_item;

		std::vector<object*> const _objects;

	public:
		object_item (std::span<object* const> objects);
		virtual ~object_item();

		const std::vector<object*>& objects() const { return _objects; }

		object* single_object() const;

		pgitem* find_child (const property* prop) const;

	private:
		void on_property_changing (object* obj, const property_change_args& args);
		void on_property_changed (object* obj, const property_change_args& args);
		virtual std::vector<std::unique_ptr<pgitem>> create_children() override final;
	};

	class group_item : public expandable_item
	{
		using base = expandable_item;

		object_item* const _parent;
		const property_group* const _group;

		text_layout_with_metrics _layout;

		std::unique_ptr<pgitem> make_child_item (const property* prop);

	public:
		group_item (object_item* parent, const property_group* group);

		virtual object_item* parent() const { return _parent; }

		virtual std::vector<std::unique_ptr<pgitem>> create_children() override;
		virtual void perform_layout() override;
		virtual void render (const render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused) const override;
		virtual float content_height() const override;
		virtual bool selectable() const override { return false; }
		virtual std::string description_title() const override final { return { }; }
		virtual std::string description_text() const override final { return { }; }
	};

	class root_item : public object_item
	{
		using base = object_item;

		property_grid_i* const _grid;
		std::string _heading;
		text_layout_with_metrics _text_layout;

	public:
		root_item (property_grid_i* grid, const char* heading, std::span<object* const> objects);

		property_grid_i* grid() const { return _grid; }

		virtual expandable_item* parent() const override { assert(false); return nullptr; }
		virtual size_t indent() const override final { return 0; }
		virtual root_item* root() override final { return this; }
		virtual void perform_layout() override;
		virtual void render (const render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused) const override final;
		virtual float content_height() const override final;
		virtual bool selectable() const override final { return false; }
		virtual std::string description_title() const override final { return { }; }
		virtual std::string description_text() const override final { return { }; }
	};

	// TODO: value from collection / value from pd

	class value_item abstract : public pgitem
	{
		using base = pgitem;

		group_item*           const _parent;
		const value_property* const _property;

		struct value_layout
		{
			text_layout_with_metrics tl;
			bool readable;
		};

		text_layout_with_metrics _name;
		value_layout _value;

		text_layout_with_metrics create_name_layout() const;
		value_layout create_value_layout() const;

	public:
		value_item (group_item* parent, const value_property* property);

		const value_property* property() const { return _property; }
		virtual void render_value (const render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused, bool data_bound) const = 0;

		const text_layout_with_metrics& name_layout() const { return _name; }
		const value_layout& value() const { return _value; }
		bool can_edit() const;
		bool multiple_values() const;

		virtual group_item* parent() const override final { return _parent; }
		virtual bool selectable() const override final { return true; }
		virtual void perform_layout() override;
		virtual float content_height() const override final;
		virtual void render (const render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused) const override final;
		virtual std::string description_title() const override final;
		virtual std::string description_text() const override final;

		bool changed_from_default() const;

	protected:
		friend class object_item;
		virtual void on_value_changed();
	};

	class default_value_pgitem : public value_item
	{
		using base = value_item;

	public:
		using base::base;
		virtual void render_value (const render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused, bool data_bound) const override final;

	protected:
		virtual HCURSOR cursor() const override final;
		virtual void on_mouse_down (mouse_button button, modifier_key mks, POINT pt, D2D1_POINT_2F dip, float item_y) override;
		virtual void on_mouse_up   (mouse_button button, modifier_key mks, POINT pt, D2D1_POINT_2F dip, float item_y) override;
	};
}
