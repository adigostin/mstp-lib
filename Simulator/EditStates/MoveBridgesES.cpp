
#include "pch.h"
#include "EditState.h"
#include "Bridge.h"

using namespace std;

class MoveBridgeES : public EditState
{
	typedef EditState base;
	D2D1_POINT_2F _mouseDownWLocation;
	vector<D2D1_POINT_2F> _initialLocations;
	vector<D2D1_SIZE_F> _offsets;
	bool _completed = false;

public:
	MoveBridgeES (const EditStateDeps& deps)
		: base(deps)
	{
		auto& objects = _selection->GetObjects();
		assert (all_of(objects.begin(), objects.end(),
			[](Object* so) { return dynamic_cast<Bridge*>(so) != nullptr; }));
	}

	virtual void OnMouseDown (const MouseLocation& location, MouseButton button) override final
	{
		_mouseDownWLocation = location.w;
		for (auto o : _selection->GetObjects())
		{
			auto b = dynamic_cast<Bridge*>(o); assert (b != nullptr);
			_initialLocations.push_back (b->GetLocation());
			_offsets.push_back ({ location.w.x - b->GetLeft(), location.w.y - b->GetTop() });
		}
	}

	virtual void OnMouseMove (const MouseLocation& location) override final
	{
		for (size_t i = 0; i < _selection->GetObjects().size(); i++)
		{
			auto b = dynamic_cast<Bridge*>(_selection->GetObjects()[i]); assert (b != nullptr);
			b->SetLocation (location.w.x - _offsets[i].width, location.w.y - _offsets[i].height);
		}
	}

	virtual std::optional<LRESULT> OnKeyDown (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			for (size_t i = 0; i < _selection->GetObjects().size(); i++)
				dynamic_cast<Bridge*>(_selection->GetObjects()[i])->SetLocation (_initialLocations[i]);

			_completed = true;
			::InvalidateRect (_pw->GetEditArea()->GetHWnd(), nullptr, FALSE);
			return 0;
		}

		return nullopt;
	}

	virtual void OnMouseUp (const MouseLocation& location, MouseButton button) override final
	{
		_completed = true;
	}

	virtual bool Completed() const override final { return _completed; }
};

unique_ptr<EditState> CreateStateMoveBridges (const EditStateDeps& deps) { return unique_ptr<EditState>(new MoveBridgeES(deps)); }
