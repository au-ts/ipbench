#!/usr/bin/env python3

import socketserver
import sys
import re
import os
import signal
import mmap
from optparse import OptionParser
from traceback import print_exc

HELP_TEXT = "\nHELP OPTIONS"           \
            "\n------------"           \
            "\nHELP"                   \
            "\nHELLO"                  \
            "\nLOAD"                   \
            "\nSETUP target port args" \
            "\nSTART"                  \
            "\nABORT"                  \
            "\nSTATUS\n"

HELP_TEXT_TARGET = "\nHELP OPTIONS" \
                   "\n------------" \
                   "\nHELP"         \
                   "\nHELLO"        \
                   "\nLOAD"         \
                   "\nSETUP args"   \
                   "\nSTART"        \
                   "\nSTOP"         \
                   "\nABORT"        \
                   "\nSTATUS\n"

options = None
server = None
shunt = None
test_running = None

# we need something that can be seen by all processes
# this is by no means a real mutex, but it does the job
class TestStatus:
    def __init__(self):
        self.mmap_obj = mmap.mmap(-1, 20,
                                  mmap.MAP_SHARED|mmap.MAP_ANONYMOUS)
        self.clear_pid()

    # we really have to hack around the python concept of a mmap
    def write_pid(self, pid):
        self.mmap_obj.write(str(pid).encode('utf-8') + b'\n')
        self.mmap_obj.seek(0)

    def read_pid(self):
        string_pid = self.mmap_obj.readline()
        self.mmap_obj.seek(0)
        return int(string_pid)

    def clear_pid(self):
        self.mmap_obj[0:2] = b'0\n'
        dbprint("[clear_pid] PID Cleared, now " + str(self.read_pid()))

    def running(self, pid):
        dbprint("[running] setting running pid to " + str(pid))
        self.write_pid(pid)

    def not_running(self):
        self.clear_pid()

    def is_running(self):
        p = self.read_pid()
        return (p > 0)

    def running_pid(self):
        return self.read_pid()


#
# debugging output
#
def dbprint(msg):
    if options.debug:
        print(msg, file=sys.stderr)

def debugging():
    return options.debug

def str_status(errCode, *msg):
    errCodes = {
        100 : "IPBENCH V1.0",
        200 : "OK",
        220 : "VALID DATA",
        221 : "INVALID DATA",
        222 : "CLOSING CONTROL CONNECTION",
        400 : "ERROR",
        421 : "SERVICE NOT AVAILABLE",
        451 : "LOCAL PROCESSING ERROR",
        500 : "SYNTAX ERROR"
        }

    if (len(msg) == 0):
        return  (repr(errCode) + " " + errCodes[errCode] + "\n" )
    return (repr(errCode) + " " + errCodes[errCode] + " (" + " ".join(msg) +  ")\n")


#a simple state machine class
class StateMachine:
    def __init__(self):
        self.handlers = {}
        self.startState = None
        self.endStates = []

    def add_state(self, name, handler, end_state=0):
        name = name.upper()
        self.handlers[name] = handler
        if end_state:
            self.endStates.append(name)

    def set_start(self, name):
        self.startState = name.upper()

    def run(self, cargo):
        try:
            handler = self.handlers[self.startState]
        except:
            raise Exception("InitializationError", "must call .set_start() before .run()")

        if not self.endStates:
            raise Exception("InitializationError", "at least one state must be an end_state")

        while 1:
            (newState, cargo) = handler(cargo)
            if newState.upper() in self.endStates:
                break
            else:
                handler = self.handlers[newState.upper()]


