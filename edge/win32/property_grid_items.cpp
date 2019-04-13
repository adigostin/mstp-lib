
#include "pch.h"
#include "property_grid_items.h"
#include "property_grid.h"
#include "utility_functions.h"

using namespace edge;
using namespace D2D1;

static constexpr float text_lr_padding = 3;
static constexpr float title_lr_padding = 4;
static constexpr float title_ud_padding = 2;

root_item* pgitem::root()
{
	return _parent->root();
}

void expandable_item::expand()
{
	assert (!_expanded);
	_children = this->create_children();
	_expanded = true;
}

#pragma region object_item
object_item::object_item (expandable_item* parent, object* const* objects, size_t size)
	: base(parent), _objects(objects, objects + size)
{
	for (auto obj : _objects)
	{
		obj->property_changing().add_handler(&on_property_changing, this);
		obj->property_changed().add_handler(&on_property_changed, this);
	}
}

object_item::~object_item()
{
	for (auto obj : _objects)
	{
		obj->property_changed().remove_handler(&on_property_changed, this);
		obj->property_changing().remove_handler(&on_property_changing, this);
	}
}

//static
void object_item::on_property_changing (void* arg, object* obj, const property* prop)
{
}

//static
void object_item::on_property_changed (void* arg, object* obj, const property* prop)
{
	auto oi = static_cast<object_item*>(arg);
	auto root_item = oi->root();
	for (auto& gi : oi->children())
	{
		for (auto& child_item : static_cast<group_item*>(gi.get())->children())
		{
			if (auto value_item = dynamic_cast<value_pgitem*>(child_item.get()); value_item->_prop == prop)
			{
				value_item->recreate_value_text_layout();
				break;
			}
		}
	}
}

pgitem* object_item::find_child (const property* prop) const
{
	for (auto& item : children())
	{
		if (auto value_item = dynamic_cast<value_pgitem*>(item.get()); value_item->_prop == prop)
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
	std::transform (groups.begin(), groups.end(), std::back_inserter(items),
					[this](const property_group* n) { return std::make_unique<group_item>(this, n); });
	return items;
}
#pragma endregion

#pragma region group_item
group_item::group_item (object_item* parent, const property_group* group)
	: base(parent), _group(group)
{
	expand();
}

std::vector<std::unique_ptr<pgitem>> group_item::create_children()
{
	std::vector<std::unique_ptr<pgitem>> items;

	auto type = parent()->objects().front()->type();

	for (auto prop : type->make_property_list())
	{
		if (prop->_ui_visible == ui_visible::yes)
		{
			if (auto value_prop = dynamic_cast<const value_property*>(prop))
			{
				if (value_prop->_group == _group)
					items.push_back (std::make_unique<value_pgitem>(this, value_prop));
			}
			else
				assert(false); // not implemented
		}
	}
	
	return items;
}

void group_item::create_text_layouts (IDWriteFactory* factory, IDWriteTextFormat* format, const item_layout_horz& l, float line_thickness)
{
	com_ptr<IDWriteTextFormat> tf;
	auto hr = factory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
										 DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &tf);
	float layout_width = std::max (0.0f, l.x_right -l.x_name - 2 * title_lr_padding);
	_layout = text_layout::create (factory, tf, _group->name, layout_width);
}

void group_item::render (const render_context& rc, const item_layout& l, float line_thickness, bool selected, bool focused) const
{
	rc.dc->FillRectangle ({ l.x_left, l.y_top, l.x_right, l.y_bottom }, rc.back_brush);
	rc.dc->DrawTextLayout ({ l.x_name + text_lr_padding, l.y_top }, _layout.layout, rc.fore_brush);
}

float group_item::content_height() const
{
	return _layout.metrics.height;
}
#pragma endregion

#pragma region root_item
root_item::root_item (property_grid_i* grid, const char* heading, object* const* objects, size_t size)
	: base(nullptr, objects, size), _grid(grid), _heading(heading)
{
	expand();
}

void root_item::create_text_layouts (IDWriteFactory* factory, IDWriteTextFormat* format, const item_layout_horz& l, float line_thickness)
{
	// TODO: padding
	com_ptr<IDWriteTextFormat> tf;
	auto hr = factory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
	                                     DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &tf);
	float layout_width = std::max (0.0f, l.x_right -l.x_left - 2 * title_lr_padding);
	_text_layout = text_layout::create (factory, tf, _heading, layout_width);
}

void root_item::render (const render_context& rc, const item_layout& l, float line_thickness, bool selected, bool focused) const
{
	com_ptr<ID2D1SolidColorBrush> brush;
	rc.dc->CreateSolidColorBrush (GetD2DSystemColor(COLOR_ACTIVECAPTION), &brush);
	D2D1_RECT_F rect = { l.x_left, l.y_top, l.x_right, l.y_bottom };
	rc.dc->FillRectangle (&rect, brush);
	brush->SetColor (GetD2DSystemColor(COLOR_CAPTIONTEXT));
	rc.dc->DrawTextLayout ({ l.x_left + title_lr_padding, l.y_top + title_ud_padding }, _text_layout.layout, brush);
}

float root_item::content_height() const
{
	return _text_layout.metrics.height + 2 * title_ud_padding;
}
#pragma endregion

#pragma region value_pgitem
value_pgitem::value_pgitem (group_item* parent, const value_property* prop)
	: base(parent), _prop(prop)
{ }

