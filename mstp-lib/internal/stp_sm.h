
#ifndef MSTP_LIB_STP_SM_H
#define MSTP_LIB_STP_SM_H

struct STP_BRIDGE;

template<typename State>
struct SM_INFO
{
	const char* smName;
	const char* (*getStateName) (State state);
	State (*checkConditions) (const STP_BRIDGE* bridge, int givenPort, int givenTree, State state);
	void (*initState) (STP_BRIDGE* bridge, int givenPort, int givenTree, State state, unsigned int timestamp);
};

struct TopologyChange {
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

	static const SM_INFO<State> sm;
};

struct PortTimers {
	enum State {
		ONE_SECOND = 1,
		TICK,
	};

	static const SM_INFO<State> sm;
};

struct PortProtocolMigration {
	enum State {
		CHECKING_RSTP = 1,
		SELECTING_STP,
		SENSING,
	};

	static const SM_INFO<State> sm;
};

struct PortReceive {
	enum State {
		UNDEFINED,
		DISCARD,
		RECEIVE,
	};

	static const SM_INFO<State> sm;
};

struct BridgeDetection {
	enum State {
		UNDEFINED,
		NOT_EDGE,
		EDGE,
		ISOLATED,
	};

	static const SM_INFO<State> sm;
};

struct PortInformation {
	enum State {
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

	static const SM_INFO<State> sm;
};

struct PortRoleSelection {
	enum State {
		UNDEFINED,
		INIT_TREE,
		ROLE_SELECTION,
	};

	static const SM_INFO<State> sm;
};

struct PortRoleTransitions {
	enum State {
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

	static const SM_INFO<State> sm;
};

struct PortStateTransition {
	enum State {
		UNDEFINED,
		DISCARDING,
		LEARNING,
		FORWARDING,
	};

	static const SM_INFO<State> sm;
};

struct L2GPortReceive {
	enum State {
		UNDEFINED,
		INIT,
		PSEUDO_RECEIVE,
		DISCARD,
		L2GP,
	};

	static const SM_INFO<State> sm;
};

struct PortTransmit {
	enum State {
		UNDEFINED,
		TRANSMIT_INIT,
		TRANSMIT_PERIODIC,
		TRANSMIT_CONFIG,
		TRANSMIT_TCN,
		TRANSMIT_RSTP,
		IDLE,
	};

	static const SM_INFO<State> sm;
};

#endif