##
## Client Object
## This implements the state machine
##
class IpbenchClient:
    def __init__(self, fds):
        global shunt, options

        dbprint("IpbenchClient [__init__] : client mode")
        import ipbench_client
        shunt = ipbench_client

        if (options.debug):
            shunt.enable_debug()

        (self.rfile, self.wfile) = fds

    def send(self, mesg):
        self.wfile.write(mesg.encode('utf-8'))

    def readline(self):
        return self.rfile.readline().decode('utf-8')

    def parse_common_opts(self):
        global test_running, options
        while (1):
            line_in = self.readline().strip()

            cmd = re.match("\S*", line_in).group(0)
            if (cmd == None):
                self.send(str_status(500))
                continue

            elif (cmd == "HELP"):
                if (options.target):
                    self.send(HELP_TEXT_TARGET)
                else:
                    self.send(HELP_TEXT)
                self.send(str_status(200))

            elif (cmd == "STATUS"):
                if (options.target):
                    self.send("Target Daemon mode\n")
                else:
                    self.send("Testing Daemon mode\n")
                if (not test_running.is_running()):
                    self.send("Not running a test\n")
                else:
                    pid = test_running.running_pid()
                    self.send("Running a test (%d)\n" % pid)
                self.send(str_status(200))

            else:
                dbprint("[parse_common_opts]: got line " + line_in)
                return line_in

    def listen(self, cargo):
        global test_running
        dbprint("[listen] start");
        while 1:
            line_in = self.parse_common_opts()
            if (line_in == "ABORT"):
                return("ABORT",1)
            if (line_in == "QUIT"):
                return("ERROR",(222, "Come back soon!"))

            cmd = re.match("(\S+)", line_in)
            if (cmd == None):
                self.send(str_status(500))
                continue
            else:
                cmd = cmd.group(0)

            if (cmd == "HELLO"):
                #process the hello, if we are already running bail
                if ( test_running.is_running()):
                    return ("ERROR", (421, "Already Running"))
                else:
                    self.send(str_status(200, "Ready to go"))
                    test_running.running(os.getpid())
                    return("SETUP", 1)

            self.send(str_status(500))

    #the setup state
    def setup(self, val):
        global shunt
        dbprint("[setup] start")
        while 1:
            line_in = self.parse_common_opts()
            if (line_in == "ABORT"):
                return("ABORT",1)
            if (line_in == "QUIT"):
                return("ERROR",(222, "Come back soon!"))

            rematch = re.match("(\w+) ([\s\w]+)", line_in)
            if (rematch == None):
                self.send(str_status(500))
                continue
            else:
                cmd = rematch.group(1)
                arg = rematch.group(2)

                if (cmd == "LOAD"):
                    try:
                        shunt.load_plugin(arg)
                    except RuntimeError as detail:
                        return("ERROR", (451, str(detail).strip()))
                    else:
                        #go into an inner loop where we take a setup command
                        while 1:
                            self.send(str_status(200))

                            line_in = self.parse_common_opts()
                            if (line_in == "ABORT"):
                                return("ABORT",1)
                            if (line_in == "QUIT"):
                                return("ERROR",(222, "Come back soon!"))

                            rematch = re.match("(\w+) (..*)", line_in)
                            if (rematch == None):
                                self.send(str_status(500))
                                continue
                            else:
                                cmd = rematch.group(1)
                                arglist = rematch.group(2)

                                for arg in arglist.split("||"):
                                    (subcmd,val) = arg.split("::")
                                    if (subcmd == "target"):
                                        test_target = val
                                    if (subcmd == "port"):
                                        test_port = int(val)
                                    if (subcmd == "args"):
                                        test_args = val[1:-1]

                            if (cmd == "SETUP"):
                                try:
                                    if (not options.target):
                                        dbprint("[setup] Setting up client with " +
                                                test_target + " " + repr(test_port) + " " + test_args)
                                        shunt.setup(test_target, test_port, test_args)
                                    else:
                                        dbprint("[setup] Setting up target with args " + test_args)
                                        shunt.setup(test_args)

                                except RuntimeError as detail:
                                    return("ERROR", (451, str(detail).strip()))

                            self.send(str_status(200))
                            return("RUN", 1)

                self.send(str_status(500))

    def run(self, val):
        dbprint("[run] start")
        while 1:
            line_in = self.parse_common_opts()
            if (line_in == "ABORT"):
                return("ABORT",1)
            if (line_in == "QUIT"):
                return("ERROR",(222, "Come back soon!"))

            rematch = re.match("(\w+)", line_in)
            if (rematch == None):
                self.send(str_status(500))
                continue
            else:
                cmd = rematch.group(1)

                #the start command kicks us off; when we return we go
                #into marshall phase
                if (cmd == "START"):
                    dbprint("[run] starting actual test")
                    try:
                        shunt.start()
                    except RuntimeError as detail:
                        return("ERROR", (451, str(detail).strip()))
                    dbprint("[run] test complete")
                    return("MARSHALL", 1)
                self.send(str_status(500))

    def marshall(self, val):
        global test_running
        dbprint("[run] marshall start")
        try:
            (valid,data) = shunt.marshall()
        except RuntimeError as detail:
            return("ERROR", (451, str(detail).strip()))

        if (valid != 0):
            self.send(str_status(221, "Data to follow"))
        else:
            self.send(str_status(220, "Data to follow"))

        self.send("Content-length: " + repr(len(data)) + "\n")
        self.wfile.write(data)

        test_running.not_running()
        return("RESET", 1)

    #dummy function that falls out of the state machine and resets
    def reset(self, fd): pass

    def error(self, val):
        global test_running
        dbprint("[error] start")
        (errCode, msg) = val
        # if aborting because test is already running, don't
        # flag test as not running!
        if (errCode != 421):
            test_running.not_running()
        self.send(str_status(errCode, msg))
        return ("RESET",1)

    def abort(self, val):
        global test_running
        if (not test_running.is_running()):
            dbprint("[abort] test isn't actually running ...")
            return ("RESET",1)

        pid_to_kill = test_running.running_pid()
        dbprint("[abort] " + repr(os.getpid()) + " aborting " + repr(pid_to_kill))
        try:
            os.kill(pid_to_kill,signal.SIGINT)
        except ProcessLookupError:
           test_running.not_running()
           return
        try:
            os.kill(pid_to_kill,signal.SIGKILL)
        except ProcessLookupError:
            pass
        #usually we will have killed *ourself*, but possibly not
        dbprint("[abort] reset")
        return("RESET", 1)


