#include "nfsstones.h"

struct ipbench_plugin ipbench_plugin = {
	.magic = "IPBENCH_PLUGIN",
	.name = "nfsstone",
	.id = 0x11,
	.descr = "Nfsstone Test",
	.default_port = 6123,
	.type = IPBENCH_CLIENT,
	.setup = &nfsstone_setup,
	.setup_controller = &nfsstone_setup_controller,
	.start = &nfsstone_start,
	.stop = &nfsstone_stop,
	.marshall = &nfsstone_marshall,
	.marshall_cleanup = &nfsstone_marshall_cleanup,
	.unmarshall = &nfsstone_unmarshall,
	.unmarshall_cleanup = &nfsstone_unmarshall_cleanup,
	.output = &nfsstone_output,
};
