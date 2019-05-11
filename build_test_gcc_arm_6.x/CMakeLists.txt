# CMakeList.txt : CMake project for build_test_gcc_arm_6.x, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
add_executable (build_test_gcc_arm_6.x
	"build_test_gcc_arm_6.x.cpp" "build_test_gcc_arm_6.x.h"
	"../mstp-lib/stp.h"
	"../mstp-lib/internal/stp.cpp"
	"../mstp-lib/internal/stp_base_types.cpp" "../mstp-lib/internal/stp_base_types.h"
	"../mstp-lib/internal/stp_bpdu.cpp" "../mstp-lib/internal/stp_bpdu.h"
	"../mstp-lib/internal/stp_bridge.h"
	"../mstp-lib/internal/stp_log.cpp" "../mstp-lib/internal/stp_log.h"
	"../mstp-lib/internal/stp_md5.cpp" "../mstp-lib/internal/stp_md5.h"
	"../mstp-lib/internal/stp_port.h"
	"../mstp-lib/internal/stp_procedures.cpp" "../mstp-lib/internal/stp_procedures.h"
	"../mstp-lib/internal/stp_sm.h"
	"../mstp-lib/internal/stp_sm_bridge_detection.cpp"
	"../mstp-lib/internal/stp_sm_l2g_port_receive.cpp"
	"../mstp-lib/internal/stp_sm_port_information.cpp"
	"../mstp-lib/internal/stp_sm_port_protocol_migration.cpp"
	"../mstp-lib/internal/stp_sm_port_receive.cpp"
	"../mstp-lib/internal/stp_sm_port_role_selection.cpp"
	"../mstp-lib/internal/stp_sm_port_role_transitions.cpp"
	"../mstp-lib/internal/stp_sm_port_state_transition.cpp"
	"../mstp-lib/internal/stp_sm_port_timers.cpp"
	"../mstp-lib/internal/stp_sm_port_transmit.cpp"
	"../mstp-lib/internal/stp_sm_topology_change.cpp"
)

# TODO: Add tests and install targets if needed.
