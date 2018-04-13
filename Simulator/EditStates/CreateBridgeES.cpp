
#include "pch.h"
#include "EditState.h"
#include "Bridge.h"
#include "Win32/UtilityFunctions.h"

using namespace std;
using namespace D2D1;

class CreateBridgeES : public EditState
{
	typedef EditState base;
	bool _completed = false;
	unique_ptr<Bridge> _bridge;

public:
	using base::base;

	virtual void OnMouseMove (const MouseLocation& location) override final
	{
		if (_bridge == nullptr)
		{
			unsigned int portCount = 4;
			unsigned int mstiCount = 4;
			size_t macAddressesToReserve = std::max ((size_t) 1 + portCount, (size_t) 16);
			auto macAddress = _pw->GetProject()->AllocMacAddressRange(macAddressesToReserve);
			_bridge.reset (new Bridge (portCount, mstiCount, macAddress.bytes));
		}

		_bridge->SetLocation (location.w.x - _bridge->GetWidth() / 2, location.w.y - _bridge->GetHeight() / 2);
		::InvalidateRect (_pw->GetEditArea()->GetHWnd(), nullptr, FALSE);
	}

	virtual void OnMouseUp (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		if (_bridge != nullptr)
		{
			Bridge* b = _bridge.get();
			size_t insertIndex = _project->GetBridges().size();
			_project->InsertBridge(insertIndex, move(_bridge));
			STP_StartBridge (_project->GetBridges().back()->GetStpBridge(), GetMessageTime());
			_project->SetChangedFlag(true);
			_selection->Select(b);
		}

		_completed = true;
	}

	void RecreateBridge (unsigned int numberOfPorts)
	{
		auto centerX = _bridge->GetLeft() + _bridge->GetWidth() / 2;
		auto centerY = _bridge->GetTop() + _bridge->GetHeight() / 2;
		_bridge.reset (new Bridge(numberOfPorts, _bridge->GetMstiCount(), _bridge->GetBridgeAddress().data()));
		_bridge->SetLocation (centerX - _bridge->GetWidth() / 2, centerY - _bridge->GetHeight() / 2);
		::InvalidateRect (_pw->GetEditArea()->GetHWnd(), nullptr, FALSE);
	}

	virtual std::optional<LRESULT> OnKeyDown (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_completed = true;
			::InvalidateRect (_pw->GetEditArea()->GetHWnd(), nullptr, FALSE);
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

	virtual void Render (ID2D1RenderTarget* rt) override final
	{
		if (_bridge != nullptr)
		{
			auto zoom_tr = _pw->GetEditArea()->GetZoomTransform();
			D2D1_MATRIX_3X2_F oldtr;
			rt->GetTransform(&oldtr);
			rt->SetTransform(zoom_tr);
			_bridge->Render (rt, _pw->GetEditArea()->GetDrawingObjects(), _pw->GetSelectedVlanNumber(), ColorF(ColorF::LightGreen));
			rt->SetTransform(&oldtr);

			auto x = _bridge->GetLeft() + _bridge->GetWidth() / 2;
			auto y = _bridge->GetBottom() + Port::ExteriorHeight * 1.1f;
			auto centerD = zoom_tr.TransformPoint({ x, y });
			_editArea->RenderHint (rt, centerD, L"Press + or - to change the number of ports.",
								   DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, true);
		}
	}

	virtual bool Completed() const override final { return _completed; }
};

unique_ptr<EditState> CreateStateCreateBridge (const EditStateDeps& deps) { return unique_ptr<EditState>(new CreateBridgeES(deps)); }
