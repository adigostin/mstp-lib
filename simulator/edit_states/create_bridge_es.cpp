
#include "pch.h"
#include "edit_state.h"
#include "Bridge.h"
#include "win32/utility_functions.h"

using namespace std;
using namespace D2D1;
using namespace edge;

class CreateBridgeES : public edit_state
{
	typedef edit_state base;
	bool _completed = false;
	unique_ptr<Bridge> _bridge;

public:
	using base::base;

	virtual void process_mouse_move (const MouseLocation& location) override final
	{
		if (_bridge == nullptr)
		{
			unsigned int portCount = 4;
			unsigned int mstiCount = 4;
			size_t macAddressesToReserve = std::max ((size_t) 1 + portCount, (size_t) 16);
			auto macAddress = _project->AllocMacAddressRange(macAddressesToReserve);
			_bridge.reset (new Bridge (portCount, mstiCount, macAddress));
		}

		_bridge->SetLocation (location.w.x - _bridge->GetWidth() / 2, location.w.y - _bridge->GetHeight() / 2);
		::InvalidateRect (_ea->hwnd(), nullptr, FALSE);
	}

	virtual void process_mouse_button_up (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		if (_bridge != nullptr)
		{
			Bridge* b = _bridge.get();
			size_t insertIndex = _project->bridges().size();
			_project->insert_bridge(insertIndex, move(_bridge));
			STP_StartBridge (_project->bridges().back()->stp_bridge(), GetMessageTime());
			_project->SetChangedFlag(true);
			_selection->select(b);
		}

		_completed = true;
	}

	void RecreateBridge (unsigned int numberOfPorts)
	{
		auto centerX = _bridge->GetLeft() + _bridge->GetWidth() / 2;
		auto centerY = _bridge->GetTop() + _bridge->GetHeight() / 2;
		_bridge.reset (new Bridge(numberOfPorts, _bridge->GetMstiCount(), _bridge->bridge_address()));
		_bridge->SetLocation (centerX - _bridge->GetWidth() / 2, centerY - _bridge->GetHeight() / 2);
		::InvalidateRect (_ea->hwnd(), nullptr, FALSE);
	}

	virtual std::optional<LRESULT> process_key_or_syskey_down (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_completed = true;
			::InvalidateRect (_ea->hwnd(), nullptr, FALSE);
			return 0;
		}

		if ((virtualKey == VK_SUBTRACT) || (virtualKey == VK_OEM_MINUS))
		{
			if (_bridge->GetPortCount() > 2)
				RecreateBridge (_bridge->GetPortCount() - 1);
			return 0;
		}

		if ((virtualKey == VK_ADD) || (virtualKey == VK_OEM_PLUS))
		{
			if (_bridge->GetPortCount() < 4095)
				RecreateBridge (_bridge->GetPortCount() + 1);
			return 0;
		}

		return nullopt;
	}

	virtual void render (ID2D1DeviceContext* dc) override final
	{
		if (_bridge != nullptr)
		{
			Matrix3x2F oldtr;
			dc->GetTransform(&oldtr);
			dc->SetTransform (_ea->GetZoomTransform() * oldtr);

			_bridge->Render (dc, _ea->drawing_resources(), _pw->selected_vlan_number(), ColorF(ColorF::LightGreen));

			dc->SetTransform(&oldtr);

			auto x = _bridge->GetLeft() + _bridge->GetWidth() / 2;
			auto y = _bridge->GetBottom() + Port::ExteriorHeight * 1.1f;
			auto centerD = _ea->GetZoomTransform().TransformPoint({ x, y });
			_ea->render_hint (dc, centerD, L"Press + or - to change the number of ports.",
								   DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, true);
		}
	}

	virtual bool completed() const override final { return _completed; }
};

unique_ptr<edit_state> CreateStateCreateBridge (const edit_state_deps& deps) { return unique_ptr<edit_state>(new CreateBridgeES(deps)); }
