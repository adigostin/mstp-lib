
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2017 Adi Gostin, distributed under Apache License v2.0.

#include "802_1Q_2011_procedures.h"
#include "../stp_bridge.h"
#include <assert.h>
#include <stddef.h>

// See §13.35 in 802.1Q-2011

enum
{
	UNDEFINED,

	INIT_PORT,
	DISABLE_PORT,
	DISABLED_PORT,

	MASTER_PORT,
	MASTER_PROPOSED,
	MASTER_AGREED,
	MASTER_SYNCED,
	MASTER_RETIRED,
	MASTER_FORWARD,
	MASTER_LEARN,
	MASTER_DISCARD,

	ROOT_PORT,
	ROOT_PROPOSED,
	ROOT_AGREED,
	ROOT_SYNCED,
	REROOT,
	ROOT_FORWARD,
	ROOT_LEARN,
	REROOTED,
	ROOT_DISCARD,

	DESIGNATED_PROPOSE,
	DESIGNATED_AGREE,
	DESIGNATED_SYNCED,
	DESIGNATED_RETIRED,
	DESIGNATED_FORWARD,
	DESIGNATED_LEARN,
	DESIGNATED_DISCARD,
	DESIGNATED_PORT,

	ALTERNATE_PROPOSED,
	ALTERNATE_AGREED,
	BLOCK_PORT,
	BACKUP_PORT,
	ALTERNATE_PORT,
};

// ============================================================================

const char* PortRoleTransitions_802_1Q_2011_GetStateName (SM_STATE state)
{
	switch (state)
	{
		case INIT_PORT:			return "INIT_PORT";
		case DISABLE_PORT:		return "DISABLE_PORT";
		case DISABLED_PORT:		return "DISABLED_PORT";

		case MASTER_PORT:		return "MASTER_PORT";
		case MASTER_PROPOSED:	return "MASTER_PROPOSED";
		case MASTER_AGREED:		return "MASTER_AGREED";
		case MASTER_SYNCED:		return "MASTER_SYNCED";
		case MASTER_RETIRED:	return "MASTER_RETIRED";
		case MASTER_FORWARD:	return "MASTER_FORWARD";
		case MASTER_LEARN:		return "MASTER_LEARN";
		case MASTER_DISCARD:	return "MASTER_DISCARD";

		case ROOT_PORT:			return "ROOT_PORT";
		case ROOT_PROPOSED:		return "ROOT_PROPOSED";
		case ROOT_AGREED:		return "ROOT_AGREED";
		case ROOT_SYNCED:		return "ROOT_SYNCED";
		case REROOT:			return "REROOT";
		case ROOT_FORWARD:		return "ROOT_FORWARD";
		case ROOT_LEARN:		return "ROOT_LEARN";
		case REROOTED:			return "REROOTED";
		case ROOT_DISCARD:		return "ROOT_DISCARD";

		case DESIGNATED_PROPOSE:	return "DESIGNATED_PROPOSE";
		case DESIGNATED_AGREE:		return "DESIGNATED_AGREE";
		case DESIGNATED_SYNCED:		return "DESIGNATED_SYNCED";
		case DESIGNATED_RETIRED:	return "DESIGNATED_RETIRED";
		case DESIGNATED_FORWARD:	return "DESIGNATED_FORWARD";
		case DESIGNATED_LEARN:		return "DESIGNATED_LEARN";
		case DESIGNATED_DISCARD:	return "DESIGNATED_DISCARD";
		case DESIGNATED_PORT:		return "DESIGNATED_PORT";

		case ALTERNATE_PORT:	return "ALTERNATE_PORT";
		case BACKUP_PORT:		return "BACKUP_PORT";
		case ALTERNATE_PROPOSED:return "ALTERNATE_PROPOSED";
		case BLOCK_PORT:		return "BLOCK_PORT";
		case ALTERNATE_AGREED:	return "ALTERNATE_AGREED";

		default:				return "(undefined)";
	}
}

// ============================================================================

