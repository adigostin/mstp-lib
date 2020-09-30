
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "property_grid_items.h"
#include "property_grid.h"
#include "utility_functions.h"

using namespace D2D1;

namespace edge
{
	size_t pgitem::indent() const
	{
		return parent()->indent() + 1;
	}

	float pgitem::content_height_aligned() const
	{
		auto grid = root()->grid();
		float pixel_width = grid->window()->pixel_width();
		return std::ceilf (content_height() / pixel_width) * pixel_width + grid->line_thickness();
	}

	root_item* pgitem::root()
	{
		return parent()->root();
	}

	void expandable_item::expand()
	{
		assert (!_expanded);
		_children = this->create_children();
		_expanded = true;
	}

	void expandable_item::collapse()
	{
		assert(false); // not implemented
	}

	#pragma region object_item
	object_item::object_item (std::span<object* const> objects)
		: _objects(objects.begin(), objects.end())
	{
		for (auto obj : _objects)
		{
			obj->property_changing().add_handler<&object_item::on_property_changing>(this);
			obj->property_changed().add_handler<&object_item::on_property_changed>(this);
		}
	}

	object_item::~object_item()
	{
		for (auto obj : _objects)
		{
			obj->property_changed().remove_handler<&object_item::on_property_changed>(this);
			obj->property_changing().remove_handler<&object_item::on_property_changing>(this);
		}
	}

	void object_item::on_property_changing (object* obj, const property_change_args& args)
	{
	}

	void object_item::on_property_changed (object* obj, const property_change_args& args)
	{
		if (auto pgv = dynamic_cast<const pg_visible_property_i*>(args.property); pgv && !pgv->pg_visible(_objects))
			return;

		auto root_item = this->root();

		if (auto prop = dynamic_cast<const value_property*>(args.property))
		{
			for (auto& gi : children())
			{
				for (auto& child_item : static_cast<group_item*>(gi.get())->children())
				{
					if (auto vi = dynamic_cast<value_item*>(child_item.get()); vi->_property == prop)
					{
						vi->on_value_changed();
						break;
					}
				}
			}
		}
		else if (auto prop = dynamic_cast<const value_collection_property*>(args.property))
		{
			assert(false); // not implemented
		}
		else
		{
			assert(false); // not implemented
		}
	}

	object* object_item::single_object() const
	{
		auto& objs = this->objects();
		assert(objs.size() == 1);
		return objs.front();
	}

	pgitem* object_item::find_child (const property* prop) const
	{
		for (auto& item : children())
		{
			if (auto value_item = dynamic_cast<default_value_pgitem*>(item.get()); value_item->property() == prop)
				return item.get();
		}

		return nullptr;
	}

	std::vector<std::unique_ptr<pgitem>> object_item::create_children()
	{
		if (_objects.empty())
			return { };

		auto type = _objects[0]->type();
		if (!std::all_of (_objects.begin(), _objects.end(), [type](object* o) { return o->type() == type; }))
			return { };

		struct group_comparer
		{
			bool operator() (const property_group* g1, const property_group* g2) const { return g1->prio < g2->prio; }
		};

		std::set<const property_group*, group_comparer> groups;

		for (auto prop : type->make_property_list())
		{
			if (groups.find(prop->_group) == groups.end())
				groups.insert(prop->_group);
		}

		std::vector<std::unique_ptr<pgitem>> items;
		for (const property_group* g : groups)
			items.push_back (std::make_unique<group_item>(this, g));

		return items;
	}
	#pragma endregion

	#pragma region group_item
	group_item::group_item (object_item* parent, const property_group* group)
		: _parent(parent), _group(group)
	{
		perform_layout();
		expand();
	}

	std::unique_ptr<pgitem> group_item::make_child_item (const property* prop)
	{
		if (auto f = dynamic_cast<const pg_custom_item_i*>(prop))
			return f->create_item(this, prop);

		if (auto value_prop = dynamic_cast<const value_property*>(prop))
			return std::make_unique<default_value_pgitem>(this, value_prop);

		// TODO: placeholder pg item for unknown types of properties
		throw not_implemented_exception();
	}

