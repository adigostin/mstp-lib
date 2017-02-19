
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

	virtual void OnMouseDown (const MouseLocation& location, MouseButton button) override final
	{
		_mouseDownWLocation = location.w;
		for (auto o : _selection->GetObjects())
		{
			auto b = dynamic_cast<Bridge*>(o.Get()); assert (b != nullptr);
			_offsets.push_back ({ location.w.x - b->GetLeft(), location.w.y - b->GetTop() });
		}
	}

	virtual void OnMouseMove (const MouseLocation& location) override final
	{
		for (size_t i = 0; i < _selection->GetObjects().size(); i++)
		{
			auto b = dynamic_cast<Bridge*>(_selection->GetObjects()[i].Get()); assert (b != nullptr);
			b->SetLocation (location.w.x - _offsets[i].width, location.w.y - _offsets[i].height);
		}
	}

	virtual void OnMouseUp (const MouseLocation& location, MouseButton button) override final
	{
		_completed = true;
	}

	virtual bool Completed() const override final { return _completed; }
};

unique_ptr<EditState> CreateStateMoveBridges (const EditStateDeps& deps) { return unique_ptr<EditState>(new MoveBridgeES(deps)); }
