
#include "include/pg/property_grid.h"
#include "object_item.h"
#include "edge/utility_functions.h"
#include "edge/d2d_renderer.h"

using namespace pg;
using namespace edge;
using namespace std::placeholders;

class object_picker_popup
{
	static constexpr DWORD style = WS_POPUPWINDOW;
	static constexpr DWORD ex_style = WS_EX_NOACTIVATE;
	static constexpr char bottom_hint[] = "Click a color name to select it, click a colored cell to edit the color.";

	static inline const WNDCLASSEX wnd_class = {
		.cbSize = sizeof(WNDCLASSEX),
		.style = CS_DBLCLKS | CS_DROPSHADOW,
		.hCursor = ::LoadCursor (nullptr, IDC_ARROW),
		.lpszClassName = L"object_picker_popup",
	};

	object_property_item_i*  const _item;
	std::function<void(const concrete_type*)> const _callback;
	const edge::theme_color_provider_i*       const _tcp;
	property_grid_i*         const _grid;
	float                    const _pw;
	float                    const _line_width;
	float                    const _lrpadding;
	float                    const _udpadding;
	std::vector<std::pair<const concrete_type*, text_layout_with_metrics>> const _types;
	float                    const _max_layout_width;
	float                    const _max_layout_height;
	float                    const _client_width;
	float                    const _client_height;
	std::unique_ptr<win32_window_i> const _window;
	std::unique_ptr<d2d_renderer_i> const _renderer;
	//text_layout_with_metrics const _bottom_hint_text;
	HHOOK _mouse_hook = nullptr;

public:
	object_picker_popup (object_property_item_i* item, float item_y, std::function<void(const concrete_type*)> callback, const edge::theme_color_provider_i* tcp)
		: _item(item)
		, _callback(callback)
		, _tcp(tcp)
		, _grid(item->grid())
		, _pw(edge::pixel_width(_grid->window().hwnd())) // we assume popup we're going to create will have same DPI
		, _line_width(std::round(0.6f / _pw) * _pw)
		, _lrpadding(std::round(5 / _pw) * _pw)
		, _udpadding(std::round(5 / _pw) * _pw)
		, _types(make_types(_grid->renderer()->dwrite_factory(), _grid->bold_text_format(), item->property()))
		, _max_layout_width(std::max_element(_types.begin(), _types.end(), [](auto& a, auto& b) { return a.second.width() < b.second.width(); })->second.width())
		, _max_layout_height(std::max_element(_types.begin(), _types.end(), [](auto& a, auto& b){ return a.second.height() < b.second.height(); })->second.height())
		, _client_width(_lrpadding + std::ceil(_max_layout_width / _pw) * _pw + _lrpadding)
		, _client_height(std::accumulate(_types.begin(), _types.end(), 0.0f, [this](float a, auto& b) { return a + _udpadding + std::ceil(b.second.height() / _pw) * _pw + _udpadding + ((b.first != _types.back().first) ? _line_width : 0); }))
		, _window(make_window(wnd_class, ex_style, style, _grid->window().hwnd(), _grid->calc_popup_window_pos(item, item_y, { _client_width, _client_height }, style, ex_style)))
		, _renderer(make_d2d_renderer(*_window, _grid->renderer()->d3d_dc(), _grid->renderer()->dwrite_factory(), _grid->renderer()->d2d_factory()))
	{
		_window->window_proc().add_handler<&object_picker_popup::on_window_proc>(this);
		_renderer->render().add_handler<&object_picker_popup::on_render>(this);
		::ShowWindow (_window->hwnd(), SW_SHOWNOACTIVATE);
	}

	~object_picker_popup()
	{
		::ShowWindow (_window->hwnd(), SW_HIDE);
		_renderer->render().remove_handler<&object_picker_popup::on_render>(this);
		_window->window_proc().remove_handler<&object_picker_popup::on_window_proc>(this);
	}

private:

	static std::vector<std::pair<const concrete_type*, text_layout_with_metrics>> make_types
	(
		IDWriteFactory* dwrite_factory,
		IDWriteTextFormat* text_format,
		const object_property* prop
	)
	{
		std::vector<std::pair<const concrete_type*, text_layout_with_metrics>> types;

		for (auto t : concrete_type::known_types())
		{
			if (t->is_same_or_derived_from(prop->child_type()))
			{
				text_layout_with_metrics name (dwrite_factory, text_format, t->name);
				types.push_back ({ t, std::move(name) });
			}
		}

		return types;
	}

