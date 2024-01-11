#!/usr/bin/env python3
"""
ipbench.py

Module encapsulating all controller logic.
"""

import sys
import re
import socket
import select
from copy import copy
from optparse import OptionParser
from lxml.etree import parse
import ipbench_client
import ipbench_target

OPTIONS = None
SELECTED_TEST = None


def dbprint(msg):
    """
    Debug print.

    Args:
        msg: message to print
    """
    global OPTIONS
    if OPTIONS.debug:
        sys.stderr.write(msg + "\n")


class IpBenchError(Exception):
    """
    Simple class for error representation.

    Attributes:
        value: Error value
    """

    def __init__(self, value):
        self.value = value

    def __str__(self):
        return repr(self.value)


class IpbenchTestClient:
    """
    Class encapsulating logic for controller machine to spin up tests.
    """

    def __init__(self, hostname, port, test_target, test_port, client_id, test_name, test_ptr, test_args=None):
        self.hostname = hostname
        self.port = port
        self.test_port = test_port
        self.test_target = test_target
        self.test_args = test_args
        self.socket = None
        self.client_id = client_id    # Identifier for this client
        self.test_name = test_name
        self.test_ptr = test_ptr

        dbprint("[IpbenchTestClient:__init__] : client " + self.hostname +
                " port " + repr(self.port) + " test_port " + repr(self.test_port) + " test_args " + self.test_args)

    # for select()
    def fileno(self):
        """
        Return file number of opened socket to client.
        """
        return self.socket.fileno()

    def _parse_return_code(self, data):
        """
        Parse a return code. Used for telnet interface to clients.
        Return codes are of the form 123 CODE(MESSAGE)

        Params:
            data: Return code from telnet
        """
        status = {}
        status["code"] = int(data[0:3])
        fence1 = data.find("(")
        if (fence1 == -1):
            status["str"] = data[4:].strip()
            status["msg"] = ""
        else:
            fence2 = data.find(")")
            status["str"] = data[4:fence1].strip()
            status["msg"] = data[fence1+1:fence2].strip()

        dbprint("[parse_return_code] : "
                + " code=" + repr(status["code"])
                + "|str=" + status["str"]
                + "|msg=" + status["msg"]+"|")
        return status

    def parse_return_code(self):
        """
        Get and parse a return code from telnet.
        """
        data = self.socket.recv(1024).decode('utf-8')
        return self._parse_return_code(data)

    def send_command(self, cmd):
        """
        Send a command to the client over telnet.

        Params:
            cmd: Command to send
        """
        dbprint("[send_command] : " + cmd)
        self.socket.send((cmd + "\n").encode('utf-8'))

    def connect(self):
        """
        Connect to the client machine. Creates a socket and tests that
        communication works as expected.

        Raises an IpBenchError exception on failure.
        """
        global OPTIONS

        dbprint("[connect] start | hostname " + self.hostname +
                " | port " + repr(self.port) + " | ")

        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            try:
                self.socket.connect((self.hostname, self.port))
            except OSError:
                sys.stderr.write(f"Can't connect to {self.hostname} on port {repr(self.port)}"
                                 + " ... invalid host or port?\n")
                sys.exit(1)
            status = self.parse_return_code()

            if (status["code"] != 100):
                raise IpBenchError("Invalid Version Flag")

            if (OPTIONS.reset):
                self.send_command("ABORT")
                # the connection should have closed; re-open
                self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.socket.connect((self.hostname, self.port))
                status = self.parse_return_code()
                if (status["code"] != 100):
                    raise IpBenchError("Invalid Version Flag")
            # continue
            # TODO: check versions
            self.send_command("HELLO")
            status = self.parse_return_code()
            if (status["code"] != 200):
                raise IpBenchError("HELLO to " + self.hostname + " failed (" +
                                   str(status["code"]) + " " + status["str"] + ")")
            self.send_command("LOAD " + self.test_name)
            status = self.parse_return_code()
            if (status["code"] == 421):
                raise IpBenchError("LOAD to " + self.hostname +
                                   " failed (" + status["msg"] + ")")
            elif (status["code"] != 200):
                raise IpBenchError("LOAD Failed")
        except IpBenchError:
            raise

    def setup(self):
        """
        Set up a client. Assumes that connection is already established.
        """

        global OPTIONS

        try:
            if self.test_port is None:
                self.test_port = 0
            if self.test_target is None:
                raise IpBenchError("Must specify --test-target")
            if self.test_args is None:
                self.test_args = ""

            dbprint("[Setup]: target: " + self.test_target)
            dbprint("[Setup]: port: " + str(self.test_port))
            dbprint("[Setup]: args: " + self.test_args)
            self.send_command("SETUP target::" + self.test_target + "||" +
                              "port::" + repr(self.test_port) + "||"
                              'args::"' + self.test_args + '"')
            status = self.parse_return_code()
            if (status["code"] != 200):
                raise IpBenchError("SETUP to " + self.hostname +
                                   "failed (" + str(status["code"]) + " " + status["str"] + ")")
        except IpBenchError:
            raise

    def start(self):
        """
        Send command to start a client.
        """
        self.send_command("START")

    def call_test_unmarshall(self, client_data, valid):
        """
        Invoke unmarshall feature in test module.
        """
        self.test_ptr.unmarshall(int(self.client_id), client_data, valid)

    def unmarshall(self):
        """
        Start an unmarshalling operation on the remote client and
        try to interpret data.
        """
        dbprint("[unmarshall] client " + self.hostname + " unmarshall ")
        buffer = self.socket.recv(128)
        statusline = copy(buffer).decode('utf-8', errors='ignore')
        newline = statusline.find('\n')
        status = self._parse_return_code(statusline[:newline])
        buffer = buffer[newline+1:]
        if (status["code"] == 220):
            valid = True
        elif (status["code"] == 221):
            valid = False
        else:
            raise IpBenchError("Test failed  : " + status["msg"])

        # clenline is the line with the content-length in it
        initial_content = buffer + self.socket.recv(128)
        clenline = copy(initial_content).decode('ascii', errors='ignore')
        newline = clenline.find('\n')
        r = re.match("Content-length: (\d+)", clenline[:newline])
        if r.group(1) is None:
            raise IpBenchError("Invalid Content-length")
        content_length = int(r.group(1))
        dbprint("[unmarshall] client " + self.hostname +
                " content-length: " + repr(content_length))

        client_data = initial_content[newline + 1:]
        while (len(client_data) != content_length):
            dbprint("[unmarshall] client " + self.hostname +
                    " len(" + repr(len(client_data)) + ") < " + repr(content_length))
            newdata = self.socket.recv(content_length - len(client_data))
            dbprint(("[unmarshall] got " + repr(len(newdata)) + " new bytes "))
            client_data = client_data + newdata

        # wrapper, as the target test doesn't need to be passed an id
        self.call_test_unmarshall(client_data, valid)

    def close(self):
        """
        Terminate a connection with the client. Triggers the ipbench daemon to exit
        and closes socket.
        """
        self.send_command("QUIT")
        self.socket.close()


