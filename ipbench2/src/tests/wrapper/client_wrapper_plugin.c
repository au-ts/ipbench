#include "client_wrapper.h"

struct ipbench_plugin ipbench_plugin = {
	.magic = "IPBENCH_PLUGIN",
	.name = "client_wrapper",
	.id = 0x31,
	.descr = "The Client Wrap Test",
	.default_port = 6123,
	.type = IPBENCH_CLIENT,
	.setup = &client_wrap_setup,
	.setup_controller = &client_wrap_setup_controller,
	.start = &client_wrap_start,
	.stop = &client_wrap_stop,
	.marshall = &client_wrap_marshall,
	.marshall_cleanup = &client_wrap_marshall_cleanup,
	.unmarshall = &client_wrap_unmarshall,
	.unmarshall_cleanup = &client_wrap_unmarshall_cleanup,
	.output = &client_wrap_output,
};
