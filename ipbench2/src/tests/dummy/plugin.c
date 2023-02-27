#include "dummy.h"

struct ipbench_plugin ipbench_plugin = {
	.magic = "IPBENCH_PLUGIN",
	.name = "dummy",
	.id = 0x1,
	.descr = "The Dummy Test",
	.default_port = 6123,
	.type = IPBENCH_CLIENT,
	.setup = &dummy_setup,
	.setup_controller = &dummy_setup_controller,
	.start = &dummy_start,
	.stop = &dummy_stop,
	.marshall = &dummy_marshall,
	.marshall_cleanup = &dummy_marshall_cleanup,
	.unmarshall = &dummy_unmarshall,
	.unmarshall_cleanup = &dummy_unmarshall_cleanup,
	.output = &dummy_output,
};
