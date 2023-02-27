#include "target_wrapper.h"

struct ipbench_plugin ipbench_plugin = {
	.magic = "IPBENCH_PLUGIN",
	.name = "target_wrapper",
	.id = 0x30,
	.descr = "The Wrapper Target Test",
	.default_port = 6123,
	.type = IPBENCH_TARGET,
	.setup = &target_wrap_setup,
	.setup_controller = &target_wrap_setup_controller,
	.start = &target_wrap_start,
	.stop = &target_wrap_stop,
	.marshall = &target_wrap_marshall,
	.marshall_cleanup = &target_wrap_marshall_cleanup,
	.unmarshall = &target_wrap_unmarshall,
	.unmarshall_cleanup = &target_wrap_unmarshall_cleanup,
	.output = &target_wrap_output,
};
