
#ifndef MSTP_LIB_STP_SM_H
#define MSTP_LIB_STP_SM_H

#include "stp_base_types.h"

template<typename State, typename PortTreeArgs>
struct StateMachine
{
#if STP_USE_LOG
	const char* smName;
	const char* (*getStateName) (State state);
#endif
	State (*checkConditions) (const STP_BRIDGE* bridge, PortTreeArgs portTreeArgs, State state);
	void (*initState) (STP_BRIDGE* bridge, PortTreeArgs portTreeArgs, State state, unsigned int timestamp);
};

struct PortAndTree
{
	PortIndex portIndex;
	TreeIndex treeIndex;
};

namespace TopologyChange {
	enum State {
		ACTIVE = 1,
		INACTIVE,
		LEARNING,
		DETECTED,
		NOTIFIED_TCN,
		NOTIFIED_TC,
		PROPAGATING,
		ACKNOWLEDGED,
	};

	extern const StateMachine<State, PortAndTree> sm;
};

namespace PortTimers {
	enum State {
		ONE_SECOND = 1,
		TICK,
	};

	extern const StateMachine<State, PortIndex> sm;
};

namespace PortProtocolMigration {
	enum State {
		CHECKING_RSTP = 1,
		SELECTING_STP,
		SENSING,
	};

	extern const StateMachine<State, PortIndex> sm;
};

namespace PortReceive {
	enum State {
		DISCARD = 1,
		RECEIVE,
	};

	extern const StateMachine<State, PortIndex> sm;
};

namespace BridgeDetection {
	enum State {
		NOT_EDGE = 1,
		EDGE,
		ISOLATED,
	};

	extern const StateMachine<State, PortIndex> sm;
};

namespace PortInformation {
	enum State {
		DISABLED = 1,
		AGED,
		UPDATE,
		SUPERIOR_DESIGNATED,
		REPEATED_DESIGNATED,
		INFERIOR_DESIGNATED,
		NOT_DESIGNATED,
		OTHER,
		CURRENT,
		RECEIVE,
	};

	extern const StateMachine<State, PortAndTree> sm;
}

namespace PortRoleSelection {
	enum State {
		INIT_TREE = 1,
		ROLE_SELECTION,
	};

	extern const StateMachine<State, TreeIndex> sm;
};

namespace PortRoleTransitions {
	enum State {
		INIT_PORT = 1,
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

	extern const StateMachine<State, PortAndTree> sm;
};

namespace PortStateTransition {
	enum State {
		DISCARDING = 1,
		LEARNING,
		FORWARDING,
	};

	extern const StateMachine<State, PortAndTree> sm;
};

namespace L2GPortReceive {
	enum State {
		INIT = 1,
		PSEUDO_RECEIVE,
		DISCARD,
		L2GP,
	};

	extern const StateMachine<State, PortIndex> sm;
};

namespace PortTransmit {
	enum State {
		TRANSMIT_INIT = 1,
		TRANSMIT_PERIODIC,
		TRANSMIT_CONFIG,
		TRANSMIT_TCN,
		TRANSMIT_RSTP,
		AGREE_SPT,
		IDLE,
	};

	extern const StateMachine<State, PortIndex> sm;
};

#endif
