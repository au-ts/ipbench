# Implementation

This page describes the internal design of ipbench for the consideration of maintainers. Everything below refers to files in `ipbench2/src` unless stated otherwise.

## ipbench core

Ipbench has two executables. `ipbench` - for the controller, and `ipbenchd` - the daemon used by testers and optionally the test target.

### ipbench.py

Entrypoint for controller. Contains logic for spinning up the client test and handshake with the test target. Control flow is straightforward:

1. Uses `OptionParser` to collect args, read config file if specified.
2. Use doc to find tests and clients → comes from `parse().getroot(options.config)` → this just uses some black magic to generically parse the XML config file
3. Load commandline parameters. Commandline is much more limited than config files.
4. Set up `ipbench_client` , activate controller
5. Set up each target. Invokes C code from here with swig + IpbenchTestTarget. Makes each target connect
6. Set up each client. Same deal as above with C code + IpbenchTestClient
7. Start test on each target.
8. Send start command to each client
9. Wait for tests to complete and then unmarshall results
10. Stop target tests and grab any results they may have reported using CPU meter
11. Display results, close connections and exit

### ipbenchd.py

Ipbench daemon. Used by testers and test target (if desired). The testers are responsible for actually dispatching the packets to the target.

Contains several features:
* TestStatus - cursed mmap based information sharing between testers.
* StateMachine - generic state machine class implemented for testers.
* IpbenchClient - implementation of client.


## Plugins and tests

`pymod` contains bridge code to connect the Python main modules to the C-based tests using SWIG. `plugin.h` defines the interface for plugins to use. `tests` contains the actual plugins.

### Adding tests

Tests are compiled modularly - to add a new test you simply need to create a new directory in `/src/tests` which implements the test interface described in the sections below. You also need to update the following automake/autoconf files:

* ipbench2/configure.in
    
    `AC_OUTPUT`
        Add the Makefile target that autoconf will generate after automake has been run. This means you will create a Makefile.am file to build the test.

* ipbench2/src/tests/Makefile.am
    
    `SUBDIRS`
        Append your test directory to the SUBDIR target.

* ipbench2/src/tests/<testname>/Makefile.am
    
    `lib<test>_la_SOURCES`
       All files that are used for the generation of the test must be included in this variable, or make dist will not generate a complete source tree.


### plugin.h

This file specifies the skeleton for all ipbench plugins including the tests. The implementer of this file will #define either `IPBENCH_CLIENT` or `IPBENCH_TARGET`. All plugins must #include this file, and attach function vectors to the plugin's implementation of setup, test, etc. in their instance of the ipbench_plugin struct.

plugin.h specifies the `ipbench_plugin` struct which is used by the shunt code.

Below are the mandatory functions, all of whom are fields in the plugin struct, that must be defined:

* `setup_controller`: defines any arguments the controller may need to parse.
* `start`: Start the test and grab timestamp.
* `stop`: Stop the test and grab timestamp.
* `marshall`: Given a pointer+size of the block of data to send back to the controller, send back.
* `marshall_cleanup`: Clean up after marshalling. Takes the same block of data provided to marshall.
* `unmarshall`: Called by controller to decipher results - plugin is responsible for both sides of marshalling interface.
* `unmarshall_cleanup`: Same as marshall cleanup.
* `output`: Aggregate and output results. Has client and test target version.


### ipbench.c

Main interface logic. Implemented by both the controller and testers, as well as the test target. This module iterates over the directory containing all of the compiled plugins and attempts to open them.

Tests are injected into a struct and manipulated from there. E.g. to start, a function pointer to the entry of the test is called.

This module offers a marshall function for both clients and targets. This is used to package data into a `marshalled_data` struct for transmission between systems.

There is an unmarshall function which has a different implementation for testers and clients. Clients include a `clientid` in their unmarshalled data, while targets simply rip the data straight out and store it in the `ipbench_plugin` struct.

#### Test client

Clients have a setup stage. Tests can do as they please to set up and the client will wait. The controller will also be prompted to start up by the test client.

#### Test target

Test targets await a signal to start, and then will also enter a setup stage.

## Tests

Tests are implemented in `ipbench2/src/tests`. Unlike the core of the program, all tests are implemented in C. Each test can consist of both tester and test target modules, defined by headers with the #define `IPBENCH_TEST_TARGET` or `IPBENCH_TEST_CLIENT`. When `ipbench.c` opens the directory, it will grab the associated halves and install them on the associated tester/client.

### Test design

Each test must implement several mandatory features.

* import of `plugin.h` - defines interface for `pymod` to interact with.
* `(test).h` - main header file. Must declare 