# the test target is very similar, however it doesn't have the test_target
# hostname since it has no target!
class IpbenchTestTarget(IpbenchTestClient):
    """
    Class encapsulating control over an ipbench target instance for the controller.

    This is only used when a test which expects an instance of ipbenchd --target
    to be running is used. All logic here is simply to manipulate that ipbenchd instance.
    """

    def __init__(self, hostname, port, test_name, test_ptr, test_args=""):
        self.hostname = hostname
        self.port = port
        self.s = None
        self.test_args = test_args
        self.test_name = test_name
        self.test_ptr = test_ptr
        dbprint("[IpbenchTestTarget:__init__] : client " + hostname)
        # TODO: fix inheritance here. Bad practice to have this inherit but never call super or do much with it.

    # setup needs to be slightly different because it only sends the
    # arguments
    def setup(self):
        global OPTIONS

        try:
            self.send_command('SETUP args::"' + self.test_args + '"')
            status = self.parse_return_code()
            if status["code"] != 200:
                raise IpBenchError(
                    "SETUP Failed (" + str(status["code"]) + " " + status["str"] + ")")
        except IpBenchError:
            raise

    # clients don't have a stop command
    def stop(self):
        """
        Trigger test target to stop.
        """
        self.send_command("STOP")

    def call_test_unmarshall(self, client_data, valid):
        """
        Begin unmarshalling operation on test module.
        """
        self.test_ptr.unmarshall(client_data, valid)

##
# main function
##


