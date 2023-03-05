#!/usr/bin/env python3

import ipbench_client, ipbench_target
import sys
import re
import socket
import select
from copy import copy
from optparse import OptionParser
from lxml.etree import parse

options = None
thetest = None

def dbprint(msg):
    global options
    if options.debug:
        sys.stderr.write(msg + "\n")
#        print >> sys.stderr, msg

class IpBenchError(Exception):
    def __init__(self, value):
        self.value = value
    def __str__(self):
        return repr(self.value)

class IpbenchTestClient :
    """A client class"""
    def __init__(self, hostname, port, test_target, test_port, test_args, id, test_name, test_ptr):
        self.hostname = hostname
        self.port = port
        self.test_port = test_port
        self.test_target = test_target
        self.test_args = test_args
        self.s = None
        self.id = id
        self.test_name = test_name
        self.test_ptr = test_ptr
        if (self.test_args == None):
            self.test_args = ""

        dbprint("[IpbenchTestClient:__init__] : client " + self.hostname +
                " port " + repr(self.port) + " test_port " + repr(self.test_port) + " test_args " + self.test_args)

    #for select()
    def fileno(self):
        return self.s.fileno()

    #parse a return code that looks like
    # 123 CODE(MESSAGE)
    def _parse_return_code(self, data):
        status = {}
        status["code"] = int(data[0:3])
        f = data.find("(")
        if ( f == -1):
            status["str"] = data[4:].strip()
            status["msg"] = ""
        else :
            f2 = data.find(")")
            status["str"] = data[4:f].strip()
            status["msg"] = data[f+1:f2].strip()

        dbprint( "[parse_return_code] : "
                 + " code=" +repr(status["code"])
                 + "|str=" + status["str"]
                 + "|msg=" + status["msg"]+"|")
        return status

    def parse_return_code(self):
        data = self.s.recv(1024).decode('utf-8')
        return self._parse_return_code(data)
    
    def send_command(self, cmd):
        dbprint("[send_command] : " + cmd)
        self.s.send((cmd + "\n").encode('utf-8'))

    def connect(self):
        global options

        dbprint("[connect] start | hostname " + self.hostname + " | port " + repr(self.port) + " | ");

        try:
            self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            try:
                self.s.connect((self.hostname, self.port))
            except:
                sys.stderr.write("Can't connect to " + self.hostname +
                                 " on port " + repr(self.port) +
                                 " ... invalid host or port?\n")
                sys.exit(1)
            status = self.parse_return_code()

            if (status["code"] != 100):
                raise IpBencherror("Invalid Version Flag")

            if (options.reset):
                self.send_command("ABORT")
                #the connection should have closed; re-open
                self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.s.connect((self.hostname, self.port))
                status = self.parse_return_code()
                if (status["code"] != 100):
                    raise IpBenchError("Invalid Version Flag")
            #continue
            #TODO: check versions
            self.send_command("HELLO")
            status = self.parse_return_code()
            if (status["code"] != 200):
                raise IpBenchError("HELLO to " + self.hostname + " failed (" + str(status["code"]) + " " + status["str"] + ")")
            self.send_command("LOAD " + self.test_name)
            status = self.parse_return_code()
            if (status["code"] == 421):
                raise IpBenchError("LOAD to " + self.hostname + " failed ("+ status["msg"] + ")")
            elif (status["code"] != 200):
                raise IpBenchError("LOAD Failed")
        except IpBenchError:
            raise

    def setup(self):
        global options

        try:
            if (self.test_port == None):
                self.test_port = 0
            if (self.test_target == None):
                raise IpBenchError("Must specify --test-target");
            if (self.test_args == None):
                self.test_args = ""
        
            dbprint("[Setup]: target: " + self.test_target);
            dbprint("[Setup]: port: " + str(self.test_port));
            dbprint("[Setup]: args: " + self.test_args);
            self.send_command("SETUP target::" + self.test_target + "||" +
                              "port::" + repr(self.test_port) + "||"
                              'args::"' + self.test_args + '"')
            status = self.parse_return_code()
            if (status["code"] != 200):
                raise IpBenchError("SETUP to " + self.hostname + "failed (" + str(status["code"]) + " " + status["str"] + ")")
        except IpBenchError:
            raise

    def start(self):
        # return is parsed in unmarshall
        self.send_command("START")

    def call_test_unmarshall(self, client_data, valid):
        self.test_ptr.unmarshall(int(self.id), client_data, valid)

    def unmarshall(self):
        dbprint("[unmarshall] client " + self.hostname + " unmarshall ")
        buffer = self.s.recv(128)
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

        #clenline is the line with the content-length in it
        initial_content = buffer + self.s.recv(128)
        clenline = copy(initial_content).decode('ascii', errors='ignore')
        newline = clenline.find('\n')
        r = re.match("Content-length: (\d+)", clenline[:newline]);
        if (r.group(1) == None):
            raise IpBenchError("Invalid Content-length")
        content_length = int(r.group(1))
        dbprint("[unmarshall] client " + self.hostname +
                " content-length: " + repr(content_length))

        client_data = initial_content[newline + 1:]
        while (len(client_data) != content_length):
            dbprint("[unmarshall] client " + self.hostname +
                    " len(" + repr(len(client_data)) + ") < " + repr(content_length))
            newdata = self.s.recv(content_length - len(client_data))
            dbprint(("[unmarshall] got " + repr(len(newdata)) + " new bytes "))
            client_data = client_data + newdata

        #wrapper, as the target test doesn't need to be passed an id
        self.call_test_unmarshall(client_data, valid)

    def close(self):
        self.send_command("QUIT")
        self.s.close()


