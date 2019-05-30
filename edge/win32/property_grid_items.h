
#pragma once
#include "..\object.h"
#include "com_ptr.h"
#include "d2d_window.h"

namespace edge
{
	struct property_grid_i;
	class root_item;

	static constexpr float font_size = 12;

	struct render_context
	{
		ID2D1DeviceContext* dc;
		com_ptr<ID2D1SolidColorBrush> back_brush;
		com_ptr<ID2D1SolidColorBrush> fore_brush;
		com_ptr<ID2D1SolidColorBrush> selected_back_brush_focused;
		com_ptr<ID2D1SolidColorBrush> selected_back_brush_not_focused;
		com_ptr<ID2D1SolidColorBrush> selected_fore_brush;
		com_ptr<ID2D1SolidColorBrush> disabled_fore_brush;
	};

	struct item_layout_horz
	{
		float x_left;
		float x_name;
		float x_value;
		float x_right;
	};

	struct item_layout : item_layout_horz
	{
		float y_top;
		float y_bottom;
	};

	class expandable_item;

	class pgitem
	{
		pgitem (const pgitem&) = delete;
		pgitem& operator= (const pgitem&) = delete;

		expandable_item* const _parent;

	public:
		pgitem (expandable_item* parent)
			: _parent(parent)
		{ }
		virtual ~pgitem () { }

		expandable_item* parent() const { return _parent; }

		virtual root_item* root();

		virtual void create_text_layouts (IDWriteFactory* factory, IDWriteTextFormat* format, const item_layout_horz& l, float line_thickness) = 0;
		virtual void recreate_value_text_layout() = 0;
		virtual void render (const render_context& rc, const item_layout& l, float line_thickness, bool selected, bool focused) const = 0;
		virtual float content_height() const = 0;
		virtual HCURSOR cursor() const { return nullptr; }
		virtual bool selectable() const = 0;
		virtual void process_mouse_button_down (mouse_button button, UINT modifiers, POINT pt, D2D1_POINT_2F dip, const item_layout& layout) { }
		virtual void process_mouse_button_up   (mouse_button button, UINT modifiers, POINT pt, D2D1_POINT_2F dip, const item_layout& layout) { }
	};

	class expandable_item : public pgitem
	{
		using base = pgitem;
		using base::pgitem;

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
		object_item (expandable_item* parent, object* const* objects, size_t size);
		virtual ~object_item();

		const std::vector<object*>& objects() const { return _objects; }

		pgitem* find_child (const property* prop) const;

	private:
		static void on_property_changing (void* arg, object* obj, const property_change_args& args);
		static void on_property_changed (void* arg, object* obj, const property_change_args& args);
		virtual std::vector<std::unique_ptr<pgitem>> create_children() override final;
	};

	class group_item : public expandable_item
	{
		using base = expandable_item;

		text_layout _layout;

	public:
		const property_group* const _group;

		group_item (object_item* parent, const property_group* group);

		object_item* parent() const { return static_cast<object_item*>(base::parent()); }

		virtual std::vector<std::unique_ptr<pgitem>> create_children() override;
		virtual void create_text_layouts (IDWriteFactory* factory, IDWriteTextFormat* format, const item_layout_horz& l, float line_thickness) override;
		virtual void recreate_value_text_layout() override { }
		virtual void render (const render_context& rc, const item_layout& l, float line_thickness, bool selected, bool focused) const override;
		virtual float content_height() const override;
		virtual bool selectable() const override { return false; }
	};

	class root_item : public object_item
	{
		using base = object_item;

		std::string _heading;
		text_layout _text_layout;

	public:
		property_grid_i* const _grid;

		root_item (property_grid_i* grid, const char* heading, object* const* objects, size_t size);

		virtual root_item* root() override final { return this; }
		virtual void create_text_layouts (IDWriteFactory* factory, IDWriteTextFormat* format, const item_layout_horz& l, float line_thickness) override final;
		virtual void recreate_value_text_layout() override final { assert(false); }
		virtual void render (const render_context& rc, const item_layout& l, float line_thickness, bool selected, bool focused) const override final;
		virtual float content_height() const override final;
		virtual bool selectable() const override final { return false; }
	};

	// TODO: value from collection / value from pd
	class value_pgitem : public pgitem
	{
		using base = pgitem;

	public:
		const value_property* const _prop;

		value_pgitem (group_item* parent, const value_property* prop);

		group_item* parent() const { return static_cast<group_item*>(base::parent()); }

		virtual void recreate_value_text_layout() override final;
	private:
		virtual void create_text_layouts (IDWriteFactory* factory, IDWriteTextFormat* format, const item_layout_horz& l, float line_thickness) override final;
		virtual void render (const render_context& rc, const item_layout& l, float line_thickness, bool selected, bool focused) const override;
		virtual float content_height() const override;
		virtual HCURSOR cursor() const override final;
		virtual bool selectable() const override final { return true; }
		virtual void process_mouse_button_down (mouse_button button, UINT modifiers, POINT pt, D2D1_POINT_2F dip, const item_layout& layout) override;
		virtual void process_mouse_button_up   (mouse_button button, UINT modifiers, POINT pt, D2D1_POINT_2F dip, const item_layout& layout) override;

		void create_value_layout_internal (IDWriteFactory* factory, IDWriteTextFormat* format, float width);
		bool multiple_values() const;
		bool can_edit() const;

		text_layout _name;

		struct
		{
			text_layout tl;
			bool readable;
		} _value;
	};
}
