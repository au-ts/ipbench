#include "nfs_latency.h"

struct ipbench_plugin ipbench_plugin = {
	.magic = "IPBENCH_PLUGIN",
	.name = "nfs_latency",
	.id = 0x4,
	.descr = "NFS Latency Test",
	.default_port = 6123,
	.type = IPBENCH_CLIENT,
	.setup = &nfs_latency_setup,
	.setup_controller = &nfs_latency_setup_controller,
	.start = &nfs_latency_start,
	.stop = &nfs_latency_stop,
	.marshall = &nfs_latency_marshall,
	.marshall_cleanup = &nfs_latency_marshall_cleanup,
	.unmarshall = &nfs_latency_unmarshall,
	.unmarshall_cleanup = &nfs_latency_unmarshall_cleanup,
	.output = &nfs_latency_output,
};
