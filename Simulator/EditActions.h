
#pragma once
#include "Simulator.h"
#include "Wire.h"

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

struct EnableDisableStpAction : EditAction
{
	std::vector<std::pair<Bridge*, bool>> _bridgesAndOldEnable;
	bool _newEnable;

	EnableDisableStpAction (const std::vector<Bridge*>& bridges, bool newEnable)
	{
		std::transform (bridges.begin(), bridges.end(), std::back_inserter(_bridgesAndOldEnable),
						[](Bridge* b) { return std::make_pair(b, STP_IsBridgeStarted(b->GetStpBridge())); });
		_newEnable = newEnable;
	}

	virtual void Redo() override final
	{
		auto timestamp = GetTimestampMilliseconds();
		for (auto& p : _bridgesAndOldEnable)
		{
			if (_newEnable)
				STP_StartBridge(p.first->GetStpBridge(), timestamp);
			else
				STP_StopBridge(p.first->GetStpBridge(), timestamp);
		}
	}

	virtual void Undo() override final
	{
		auto timestamp = GetTimestampMilliseconds();
		for (auto& p : _bridgesAndOldEnable)
		{
			if (p.second)
				STP_StartBridge(p.first->GetStpBridge(), timestamp);
			else
				STP_StopBridge(p.first->GetStpBridge(), timestamp);
		}
	}

	virtual std::string GetName() const override final { return std::string (_newEnable ? "Enable STP" : "Disable STP"); }
};

struct SetBridgeAddressAction : public EditAction
{
	std::vector<std::pair<Bridge*, STP_BRIDGE_ADDRESS>> _bridgesAndOldAddresses;
	STP_BRIDGE_ADDRESS _newAddress;

	SetBridgeAddressAction (const std::vector<Bridge*>& bridges, STP_BRIDGE_ADDRESS newAddress)
	{
		std::transform (bridges.begin(), bridges.end(), back_inserter(_bridgesAndOldAddresses),
						[](Bridge* b) { return std::make_pair(b, *STP_GetBridgeAddress(b->GetStpBridge())); });
		_newAddress = newAddress;
	}

	virtual void Redo() override final
	{
		auto timestamp = GetTimestampMilliseconds();
		for (auto& p : _bridgesAndOldAddresses)
			STP_SetBridgeAddress (p.first->GetStpBridge(), _newAddress.bytes, timestamp);
	}
	virtual void Undo() override final
	{
		auto timestamp = GetTimestampMilliseconds();
		for (auto& p : _bridgesAndOldAddresses)
			STP_SetBridgeAddress (p.first->GetStpBridge(), p.second.bytes, timestamp);
	}

	virtual std::string GetName() const override final { return "Set Bridge Address"; }
};

struct SetMstConfigNameAction : public EditAction
{
	std::vector<std::pair<Bridge*, std::string>> _bridgesAndOldNames;
	std::string _newName;

	SetMstConfigNameAction (const std::vector<Bridge*>& bridges, std::string&& newName)
	{
		std::transform (bridges.begin(), bridges.end(), back_inserter(_bridgesAndOldNames),
						[](Bridge* b) { return std::make_pair(b, std::string(STP_GetMstConfigId(b->GetStpBridge())->ConfigurationName, 32)); });
		_newName = move(newName);
	}

	virtual void Redo() override final
	{
		auto timestamp = GetTimestampMilliseconds();
		for (auto& p : _bridgesAndOldNames)
			STP_SetMstConfigName (p.first->GetStpBridge(), _newName.c_str(), timestamp);
	}

	virtual void Undo() override final
	{
		auto timestamp = GetTimestampMilliseconds();
		for (auto& p : _bridgesAndOldNames)
			STP_SetMstConfigName (p.first->GetStpBridge(), p.second.c_str(), timestamp);
	}

	virtual std::string GetName() const override final { return "Set MST Config Name"; }
};

struct ChangeStpVersionAction : public EditAction
{
	std::vector<std::pair<Bridge*, STP_VERSION>> _bridgesAndOldVersions;
	STP_VERSION _newVersion;

	ChangeStpVersionAction (const std::vector<Bridge*>& bridges, STP_VERSION newVersion)
	{
		std::transform (bridges.begin(), bridges.end(), back_inserter(_bridgesAndOldVersions),
						[](Bridge* b) { return std::make_pair(b, STP_GetStpVersion(b->GetStpBridge())); });
		_newVersion = newVersion;
	}

	virtual void Redo() override final
	{
		auto timestamp = GetTimestampMilliseconds();
		for (auto& p : _bridgesAndOldVersions)
			STP_SetStpVersion (p.first->GetStpBridge(), _newVersion, timestamp);
	}

	virtual void Undo() override final
	{
		auto timestamp = GetTimestampMilliseconds();
		for (auto& p : _bridgesAndOldVersions)
			STP_SetStpVersion (p.first->GetStpBridge(), p.second, timestamp);
	}

	virtual std::string GetName() const override final { return "Change STP version"; }
};

struct ChangeBridgePrioAction : EditAction
{
	std::vector<std::pair<Bridge*, unsigned short>> _bridgesAndOldPrios;
	unsigned short _newPrio;
	unsigned int _vlan;

	ChangeBridgePrioAction(const std::vector<Bridge*> bridges, unsigned short newPrio, unsigned int vlan)
	{
		auto getPrio = [](Bridge* b, unsigned int vlan) { return STP_GetBridgePriority(b->GetStpBridge(), STP_GetTreeIndexFromVlanNumber(b->GetStpBridge(), vlan)); };
		std::transform (bridges.begin(), bridges.end(), back_inserter(_bridgesAndOldPrios), [=](Bridge* b) { return std::make_pair(b, getPrio(b, vlan)); });
		_newPrio = newPrio;
		_vlan = vlan;
	}

	virtual void Redo() override final
	{
		auto timestamp = GetTimestampMilliseconds();
		for (auto& p : _bridgesAndOldPrios)
		{
			auto treeIndex = STP_GetTreeIndexFromVlanNumber (p.first->GetStpBridge(), _vlan);
			STP_SetBridgePriority (p.first->GetStpBridge(), treeIndex, _newPrio, timestamp);
		}
	}

	virtual void Undo() override final
	{
		auto timestamp = GetTimestampMilliseconds();
		for (auto& p : _bridgesAndOldPrios)
		{
			auto treeIndex = STP_GetTreeIndexFromVlanNumber (p.first->GetStpBridge(), _vlan);
			STP_SetBridgePriority (p.first->GetStpBridge(), treeIndex, p.second, timestamp);
		}
	}

	virtual std::string GetName() const override final { return "Change bridge priority"; }
};

