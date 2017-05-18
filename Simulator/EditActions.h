
#pragma once
#include "Simulator.h"

struct DeleteEditAction : public EditAction
{
	IProject* const _project;
	std::vector<std::pair<std::unique_ptr<Bridge>, size_t>> _bridges;
	std::vector<std::pair<std::unique_ptr<Wire>,   size_t>> _wires;
	std::vector<IProject::ConvertedWirePoint> _convertedWirePoints;

	DeleteEditAction (IProject* project, ISelection* selection)
		: _project(project)
	{
		for (size_t bi = 0; bi < project->GetBridges().size(); bi++)
		{
			if (selection->Contains(project->GetBridges()[bi].get()))
				_bridges.push_back ({ nullptr, bi });
		}

		for (size_t wi = 0; wi < project->GetWires().size(); wi++)
		{
			if (selection->Contains(project->GetWires()[wi].get()))
				_wires.push_back ({ nullptr, wi });
		}
	}

	virtual void Redo() override final
	{
		for (auto it = _wires.rbegin(); it != _wires.rend(); it++)
			it->first = _project->RemoveWire(it->second);

		for (auto it = _bridges.rbegin(); it != _bridges.rend(); it++)
			it->first = _project->RemoveBridge (it->second, &_convertedWirePoints);
	}

	virtual void Undo() override final
	{
		for (auto it = _bridges.begin(); it != _bridges.end(); it++)
			_project->InsertBridge (it->second, move(it->first), &_convertedWirePoints);

		for (auto it = _wires.begin(); it != _wires.end(); it++)
			_project->InsertWire (it->second, move(it->first));
	}

	virtual std::string GetName() const override final { return "Delete objects"; }
};

