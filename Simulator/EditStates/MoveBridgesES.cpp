
#include "pch.h"
#include "EditState.h"
#include "Bridge.h"

using namespace std;

class MoveBridgeES : public EditState
{
	typedef EditState base;
	D2D1_POINT_2F _mouseDownWLocation;
	vector<D2D1_SIZE_F> _offsets;
	bool _completed = false;

public:
	MoveBridgeES (const EditStateDeps& deps)
		: base(deps)
	{
		assert (all_of(_selection->GetObjects().begin(), _selection->GetObjects().end(),
			[](const ComPtr<Object>& so) { return dynamic_cast<Bridge*>(so.Get()); }));
	}

	virtual void OnMouseDown (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation, MouseButton button, HTCodeAndObject ht) override final
	{
		_mouseDownWLocation = wLocation;
		for (auto o : _selection->GetObjects())
		{
			auto b = dynamic_cast<Bridge*>(o.Get()); assert (b != nullptr);
			_offsets.push_back ({ wLocation.x - b->GetLeft(), wLocation.y - b->GetTop() });
		}
	}

	virtual void OnMouseMove (D2D1_POINT_2F dLocation, D2D1_POINT_2F wLocation) override final
	{
		for (size_t i = 0; i < _selection->GetObjects().size(); i++)
		{
			auto b = dynamic_cast<Bridge*>(_selection->GetObjects()[i].Get()); assert (b != nullptr);
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