##
## TARGET
##
class IpbenchTarget(IpbenchClient):
    def __init__(self, fds):
        global shunt, options

        dbprint("[__init__] IpbenchClient [__init__] : target mode")
        import ipbench_target
        shunt = ipbench_target

        if (options.debug):
            shunt.enable_debug()

        (self.rfile, self.wfile) = fds

    #only run() needs to be different for the target; as the
    #test must be forked and run in a sub process that is killed
    #when we receive the STOP command
    def run(self, val):
        dbprint("[run] start")
        while 1:
            line_in = self.parse_common_opts()
            if (line_in == "ABORT"):
                return("ABORT",1)
            if (line_in == "QUIT"):
                return("ERROR",(222, "Come back soon!"))

            rematch = re.match("(\w+)", line_in)
            if (rematch == None):
                self.send(str_status(500))
                continue
            else:
                cmd = rematch.group(1)

                if (cmd == "START"):
                    dbprint("[run] starting target test")
                    ppid = os.getpid()
                    pid = os.fork()
                    if pid > 0:
                        # Parent
                        try:
                            shunt.start()
                        except RuntimeError as detail:
                            os.kill(pid, signal.SIGKILL)
                            dbprint("[run] target test start() failed!")
                            return("ERROR", (451, str(detail).strip()))
                    else:
                        #we are in a child, listen for stop and send SIGUSR1
                        #to parent to stop the test
                        #write_status(self.wfile, 200)
                        dbprint("Child")
                        #self.send("Child")
                        line_in = self.parse_common_opts()
                        if (line_in == "ABORT"):
                            os.kill(ppid, signal.SIGINT)
                            os._exit(0)
                        if (line_in == "QUIT"):
                            return("ERROR",(222, "Come back soon!"))
                        rematch = re.match("(\w+)", line_in)
                        if (rematch == None):
                            self.send(str_status(500))
                            continue
                        else:
                            cmd = rematch.group(1)
                            if (cmd == "STOP"):
                                dbprint("[run] got stop command, sending SIGUSR1 to " + repr(ppid))
                                os.kill(ppid, signal.SIGUSR1)
                                # Can't use sys.exit() as it'll close the socket
                                os._exit(0)

                    # In parent
                    dbprint("[run] test complete")
                    return("MARSHALL", 1)
                #unknown command
                self.send(str_status(500))

##
## Request Handlers
##
class IpbenchRequestHandler(socketserver.StreamRequestHandler):

    def handle(self):
        global options
        try:
            self.wfile.write(str_status(100).encode('utf-8'))
        except Exception as e:
            dbprint("Exception %s" % str(e))

        try:
            dbprint("[handle_child] new handler ["+repr(os.getpid())+"]")

            m = StateMachine()
            if (options.target):
                c = IpbenchTarget((self.rfile, self.wfile))
            else:
                c = IpbenchClient((self.rfile, self.wfile))

            #setup state machine
            m.add_state("LISTEN", c.listen)
            m.add_state("SETUP", c.setup)
            m.add_state("RUN", c.run)
            m.add_state("MARSHALL", c.marshall)
            m.add_state("ERROR", c.error)
            m.add_state("ABORT", c.abort)
            m.set_start("LISTEN")
            m.add_state("RESET", c.reset, end_state = 1)
            m.run(1)

            dbprint("[handle] state machine done")
        except Exception as e:
        #exit and close
            dbprint("[handle] " + repr(os.getpid()) + " caught exception ... finishing up: " + str(e))
            print_exc()
            sys.exit(0)


class IpbenchClientServer(socketserver.ForkingMixIn,socketserver.TCPServer):
    allow_reuse_address = True

    def handle_error(self, request, client_address):
        """Handle an error gracefully.  Overrides standard method.

        The default is to print a traceback and continue, except in
        the case of a SystemExit exception, which should terminate
        processing.

        """
        raise
        dbprint("[handle_error] handling it ...")
        import exceptions
        etype, evalue = sys.exc_info()[:2]
        if etype is exceptions.SystemExit: raise
# extra exception debugging if you need it
#        print '-'*40
#        print 'Exception happened during processing of request from',
#        print client_address
#        import traceback
#        traceback.print_exc(1000, sys.stdout)
#        print '-'*40
#        sys.stdout.flush()



##
## MAIN
##
def main():
    global options, server
    usage = "usage: %prog [options]"
    parser = OptionParser(usage, version="%prog 2.0")
    parser.add_option("-i", "--ip", dest="ip",
                      help="Ip to bind to", type="string", default="", action="store")
    parser.add_option("-p", "--port", dest="port",
                      help="Port to listen on", type="int", default=8036, action="store")
    parser.add_option("-d", "--debug", dest="debug", action="store_true",
                      help="Enable Debugging", default=False)
    parser.add_option("-t", "--target", dest="target", action="store_true",
                      help="Put the daemon in target mode")

    (options, args) = parser.parse_args()

    if (options.target):
        options.port = 8037

    server = IpbenchClientServer((options.ip, options.port), IpbenchRequestHandler)


    global test_running
    test_running = TestStatus()
    dbprint("[main] listening on port " + repr(options.port) + " [" + repr(os.getpid()) + "]")

    try:
        server.serve_forever()
    except:
        dbprint("[main] caught exception from serve_forever()")

if __name__ == "__main__":
    main()