	std::optional<LRESULT> on_window_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (msg == WM_LBUTTONDOWN)
		{
			POINT pp = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
			uint32_t dpi = edge::dpi(hwnd);
			D2D1_POINT_2F pd = pointp_to_pointd(pp, dpi);
			float y = 0;
			for (auto& t : _types)
			{
				y += (_udpadding + std::ceil(t.second.height() / _pw) * _pw + _udpadding + _line_width);
				if (y >= pd.y)
				{
					_callback(t.first);
					break;
				}
			}

			return 0;
		}

		return std::nullopt;
	}

	void on_render (HWND hwnd, ID2D1DeviceContext* dc) const
	{
		D2D1_COLOR_F fore_color = _tcp->color_d2d(theme_color::foreground);
		com_ptr<ID2D1SolidColorBrush> fore;
		dc->CreateSolidColorBrush (fore_color, &fore);

		D2D1_COLOR_F disabled_fore_color = _tcp->color_d2d(theme_color::disabled_fore);
		com_ptr<ID2D1SolidColorBrush> disabled_fore;
		dc->CreateSolidColorBrush (disabled_fore_color, &disabled_fore);

		uint32_t dpi = edge::dpi(hwnd);
		float pw = edge::pixel_width(dpi);
		dc->SetTransform(edge::dpi_transform(dpi));
		dc->Clear(_tcp->color_d2d(theme_color::background));
		float y = 0;
		for (auto& t : _types)
		{
			y += _udpadding;
			dc->DrawTextLayout ({ _lrpadding, y }, t.second, fore);
			y += std::ceil(_max_layout_height / pw) * pw;
			y += _udpadding;
			y += _line_width / 2;
			dc->DrawLine ({ 0, y }, { _client_width, y }, disabled_fore, _line_width);
			y += _line_width / 2;
		}
	}

	static LRESULT CALLBACK mouse_hook_proc(int Code, WPARAM wParam, LPARAM lParam);
};

class object_property_item : public object_property_item_i, public object_list_i
{
	event_manager _em;
	group_item_i* const _parent;
	const edge::object_property* const _prop;
	std::optional<object_item_child_manager> _child_manager;
	edge::text_layout_with_metrics _name;
	enum class value_state { all_null, multiple_selection, all_same_type };
	std::pair<value_state, edge::text_layout_with_metrics> _value;

	static inline std::optional<object_picker_popup> _popup;

public:
	object_property_item (group_item_i* parent, const object_property* prop)
		: _parent(parent)
		, _prop(prop)
	{
		auto& objs = _parent->parent()->objects();
		objs.objects_change().add_handler<&object_property_item::on_parent_objects_change>(this);
		perform_layout();
	}

	~object_property_item()
	{
		item_removing_e::invoker(_em).invoke(this);
		auto& objs = _parent->parent()->objects();
		objs.objects_change().remove_handler<&object_property_item::on_parent_objects_change>(this);
		_popup.reset();
	}

	// object_list_i
	virtual size_t size() const override final { return _parent->parent()->objects().size(); }
	virtual edge::object* operator[](size_t index) const override final { return _prop->get(_parent->parent()->objects()[index]); }
	virtual change_e::subscriber objects_change() override final { return change_e::subscriber(_em); }

	static std::vector<object*> get_child_selected_objects (const object_property* prop, std::span<object* const> parent_objects)
	{
		std::vector<object*> res;
		res.reserve(parent_objects.size());
		for (object* o : parent_objects)
			res.push_back(prop->get(o));
		return res;
	}

	virtual group_item_i* parent() const override final { return _parent; }

	text_layout_with_metrics make_name_layout() const
	{
		auto grid = this->grid();
		uint32_t dpi = edge::dpi(grid->window().hwnd());
		auto dwf = grid->renderer()->dwrite_factory();
		float ncx = grid->name_column_left(indent());
		float name_width = grid->value_column_left(dpi) - ncx;
		return text_layout_with_metrics(dwf, grid->text_format(), _prop->name(), name_width);
	}