# the test target is very similar, however it doesn't have the test_target
# hostname since it has no target!
class IpbenchTestTarget(IpbenchTestClient) :
    def __init__(self, hostname, port, test_args, test_name, test_ptr):
        self.hostname = hostname
        self.port = port
        self.s = None
        self.test_args = test_args
        self.test_name = test_name
        self.test_ptr = test_ptr
        dbprint("[IpbenchTestTarget:__init__] : client " + hostname)

    #setup needs to be slightly different because it only sends the
    #arguments
    def setup(self):
        global options

        try:
            if (self.test_args == None):
                self.test_args = ""
            self.send_command('SETUP args::"' + self.test_args + '"')
            status = self.parse_return_code()
            if (status["code"] != 200):
                raise IpBenchError("SETUP Failed (" + str(status["code"]) + " " + status["str"] + ")")
        except IpBenchError:
            raise

    #clients don't have a stop command
    def stop(self):
        self.send_command("STOP")

    def call_test_unmarshall(self, client_data, valid):
        self.test_ptr.unmarshall(client_data, valid)

##
##  main function
##
def main():
    global options
    clients = []
    targets = []

    usage = "usage: %prog [options]"
    parser = OptionParser(usage, version="%prog 2.1")
    parser.add_option("-d", "--debug", dest="debug", action="store_true",
                      help="Enable Debugging", default=False)

    parser.add_option("-p", "--port", dest="port",
                      help="Specify a port to connect to remote clients.",
                      type="int", default=8036, action="store")

    parser.add_option("--config", dest="config", action="store",
                      help="Specify an input config file (XML based, see documentation)", default=None)

    parser.add_option("-c", "--client", dest="clients", action="append",
                      help="Specify a remote client to connect to.", default=[])

    parser.add_option("-t", "--test", dest="test", action="store",
                      help="The test to run", default=None)

    parser.add_option("-a", "--test-args", dest="test_args", action="store",
                      type="string", help="Arguments for the test", default="")

    parser.add_option("-r", "--reset", dest="reset", action="store_true",
                      help="Send an ABORT before setup", default=False)

    parser.add_option("-T", "--test-target", dest="test_target", action="store",
                      help="The target machine that clients should direct testing towards",
                      default=None)

    parser.add_option("-P", "--test-port", dest="test_port", action="store", type="int",
                      help="The port the target machines should use for the test",
                      default=0)

    parser.add_option("--target-test", dest="target_test", action="store", type="string",
                      help="Companion test to run on the DUT", default = None)

    parser.add_option("--target-test-hostname", dest="target_test_hostname", action="store", type="string",
                      help="The IP address or hostname to connect to the target test machine", default = None)

    parser.add_option("--target-test-args", dest="target_test_args", action="store", type="string",
                      help="Arguments for the companion test to run on the DUT")

    parser.add_option("--target-test-port", dest="target_test_port", action="store", type="int",
                      help="Port to talk to the target test daemon",
                      default=8037)

    parser.add_option("--test-controller-args", dest="controller_args", action="store", type="string",
                      help="Arguments for the controller for the main test",
                      default=None)

    parser.add_option("--target-test-controller-args", dest="target_controller_args", action="store", type="string",
                      help="Arguments for the controller for the target test",
                      default=None)

    (options, args) = parser.parse_args()

    # read in a config file, if specified, and build up a list of clients
    if (options.config):
        dbprint("Reading config from " + options.config)
        doc = parse(options.config).getroot()


        tests = doc.findall('test')
        if (len(tests) > 1):
            print("Error in config: please only have one <test> section")
            sys.exit(1)

        # go through attributes for <test>
        for test in tests:
            options.test = str(test.get("name", None))
            options.test_args = str(test.get('args', ''))
            options.test_port = int(test.get('port', '0'))
            options.test_target = str(test.get('target', ''))
            options.controller_args = str(test.get('controller_args', ''))

        # the test should be known by now
        if (options.test == None):
            print("Error in config: please specify a test with either --test or in <test>")
            sys.exit(1)

        # go through each <client>, override default values from attributes and add it to the
        # clients[] list
        data = tests[0].findall('./client')
        for cclients in data:
            newclient = {
                "hostname":None,
                "port":options.port,
                "test_port":options.test_port,
                "test_args":str(options.test_args),    # careful with encodings
                "test_target":str(options.test_target)
                }
            newclient["hostname"] = cclients.get('hostname', None)
            newclient["port"] = int(cclients.get('port', options.port))
            newclient["test_port"] = int(cclients.get('test_port', options.test_port))
            newclient["test_args"] = cclients.get('test_args', str(options.test_args))
            newclient["test_target"] = cclients.get('test_target', str(options.test_target))

            if (newclient["hostname"] == None):
                print("Please specifiy a hostname for the client!")
                sys.exit(1)
            clients.append(newclient)

        # now look for the <target_test> section, add to the targets list
        target_tests = doc.findall("target_test")
        for target_test in target_tests:
            target_test_name = target_test.get('name')
            options.target_test_args = target_test.get('args')
            options.target_controller_args = str(target_test.get('controller_args'))

            # it is possible that there will be multiple <target_test>
            # sections, each  with multiple <targets> in them.  so
            # make sure we only select those targets for the target
            # test we are currently looking at. 
            tclients = doc.findall("target_test[@name=\'"+target_test_name+"\']/target")
            if (not (len(tclients) > 0)):
                print("Please specify some targets in the <target_test> section!")
                sys.exit(1)
            for tclient in tclients:
                newtarget = {
                    "hostname":options.target_test_hostname,
                    "port":options.target_test_port,
                    "test_args":str(options.target_test_args),  #workaround -- by default
                    "test": str(target_test_name)               #comes encoded in utf-16
                    }
                for arg in list(tclient.keys()):
                    newtarget["hostname"] = tclient.get('hostname',
                                                        options.target_test_hostname)
                    newtarget["port"] = int(tclient.get('port', options.target_test_port))
                    newtarget["test_args"] = str(tclient.get('test_args', options.target_test_args))

                    if (newtarget["hostname"] == None):
                        print("Please specify a hostname for the target!")
                        sys.exit(1)

                targets.append(newtarget)

    # finally, append things that are specified from the command line
    if (options.clients):
        if (options.test == None):
            print("Please specify a test with --test!")
            sys.exit(1)
        for client in options.clients:
            newclient = {
                "hostname":client,
                "port":options.port,
                "test_port":options.test_port,
                "test_args":options.test_args,
                "test_target":options.test_target
                }
            clients.append(newclient)

    # you are more limited in your target test choices from the
    # command line, for example you can only specify one target
    # and one test.  but the simple case is covered
    if (options.target_test):
        if (options.target_test_hostname == None):
            print("Please specify a target test hostname with --target-test-hostname!")
            sys.exit(1)
        newtarget = {
            "hostname": options.target_test_hostname,
            "port": options.target_test_port,
            "test_args": options.target_test_args,
            "test": options.target_test
            }
        targets.append(newtarget)


    # sanity check stuff
    if (options.test == None):
        print("Please specify a test")
        sys.exit(1)

    if (not len(clients)):
        print("Please specify some clients")
        sys.exit(1)

    # seeing as all clients are running the same test, we
    # only need one object to interact with them.
    client_test = ipbench_client
    if (options.debug):
        client_test.enable_debug()
    client_test.load_plugin(options.test)
    client_test.setup_controller(len(clients), options.controller_args)

    try:
        # setup each of the targets
        for target in targets:
            dbprint("[main] setting up target " + target["hostname"])

            target["shuntobj"] = ipbench_target
            if (options.debug):
                target["shuntobj"].enable_debug()
            target["shuntobj"].load_plugin(target["test"])
            target["shuntobj"].setup_controller(options.target_controller_args)

            target["testobj"] = IpbenchTestTarget(target["hostname"], target["port"],
                                                  target["test_args"], target["test"],
                                                  target["shuntobj"])
            target["testobj"].connect()
            target["testobj"].setup()

        # setup each of the clients.
        id = 0
        for client in clients:
            dbprint("[main] setting up client " + client["hostname"])

            if client["test_port"] == 0:
                    client["test_port"] = client_test.get_default_port()

            client["testobj"] = IpbenchTestClient(client["hostname"], client["port"],
                                                  client["test_target"], client["test_port"],
                                                  client["test_args"],id,options.test,client_test)
            client["testobj"].connect()
            id = id + 1
            client["testobj"].setup()

        #start the target test(s) first (wait for an OK response)
        for target in targets:
            target["testobj"].start()

        #send START command (error responses will be polled for below)
        for client in clients:
            dbprint("[main] client "+ client["hostname"] + " start")
            client["testobj"].start()

        wait_to_read = []
        for client in clients:
            wait_to_read.append(client["testobj"])
        wait_for_error = []
        for target in targets:
            wait_for_error.append(target["testobj"])

        #simply select() on all the fd's of the clients
        #(client["testobj"].fileno() provides the fd)
        #once they have data, unmarshall it.
        while len(wait_to_read) > 0:
            (toread, towrite, toexcept) = select.select(wait_to_read, [], [], 0.2)
            for client in toread:
                client.unmarshall()
                wait_to_read.remove(client)
            #additionally; poll any targets incase they have returned
            #some sort of error and we need to abort.  don't wait around
            #for this however
            (toread, towrite, toexcept) = select.select(wait_for_error, [], [], 0.2)
            if len(toread) > 0 :
                raise IpBenchError("Target returned an error!")

        #once all clients have reported in, stop any target tests
        #and get their data
        for target in targets:
            target["testobj"].stop()
            target["testobj"].unmarshall()

        #all the clients aggregate their data and output() shows it
        client_test.output()

        #each target gets to display it's own output
        for target in targets:
            target["shuntobj"].output()

    except IpBenchError as e:
        print("Ipbench failed : " + e.value)

    #close all connections and quit
    for client in clients + targets:
        client["testobj"].close()

if __name__ == "__main__":
    main()
