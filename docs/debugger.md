# Debugging

Key files:

| unit                                                                                       | docs                                  |
|--------------------------------------------------------------------------------------------|---------------------------------------|
| [debugger.c](https://github.com/babevm/babevm/blob/master/src/c/debugger/debugger.c){:target="_blank"} | [link](doxygen/html/debugger_8c.html) |


An implementation of the Java Debug Wire Protocol is included.  Almost all standard facilities are there, as well as some optional ones (like inspecting thread monitors and getting raw bytecodes).  Including a JDWP implementation means standard debuggers such as Intellij, VSCode or Eclipse can attach to a running Babe VM instance (or vice-versa) to perform source level debugging with all breakpoint and variable inspection facilities.  Additionally, initial support for JSR045 "Debugging Other Languages" is included, so that a debugger can debug bytecodes / source written in a non-Java language.

Also, there are very few readable example of how remote-debugging actually works in practice, so hopefully the documentation in this work helps to unveil some mysteries. It is not always straight forward, but hopefully the unit level documentation helps.  The docs link above provides a fairly details explanation.    

The JDWP debugger-support code can be optionally compiled in/out of the VM using the `BVM_DEBUGGER_ENABLE` define.  Depending on the compiler, the size of this code may vary but the debugger support code is quite substantial.  Consider excluding it for production builds - the VM will be smaller and will run faster - some performance optimisations are not possible when debugger code is included - namely the use of 'fast opcodes'.  Also, more work is performed every bytecode if debugging is build in.   See [Interpreter](interpreter.md).

The transport layer of the debugger support is abstracted so that any transport may be implemented.  A sockets implementation is provided, but (in theory) the transport could be anything from shared-memory to whatever.  Both client and server modes are supported by the transport abstraction meaning that the VM (in client mode) can 'attach' to a remote running debugger that is waiting for an incoming connection, or (in server mode) will listen for a remote debugger to connect to it.  The VM can also suspend itself on startup waiting for an incoming debugger connection.  The attach and listen addresses can be set using standard JDWP command line options.

The debugger code does complicate things sometimes.  In a VM with native threads the interaction with a debugger might be simpler in the sense when the debugger suspends a thread it really is suspended.  The whole running java/C thread is suspended.  With emulated ('green') threads it is a little more complex and sometimes the VM has to inspect a thread's 'suspend' status after an interaction with the debugger to determine what it should do next.  This can cause some complexity, but hopefully the code comments describe what is going on well enough.

Developers should note that debuggers interact really quite a lot with running VMs - they are very chatty.  Lots of small packets go between debugger and VM - often multiple times for exactly the same thing.  Before attempting to provide 'on-platform' debugger communications it may be worth considering whether 'off-platform' debugging provides what is needed.

To enable the VM debug (assuming it has been built with `BVM_DEBUGGER_ENABLE` defined) the debug config is passed on the command line. Both arguments are required.

```
 -Xdebug -Xrunjdwp:transport=dt_socket,server=y,suspend=y,address=<port>
 ```

The `Xdebug` signals to enable JDWP.

If `server=y` the VM will accept incoming debugger connections on the numeric port given by `address` - the IP address is ignored - just the port is used.  If `server=n` then the VM will attempt to connect to a listening debugger at the address given in `address` as format `<ipaddress>:<port>`.

If `suspend=y` and `server=y` then the VM will pause its startup process and wait for an incoming debugger connection.

At this time, the only implemented transport is `dt_socket`.

