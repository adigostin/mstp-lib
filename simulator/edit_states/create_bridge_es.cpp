
#include "pch.h"
#include "edit_state.h"
#include "bridge.h"
#include "win32/d2d_window.h"

class create_bridge_es : public edit_state
{
	using base = edit_state;

	static constexpr mac_address null_address = { 0, 0, 0, 0, 0, 0 };
	bool _completed = false;
	std::unique_ptr<bridge> _bridge;

	using base::base;

	virtual void process_mouse_move (const mouse_location& ml) override final
	{
		if (_bridge == nullptr)
			_bridge = make_temp_bridge(4, 4, ml.w);

		_bridge->set_location (ml.w.x - _bridge->width() / 2, ml.w.y - _bridge->height() / 2);
		::InvalidateRect (_ew->hwnd(), nullptr, FALSE);
	}

	virtual void process_mouse_button_up (edge::mouse_button button, UINT modifierKeysDown, const mouse_location& location) override final
	{
		if (_bridge != nullptr)
		{
			size_t number_of_addresses_to_reserve = (_bridge->port_count() + 15) / 16 * 16;
			auto bridge_address = _project->alloc_mac_address_range(number_of_addresses_to_reserve);
			auto b = std::make_unique<bridge>(_bridge->port_count(), _bridge->msti_count(), bridge_address);
			b->set_stp_enabled(true);
			b->set_location(_bridge->location());

			size_t insert_index = _project->bridges().size();
			_project->insert_bridge(insert_index, std::move(b));
			_project->SetChangedFlag(true);
			_selection->select(_project->bridges().back().get());
		}

		_completed = true;
	}

	static std::unique_ptr<bridge> make_temp_bridge (size_t port_count, size_t msti_count, D2D1_POINT_2F center)
	{
		auto new_bridge = std::make_unique<bridge>(port_count, msti_count, null_address);
		new_bridge->set_location (center.x - new_bridge->width() / 2, center.y - new_bridge->height() / 2);
		return new_bridge;
	}

	virtual std::optional<LRESULT> process_key_or_syskey_down (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_completed = true;
			_ew->invalidate();
			return 0;
		}

		static constexpr UINT keys[] = { VK_SUBTRACT, VK_OEM_MINUS, VK_LEFT, VK_ADD, VK_OEM_PLUS, VK_RIGHT, VK_UP, VK_DOWN };
		if ((_bridge != nullptr) && (std::find(std::begin(keys), std::end(keys), virtualKey) != std::end(keys)))
		{
			size_t new_port_count = _bridge->port_count();
			size_t new_msti_count = _bridge->msti_count();

			if ((virtualKey == VK_SUBTRACT) || (virtualKey == VK_OEM_MINUS) || (virtualKey == VK_LEFT))
			{
				if (new_port_count > 1)
					new_port_count--;
			}
			else if ((virtualKey == VK_ADD) || (virtualKey == VK_OEM_PLUS) || (virtualKey == VK_RIGHT))
			{
				if (new_port_count < 4095)
					new_port_count++;
			}
			else if (virtualKey == VK_UP)
			{
				if (new_msti_count < 64)
					new_msti_count++;
			}
			else if (virtualKey == VK_DOWN)
			{
				if (new_msti_count > 0)
					new_msti_count--;
			}

			if ((new_port_count != _bridge->port_count()) || (new_msti_count != _bridge->msti_count()))
			{
				D2D1_POINT_2F center = { _bridge->left() + _bridge->width() / 2, _bridge->top() + _bridge->height() / 2 };
				_bridge = make_temp_bridge (new_port_count, new_msti_count, center);
				_ew->invalidate();
			}

			return 0;
		}

		return std::nullopt;
	}

	virtual void render (ID2D1DeviceContext* dc) override final
	{
		if (_bridge != nullptr)
		{
			D2D1::Matrix3x2F oldtr;
			dc->GetTransform(&oldtr);
			dc->SetTransform (_ew->zoom_transform() * oldtr);

			_bridge->render (dc, _ew->drawing_resources(), _pw->selected_vlan_number(), D2D1::ColorF(D2D1::ColorF::LightGreen));

			dc->SetTransform(&oldtr);

			auto x = _bridge->left() + _bridge->width() / 2;
			auto y = _bridge->bottom() + port::ExteriorHeight * 1.1f;
			auto centerD = _ew->zoom_transform().TransformPoint({ x, y });
			std::stringstream ss;
			ss << "Port Count = " << _bridge->port_count() << ", MSTI Count = " << _bridge->msti_count() << "\r\n"
				<< "Press Arrow Left / Right to change the number of ports.\r\n"
				<< "Press Arrow Up / Down to change the number of MSTIs.";
			_ew->render_hint (dc, centerD, ss.str(), DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, true);
		}
	}

	virtual bool completed() const override final { return _completed; }
};

std::unique_ptr<edit_state> create_state_create_bridge (const edit_state_deps& deps) { return std::make_unique<create_bridge_es>(deps); }