	std::pair<object_property_item::value_state, text_layout_with_metrics> make_value_layout() const
	{
		auto grid = this->grid();
		auto dwf = grid->renderer()->dwrite_factory();

		uint32_t dpi = edge::dpi(grid->window().hwnd());
		float width = grid->value_column_right(dpi) - grid->value_column_left(dpi) - grid->line_width(dpi) - 2 * text_lr_padding;
		if (width <= 0)
			return { };

		auto& objs = parent()->parent()->objects();
		if (objs.all ([prop=_prop](object* o) { return !prop->get(o); }))
			return { value_state::all_null, text_layout_with_metrics(dwf, grid->text_format(), "(not set)", width) };

		auto type = _prop->get(objs.front()) ? _prop->get(objs.front())->type() : nullptr;
		bool all_same_type = objs.all([prop=_prop,type](object* o) { return (prop->get(o) ? prop->get(o)->type() : nullptr) == type; });
		if (!all_same_type)
			return { value_state::multiple_selection, text_layout_with_metrics(dwf, grid->bold_text_format(), "(multiple selection)", width) };

		return { value_state::all_same_type, text_layout_with_metrics(dwf, grid->bold_text_format(), type->name, width) };
	}

	virtual void perform_layout() override final
	{
		_name = make_name_layout();
		_value = make_value_layout();

		grid()->invalidate_item(this);
	}

	D2D1_RECT_F expand_button_click_rect (float item_y) const
	{
		rassert (_value.first == value_state::all_same_type);
		auto grid = this->grid();
		uint32_t dpi = edge::dpi(grid->window().hwnd());
		float name_line_x = grid->expand_column_left(dpi) + indent() * grid->indent_width();
		return { name_line_x - grid->indent_width(), item_y, name_line_x, item_y + this->content_height() };
	}

	virtual void render (const render_context& rc, float y, bool selected, bool hot, bool focused) const override final
	{
		render_default_background(rc, y, selected, hot, focused);

		auto grid = this->grid();
		uint32_t dpi = edge::dpi(grid->window().hwnd());
		auto lt = grid->line_width(dpi);
		float pw = edge::pixel_width(dpi);
		float height = std::ceil(this->content_height() / pw) * pw;

		float name_line_x = grid->expand_column_left(dpi) + indent() * grid->indent_width();

		if (_value.first == value_state::all_same_type)
			render_expand_button(rc, y);

		rc.dc->DrawLine ({ name_line_x + lt/2, y }, { name_line_x + lt/2, y + height }, rc.disabled_fore, lt);
		rc.dc->DrawTextLayout ({ name_line_x + lt + text_lr_padding, y }, _name, rc.fore);

		if (auto& tl = _value.second)
		{
			float value_linex = grid->value_column_left(dpi);
			rc.dc->DrawLine ({ value_linex + lt/2, y }, { value_linex + lt/2, y + height }, rc.disabled_fore, lt);
			rc.dc->DrawTextLayout ({ grid->value_column_left(dpi) + lt + text_lr_padding, y }, _value.second, rc.fore);
		}
	}

	virtual float content_height() const override final
	{
		return std::max(_name.height(), _value.second.height());
	}

	virtual HCURSOR cursor_at (D2D1_POINT_2F pd, float item_y) const override final
	{
		auto grid = this->grid();
		if (grid->read_only())
			return ::LoadCursor(nullptr, IDC_ARROW);

		return ::LoadCursor(nullptr, IDC_HAND);
	}

	virtual bool selectable() const override final { return true; }

	virtual void on_mouse_down (const edge::mouse_ud_args& ma, float item_y) override final
	{
		if (_value.first == value_state::all_same_type)
		{
			if (point_in_rect(expand_button_click_rect(item_y), ma.pd))
			{
				if (expanded())
					collapse();
				else
					expand();
			}
		}
	}

	virtual void on_mouse_up (const edge::mouse_ud_args& ma, float item_y) override final
	{
		auto grid = this->grid();
		uint32_t dpi = edge::dpi(grid->window().hwnd());
		if (grid->read_only())
			return;
		auto vcx = grid->value_column_left(dpi);
		if (ma.pd.x < vcx)
			return;

		_popup.emplace(this, item_y, std::bind(&object_property_item::on_object_picked, this, std::placeholders::_1), grid->tcp());
	}

	virtual std::string description_title() const override final
	{
		return _prop->name();
	}