SM_STATE PortRoleTransitions_802_1Q_2011_CheckConditions (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state)
{
	assert (givenPort != -1);
	assert (givenTree != -1);

	PORT* port = bridge->ports [givenPort];
	PORT_TREE* tree = port->trees [givenTree];

	// ------------------------------------------------------------------------
	// Check global conditions.

	if (bridge->BEGIN)
	{
		if (state == INIT_PORT)
		{
			// The entry block for this state has been executed already.
			return 0;
		}

		return INIT_PORT;
	}

	if (tree->selected && !tree->updtInfo)
	{
		if ((tree->selectedRole == STP_PORT_ROLE_DISABLED) && (tree->role != tree->selectedRole))
			return DISABLE_PORT;

		if ((tree->selectedRole == STP_PORT_ROLE_MASTER) && (tree->role != tree->selectedRole))
			return MASTER_PORT;

		if ((tree->selectedRole == STP_PORT_ROLE_ROOT) && (tree->role != tree->selectedRole))
			return ROOT_PORT;

		if ((tree->selectedRole == STP_PORT_ROLE_DESIGNATED) && (tree->role != tree->selectedRole))
			return DESIGNATED_PORT;

		if (((tree->selectedRole == STP_PORT_ROLE_ALTERNATE) || (tree->selectedRole == STP_PORT_ROLE_BACKUP)) && (tree->role != tree->selectedRole))
			return BLOCK_PORT;
	}

	// ------------------------------------------------------------------------
	// Check exit conditions from each state.

	// ------------------------------------------------------------------------
	// Disabled

	if (state == INIT_PORT)
		return DISABLE_PORT;

	if (state == DISABLE_PORT)
	{
		if (tree->selected && !tree->updtInfo)
		{
			if (!tree->learning && !tree->forwarding)
				return DISABLED_PORT;
		}

		return 0;
	}

	if (state == DISABLED_PORT)
	{
		if (tree->selected && !tree->updtInfo)
		{
			if ((tree->fdWhile != MaxAge (bridge, givenPort)) || tree->sync || tree->reRoot || !tree->synced)
				return DISABLED_PORT;
		}

		return 0;
	}

	// ------------------------------------------------------------------------
	// Master

	if (state == MASTER_PORT)
	{
		if (tree->selected && !tree->updtInfo)
		{
			if (((tree->sync && !tree->synced) || (tree->reRoot && (tree->rrWhile != 0)) || tree->disputed) && !port->operEdge && (tree->learn || tree->forward))
				return MASTER_DISCARD;

			if (((tree->fdWhile == 0) || allSynced (bridge, givenPort, givenTree)) && !tree->learn)
				return MASTER_LEARN;

			if (((tree->fdWhile == 0) || allSynced (bridge, givenPort, givenTree)) && (tree->learn && !tree->forward))
				return MASTER_FORWARD;

			if (tree->proposed && !tree->agree)
				return MASTER_PROPOSED;

			if ((allSynced (bridge, givenPort, givenTree) && !tree->agree) || (tree->proposed && tree->agree))
				return MASTER_AGREED;

			if ((!tree->learning && !tree->forwarding && !tree->synced) || (tree->agreed && !tree->synced) || (port->operEdge && !tree->synced) || (tree->sync && tree->synced))
				return MASTER_SYNCED;

			if (tree->reRoot && (tree->rrWhile == 0))
				return MASTER_RETIRED;
		}

		return 0;
	}

	if (state == MASTER_PROPOSED)
		return MASTER_PORT;

	if (state == MASTER_AGREED)
		return MASTER_PORT;

	if (state == MASTER_SYNCED)
		return MASTER_PORT;

	if (state == MASTER_RETIRED)
		return MASTER_PORT;

	if (state == MASTER_FORWARD)
		return MASTER_PORT;

	if (state == MASTER_LEARN)
		return MASTER_PORT;

	if (state == MASTER_DISCARD)
		return MASTER_PORT;

	// ------------------------------------------------------------------------
	// Root

	if (state == ROOT_PORT)
	{
		if (tree->selected && !tree->updtInfo)
		{
			if (tree->proposed && !tree->agree)
				return ROOT_PROPOSED;

			if ((allSynced (bridge, givenPort, givenTree) && !tree->agree) || (tree->proposed && tree->agree))
				return ROOT_AGREED;

			if ((tree->agreed && !tree->synced) || (tree->sync && tree->synced))
				return ROOT_SYNCED;

			if (!tree->forward && (tree->rbWhile == 0) && !tree->reRoot)
				return REROOT;

			if (tree->rrWhile != FwdDelay (bridge, givenPort))
				return ROOT_PORT;

			if (tree->disputed)
				return ROOT_DISCARD;

			if (tree->reRoot && tree->forward)
				return REROOTED;

			if (((tree->fdWhile == 0) || (reRooted (bridge, givenPort, givenTree) && (tree->rbWhile == 0) && rstpVersion (bridge))) && !tree->learn)
				return ROOT_LEARN;

			if (((tree->fdWhile == 0) || (reRooted (bridge, givenPort, givenTree) && (tree->rbWhile == 0) && rstpVersion (bridge))) && tree->learn && !tree->forward)
				return ROOT_FORWARD;
		}

		return 0;
	}

	if (state == ROOT_PROPOSED)
		return ROOT_PORT;

	if (state == ROOT_AGREED)
		return ROOT_PORT;

	if (state == ROOT_SYNCED)
		return ROOT_PORT;

	if (state == REROOT)
		return ROOT_PORT;

	if (state == ROOT_FORWARD)
		return ROOT_PORT;

	if (state == ROOT_LEARN)
		return ROOT_PORT;

	if (state == REROOTED)
		return ROOT_PORT;

	if (state == ROOT_DISCARD)
		return ROOT_PORT;

	// ------------------------------------------------------------------------
	// Designated

	if (state == DESIGNATED_PORT)
	{
		if (tree->selected && !tree->updtInfo)
		{
			if (!tree->forward && !tree->agreed && !tree->proposing && !port->operEdge)
				return DESIGNATED_PROPOSE;

			if (allSynced (bridge, givenPort, givenTree) && (tree->proposed || !tree->agree))
				return DESIGNATED_AGREE;

			if ((!tree->learning && !tree->forwarding && !tree->synced)
				|| (tree->agreed && !tree->synced)
				|| (port->operEdge && !tree->synced)
				|| (tree->sync && tree->synced))
			{
				return DESIGNATED_SYNCED;
			}

			if (tree->reRoot && (tree->rrWhile == 0))
				return DESIGNATED_RETIRED;

			if (((tree->sync && !tree->synced) || (tree->reRoot && (tree->rrWhile != 0)) || tree->disputed || port->isolate) && !port->operEdge && (tree->learn || tree->forward))
				return DESIGNATED_DISCARD;

			if (((tree->fdWhile == 0) || tree->agreed || port->operEdge) && ((tree->rrWhile == 0) || !tree->reRoot) && !tree->sync && !tree->learn && !port->isolate)
				return DESIGNATED_LEARN;

			if (((tree->fdWhile == 0) || tree->agreed || port->operEdge) && ((tree->rrWhile == 0) || !tree->reRoot) && !tree->sync && (tree->learn && !tree->forward) && !port->isolate)
				return DESIGNATED_FORWARD;
		}

		return 0;
	}

	if (state == DESIGNATED_FORWARD)
		return DESIGNATED_PORT;

	if (state == DESIGNATED_PROPOSE)
		return DESIGNATED_PORT;

	if (state == DESIGNATED_LEARN)
		return DESIGNATED_PORT;

	if (state == DESIGNATED_AGREE)
		return DESIGNATED_PORT;

	if (state == DESIGNATED_DISCARD)
		return DESIGNATED_PORT;

	if (state == DESIGNATED_SYNCED)
		return DESIGNATED_PORT;

	if (state == DESIGNATED_RETIRED)
		return DESIGNATED_PORT;

	// ------------------------------------------------------------------------
	// Alternate / Backup

	if (state == ALTERNATE_PORT)
	{
		if (tree->selected && !tree->updtInfo)
		{
			if (tree->proposed && !tree->agree)
				return ALTERNATE_PROPOSED;

			if ((allSynced (bridge, givenPort, givenTree) && !tree->agree) || (tree->proposed && tree->agree))
				return ALTERNATE_AGREED;

			if ((tree->fdWhile != forwardDelay (bridge, givenPort)) || tree->sync || tree->reRoot || !tree->synced)
				return ALTERNATE_PORT;

			if ((tree->rbWhile != 2 * HelloTime (bridge, givenPort)) && (tree->role == STP_PORT_ROLE_BACKUP))
				return BACKUP_PORT;
		}

		return 0;
	}

	if (state == BACKUP_PORT)
		return ALTERNATE_PORT;

	if (state == ALTERNATE_PROPOSED)
		return ALTERNATE_PORT;

	if (state == ALTERNATE_AGREED)
		return ALTERNATE_PORT;

	if (state == BLOCK_PORT)
	{
		if (tree->selected && !tree->updtInfo)
		{
			if (!tree->learning && !tree->forwarding)
				return ALTERNATE_PORT;
		}

		return 0;
	}

	assert (false);
	return 0;
}

