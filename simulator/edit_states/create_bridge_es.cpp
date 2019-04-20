
#include "pch.h"
#include "edit_state.h"
#include "bridge.h"
#include "win32/utility_functions.h"

class create_bridge_es : public edit_state
{
	typedef edit_state base;
	bool _completed = false;
	std::unique_ptr<bridge> _bridge;

public:
	using base::base;

	virtual void process_mouse_move (const mouse_location& location) override final
	{
		if (_bridge == nullptr)
		{
			uint32_t portCount = 4;
			uint32_t mstiCount = 4;
			size_t macAddressesToReserve = std::max ((size_t) 1 + portCount, (size_t) 16);
			auto macAddress = _project->AllocMacAddressRange(macAddressesToReserve);
			_bridge.reset (new bridge (portCount, mstiCount, macAddress));
		}

		_bridge->SetLocation (location.w.x - _bridge->GetWidth() / 2, location.w.y - _bridge->GetHeight() / 2);
		::InvalidateRect (_ew->hwnd(), nullptr, FALSE);
	}

	virtual void process_mouse_button_up (edge::mouse_button button, UINT modifierKeysDown, const mouse_location& location) override final
	{
		if (_bridge != nullptr)
		{
			bridge* b = _bridge.get();
			size_t insertIndex = _project->bridges().size();
			_project->insert_bridge(insertIndex, move(_bridge));
			STP_StartBridge (_project->bridges().back()->stp_bridge(), GetMessageTime());
			_project->SetChangedFlag(true);
			_selection->select(b);
		}

		_completed = true;
	}

	void recreate_bridge (unsigned int numberOfPorts)
	{
		auto centerX = _bridge->GetLeft() + _bridge->GetWidth() / 2;
		auto centerY = _bridge->GetTop() + _bridge->GetHeight() / 2;
		_bridge.reset (new bridge(numberOfPorts, _bridge->msti_count(), _bridge->bridge_address()));
		_bridge->SetLocation (centerX - _bridge->GetWidth() / 2, centerY - _bridge->GetHeight() / 2);
		::InvalidateRect (_ew->hwnd(), nullptr, FALSE);
	}

	virtual std::optional<LRESULT> process_key_or_syskey_down (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_completed = true;
			::InvalidateRect (_ew->hwnd(), nullptr, FALSE);
			return 0;
		}

		if ((virtualKey == VK_SUBTRACT) || (virtualKey == VK_OEM_MINUS))
		{
			if (_bridge->port_count() > 2)
				recreate_bridge (_bridge->port_count() - 1);
			return 0;
		}

		if ((virtualKey == VK_ADD) || (virtualKey == VK_OEM_PLUS))
		{
			if (_bridge->port_count() < 4095)
				recreate_bridge (_bridge->port_count() + 1);
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
			dc->SetTransform (_ew->GetZoomTransform() * oldtr);

			_bridge->Render (dc, _ew->drawing_resources(), _pw->selected_vlan_number(), D2D1::ColorF(D2D1::ColorF::LightGreen));

			dc->SetTransform(&oldtr);

			auto x = _bridge->GetLeft() + _bridge->GetWidth() / 2;
			auto y = _bridge->GetBottom() + port::ExteriorHeight * 1.1f;
			auto centerD = _ew->GetZoomTransform().TransformPoint({ x, y });
			_ew->render_hint (dc, centerD, L"Press + or - to change the number of ports.",
								   DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, true);
		}
	}

	virtual bool completed() const override final { return _completed; }
};

std::unique_ptr<edit_state> create_state_create_bridge (const edit_state_deps& deps) { return std::make_unique<create_bridge_es>(deps); }