	virtual std::string description_text() const override final
	{
		auto ui_prop = dynamic_cast<const ui_property_i*>(property());
		return ui_prop && ui_prop->description() ? std::string(ui_prop->description()) : std::string();
	}

	virtual item_removing_e::subscriber item_removing() override final
	{
		return item_removing_e::subscriber(_em);
	}
	#pragma region property_item_i
	virtual const edge::object_property* property() const override final { return _prop; }

	// Following two function are called by the parent item (itself of type object_item) when our property changes.
	virtual void on_property_changing (size_t object_index, const edge::property_change_args& args) override final
	{
		auto* objprop_args = checked_static_cast<const object_property_change_args*>(&args);
		object* child_object_to_insert = objprop_args->other_child;
	}

	virtual void on_property_changed (size_t object_index, const edge::property_change_args& args) override final
	{
		auto* objprop_args = checked_static_cast<const object_property_change_args*>(&args);
		object* child_object_removed = objprop_args->other_child;
		perform_layout();
	}
	#pragma endregion

	void on_parent_objects_change (const change_args& args)
	{
		if (auto* inserting = std::get_if<inserting_args>(&args))
		{
			std::vector<object*> children;
			std::transform(inserting->objects_to_insert.begin(), inserting->objects_to_insert.end(), std::back_inserter(children), [p=_prop](object* parent) { return p->get(parent); });
			change_e::invoker(_em).invoke(inserting_args{ children });
		}
		else if (auto* inserted = std::get_if<inserted_args>(&args))
		{
			change_e::invoker(_em).invoke(args);
			perform_layout();
		}
		else if (auto* removing = std::get_if<removing_args>(&args))
		{
			change_e::invoker(_em).invoke(args);
		}
		else if (auto* removed = std::get_if<removed_args>(&args))
		{
			std::vector<object*> children;
			std::transform(removed->objects_removed.begin(), removed->objects_removed.end(), std::back_inserter(children), [p=_prop](object* parent) { return p->get(parent); });
			change_e::invoker(_em).invoke(removed_args{ children });
			perform_layout();
		}
		else if (auto* replacing = std::get_if<replacing_args>(&args))
		{
			std::vector<object*> children_to_insert;
			std::transform(replacing->new_objs.begin(), replacing->new_objs.end(), std::back_inserter(children_to_insert), [p=_prop](object* parent) { return p->get(parent); });
			change_e::invoker(_em).invoke(replacing_args{ replacing->index, children_to_insert });
		}
		else if (auto* replaced = std::get_if<replaced_args>(&args))
		{
			std::vector<object*> children_removed;
			std::transform(replaced->old_objs.begin(), replaced->old_objs.end(), std::back_inserter(children_removed),  [p=_prop](object* parent) { return p->get(parent); });
			change_e::invoker(_em).invoke(replaced_args{ replaced->index, children_removed });
			perform_layout();
		}
		else
			rassert(false);
	}

	void on_object_picked (const edge::concrete_type* type)
	{
		auto& objs = parent()->parent()->objects();
		bool different_type_selected = objs.any([prop=_prop,type](object* o) { return (prop->get(o) ? prop->get(o)->type() : nullptr) != type; });
		if (different_type_selected)
		{
			this->grid()->change_property (objs, _prop, type);
			expand_all();
		}

		_popup.reset();
	}

	#pragma region expandable_item_i
	virtual item_i* as_item() override final { return this; }

	virtual size_t child_count() const override final { return _child_manager.has_value() ? _child_manager->children().size() : 0; }

	virtual item_i* child_at(size_t index) const override final
	{
		return _child_manager->children()[index].get();
	}

	virtual bool expanded() const override final { return _child_manager.has_value(); }
	
	virtual void expand() override final
	{
		rassert(!_child_manager);
		_child_manager.emplace(this, *this);
		this->grid()->invalidate();
	}

	virtual void collapse() override final
	{
		if (_child_manager)
		{
			_child_manager.reset();
			this->grid()->invalidate();
		}
	}
	#pragma endregion

	#pragma region object_item_i
	virtual object_list_i& objects() override final
	{
		return *this;
	}
	#pragma endregion
};

std::unique_ptr<property_item_i> make_object_property_item (group_item_i* parent, const edge::object_property* prop)
{
	return std::make_unique<object_property_item>(parent, prop);
}
