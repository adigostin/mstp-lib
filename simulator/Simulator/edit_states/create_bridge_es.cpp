
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "edit_state.h"

using namespace edge;

class create_bridge_es : public edit_state
{
	using base = edit_state;

	static constexpr mac_address null_address = { 0, 0, 0, 0, 0, 0 };
	bool _completed = false;
	com_ptr<IBridge> _bridge;

	using base::base;

	virtual handled OnMouseButtonDown (mouse_button button, UINT mks, const mouse_location& ml) override final
	{
		return handled(true); // discard it
	}

	virtual void OnMouseMove (const mouse_location& ml) override final
	{
		if (_bridge == nullptr)
		{
			auto hr = make_temp_bridge(4, 4, ml.w, &_bridge); LOG_IF_FAILED(hr);
		}

		auto size = _bridge->size();
		_bridge->set_location ({ ml.w.x - size.cx / 2, ml.w.y - size.cy / 2 });
		InvalidateRect(_ew->hWnd(), nullptr, 0);
	}

	virtual handled OnMouseButtonUp (mouse_button button, UINT mks, const mouse_location& ml) override final
	{
		if (button != mouse_button::left)
			return handled(true); // discard it

		if (_bridge)
		{
			size_t number_of_addresses_to_reserve = (_bridge->PortCount() + 15) / 16 * 16;
			auto bridge_address = _project->alloc_mac_address_range(number_of_addresses_to_reserve);
			com_ptr<IBridge> b;
			// Make a new bridge with the correct MAC address so it gets a MST config name
			auto hr = MakeBridge (_bridge->PortCount(), _bridge->msti_count(), bridge_address, &b); LOG_IF_FAILED(hr);
			b->set_stp_enabled(true);
			b->set_location(_bridge->location());
			_project->AddBridge(b);
			_project->SetChangedFlag(true);
			_selection->select(b.try_query<IDispatch>());
		}

		_completed = true;

		return handled(true);
	}

	static HRESULT make_temp_bridge (uint32_t port_count, uint32_t msti_count, POINT center, IBridge** ppBridge)
	{
		com_ptr<IBridge> bridge;
		auto hr = MakeBridge (port_count, msti_count, null_address, &bridge); RETURN_IF_FAILED(hr);
		auto size = bridge->size();
		bridge->set_location ({ center.x - size.cx / 2, center.y - size.cy / 2 });
		*ppBridge = bridge.detach();
		return S_OK;
	}

	virtual handled process_key_or_syskey_down (uint32_t virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_completed = true;
			InvalidateRect(_ew->hWnd(), nullptr, 0);
			return handled(true);
		}

		static constexpr UINT keys[] = { VK_SUBTRACT, VK_OEM_MINUS, VK_LEFT, VK_ADD, VK_OEM_PLUS, VK_RIGHT, VK_UP, VK_DOWN };
		if ((_bridge != nullptr) && (std::find(std::begin(keys), std::end(keys), virtualKey) != std::end(keys)))
		{
			uint32_t new_port_count = _bridge->PortCount();
			uint32_t new_msti_count = _bridge->msti_count();

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

			if ((new_port_count != _bridge->PortCount()) || (new_msti_count != _bridge->msti_count()))
			{
				auto size = _bridge->size();
				POINT center = { _bridge->left() + size.cx / 2, _bridge->top() + size.cy / 2 };
				auto hr = make_temp_bridge (new_port_count, new_msti_count, center, &_bridge); LOG_IF_FAILED(hr);
				::InvalidateRect (_ew->hWnd(), nullptr, FALSE);
			}

			return handled(true);
		}

		return handled(false);
	}

	virtual void render (ID2D1DeviceContext* dc) override final
	{
		if (_bridge != nullptr)
		{
			D2D1::Matrix3x2F oldtr;
			dc->GetTransform(&oldtr);
			dc->SetTransform (_ew->zoomer()->zoom_transform() * oldtr);

			DWORD vlan;
			_vlanSelection->GetSelectedVlan(&vlan);
			_bridge->Render (dc, _ew->drawing_resources(), vlan, D2D1::ColorF(D2D1::ColorF::LightGreen));

			dc->SetTransform(&oldtr);

			auto size = _bridge->size();
			auto x = _bridge->left() + size.cx / 2;
			auto y = _bridge->bottom() + PortExteriorHeight * 1.1f;
			auto centerD = _ew->zoomer()->zoom_transform().TransformPoint({ (float)x, (float)y });
			wil::unique_process_heap_string ss;
			auto hr = wil::str_printf_nothrow(ss, L"Port Count = %u, MSTI Count = %u\r\n"
				"Press Arrow Left / Right to change the number of ports.\r\n"
				"Press Arrow Up / Down to change the number of MSTIs.",
				_bridge->PortCount(), _bridge->msti_count()); LOG_IF_FAILED(hr);
			_ew->RenderHint (dc, centerD, ss.get(), DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, true);
		}
	}

	virtual bool completed() const override final { return _completed; }
};

std::unique_ptr<edit_state> create_state_create_bridge (const edit_state_deps& deps) { return std::make_unique<create_bridge_es>(deps); }