def main():
    """
    Main function for ipbench tester.
    """
    global OPTIONS
    clients = []
    targets = []

    usage = "usage: %prog [options]"
    parser = OptionParser(usage, version="%prog 2.1.1")
    parser.add_option("-d", "--debug", dest="debug", action="store_true",
                      help="Enable debugging; verbose output.", default=False)

    parser.add_option("-c", "--client", dest="clients", action="append",
                      help="Hostname or IP of a client for the controller to connect to. Repeat this argument multiple times to specify multiple clients.", default=[])

    parser.add_option("-p", "--port", dest="port",
                      help="Port on the clients which the controller will connect to.",
                      type="int", default=8036, action="store")

    parser.add_option("-t", "--test", dest="test", action="store",
                      help="Name of test the clients will run.", default=None)

    parser.add_option("-a", "--test-args", dest="test_args", action="store",
                      type="string", help="Arguments affecting how the clients run their test.", default="")

    parser.add_option("-r", "--reset", dest="reset", action="store_true",
                      help="Send an ABORT to clients before each setup. Useful for cleaning up failed tests.", default=False)

    parser.add_option("-T", "--test-target", dest="test_target", action="store",
                      help="Hostname or IP of the target that the clients will direct their tests at.",
                      default=None)

    parser.add_option("-P", "--test-port", dest="test_port", action="store", type="int",
                      help="Port on the target that the clients will direct their tests at.",
                      default=0)

    parser.add_option("--target-test-hostname", dest="target_test_hostname", action="store", type="string",
                      help="Hostname or IP of the target for the controller to connect to.", default=None)

    parser.add_option("--target-test-port", dest="target_test_port", action="store", type="int",
                      help="Port on the target that the controller will connect to.",
                      default=8037)

    parser.add_option("--target-test", dest="target_test", action="store", type="string",
                      help="Name of test the target will run.", default=None)

    parser.add_option("--target-test-args", dest="target_test_args", action="store", type="string",
                      help="Arguments affecting how the target runs its test.")

    parser.add_option("--test-controller-args", dest="controller_args", action="store", type="string",
                      help="Arguments for the setup_controller function of the test running on the clients. See individual tests for details.",
                      default=None)

    parser.add_option("--target-test-controller-args", dest="target_controller_args", action="store", type="string",
                      help="Arguments for the setup_controller function of the test running on the target. See individual tests for details.",
                      default=None)

    parser.add_option("--config", dest="config", action="store",
                      help="Specify an input config file (XML based, see documentation). Allows flexible and repeatable configuration without command line arguments.", default=None)

    (OPTIONS, _) = parser.parse_args()

    # read in a config file, if specified, and build up a list of clients
    if OPTIONS.config:
        dbprint("Reading config from " + OPTIONS.config)
        doc = parse(OPTIONS.config).getroot()

        tests = doc.findall('test')
        if (len(tests) > 1):
            print("Error in config: please only have one <test> section")
            sys.exit(1)

        # go through attributes for <test>
        for test in tests:
            OPTIONS.test = str(test.get("name", None))
            OPTIONS.test_args = str(test.get('args', ''))
            OPTIONS.test_port = int(test.get('port', '0'))
            OPTIONS.test_target = str(test.get('target', ''))
            OPTIONS.controller_args = str(test.get('controller_args', ''))

        # the test should be known by now
        if OPTIONS.test is None:
            print(
                "Error in config: please specify a test with either --test or in <test>")
            sys.exit(1)

        # go through each <client>, override default values from attributes and add it to the
        # clients[] list
        data = tests[0].findall('./client')
        for cclients in data:
            newclient = {
                "hostname": None,
                "port": OPTIONS.port,
                "test_port": OPTIONS.test_port,
                # careful with encodings
                "test_args": str(OPTIONS.test_args),
                "test_target": str(OPTIONS.test_target)
            }
            newclient["hostname"] = cclients.get('hostname', None)
            newclient["port"] = int(cclients.get('port', OPTIONS.port))
            newclient["test_port"] = int(
                cclients.get('test_port', OPTIONS.test_port))
            newclient["test_args"] = cclients.get(
                'test_args', str(OPTIONS.test_args))
            newclient["test_target"] = cclients.get(
                'test_target', str(OPTIONS.test_target))

            if newclient["hostname"] is None:
                print("Please specifiy a hostname for the client!")
                sys.exit(1)
            clients.append(newclient)

        # now look for the <target_test> section, add to the targets list
        target_tests = doc.findall("target_test")
        for target_test in target_tests:
            target_test_name = target_test.get('name')
            OPTIONS.target_test_args = target_test.get('args')
            OPTIONS.target_controller_args = str(
                target_test.get('controller_args'))

            # it is possible that there will be multiple <target_test>
            # sections, each  with multiple <targets> in them.  so
            # make sure we only select those targets for the target
            # test we are currently looking at.
            tclients = doc.findall(
                "target_test[@name=\'"+target_test_name+"\']/target")
            if (not (len(tclients) > 0)):
                print("Please specify some targets in the <target_test> section!")
                sys.exit(1)
            for tclient in tclients:
                newtarget = {
                    "hostname": OPTIONS.target_test_hostname,
                    "port": OPTIONS.target_test_port,
                    # workaround -- by default
                    "test_args": str(OPTIONS.target_test_args),
                    "test": str(target_test_name)  # comes encoded in utf-16
                }
                for arg in list(tclient.keys()):
                    newtarget["hostname"] = tclient.get('hostname',
                                                        OPTIONS.target_test_hostname)
                    newtarget["port"] = int(tclient.get(
                        'port', OPTIONS.target_test_port))
                    newtarget["test_args"] = str(tclient.get(
                        'test_args', OPTIONS.target_test_args))

                    if newtarget["hostname"] is None:
                        print("Please specify a hostname for the target!")
                        sys.exit(1)

                targets.append(newtarget)

    # finally, append things that are specified from the command line
    if OPTIONS.clients:
        if OPTIONS.test is None:
            print("Please specify a test with --test!")
            sys.exit(1)
        for client in OPTIONS.clients:
            newclient = {
                "hostname": client,
                "port": OPTIONS.port,
                "test_port": OPTIONS.test_port,
                "test_args": OPTIONS.test_args,
                "test_target": OPTIONS.test_target
            }
            clients.append(newclient)

    # you are more limited in your target test choices from the
    # command line, for example you can only specify one target
    # and one test.  but the simple case is covered
    if OPTIONS.target_test:
        if OPTIONS.target_test_hostname is None:
            print("Please specify a target test hostname with --target-test-hostname!")
            sys.exit(1)
        newtarget = {
            "hostname": OPTIONS.target_test_hostname,
            "port": OPTIONS.target_test_port,
            "test_args": OPTIONS.target_test_args,
            "test": OPTIONS.target_test
        }
        targets.append(newtarget)

    # sanity check stuff
    if OPTIONS.test is None:
        print("Please specify a test")
        sys.exit(1)

    if len(clients) == 0:
        print("Please specify some clients")
        sys.exit(1)

    # seeing as all clients are running the same test, we
    # only need one object to interact with them.
    client_test = ipbench_client
    if OPTIONS.debug:
        client_test.enable_debug()
    client_test.load_plugin(OPTIONS.test)
    client_test.setup_controller(len(clients), OPTIONS.controller_args)

    try:
        # setup each of the targets
        for target in targets:
            dbprint("[main] setting up target " + target["hostname"])

            target["shuntobj"] = ipbench_target
            if OPTIONS.debug:
                target["shuntobj"].enable_debug()
            target["shuntobj"].load_plugin(target["test"])
            target["shuntobj"].setup_controller(OPTIONS.target_controller_args)

            target["testobj"] = IpbenchTestTarget(target["hostname"], target["port"],
                                                  target["test_args"], target["test"],
                                                  target["shuntobj"])
            target["testobj"].connect()
            target["testobj"].setup()

        # setup each of the clients.
        client_id = 0
        for client in clients:
            dbprint("[main] setting up client " + client["hostname"])

            if client["test_port"] == 0:
                client["test_port"] = client_test.get_default_port()

            client["testobj"] = IpbenchTestClient(client["hostname"], client["port"],
                                                  client["test_target"], client["test_port"],
                                                  client["test_args"], client_id, OPTIONS.test, client_test)
            client["testobj"].connect()
            client_id = client_id + 1
            client["testobj"].setup()

        # start the target test(s) first (wait for an OK response)
        for target in targets:
            target["testobj"].start()

        # send START command (error responses will be polled for below)
        for client in clients:
            dbprint("[main] client " + client["hostname"] + " start")
            client["testobj"].start()

        wait_to_read = []
        for client in clients:
            wait_to_read.append(client["testobj"])
        wait_for_error = []
        for target in targets:
            wait_for_error.append(target["testobj"])

        # simply select() on all the fd's of the clients
        # (client["testobj"].fileno() provides the fd)
        # once they have data, unmarshall it.
        while len(wait_to_read) > 0:
            (to_read, to_write, to_except) = select.select(
                wait_to_read, [], [], 0.2)
            for client in to_read:
                client.unmarshall()
                wait_to_read.remove(client)
            # additionally; poll any targets incase they have returned
            # some sort of error and we need to abort.  don't wait around
            # for this however
            (to_read, to_write, to_except) = select.select(
                wait_for_error, [], [], 0.2)
            if len(to_read) > 0:
                raise IpBenchError("Target returned an error!")

        # once all clients have reported in, stop any target tests
        # and get their data
        for target in targets:
            target["testobj"].stop()
            target["testobj"].unmarshall()

        # all the clients aggregate their data and output() shows it
        client_test.output()

        # each target gets to display it's own output
        for target in targets:
            target["shuntobj"].output()

    except IpBenchError as err:
        print("Ipbench failed : " + err.value)

    # close all connections and quit
    for client in clients + targets:
        client["testobj"].close()


if __name__ == "__main__":
    main()