	std::vector<std::unique_ptr<pgitem>> group_item::create_children()
	{
		std::vector<std::unique_ptr<pgitem>> items;

		auto type = parent()->objects().front()->type();

		for (auto prop : type->make_property_list())
		{
			if (auto pgv = dynamic_cast<const pg_visible_property_i*>(prop); !pgv || pgv->pg_visible(_parent->objects()))
			{
				if (prop->_group == _group)
					items.push_back(make_child_item(prop));
			}
		}

		return items;
	}

	void group_item::perform_layout()
	{
		auto grid = root()->grid();
		float layout_width = grid->width() - 2 * grid->border_width() - 2 * title_lr_padding;
		if (layout_width > 0)
		{
			com_ptr<IDWriteTextFormat> tf;
			auto hr = grid->dwrite_factory()->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
				DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &tf);
			_layout = text_layout_with_metrics (grid->dwrite_factory(), tf, _group->name, layout_width);
		}
		else
			_layout = nullptr;
	}

	void group_item::render (const render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused) const
	{
		if (_layout)
		{
			float bw = root()->grid()->border_width();
			auto rectd = root()->grid()->rectd();
			float indent_width = indent() * indent_step;
			rc.dc->FillRectangle ({ rectd.left + bw, pd.y, rectd.right - bw, pd.y + content_height_aligned() }, rc.back_brush);
			rc.dc->DrawTextLayout ({ rectd.left + bw + indent_width + text_lr_padding, pd.y }, _layout, rc.fore_brush);
		}
	}

	float group_item::content_height() const
	{
		return _layout ? _layout.height() : 0;
	}
	#pragma endregion

	#pragma region root_item
	root_item::root_item (property_grid_i* grid, const char* heading, std::span<object* const> objects)
		: base(objects), _grid(grid), _heading(heading ? heading : "")
	{
		perform_layout();
		expand();
	}

	void root_item::perform_layout()
	{
		_text_layout = nullptr;
		if (!_heading.empty())
		{
			float layout_width = _grid->width() - 2 * _grid->border_width() - 2 * title_lr_padding;
			if (layout_width > 0)
			{
				auto factory = _grid->dwrite_factory();
				com_ptr<IDWriteTextFormat> tf;
				auto hr = factory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
					DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &tf);
				_text_layout = text_layout_with_metrics (factory, tf, _heading, layout_width);
			}
		}
	}

	void root_item::render (const render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused) const
	{
		if (_text_layout)
		{
			com_ptr<ID2D1SolidColorBrush> brush;
			rc.dc->CreateSolidColorBrush (GetD2DSystemColor(COLOR_ACTIVECAPTION), &brush);
			D2D1_RECT_F rect = {
				_grid->rectd().left + _grid->border_width(),
				pd.y,
				_grid->rectd().right - _grid->border_width(),
				pd.y + content_height_aligned()
			};
			rc.dc->FillRectangle (&rect, brush);
			brush->SetColor (GetD2DSystemColor(COLOR_CAPTIONTEXT));
			rc.dc->DrawTextLayout ({ rect.left + title_lr_padding, rect.top + title_ud_padding }, _text_layout, brush);
		}
	}

	float root_item::content_height() const
	{
		if (_text_layout)
			return _text_layout.height() + 2 * title_ud_padding;
		else
			return 0;
	}
	#pragma endregion

	#pragma region value_item
	value_item::value_item (group_item* parent, const value_property* property)
		: _parent(parent), _property(property)
	{
		_name = create_name_layout();
		_value = create_value_layout();
	}

	bool value_item::multiple_values() const
	{
		auto& objs = _parent->parent()->objects();
		for (size_t i = 1; i < objs.size(); i++)
		{
			if (!_property->equal(objs[0], objs[i]))
				return true;
		}

		return false;
	}

	bool value_item::can_edit() const
	{
		// TODO: Allow editing and setting a property that couldn't be read.
		if (!_value.readable)
			return false;

		if (dynamic_cast<const pg_custom_editor_i*>(_property))
			return true;

		if (auto ep = dynamic_cast<const pg_editable_property_i*>(_property))
			return ep->pg_editable(parent()->parent()->objects());

		auto& objs = parent()->parent()->objects();
		bool can_set = std::all_of (objs.begin(), objs.end(), [prop=_property](object* o) { return prop->can_set(o); });
		return can_set;
	}

	bool value_item::changed_from_default() const
	{
		for (auto o : parent()->parent()->objects())
		{
			if (_property->changed_from_default(o))
				return true;
		}

		return false;
	}

	text_layout_with_metrics value_item::create_name_layout() const
	{
		auto grid = root()->grid();
		float name_layout_width = grid->value_column_x() - grid->name_column_x(indent()) - grid->line_thickness() - 2 * text_lr_padding;
		if (name_layout_width <= 0)
			return { };

		return text_layout_with_metrics (grid->dwrite_factory(), grid->text_format(), _property->_name, name_layout_width);
	}

	value_item::value_layout value_item::create_value_layout() const
	{
		auto grid = root()->grid();

		float width = grid->rectd().right - grid->border_width() - grid->value_column_x() - grid->line_thickness() - 2 * text_lr_padding;
		if (width <= 0)
			return { };

		auto factory = grid->dwrite_factory();
		auto format = changed_from_default() ? grid->bold_text_format() : grid->text_format();

		text_layout_with_metrics tl;
		bool readable;
		try
		{
			if (multiple_values())
				tl = text_layout_with_metrics (factory, format, "(multiple values)", width);
			else
				tl = text_layout_with_metrics (factory, format, _property->get_to_string(parent()->parent()->objects().front()), width);
			readable = true;
		}
		catch (const std::exception& ex)
		{
			tl = text_layout_with_metrics (factory, format, ex.what(), width);
			readable = false;
		}

		return { std::move(tl), readable };
	}

	void value_item::perform_layout()
	{
		_name = create_name_layout();
		_value = create_value_layout();
	}

	float value_item::content_height() const
	{
		return std::max (_name ? _name.height() : 0.0f, _value.tl ? _value.tl.height() : 0.0f);
	}

	void value_item::render (const render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused) const
	{
		auto grid = root()->grid();
		auto lt = grid->line_thickness();
		float bw = grid->border_width();
		auto rectd = grid->rectd();
		float indent_width = indent() * indent_step;
		float pw = grid->window()->pixel_width();
		float height = content_height_aligned();

		D2D1_RECT_F fill_rect = { rectd.left + bw, pd.y, rectd.right - bw, pd.y + height };

		if (selected)
		{
			rc.dc->FillRectangle (&fill_rect, focused ? rc.selected_back_brush_focused.get() : rc.selected_back_brush_not_focused.get());
		}
		else
		{
			rc.item_gradient_brush->SetStartPoint ({ fill_rect.left, fill_rect.top });
			rc.item_gradient_brush->SetEndPoint ({ fill_rect.left, fill_rect.bottom });
			rc.dc->FillRectangle (&fill_rect, rc.item_gradient_brush);
		}

		bool bindable = false;
		bool bound = false;

		if (auto bp = dynamic_cast<const pg_bindable_property_i*>(_property))
		{
			bindable = true;
			auto& objs = parent()->parent()->objects();
			bound = std::any_of (objs.begin(), objs.end(), [bp](object* o) { return bp->bound(o); });
		}

		if (bindable)
		{
			float line_width_not_aligned = 1.6f;
			LONG line_width_pixels = grid->window()->lengthd_to_lengthp (line_width_not_aligned, 0);
			float line_width = grid->window()->lengthp_to_lengthd(line_width_pixels);

			D2D1::Matrix3x2F oldtr;
			rc.dc->GetTransform(&oldtr);
			float padding = line_width;
			rc.dc->SetTransform (Matrix3x2F::Translation(rectd.left + bw + padding + line_width / 2, pd.y) * oldtr);

			if (bound)
				rc.dc->FillGeometry(rc.triangle_geo, rc.data_bind_fore_brush);

			rc.dc->DrawGeometry(rc.triangle_geo, rc.data_bind_fore_brush, line_width);

			rc.dc->SetTransform(&oldtr);
		}

		float name_line_x = rectd.left + bw + indent_width + lt / 2;
		rc.dc->DrawLine ({ name_line_x, pd.y }, { name_line_x, pd.y + height }, rc.disabled_fore_brush, lt);
		auto fore = (bound ? rc.data_bind_fore_brush : (selected ? rc.selected_fore_brush : rc.fore_brush)).get();
		rc.dc->DrawTextLayout ({ rectd.left + bw + indent_width + lt + text_lr_padding, pd.y }, name_layout(), fore);

		float linex = grid->value_column_x() + lt / 2;
		rc.dc->DrawLine ({ linex, pd.y }, { linex, pd.y + height }, rc.disabled_fore_brush, lt);
		this->render_value (rc, { grid->value_column_x(), pd.y }, selected, focused, bound);
	}

	std::string value_item::description_title() const
	{
		std::stringstream ss;
		ss << property()->_name << " (" << property()->type_name() << ")";
		return ss.str();
	}

	std::string value_item::description_text() const
	{
		return property()->_description ? std::string(property()->_description) : std::string();
	}

	void value_item::on_value_changed()
	{
		_value = create_value_layout();
		root()->grid()->invalidate();
	}
	#pragma endregion

	#pragma region default_value_pgitem
	void default_value_pgitem::render_value (const render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused, bool data_bound) const
	{
		if (auto& tl = value().tl)
		{
			ID2D1Brush* brush;
			if (data_bound)
				brush = rc.data_bind_fore_brush;
			else if (root()->grid()->read_only() || !can_edit())
				brush = rc.disabled_fore_brush;
			else if (selected)
				brush = rc.selected_fore_brush.get();
			else
				brush = rc.fore_brush;

			rc.dc->DrawTextLayout ({ pd.x + root()->grid()->line_thickness() + text_lr_padding, pd.y }, value().tl, brush);
		}
	}

	HCURSOR default_value_pgitem::cursor() const
	{
		if (root()->grid()->read_only() || !can_edit())
			return ::LoadCursor(nullptr, IDC_ARROW);

		if (auto bool_p = dynamic_cast<const edge::bool_p*>(property()))
			return ::LoadCursor(nullptr, IDC_HAND);

		if (property()->nvps())
			return ::LoadCursor(nullptr, IDC_HAND);

		if (dynamic_cast<const pg_custom_editor_i*>(property()))
			return ::LoadCursor(nullptr, IDC_HAND);

		return ::LoadCursor (nullptr, IDC_IBEAM);
	}

	void default_value_pgitem::on_mouse_down (mouse_button button, modifier_key mks, POINT pp, D2D1_POINT_2F pd, float item_y)
	{
		auto grid = root()->grid();
		auto vcx = grid->value_column_x();
		if (pd.x < vcx)
			return;

		if (auto cep = dynamic_cast<const pg_custom_editor_i*>(property()))
		{
			auto editor = cep->create_editor(parent()->parent()->objects());
			editor->show(grid->window());
			return;
		}

		if (grid->read_only() || !can_edit())
			return;

		if (auto nvps = property()->nvps())
		{
			int selected_nvp_index = grid->show_enum_editor(pd, nvps);
			if (selected_nvp_index >= 0)
			{
				auto new_value_str = nvps[selected_nvp_index].name;
				auto changed = [new_value_str, prop=property()](object* o) { return prop->get_to_string(o) != new_value_str; };
				auto& objects = parent()->parent()->objects();
				if (std::any_of(objects.begin(), objects.end(), changed))
				{
					try
					{
						grid->change_property (objects, property(), new_value_str);
					}
					catch (const std::exception& ex)
					{
						auto message = utf8_to_utf16(ex.what());
						::MessageBox (grid->window()->hwnd(), message.c_str(), L"Error setting property", 0);
					}
				}

			}
		}
		else
		{
			D2D1_RECT_F editor_rect = { vcx + grid->line_thickness(), item_y, grid->rectd().right - grid->border_width(), item_y + content_height_aligned() };
			bool bold = changed_from_default();
			auto editor = grid->show_text_editor (editor_rect, bold, text_lr_padding, multiple_values() ? "" : property()->get_to_string(parent()->parent()->objects().front()));
			//editor->on_mouse_down (button, mks, pp, pd);
		}
	}

	void default_value_pgitem::on_mouse_up (mouse_button button, modifier_key mks, POINT pt, D2D1_POINT_2F dip, float item_y)
	{
	}
	#pragma endregion
}
