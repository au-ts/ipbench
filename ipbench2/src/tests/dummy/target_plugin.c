#include "dummy_target.h"

struct ipbench_plugin ipbench_plugin = {
	.magic = "IPBENCH_PLUGIN",
	.name = "dummy_target",
	.id = 0x10,
	.descr = "The Dummy Target Test",
	.default_port = 6123,
	.type = IPBENCH_TARGET,
	.setup = &dummy_target_setup,
	.setup_controller = &dummy_target_setup_controller,
	.start = &dummy_target_start,
	.stop = &dummy_target_stop,
	.marshall = &dummy_target_marshall,
	.marshall_cleanup = &dummy_target_marshall_cleanup,
	.unmarshall = &dummy_target_unmarshall,
	.unmarshall_cleanup = &dummy_target_unmarshall_cleanup,
	.output = &dummy_target_output,
};
