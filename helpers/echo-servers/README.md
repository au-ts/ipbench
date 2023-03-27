# echo-servers

For running ipbench itself, an echo server must be provided. ipbench itself will send the packets and await a response, as well as providing mechanisms to spy on the target system, but ultimately doesn't really care how those packets find their way back to it.

This directory contains several useful examples which you can use as provided or with modifications for your specific platform.

## posix

Posix-compliant OS targets. Linux, macOS, BSD, UNIX


## seL4

Targets for [seL4][sel4repo], a verified microkernel developed by Trustworthy Systems.


[sel4repo]: https://github.com/seL4/seL4