// ============================================================================

void PortRoleTransitions_802_1Q_2011_InitState (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp)
{
	assert (givenPort != -1);
	assert (givenTree != -1);

	PORT* port = bridge->ports [givenPort];
	PORT_TREE* tree = port->trees [givenTree];

	// ------------------------------------------------------------------------
	// Disabled

	if (state == INIT_PORT)
	{
		tree->role = STP_PORT_ROLE_DISABLED;
		if (bridge->callbacks.onPortRoleChanged != NULL)
			bridge->callbacks.onPortRoleChanged (bridge, givenPort, givenTree, STP_PORT_ROLE_DISABLED, timestamp);
		tree->learn = tree->forward = false;
		tree->synced = false;
		tree->sync = tree->reRoot = true;
		tree->rrWhile = FwdDelay (bridge, givenPort);
		tree->fdWhile = MaxAge (bridge, givenPort);
		tree->rbWhile = 0;
	}
	else if (state == DISABLE_PORT)
	{
		tree->role = STP_PORT_ROLE_DISABLED;
		if (bridge->callbacks.onPortRoleChanged != NULL)
			bridge->callbacks.onPortRoleChanged (bridge, givenPort, givenTree, STP_PORT_ROLE_DISABLED, timestamp);
		tree->learn = tree->forward = false;
	}
	else if (state == DISABLED_PORT)
	{
		tree->fdWhile = MaxAge (bridge, givenPort);
		tree->synced = true;
		tree->rrWhile = 0;
		tree->sync = tree->reRoot = false;
	}

	// ------------------------------------------------------------------------
	// Master

	else if (state == MASTER_PORT)
	{
		tree->role = STP_PORT_ROLE_MASTER;
		if (bridge->callbacks.onPortRoleChanged != NULL)
			bridge->callbacks.onPortRoleChanged (bridge, givenPort, givenTree, STP_PORT_ROLE_MASTER, timestamp);
	}
	else if (state == MASTER_PROPOSED)
	{
		setSyncTree (bridge, givenTree);
		tree->proposed = false;
	}
	else if (state == MASTER_AGREED)
	{
		tree->proposed = tree->sync = false;
		tree->agree = true;
	}
	else if (state == MASTER_SYNCED)
	{
		tree->rrWhile = 0;
		tree->synced = true;
		tree->sync = false;
	}
	else if (state == MASTER_RETIRED)
	{
		tree->reRoot = false;
	}
	else if (state == MASTER_FORWARD)
	{
		tree->forward = true;
		tree->fdWhile = 0;
		tree->agreed = port->sendRSTP;
	}
	else if (state == MASTER_LEARN)
	{
		tree->learn = true;
		tree->fdWhile = forwardDelay (bridge, givenPort);
	}
	else if (state == MASTER_DISCARD)
	{
		tree->learn = tree->forward = tree->disputed = false;
		tree->fdWhile = forwardDelay (bridge, givenPort);
	}

	// ------------------------------------------------------------------------
	// Root

	else if (state == ROOT_PORT)
	{
		tree->role = STP_PORT_ROLE_ROOT;
		if (bridge->callbacks.onPortRoleChanged != NULL)
			bridge->callbacks.onPortRoleChanged (bridge, givenPort, givenTree, STP_PORT_ROLE_ROOT, timestamp);
		tree->rrWhile = FwdDelay (bridge, givenPort);
	}
	else if (state == ROOT_PROPOSED)
	{
		setSyncTree (bridge, givenTree);
		tree->proposed = false;
	}
	else if (state == ROOT_AGREED)
	{
		tree->proposed = tree->sync = false;
		tree->agree = true;
		if (givenTree == CIST_INDEX)
			port->newInfo = true;
		else
			port->newInfoMsti = true;
	}
	else if (state == ROOT_SYNCED)
	{
		tree->synced = true;
		tree->sync = false;
	}
	else if (state == REROOT)
	{
		setReRootTree (bridge, givenTree);
	}
	else if (state == ROOT_FORWARD)
	{
		tree->fdWhile = 0;
		tree->forward = true;
	}
	else if (state == ROOT_LEARN)
	{
		tree->fdWhile = forwardDelay (bridge, givenPort);
		tree->learn = true;
	}
	else if (state == REROOTED)
	{
		tree->reRoot = false;
	}
	else if (state == ROOT_DISCARD)
	{
		if (tree->disputed)
			tree->rbWhile = 3 * HelloTime (bridge, givenPort);
		tree->learn = tree->forward = tree->disputed = false;
		tree->fdWhile = FwdDelay (bridge, givenPort);
	}

	// ------------------------------------------------------------------------
	// Designated

	else if (state == DESIGNATED_PORT)
	{
		tree->role = STP_PORT_ROLE_DESIGNATED;
		if (bridge->callbacks.onPortRoleChanged != NULL)
			bridge->callbacks.onPortRoleChanged (bridge, givenPort, givenTree, STP_PORT_ROLE_DESIGNATED, timestamp);

		if (cist (bridge, givenTree))
		{
			tree->proposing = tree->proposing || (!port->AdminEdge && !port->AutoEdge && port->AutoIsolate && port->operPointToPointMAC);
		}
	}
	else if (state == DESIGNATED_FORWARD)
	{
		tree->forward = true;
		tree->fdWhile = 0;
		tree->agreed = port->sendRSTP;
	}
	else if (state == DESIGNATED_PROPOSE)
	{
		tree->proposing = true;
		if (cist (bridge, givenTree))
		{
			port->edgeDelayWhile = EdgeDelay (bridge, givenPort);
		}

		if (givenTree == CIST_INDEX)
			port->newInfo = true;
		else
			port->newInfoMsti = true;
	}
	else if (state == DESIGNATED_LEARN)
	{
		tree->learn = true;
		tree->fdWhile = forwardDelay (bridge, givenPort);
	}
	else if (state == DESIGNATED_AGREE)
	{
		tree->proposed = tree->sync = false;
		tree->agree = true;
		if (givenTree == CIST_INDEX)
			port->newInfo = true;
		else
			port->newInfoMsti = true;
	}
	else if (state == DESIGNATED_DISCARD)
	{
		tree->learn = tree->forward = tree->disputed = false;
		tree->fdWhile = forwardDelay (bridge, givenPort);
	}
	else if (state == DESIGNATED_SYNCED)
	{
		tree->rrWhile = 0;
		tree->synced = true;
		tree->sync = false;
	}
	else if (state == DESIGNATED_RETIRED)
	{
		tree->reRoot = false;
	}

	// ------------------------------------------------------------------------
	// Alternate / Backup

	else if (state == ALTERNATE_PORT)
	{
		tree->fdWhile = forwardDelay (bridge, givenPort);
		tree->synced = true;
		tree->rrWhile = 0;
		tree->sync = tree->reRoot = false;
	}
	else if (state == BACKUP_PORT)
	{
		tree->rbWhile = 2 * HelloTime (bridge, givenPort);
	}
	else if (state == ALTERNATE_PROPOSED)
	{
		setSyncTree (bridge, givenTree);
		tree->proposed = false;
	}
	else if (state == ALTERNATE_AGREED)
	{
		tree->proposed = false;
		tree->agree = true;
		if (givenTree == CIST_INDEX)
			port->newInfo = true;
		else
			port->newInfoMsti = true;
	}
	else if (state == BLOCK_PORT)
	{
		tree->role = tree->selectedRole;
		if (bridge->callbacks.onPortRoleChanged != NULL)
			bridge->callbacks.onPortRoleChanged (bridge, givenPort, givenTree, tree->role, timestamp);
		tree->learn = tree->forward = false;
	}
	else
		assert (false);
}
