
#ifndef MSTP_LIB_STP_SM_H
#define MSTP_LIB_STP_SM_H

#include "stp_base_types.h"

template<typename State, typename... PortTreeArgs>
struct StateMachine
{
	const char* smName;
	const char* (*getStateName) (State state);
	State       (*checkConditions) (const STP_BRIDGE* bridge, PortTreeArgs... portTreeArgs, State state);
	void        (*initState) (STP_BRIDGE* bridge, PortTreeArgs... portTreeArgs, State state, unsigned int timestamp);
};

template<typename State> using PerPortStateMachine        = StateMachine<State, PortIndex>;
template<typename State> using PerTreeStateMachine        = StateMachine<State, TreeIndex>;
template<typename State> using PerPortPerTreeStateMachine = StateMachine<State, PortIndex, TreeIndex>;

namespace TopologyChange {
	enum State : unsigned char {
		ACTIVE = 1,
		INACTIVE,
		LEARNING,
		DETECTED,
		NOTIFIED_TCN,
		NOTIFIED_TC,
		PROPAGATING,
		ACKNOWLEDGED,
	};

	extern const PerPortPerTreeStateMachine<State> sm;
};

namespace PortTimers {
	enum State : unsigned char {
		ONE_SECOND = 1,
		TICK,
	};

	extern const PerPortStateMachine<State> sm;
};

namespace PortProtocolMigration {
	enum State : unsigned char {
		CHECKING_RSTP = 1,
		SELECTING_STP,
		SENSING,
	};

	extern const PerPortStateMachine<State> sm;
};

namespace PortReceive {
	enum State : unsigned char {
		UNDEFINED,
		DISCARD,
		RECEIVE,
	};

	extern const PerPortStateMachine<State> sm;
};

namespace BridgeDetection {
	enum State : unsigned char {
		UNDEFINED,
		NOT_EDGE,
		EDGE,
		ISOLATED,
	};

	extern const PerPortStateMachine<State> sm;
};

namespace PortInformation {
	enum State : unsigned char {
		UNDEFINED,
		DISABLED,
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

	extern const PerPortPerTreeStateMachine<State> sm;
}

namespace PortRoleSelection {
	enum State : unsigned char {
		UNDEFINED,
		INIT_TREE,
		ROLE_SELECTION,
	};

	extern const PerTreeStateMachine<State> sm;
};

namespace PortRoleTransitions {
	enum State : unsigned char {
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

	extern const PerPortPerTreeStateMachine<State> sm;
};

namespace PortStateTransition {
	enum State : unsigned char {
		UNDEFINED,
		DISCARDING,
		LEARNING,
		FORWARDING,
	};

	extern const PerPortPerTreeStateMachine<State> sm;
};

namespace L2GPortReceive {
	enum State : unsigned char {
		UNDEFINED,
		INIT,
		PSEUDO_RECEIVE,
		DISCARD,
		L2GP,
	};

	extern const PerPortStateMachine<State> sm;
};

namespace PortTransmit {
	enum State : unsigned char {
		UNDEFINED,
		TRANSMIT_INIT,
		TRANSMIT_PERIODIC,
		TRANSMIT_CONFIG,
		TRANSMIT_TCN,
		TRANSMIT_RSTP,
		AGREE_SPT,
		IDLE,
	};

	extern const PerPortStateMachine<State> sm;
};

#endif
