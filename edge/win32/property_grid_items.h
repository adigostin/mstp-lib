
#pragma once
#include "..\object.h"
#include "com_ptr.h"
#include "d2d_window.h"

namespace edge
{
	struct property_grid_i;
	class root_item;

	static constexpr float text_lr_padding = 3;

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

	struct item_layout
	{
		D2D1_POINT_2F location;
		D2D1_RECT_F name_rect;
		D2D1_RECT_F value_rect;
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

		virtual void create_text_layouts (IDWriteFactory* factory, IDWriteTextFormat* format, float name_width, float value_width) = 0;
		virtual void recreate_value_text_layout() = 0;
		virtual void render_name  (const render_context& rc, const item_layout& l, bool selected, bool focused) const = 0;
		virtual void render_value (const render_context& rc, const item_layout& l, bool selected, bool focused) const = 0;
		virtual float text_height() const = 0;
		virtual HCURSOR cursor (D2D1_SIZE_F offset) const { return nullptr; }

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

		//static std::vector<std::unique_ptr<pgitem>> create_children (object_item* parent, gsl::span<object*> selected_objects);

	private:
		static void on_property_changing (void* arg, object* obj, const property* prop);
		static void on_property_changed (void* arg, object* obj, const property* prop);
		virtual std::vector<std::unique_ptr<pgitem>> create_children() override final;
	};

	class root_item : public object_item
	{
		using base = object_item;

	public:
		property_grid_i* const _grid;

		root_item (property_grid_i* grid, object* const* objects, size_t size)
			: base(nullptr, objects, size), _grid(grid)
		{
			expand();
		}

		virtual root_item* root() override final { return this; }
		virtual void create_text_layouts (IDWriteFactory* factory, IDWriteTextFormat* format, float name_width, float value_width) override final { assert(false); }
		virtual void recreate_value_text_layout() override final { assert(false); }
		virtual void render_name  (const render_context& rc, const item_layout& l, bool selected, bool focused) const override final { assert(false); }
		virtual void render_value (const render_context& rc, const item_layout& l, bool selected, bool focused) const override final { assert(false); }
		virtual float text_height() const override final { assert(false); return 0;  }
	};

	// TODO: value from collection / value from pd
	class value_pgitem : public pgitem
	{
		using base = pgitem;

	public:
		const value_property* const _prop;

		value_pgitem (object_item* parent, const value_property* prop);

		object_item* parent() const { return static_cast<object_item*>(base::parent()); }

		virtual std::string convert_to_string() const;

		virtual void recreate_value_text_layout() override final;
	private:
		virtual void create_text_layouts (IDWriteFactory* factory, IDWriteTextFormat* format, float name_width, float value_width) override final;
		virtual void render_name  (const render_context& rc, const item_layout& l, bool selected, bool focused) const override;
		virtual void render_value (const render_context& rc, const item_layout& l, bool selected, bool focused) const override;
		virtual float text_height() const override;
		virtual HCURSOR cursor (D2D1_SIZE_F offset) const override final;
		virtual void process_mouse_button_down (mouse_button button, UINT modifiers, POINT pt, D2D1_POINT_2F dip, const item_layout& layout) override;
		virtual void process_mouse_button_up   (mouse_button button, UINT modifiers, POINT pt, D2D1_POINT_2F dip, const item_layout& layout) override;

		text_layout _name;
		text_layout _value;
	};
}
