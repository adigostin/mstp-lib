
#include "pch.h"
#include "EditState.h"
#include "Bridge.h"

using namespace std;

class MoveBridgeES : public EditState
{
	using base = EditState;

	D2D1_POINT_2F _firstBridgeInitialLocation;
	D2D1_SIZE_F _offsetFirstBridge;

	struct Info
	{
		Bridge* b;
		D2D1_SIZE_F offsetFromFirst;
	};

	std::vector<Info> _infos;
	bool _completed = false;

public:
	using base::base;

	virtual void OnMouseDown (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		auto firstBridge = static_cast<Bridge*>(_selection->GetObjects()[0]); assert (firstBridge != nullptr);
		_firstBridgeInitialLocation = firstBridge->GetLocation();

		for (auto o : _selection->GetObjects())
		{
			auto b = dynamic_cast<Bridge*>(o); assert (b != nullptr);
			_infos.push_back ({ b, b->GetLocation() - firstBridge->GetLocation() });
		}

		_offsetFirstBridge = location.w - firstBridge->GetLocation();
	}

	virtual void OnMouseMove (const MouseLocation& location) override final
	{
		auto firstBridgeLocation = location.w - _offsetFirstBridge;
		_infos[0].b->SetLocation(firstBridgeLocation);
		for (size_t i = 1; i < _infos.size(); i++)
			_infos[i].b->SetLocation (firstBridgeLocation + _infos[i].offsetFromFirst);
	}

	virtual std::optional<LRESULT> OnKeyDown (UINT virtualKey, UINT modifierKeys) override final
	{
		if (virtualKey == VK_ESCAPE)
		{
			_infos[0].b->SetLocation (_firstBridgeInitialLocation);
			for (size_t i = 1; i < _infos.size(); i++)
				_infos[i].b->SetLocation (_firstBridgeInitialLocation + _infos[i].offsetFromFirst);

			_completed = true;
			::InvalidateRect (_pw->GetEditArea()->GetHWnd(), nullptr, FALSE);
			return 0;
		}

		return nullopt;
	}

	virtual void OnMouseUp (MouseButton button, UINT modifierKeysDown, const MouseLocation& location) override final
	{
		struct Action : public EditAction
		{
			D2D1_POINT_2F const _firstBridgeInitialLocation;
			D2D1_POINT_2F const _firstBridgeFinalLocation;
			vector<Info> const _infos;

			Action (D2D1_POINT_2F firstBridgeInitialLocation, D2D1_POINT_2F firstBridgeFinalLocation, vector<Info>&& infos)
				: _firstBridgeInitialLocation(firstBridgeInitialLocation)
				, _firstBridgeFinalLocation(firstBridgeFinalLocation)
				, _infos(move(infos))
			{ }

			virtual void Redo() override final
			{
				_infos[0].b->SetLocation (_firstBridgeFinalLocation);
				for (size_t i = 0; i < _infos.size(); i++)
					_infos[i].b->SetLocation (_firstBridgeFinalLocation + _infos[i].offsetFromFirst);
			}

			virtual void Undo() override final
			{
				_infos[0].b->SetLocation (_firstBridgeInitialLocation);
				for (size_t i = 0; i < _infos.size(); i++)
					_infos[i].b->SetLocation (_firstBridgeInitialLocation + _infos[i].offsetFromFirst);
			}
		};

		auto action = unique_ptr<EditAction>(new Action (_firstBridgeInitialLocation, _infos[0].b->GetLocation(), move(_infos)));
		_actionList->AddPerformedUserAction (L"Move bridges", move(action));
		_completed = true;
	}

	virtual bool Completed() const override final { return _completed; }
};

unique_ptr<EditState> CreateStateMoveBridges (const EditStateDeps& deps) { return unique_ptr<EditState>(new MoveBridgeES(deps)); }
