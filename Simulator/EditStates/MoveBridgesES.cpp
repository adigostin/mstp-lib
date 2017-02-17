
#include "pch.h"
#include "EditState.h"

using namespace std;

class MoveBridgeES : public EditState
{
	typedef EditState base;
	D2D1_POINT_2F _mouseDownWLocation;
	vector<D2D1_SIZE_F> _offsets;
	bool _completed = false;

public:
	using base::base;

	virtual void OnMouseDown (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation, MouseButton button, Object* clickedObject) override final
	{
		_mouseDownWLocation = wLocation;
		for (auto o : _selection->GetObjects())
		{
			auto b = dynamic_cast<Bridge*>(o); assert (b != nullptr);
			_offsets.push_back ({ wLocation.x - b->GetLeft(), wLocation.y - b->GetTop() });
		}
	}

	virtual void OnMouseMove (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation) override final
	{
		for (size_t i = 0; i < _selection->GetObjects().size(); i++)
		{
			auto b = dynamic_cast<Bridge*>(_selection->GetObjects()[i]); assert (b != nullptr);
			b->SetLocation (wLocation.x - _offsets[i].width, wLocation.y - _offsets[i].height);
		}
	}

	virtual void OnMouseUp (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation, MouseButton button) override final
	{
		_completed = true;
	}

	virtual bool Completed() const override final { return _completed; }
};

unique_ptr<EditState> CreateStateMoveBridges (const EditStateDeps& deps) { return unique_ptr<EditState>(new MoveBridgeES(deps)); }