bool value_pgitem::multiple_values() const
{
	auto& objs = parent()->parent()->objects();
	for (size_t i = 1; i < objs.size(); i++)
	{
		if (!_prop->equal(objs[0], objs[i]))
			return true;
	}

	return false;
}

void value_pgitem::create_value_layout_internal (IDWriteFactory* factory, IDWriteTextFormat* format, float width)
{
	if (multiple_values())
		_value = text_layout::create (factory, format, "(multiple values)", width);
	else
		_value = text_layout::create (factory, format, _prop->get_to_string(parent()->parent()->objects().front()), width);
}

void value_pgitem::create_text_layouts (IDWriteFactory* factory, IDWriteTextFormat* format, const item_layout_horz& l, float line_thickness)
{
	_name = text_layout::create (factory, format, _prop->_name, l.x_value - l.x_name - line_thickness - 2 * text_lr_padding);
	float value_layout_width = std::max (0.0f, l.x_right - l.x_value - line_thickness - 2 * text_lr_padding);
	create_value_layout_internal (factory, format, value_layout_width);
}

void value_pgitem::recreate_value_text_layout()
{
	auto grid = root()->_grid;
	create_value_layout_internal (grid->dwrite_factory(), grid->text_format(), _value.metrics.layoutWidth);
	grid->invalidate();
}

void value_pgitem::render (const render_context& rc, const item_layout& l, float line_thickness, bool selected, bool focused) const
{
	if (selected)
	{
		D2D1_RECT_F rect = { l.x_left, l.y_top, l.x_right, l.y_bottom };
		rc.dc->FillRectangle (&rect, focused ? rc.selected_back_brush_focused.get() : rc.selected_back_brush_not_focused.get());
	}

	float name_line_x = l.x_name + line_thickness / 2;
	rc.dc->DrawLine ({ name_line_x, l.y_top }, { name_line_x, l.y_bottom }, rc.disabled_fore_brush, line_thickness);
	auto fore = selected ? rc.selected_fore_brush.get() : rc.fore_brush.get();
	rc.dc->DrawTextLayout ({ l.x_name + line_thickness + text_lr_padding, l.y_top }, _name.layout, fore);

	float linex = l.x_value + line_thickness / 2;
	rc.dc->DrawLine ({ linex, l.y_top }, { linex, l.y_bottom }, rc.disabled_fore_brush, line_thickness);
	bool canEdit = /*(_prop->_customEditor != nullptr) || */_prop->has_setter();
	fore = !canEdit ? rc.disabled_fore_brush.get() : (selected ? rc.selected_fore_brush.get() : rc.fore_brush.get());
	rc.dc->DrawTextLayout ({ l.x_value + line_thickness + text_lr_padding, l.y_top }, _value.layout, fore);
}

float value_pgitem::content_height() const
{
	return std::max (_name.metrics.height, _value.metrics.height);
}

HCURSOR value_pgitem::cursor() const
{
	if (!_prop->has_setter() && !_prop->custom_editor())
		return ::LoadCursor(nullptr, IDC_ARROW);

	if (auto bool_p = dynamic_cast<const edge::bool_p*>(_prop))
		return ::LoadCursor(nullptr, IDC_HAND);

	if (_prop->nvps() || _prop->custom_editor())
		return ::LoadCursor(nullptr, IDC_HAND);

	return ::LoadCursor (nullptr, IDC_IBEAM);
}

static const NVP bool_nvps[] = {
	{ "false", 0 },
	{ "true", 1 },
	{ nullptr, -1 },
};

void value_pgitem::process_mouse_button_down (mouse_button button, UINT modifiers, POINT pt, D2D1_POINT_2F dip, const item_layout& layout)
{
	if (dip.x < layout.x_value)
		return;

	if (_prop->custom_editor())
	{
		auto editor = _prop->custom_editor()(parent()->parent()->objects());
		editor->show(root()->_grid);
		return;
	}

	if (!_prop->has_setter())
		return;

	if (_prop->nvps() || dynamic_cast<const edge::bool_p*>(_prop))
	{
		auto nvps = _prop->nvps() ? _prop->nvps() : bool_nvps;
		int selected_nvp_index = root()->_grid->show_enum_editor(dip, nvps);
		if (selected_nvp_index >= 0)
		{
			auto new_value_str = nvps[selected_nvp_index].first;
			auto changed = [new_value_str, prop=_prop](object* o) { return prop->get_to_string(o) != new_value_str; };
			auto& objects = parent()->parent()->objects();
			if (std::any_of(objects.begin(), objects.end(), changed))
				root()->_grid->try_change_property (objects, _prop, new_value_str);
		}
	}
	else
	{
		auto lt = root()->_grid->line_thickness();
		D2D1_RECT_F editor_rect = { layout.x_value + lt, layout.y_top, layout.x_right, layout.y_bottom };
		auto editor = root()->_grid->show_text_editor (editor_rect, text_lr_padding, multiple_values() ? "" : _prop->get_to_string(parent()->parent()->objects().front()));
		editor->process_mouse_button_down (button, modifiers, pt, dip);
	}
}

void value_pgitem::process_mouse_button_up (mouse_button button, UINT modifiers, POINT pt, D2D1_POINT_2F dip, const item_layout& layout)
{
}
#pragma endregion
