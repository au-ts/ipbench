.ig
	tbl thisfile | ?roff -ms
..
.de CM
..
.nr PI 0
.ds LH
.ds CH
.ds RH
.ds LF
.ds CF %
.ds RF
.ta 0.5i +0.5i +0.5i +0.5i +0.5i +0.5i +0.5i +0.5i +0.5i
.TL
NFSSTONE
.sp
A NETWORK FILE SERVER
.br
PERFORMANCE BENCHMARK
.AU
Barry Shein
.AI
Software Tool & Die
.AU
Mike Callahan
.sp
Paul Woodbury
.AI
Encore Computer Corporation
.AB
Network file servers are becoming a critical facility in modern
computing environments. With the growth in their popularity and the
emergence of multiple vendors providing software products which adhere
to the same standards comes a need for relative performance
measurement of different configurations.  We have designed a benchmark
and report our experiences with it on different configurations of
servers and clients. The benchmark was designed to be portable
(between networked file system protocols) and tunable to reflect
different disk traffic patterns if desired. The default parameters
used were chosen to be similar to the traffic patterns of typical
networked file system environments as earlier reported in
[SANDBERG85].
.AE
.NH 0
Introduction
.\" *** Removed comma after 'standards' in last sentence
.PP
Networked file servers, computers which provide their file systems
remotely via standardized networking protocols, have become an
important component in the design of any distributed computing
environment. As a result of industry-wide standards systems designers
find they have a choice of file servers all offering similar services.
.PP
Perhaps the most important requirement, beyond compatibility and
adherence to standards, is performance. In one study of 363 users of
Sun Microsystems products "75% of responders stated `Occasional' or
`Frequent' performance limitations" as a concern at their site
[EPOCH88]. The study reported that approximately 92% of their
workstations were either diskless or dataless\** implying critical
dependence on a file server.
.\" Use of 'dataless' resolved with Matt, generic term used by Sun.
.FS
A 'dataless' workstation has a disk which provides local
swapping/paging partitions and basic system files (boot, binaries,
etc.) User files are kept on a remote file system.
.FE
.PP
File servers have several performance components: Disk I/O, Network
I/O, file cache efficiency, memory availability and CPU speed. Any one
of these might affect file server performance. Thus, one can ask two
questions, what is the overall performance of a particular file server
and what component of the file server/client is the major bottleneck
on a specific configuration.  The benchmark we present in this paper
can be useful in answering both questions, either by comparing file
servers with similar configurations, or one file server as its
configuration is varied.
.KS
.NH
Benchmark Design
.NH 2
General Assumptions
.\" *** use of 'stress' ok, phone conversation.
.PP
A file server's performance is measured at the client. A benchmark
should stress the server from a client or clients in a manner
which is reasonably representative of the kind of load seen
during very heavy usage.
.KE
.\" *** "Awkward" wording changed in first sentence.
.PP
File servers make their file systems available to clients by
satisfying several types of requests. These include reading data,
writing data, looking up files and returning file status. This is not
very different from the requests a local time-sharing user makes on a
local file system except that the requests have been indirected
through a network and some layers of protocol. In the case of Sun's
NFS\**, for example, requests pass through
.FS
NFS is a trademark of Sun Microsystems, Inc.
.FE
NFS, RPC (remote procedure call), XDR (external data representation),
UDP/IP and an ethernet\** (and/or other media), in addition to the
normal file system mechanisms of the server.
.FS
Ethernet is a registered trademark of Xerox Corporation.
.FE
.PP
By designing an overall throughput oriented benchmark we have
purposely avoided detailed analysis of individual layers of the
networked file system and have tried to remain independent of the
underlying protocols in use.  This is accomplished by using a
`black-box' approach to the problem which measures the time required
to perform a mix of high-level operations.
.PP
This benchmark should be as useful in comparing different networked
file systems (e.g., NFS vs. RFS\**) as it is in comparing different
implementations, both hardware and software, of the same networked
file system.
.FS
RFS is a registered trademark of AT&T.
.FE
.NH 2
Definition of NFSSTONE\**
.FS
The suffix "stone" derives from a tradition going back to the
Whetstone benchmark, dhrystone, dhampstone etc. Unfortunately we
couldn't think of any hygroscopic prefix nor mean to imply any networked
file system as such.
.FE
.PP
We have called the units reported by the benchmark
.B NFSSTONEs.
This is the total number of operations in one run through the program
(45,522 with the default parameters used throughout this paper)
divided by the total elapsed time in seconds. This can also be thought
of as NFS operations per second, where NFS operations represent a
mixture of requests tuned to reflect what we believe is normal usage
(although compressed into a small time.)
.NH 2
NFS Operations Mixture
.PP
Since the mix of operations can be critical the benchmark program was
designed to allow the person performing the tests to easily vary the
proportions of reads, writes, lookups etc. The default values,
however, were based on a particular mixture obtained by empirical
measurement by [SANDBERG85] as follows:
.PP
.TS
center allbox;
lb cb cb
l n n.
NFS Operation	Sun %	NFSSTONE %
lookup	50	53.0
read	30	32.0
readlink	7	7.5
getattr	5	2.3
write	3	3.2
create	1	1.4
.TE
.\" *** Why the differences? See last two sentences. Also added
.\" *** 'empirically' to first sentence and cleaned up wording.
.PP
Sun's statistics were determined by compiling nfsstat statistics, our
numbers were obtained empirically by observing similar kernel meters
after a single run of our benchmark. In short, the operation mixture
is purposely very similar. We considered the differences minor so did
not attempt to juggle the benhchmark further to fit exactly. It would
be more productive to repeat the Sun measurements in various
environments to further refine these numbers.
.PP
We consider this choice of default parameters to be sufficiently
compelling to recommend them strongly when comparing file servers. We
are happy that, should different parameters be desired, it is easy to
vary the benchmark's values to fit new models.
.\" *** Mention 10Mb ethernet.
.PP
A theoretical upper bound of throughput can be calculated for our
particular mixture. If one assumes 100% utilization of a 10Mb ethernet
(different media would be adjusted accordingly) and adds up all the
data which is passed between client and server and, finally, assumes
that pure protocol overhead and other operations such as lookup costs
nothing, then we arrive at an upper bound of 421.5 nfsstones\**. This
might be thought of as a protocol-independent upper bound (no
protocol, no matter how lightweight, should be able to exceed this.)
.\" *** Add reference to appendix.
.FS
More precisely, 45,522/(135MB / 1.25 MB/s), where 45,522 is the total
(default) NFS operation count as described earlier in the paper.  Size
of reads and writes are summarized in the appendix.
.FE
.\" **** End of last sentence in pp, less awkward.
.PP
If one adds a cost for the transactions by assuming the average
round-trip cost of an operation is 256 bytes (arbitrary, reasonable
sounding number), including protocol headers, then the upper limit
drops to around 389 nfsstones. This means that if a reported figure
exceeds 400 nfsstones we would suspect that either caching is becoming
significant or something is awry with the way the benchmark was being
run.
.NH 2
File System Caches
.PP
The file cache in the client is a factor which can critically affect
the performance of the overall networked file system. Caches are
typically allocated at bootstrap time to be a percentage of available
main memory or, in any case, will tend to reflect the total amount of
memory in the client.
.\" *** Justification? Last sentence added.
.PP
We have decided to try to mix read strategies which both uses the
cache and defeats it. The assumption is that simple sequential reading
of files promotes efficient use of the cache while `randomly' reading
blocks in large files will tend to defeat the cache (i.e., minimize
cache hits.) This is based on experience with buffer cache designs and
could be refined by actual measurement.
.\" *** Last two sentences added, consider systems where defeating
.\" *** cache might be self-defeating of the benchmark.
.PP
This can be contrasted with [KRIDLE83] where the motivation was to
allow the cache to operate by allowing locality of file reference.  By
stressing the cache we get a better feel for the capability of the
other system components, achieve some independence from particular
client memory configuration and obtain results better resembling the
behavior of servers with large numbers of clients. Clients with very
large caches might force some rethinking of the default parameters.
In [RODRIGUEZ88] a design is described which allows dynamic resizing
of buffer caches and other kernel resources. On such systems
attempting to defeat the cache further might distract from a fair
measurement of what the system is trying to provide.
.PP
We perform other operations in a straightforward manner, creating
directories and files and performing down-scaled versions of the basic
read/write tests to these new files. A list of the specific operations
can be found in appendix 1.
.NH 2
Synchronization of Clients
.PP
We use the term
.I synchronization
to mean starting the benchmark on more than one client at the same
time.  If the clients are reasonably synchronized then we expect a lot
of overlap among clients.
.\" *** 1. Close paragraphs
.\" *** 2. Change 'they' to 'all tasks'
.PP
To run the benchmark on several workstations we use a simple
synchronization mechanism so all tasks start roughly simultaneously.
This is accomplished by using a simple control program which will
create a file, use the file system mechanisms to lock it and then
start up the clients. The control program waits a short time (the
clients all block waiting for a lock on the same file) and then
unlocks the file allowing each client's lock request to complete and
the benchmark to begin. Other synchronization mechanisms could be
devised (e.g., if a system did not support network lock semantics) with
similar effect.
.PP
Although synchronization is not essential, we desire a lot of overlap
from the workstations. We have designed the benchmark to run a
sufficiently long time on a typical system so that synchronization
within several seconds is more than adequate to guarantee the desired
concurrency. Our benchmark typically takes from several minutes to
nearly an hour to complete. The time depends on the number of clients
and other factors such as the hardware and software configurations
available on the server.
.NH 2
Algorithm Overview
.PP
After synchronizing on the lock file the time is noted. We then create
a directory and fork children until 6 child processes are running.
Each child creates a file, writes to it, and reads from it in two
patterns: Sequential and non-sequential. The file is treated as a
group of fixed sized blocks. These blocks, when read non-sequentially,
are chosen by seeking to block offsets
.I "(1, n, 2, n-1, 3, n-2, ...)"
consecutively\**. Mixed in with these are other operations such as
creating and reading back symlinks, renames, creates, mkdirs and
deletes. The exact sequence is summarized in appendix 1.
.\" *** Respond to block choice algorithm question in comments.
.FS
We are certainly open to suggestions for other block choice algorithms
and reasoned arguments justifying those choices. One suggestion was to
try a quadratic hash modulo some large prime as a block choice
algorithm.  This and other methods should be tried in the future for
comparison, particularly with configuration variations such as striped
file systems.
.FE
.NH 2
Coding
.PP
The benchmark is coded in \fBC\fP. Although we do not use the stdio
library (to help avoid any differences in implementation) we read and
write in units which should be similar to those chosen by stdio with
block buffering.
.NH 2
The NFSSTONE
.PP
The result of the benchmark is a single number which we call an
NFSSTONE.  We felt it was important to be able to summarize results as
one, essentially unitless, number so different experiments could be
easily and quickly compared.
.NH
Test Configurations
.PP
To produce some sample results the benchmark was run on the
following configurations:
.NH 2
Servers
.PP
Our servers can be characterized as modern, virtual-memory
microprocessor-based systems with relatively fast disks and disk
channels. Our server is a symmetrical multi-processor system using
shared memory so we vary the number of CPUs as an additional variable.
.NH 2
Clients
.PP
The clients were all Sun3/60 workstations with either 8 or 12MB of
memory running SunOS\** release 3.4. Some were diskless and others
dataless, the program itself is small (45KB total) and resides on
another system not being tested. The clients are otherwise quiescent
and had sufficent memory, so there is no reason to believe there is any
significant interaction with paging I/O or other factors.
.FS
SunOS is a trademark of Sun Microsystems, Inc.
.FE
.NH
Results
.\" *** Comment suggested changing period of second sentence
.\" *** to semi-colon. I couldn't understand this and assumed
.\" *** you meant comma after 'tasks'?
.PP
All results are reported in units of
.I NFSSTONEs.
Our program runs six tasks (forks) on each client. We report results
based upon the total number of tasks; to calculate the number of
clients involved (i.e., workstations) simply divide the number of tasks
by six.
.NH 2
Varying The Number of Disks
.\" *** Run on sentence number 2? I assume you meant sentence 3, fixed.
.PP
In this first example we use a system with 4 CPUs, 64MB of memory and
one disk. Running NFSSTONE from six to twenty-four tasks we measure a
20% decrease in total NFSSTONEs.  When we split the clients across two
disks we see some decrease in nfsstones as we increase tasks.  The
total throughput increases about 50% when compared with the one disk
configuration. This would suggest that the first configuration was
disk limited.
.TS
center allbox;
cb s s s s
l cb s s s 
lb cb cb cb cb
n n n n n.
64MB, 1 Disk Channel, 4 CPUs
_
	Tasks
Disks	6	12	18	24
1	85	79	69	68
2		120	109	112
.TE
.NH 2
Varying the Number of CPUs
.PP
For this data we measure total NFSSTONE throughput while varying the
number of CPUs (each CPU is rated about 2 MIPs.) We see some
increase between four and six CPUs but adding two more for a total of
eight shows very little improvement.
.TS
center allbox;
cb s s s s
l cb s s s
lb cb cb cb cb
n n n n n.
64MB, 4 Disks, 2 Disk Channels
_
	Tasks
CPUs	6	12	18	24
4	83	133	153	158
6	85	147	167	183
8	85	150	179	187
.TE
.\" *** Question about why the numbers increase across horizontal axis, add pp.
.PP
The asymptotic increase from left to right in each row would indicate
that the server is not yet saturated with fewer tasks.
.NH 2
Varying the number of Disk Channels
.PP
Comparing one and two disk channel configurations we see some
difference as the number of clients increases. This suggests that
simply splitting clients among disks can be further augmented by
splitting the disks themselves among channels although not as
dramatically as adding disks or CPUs.
.TS
center allbox;
cb s s s s
l cb s s s 
lb cb cb cb cb
n n n n n.
64MB, 4 Disks, 4 CPUs
_
	Tasks
Channels	6	12	18	24
1	82	125	139	147
2	83	133	153	158
.TE
.NH 2
Conclusions
.PP
We believe that the benchmark is sensitive to variables that both
customers and vendors wish to see measured. Customers want to know
where to best spend their hardware dollars and vendors want to know
how to improve their product. Both want to know where particular
systems stand against each other. Benchmark results allow them to
examine price/performance differences.
.\" *** Comment about Sun's performance elided. General comment
.\" *** measuring more systems moved to end of fourth paragraph.
.PP
It would be nice if we could report how many nfsstones per client are
`necessary' but that varies from application to application. What we
have measured can be used to compare different hardware and software
implementations.
.PP
We plan to continue our experiments with this benchmark. Such
experiments will include many more clients, different numbers of tasks
on each client, multiple ethernets connected to one server and new
network mediums as they become available.
.PP
The hope is that this paper catalyzes others to join us in refining an
acceptable benchmark of networked file system performance and to
define suitable methodologies for testing. We do not consider this
work complete although we feel that it does set some framework for
further development. We will make all our programs publicly available
through the usual channels (anonymous ftp, usenet etc.) This should
make results from many more systems and configurations available to
the community.
.\" *** Response to ABSTRACT comment
.PP
The proportions of operations used for this paper were based upon the
results in [SANDBERG85]. Repeating those measurements in carefully
chosen environments would make those assumptions more rigorous.
.PP
Networked file system performance is important, but it is not the
entire story. Two systems might perform similarly on this benchmark
but one can be straining while the other may have quite a bit of
capacity to spare, this should be the case when either the network
bandwidth has been fully utilized or the clients become the limiting
factor. Thought should be given to what services, beyond networked
file systems, servers should provide and how this interacts with
networked file service.
.NH
Acknowledgements
.PP
This NFSSTONE benchmark was inspired by earlier work done by Howard
Eskin, Chris Jolly and Scott Palmer of the General Electric Corporate
Research and Development Center. We are indebted to Howard Eskin for
his many comments, criticisms and suggestions during the preparation
of this paper, we hope to eventually produce a benchmark which meets
his very high standards.
.sp 1i
.SH
References
.IP [EPOCH88] 1.5i
Epoch Systems Inc., "File Server Needs of High-Performance Workstation
Users", June 1988.
.IP [KRIDLE83] 1.5i
Kridle, B., McKusick, K., "Performance Effects of Disk Subsystem
Choices for VAX Systems running 4.2BSD Unix", USENIX Summer '83
Conference Proceedings, pp. 156-169.
.IP [MCKUSICK83] 1.5i
McKusick, M., Joy, W., Leffler, S. and Fabry, R. "A Fast File System for
Unix", University of California at Berkeley, Computer Systems Research
Group Technical Report #7, 1982.
.\" *** Reference added.
.IP [RODRIGUEZ88] 1.5i
Rodriguez, R., Koehler, M., Palmer, L., Palmer, R., "A Dynamic Unix
Operating System", USENIX Summer '88 Conference Proceedings, pp. 305-319.
.IP [SANDBERG85] 1.5i
Sandberg, R., "The Sun Network File System: Design, Implementation and
Experience", Sun Technical Report. A version also appeared in the
USENIX Summer 1985 Conference Proceedings, pp. 119-130, although not with the
appendix of NFS operations we reference.
.bp
.SH
Appendix 1
.PP
Overview of NFSSTONE operations
.DS
lock/synchronize
note time
repeat 2
	mkdir
	repeat 3
		mkdir
		(child)
		create
		seq_write
		seq_read
		symlink
		repeat 583
			readlink
		end
		nseq_read
		unlink
		repeat 83
			create
			fsync
			close
		end
		unlink
		seq_read
		repeat 4167
			access
		end
		seq_read
		nseq_read
		close
		rename
		unlink
		(end child)
	end
end
repeat 2
	repeat 3
		rmdir
	end
end
note time
report
.DE
.PP
.I Notes:
.PP
seq_write: sequential write of 250 * 8192 blocks.
.PP
seq_read: sequential read of 250 * 8192 blocks.
.PP
nseq_read: non-sequential read of 250 * 8192 blocks (see text
for description.)
