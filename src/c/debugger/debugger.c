/*******************************************************************
*
* Copyright 2022 Montera Pty Ltd
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*******************************************************************/

#include "../../h/bvm.h"

/**
  @file

  JDWP functions.

  @section debug-ov Debugging Overview

  The Babe VM Java debugging facilities are an implementation of the Java Debug Wire Protocol (JDWP), which is a
  layer of the Java Platform Debug Architecture (JPDA).  The JPDA is a useful standard that debuggers such as
  Intellij, VSCode, and Eclipse can implement (or build upon a reference implementation).  As of Java 1.6,
  Sun deprecated other means of providing debugging facilities in a VM.

  Not 100% of the JDWP specification is implemented in the Babe VM.  Notable omissions are:

  @li The debugger cannot call methods in the VM.
  @li The debugger cannot instantiate VM objects.
  @li Classpath information is also not provided by the VM.
  @li Nested type information for classes is not provided by the VM.
  @li Classes can not be redefined on-the-fly, nor can methods be replaced.
  @li Stack depth information for owned thread monitors is not provided.
  @li Methods cannot be forced to return early.
  @li 'Referring objects' are not supported.
  @li Instance counts are not provided.
  @li Field access and modification breakpoints are not supported.
  @li 'Instance only' filtering for events is not supported.

  Still, without these (mostly optional) things it is possible to fully and effectively debug an application.
  Some optional extras are implemented such supporting JSR045 "Debugging Other Languages".

  As a side note, many debuggers provide the ability to evaluate expressions on the fly (often called
  "conditional breakpoints").  This facility often involves calling methods in the VM  - sometimes
  'reflective' methods that are not implemented in Babe.  As a result, conditional breakpoint use in most
  debuggers will be restricted.

  Before explaining how the Babe VM implements JDWP it would be wise to provide a brief overview of the JDWP.

  @section debug-jdwp-101 JDWP 101

  The JDWP defines an on-the-wire message exchange protocol between a Debugger (aka 'front end') and a Debuggee
  (aka 'back end' or 'VM').  The JDWP does not define the transport layer for how those messages move between parties.
  It defines what the messages are, and when they should be sent.  The JDWP also defines the expected behaviour of a
  debuggee when it has received certain messages, and it expects the debuggee to attain certain states at certain
  times - for example, to suspend a thread when it has been told to do so.

  @subsection debug-jdwp-messages JDWP Messages

  The debugger/debuggee relationship is maintained by sending and receiving messages.  The messages themselves are
  not complex.  They are always either a 'command' or a 'reply'.  Command and reply message headers are the same size
  but their payload data (if any) and format is defined on a per-message-type basis.

  Typically, the messages that a debugger sends to the debuggee are 'commands'.  Debugger->debuggee commands are
  either a request for information (like the value of a method-local variable), or a request to be notified
  by the debuggee that a particular event has occurred (like when a given class is loaded, or a given
  bytecode location is about to be executed).

  The only time the debuggee sends the debugger a command is to signal that an event (which the debugger has
  requested to be notified about) has actually happened.

  So the message flow between the two parties is really:

  @li Debugger->debuggee : commands for debuggee state information and also event registration.
  @li Debuggee->debugger : replies to commands, and also registered-event command notifications.

  (Notice the debugger does not send replies to the debuggee?  The JDWP spec says the debugger does not
  have to respond to commands from the debuggee - most debuggers don't).

  So, it can largely be said that all messages relate to getting and setting state.

  All these messages are async.  Commands are sent. Commands are received. Replies are sent. Replies are
  received.  The JDWP actually specifies that these things are asynchronous.

  @subsection debug-jdwp-messages JDWP Command messages

  JDWP commands are divided logically into numbered groups of related commands called a 'commandset's.  For
  example, one commandset will deal with messages/replies for thread references, and another commandset
  deals with stack frame information, another with methods and so on.

  Within each commandset is a set of numbered commands.  Each command message carries two bytes that specify
  the commandset and command.

  Each command may carry data in its payload to further define the command - like a 'get thread name' command
  will specify the id of the thread to get the name of.  Some commands carry no further information - like
  'suspend all threads' needs nothing more.

  Like commands, each reply may or may not have associated data.  Unlike commands, replies have an error
  code - and it occupies the same bytes as the commandset/command.

  @subsection debug-jdwp-ids JDWP Identifiers (IDs)

  When a debugger requests (say) information on all threads, rather than serialise the threads in some way
  between debuggee/debugger, the debuggee will send the 'ID' of each thread.  The ID is an opaque JDWP reference
  to something.  The debugger has no knowledge of what the ID actually is within the debuggee, or how the
  debuggee actually manages those IDs.  But, the IDs are used as the means to identify a given 'thing' between
  the two parties.

  An ID is unique within the debug session.  IDs are not reused and generally therefore cannot be tied simply to a
  memory address - although this is not always the case.

  It is the responsibility of the debuggee to track all the IDs it has provided to the debugger.  This does not
  mean that by handing the debugger an ID the debuggee can no longer garbage collect the associated 'thing', it
  just means that it has to know that it has/has not been collected when the debugger makes reference to the ID in
  a command.

  Having said that, the debugger may actually ask the debuggee to *not* make an object eligible for
  garbage collection - even though it may be unreachable by the collector.

  @subsection debug-jdwp-events JDWP Events

  'Events' are the means by which a debugger can be notified when something occurs in the debuggee.
  Events are sent by the debuggee when the debugger has specifically requested it (the only exception to
  this are the VM start/death events, which are unsolicited).

  The event notification lifecycle begins with the debugger sending an 'event set' command to the
  debuggee.  Each 'event set' has an 'event kind' which defines the type of event the debugger is interested
  in (like say, an EXCEPTION eventkind - which notifies when an exception has occurred).  Additionally, each
  'event set' command may carry with it a number of 'filters' that further define the notification
  expectations of the debugger.  For example, the above EXCEPTION eventkind request may have a filter on
  it to limit notification to a particular exception class type and it subtypes.

  When the debuggee receives an 'event set' command its responsibility is to establish that definition, with
  filters, and then comply by sending the debugger event commands when an event occurs that satisfies and event
  definition and its filters.

  An event definition and its associated filters remain active until such time as the debugger sends a command
  to the debuggee to 'clear' it.

  Perhaps the most well known type of event is a 'breakpoint'.  When the debugger user (developer) sets a
  breakpoint the debugger sends an 'event set' command to the debugger specifying the location of the breakpoint.
  Note that if the class has not yet been loaded by the debuggee, the debugger may actually first request
  to be notified of the 'CLASS_PREPARE' event, specifying a filter on the breakpointed class name -
  after it receives notification that the class has been loaded, it can clear the CLASS_PREPARE event, and
  now send a breakpoint 'event set' to the debuggee.  Such is the way the two communicate.

  After receiving a breakpoint command, the debuggee must then monitor program execution and when a
  bytecode execution location is reached that satisfies the filters on the breakpoint event definition, the
  debuggee must sent a breakpoint event command to the debugger.

  @subsection debug-jdwp-suspendpolicy JDWP Event Thread Suspension Policies

  Associated with each 'event set' that a debugger may send a debuggee is a 'thread suspend policy'.  The
  policy describes what threads should have their execution suspended as the event is sent from the debuggee.
  The available policies are SUSPEND_NONE, SUSPEND_THREAD, and SUSPEND_ALL.

  When an event command is sent from the debugger it must respect the suspend policy.

  Why?  Consider the above 'breakpoint' example, where the debugger has requested a breakpoint
  on a line in a class that has not yet been loaded by the debuggee - so the debugger actually first sends a
  CLASS_PREPARE 'event set' command specifying a class name filter.  The debugger wants first to be notified
  that the class is loaded before it sets the breakpoint.

  When the debuggee actually does load the class it will send a CLASS_PREPARE event command to the debugger.  The
  debugger wants to set a breakpoint in the class before any code in it is executed, so .... it specifies
  a SUSPEND_THREAD policy for the CLASS_PREPARE event - which means that when the debuggee sends the
  CLASS_PREPARE event, it suspends the running thread immediately after it has sent the event.  The
  debugger then  (while the thread is suspended) sends a BREAKPOINT 'event set' back to the debuggee
  specifying a filter on the class and a location - and also with SUSPEND_THREAD policy on
  the breakpoint.

  After setting the new breakpoint and clearing the class prepare event request (so the debuggee will not
  send that event again), it sends a command to resume the thread.  Presto!  The debugger suspended thread
  execution when it received notification of a class being loaded, set a breakpoint for the class, and then
  resumed thread execution.

  A thread suspension policy of SUSPEND_ALL means that all threads in the debuggee will be suspended until such
  time as the debugger resumes one or all of them.  In this state, debuggees generally will simply do
  nothing except wait for commands from the debugger.

  Thread suspensions are counted - meaning a thread must be resumed the same number of times it has been
  suspended before it will truly resume execution.

  @subsection debug-jdwp-jvmti A note about JVMTI

  The Java Virtual Machine Tool Interface (JVMTI) is a standard interface layer on JNI for accessing
  state information about a VM and controlling it (like for debugging and/or profiling purposes).

  The standard JDWP implementation is implemented as a JVMTI 'agent' - it rests on the functionality provided
  by JVMTI.

  The JDWP specification is intended to be able to be implemented without JVMTI (indeed, JDWP existed
  before JVMTI) and is intended to be able to be implemented 'directly' just as the Babe VM does.

  In implementing JDWP straight 'to-the-wire', rather than on top of JVTMI we can make life a lot
  simpler for this simple VM.

  To that end, the Babe VM JDWP implementation does not use a JVMTI or JNI analogue.

  @subsection debug-jdwp-transport JDWP Transport Interface

  The JDWP specification is intended to be be 'transport agnostic' in the sense that the messages and
  behaviour are what it really defines - not how the messages get between parties.

  However, for the purposes of standardisation among JVMTI VMs, there is a JDWP Transport Interface
  specification.  This small specification defines a C interface struct used to nominate function pointers
  to functions for attaching to, listening for, closing, sending, receiving and so on.

  The Babe VM uses a very similar approach and follows the JDWP transport specification (mostly).  It uses the
  same pattern, but not necessarily in exactly the same way.  For example, the spec defines 'send_packet' and
  'read_packet' functions in the interface which operate at a high level on JDWP 'packets'.  In this
  VM the interface uses 'send_bytes' and 'read_bytes' to operate at a lower level.

  The JVMTI also defines lifecycle for the JDWP transport (and the JDWP agent for that matter), but we have no
  JVMTI analogue here so the lifecycle aspects of the transport are not there.

  @section debug-babe-impl The Babe VM JDWP Implementation

  This section provides an overview of how the JDWP specification is achieved in the Babe VM.  This section will
  take the form of "how" sub-sections.  Each subsection will provide an overview of how an particular
  aspect of JDWP spec is implemented.

  @subsection debug-babe-impl-startup VM startup with debugging

  The debugger facilities use a number of global variables with the prefix \c 'bvmd_gl_'.  Many of these variables are
  set as a result of parsing the commands line arguments \c Xdebug and \c Xrunjdwp.  For example, the
  #bvmd_gl_enabledebug global will be \c BVM_TRUE if the command line has \c Xdebug.

  At VM startup if the global variable #bvmd_gl_enabledebug is \c BVM_TRUE, debugging will be enabled for
  the life of the executing VM, otherwise, all debug facilities are bypassed and the VM operates
  largely as normal.

  If debugging *is* enabled, the VM will attempt to establish communications with the debugger immediately
  after the VM has been initialised -  even before the 'main' java class has been loaded.

  How the VM establishes debugger communications depends on the transport implementation (described in the
  next section) and on some more global variable settings.

  Firstly, the transport interface is initialised by calling #bvmd_transport_init.  This function may be
  replaced on a per-platform basis.

  If #bvmd_gl_server is \c BVM_TRUE on VM startup the VM will ask the transport to listen on the address
  defined by #bvmd_gl_address. The contents of \c bvmd_gl_address are of use only to the underlying
  transport.  The VM places no inherent restriction on the format on the listening address.  As an example,
  when listening the provided sockets transport treats the address as the port to listen on.

  If #bvmd_gl_server is \c BVM_FALSE on VM startup the VM will ask the transport to 'attach' to the address
  defined by #bvmd_gl_address.  Again, the address is only meaningful to the transport.  In the socket
  transport, the address is of the format "host:port" (but that is just for the provided socket transport).

  The values of #bvmd_gl_timeout will be passed to the transport as the timeout value.

  Failure to establish communications with the debugger means the VM will continue as though #bvmd_gl_enabledebug
  where false.

  If #bvmd_gl_suspendonstart is \c BVM_TRUE, after establishing communications with the debugger the VM
  will suspend all threads and await commands from the debugger.  It will remain in this suspended
  state until the debugger resumes it.  The VM will then proceed normally and load the main
  class and start the interp loop as per normal.

  From then on, with debugging communication established, some time is given to checking for commands
  from the debugger every thread switch.

  @subsection debug-babe-impl-comms Communicating with the Debugger

  As per the JDWP spec, messages pass between Babe and a debugger.  The Babe debuggee code relies on a
  global C interface #bvmd_gl_transport (of type #bvmd_transport_t) being populated with function
  pointers to transport functions for sending and receiving those messages.

  This interface also defines the capabilities of the underlying transport, like whether it can listen
  or attach and so on.  It allows the VM to request debug transport functionality without knowing the
  actual underlying transport or implementation.

  In JDWP, messages are actually called 'packets', and in Babe they are represented by the #bvmd_packet_t
  type which is a union of #bvmd_cmdpacket_t and #bvmd_replypacket_t.  The JDWP spec is careful to make
  the size of a command the same size as a reply so they can generally be acted upon by the same code.

  Debug packets may contain a linked list of 'packet data', represented by #bvmd_packetdata_t.  This linked
  list is the actual data payload of the packet.  Every packetdata is a fixed size and may hold, at max,
  #BVM_DEBUGGER_PACKETDATA_SIZE bytes.

  Even though the command and reply packets are largely the same animal, Babe differentiates between them when
  appropriate.  Having them as the same structure means sending and receiving can use the same code
  for both commands and replies.

  At each thread switch, the VM determines if there is anything waiting to be send to, or received from
  the debugger. If a packet is waiting to be read from the debugger, it is read, and if it is
  a command it is acted upon immediately and the reply send straight away.  If it is a reply from the
  debugger it is simply ignored.  Babe does nothing with reply packets received from the debugger.

  @subsection debug-babe-impl-streams Packet Streams

  Read and writing to a packet uses a 'streams' metaphor.  Command and reply packets are not passed around
  inside the VM per-se, they are passed around as attributes of 'packet streams'.  Babe packet streams are
  represented by the #bvmd_packetstream_t, #bvmd_instream_t, and #bvmd_outstream_t types.  All are actually the
  same, but the different names help to identifying usage within the code.

  Each stream encompasses a packet, and keep references to the current #bvmd_packetdata_t being read from / written to
  and within that, the current read/write location.

  In reality, input streams are only populated with data when they are read from the transport.  They are
  never written to.  Likewise, output streams are never read from, only written to while creating something to
  send to the debugger.  Output streams grow by adding to the \c bvmd_packetdata_t linked list within
  them as they are written to.  Input stream are not extended as such as they are only ever manipulated during
  packet reading - and there the code is more specific.

  Using an input stream to read packet data sent from the debugger and an output stream to create the packet and data
  to send to the debugger makes life much smoother.  The size of a output stream is unknown and may grow as need be.

  When the packet is finally being sent to the debugger, the packetdata linked list is traversed and all its
  data sent.

  Input and output streams both have functions to read / write to them respectively.

  @subsubsection debug-babe-impl-streams-bookmarks Bookmarks

  On occasion, when writing to an output stream, the position of a write needs to be 'bookmarked' so that
  later the written data can be overwritten with a new value.  This is especially useful when a 'length' needs
  to prefix a list of things that follow - and the length is not known until after the list is output.  To assist,
  each of the write operations on an output stream returns a #bvmd_streampos_t bookmark.  This bookmark can be used
  later to 'rewrite' some data back at some previous place in the stream.  Typically, for lists where a 'length'
  precedes the list, a zero is first written to the stream, and then later, after the entire list has been written,
  the zero length is overwritten with the real length.

  @subsubsection debug-babe-impl-streams-order Host order vs Network Order

  All numeric data written to an output stream is first converted from 'host order' to 'network order'.  So, even
  though on (say) Intel systems the 'host order' for numerics is little-endian, when they are written to the
  output stream they are converted to 'network order' (big-endian).  Likewise for reading from stream.  As
  numerics are read from an input stream, the reverse happens - they are converted from 'network order'
  to 'host order'.

  @subsection debug-babe-impl-commands Command Handling

  Each JDWP commandset has a corresponding array of function pointers to 'command handler'
  functions (#bvmd_cmdhandler_t function pointer type).  Each function pointer corresponds to a given
  command within the commandset.

  All commandset arrays are gathered into a single array of commandset arrays (yes, an array of arrays)
  named #bvmd_gl_command_sets.

  When a command packet is received from the debugger, the 2 commandset / command bytes within the
  header are inspected and used to firstly find the right commandset array of command handlers, and then within
  that array, to find the specific command handler.  The command handler function is called passing the
  input stream and also an output stream, where the command handler writes its response.

  If the specified commandset/command is not valid, or the VM does not implement it, a default command handler
  is called that simply returns an error.

  This simple structure means that command handling for a given command can be understood by locating the
  command handler.  Often the command handlers are self-contained and are handled by just a single function.

  @subsection debug-babe-impl-ids ID Management

  The VM sends IDs for numerous things to the debugger.  The JDWP spec defines a number of IDs type.  Each may
  have its own size.  In Babe, all IDs are four bytes.

  When an ID is sent to the debugger, for some types an association between the ID and the memory address is kept.

  The 'dbg id' map is where this takes place.  This is actually two hash maps cross-linked by ID and address.

  When any java object or VM #bvm_clazz_t is to be sent from VM to debugger, its address is looked up in the ID map
  to see it is already has a corresponding ID (meaning it has already been 'sent' to the debugger).  If no
  entry is found a new one is created with a new unique ID to link the address and the ID.  The ID it sent
  to the debugger.

  During garbage collection, the collector inspects the dbg ID map as it frees memory - to remove an associated
  ID/address from the map.  In this way, after a GC, any freed addresses will no longer be in the dbg ID map.

  If the debugger specifies a given ID in a command (like say, a thread ID) and a lookup into the dbg ID map
  finds nothing, then it must have been garbage collected, and an error is returned to the debugger.

  Note that only objects and clazzes get entries in the dbg ID map.  Field IDs, Method IDs, and Frame IDs are
  communicated to the debugger as their actual in-memory address.  Field and method IDs are always qualified
  by the debugger by their referencetype (clazz) IDs, so when using a field or a method ID, the dbg ID map is
  always checked first to make sure their associated clazz is still in there.  A missing clazz entry in the dbg ID
  map means the clazz was collected and the ID (memory address) of the field or method should not be
  used and the debugger is sent an error reply.

  Frame IDs are not managed in any way.  Frame IDs sent to the debugger are actually the 'locals' pointer of
  a given frame on the stack.  Tracking unique IDs as frames are pushed and popped would be a substantial amount
  of work for the VM, so here, we trust the debugger to make careful use of frame ids.

  These ID management patterns are intended to reduce the number of entries in the dbg ID map, and therefore
  memory and processor time for ID lookups and management.

  @subsection debug-babe-impl-threads Threads

  Unlike many VMs, Babe does not use native threads, it has a 'green' threads implementation that is above
  the OS's threads.  Green threads are both a blessing and a nightmare.  Blessing because they mean portability -
  nightmare because they are not real threads and a thread 'suspend' does not really suspend a true 'thread'.

  When the debugger sets an event request for (say) a CLASS_PREPARE, it associates a thread suspend policy
  with the event request so that when the VM does send an event for it, the suspend policy will be obeyed.
  Typically, for such an event, the policy will be SUSPEND_THREAD, meaning that just thread that the class prepare
  occurred in should be suspended.

  In a VM with native threads, the entire running thread with C stack and Java stack would be suspended.  Communications
  with the debugger occurs in a another thread.  In the Babe VM, the C code cannot be suspended, it must keep
  going regardless of whether the current Java thread was suspended or not.  Yes, the Java thread may be
  suspended, but C is not.

  For this reason, sometimes in the VM debugger code you will see some gymnastics with regard to thread statuses.
  Effectively, when a thread is suspended by debugger interaction, it must, in the eyes of the debugger, do nothing
  else until resumed.  That means it must cause no further events, or execute any bytecode until such time as it is
  resumed by the debugger.

  Sometimes this is not possible.  For example, say the debugger requests an event for a class prepare.  Preparing
  a class also means preparing its parent and so on (if they are not already prepared).  If one of the parent
  class prepare events causes an event and that causes the java thread to be suspended, the debugger does not want to
  see any more prepares or events AT ALL until it resumes the thread.  But, we can't just stop the C code half way
  though a bunch of class prepares - it must keep going and return eventually to the interp loop.  This is why you'll
  see 'parked events' on a thread.  If a thread is suspended but the C code is still running and has stuff to do
  before it can return to the interp loop (where it can do a thread switch) any generated events are 'parked' on
  the thread waiting to be sent to the debugger when the thread is resumed.

  This also happens with some bytecodes.  For example, if an exception is thrown, the debugger may want an event
  and may cause the thread to be suspended.  Which means halfway though processing the 'athrow' opcode, the thread
  gets suspended.  To get around this, the exception details are held on the thread until such time as the thread
  can resume and deal with it *again* and finish processing it.

  These sorts of things would not exist in a native threads VM.  But, having said that, the mechanisms use to solve
  these issues are reasonably straight forward and documented well within the code.

  Each time a thread is suspended or resumed all threads are examined to determine if all threads are
  now suspended.  If all threads are suspended, then so is the VM.  When the VM is suspended, it 'spins' on
  communications with the debugger until such time as a thread (or all of them) are again resumed and the interp
  loop can continue.

  Have a look at the #bvm_vmthread_t struct.  You will see a number of members that are prefix with \c 'dbg_'.  These are
  members that are there specifically to assist with debugging.

  @subsection debug-babe-impl-breakpoints Breakpoints

  The way the Babe VM implements breakpoints and single stepping is worth some discussion.  When a debugger requests
  a breakpoint it sends an event request for a BREAKPOINT and a number of filters that identify the class, method, and
  the breakpoint position within the method.  The position may either be a line number (if the event step size
  is LINE), or a bytecode offset (if the event step size is BVM_MIN).

  With a breakpoint, the JVMS already provides us with an opcode (202) that it anticipates VMs will use for breakpoint
  purposes.  Babe does.  What happens is this: the actual bytecode described by the BREAKPOINT event request is substituted
  with the (202) JVMS 'breakpoint' opcode.  The original opcode is held with the BREAKPOINT event request.

  As the code is executing (at normal pace), we want to know when the breakpoint has been reached.  It is simple now, we
  just need to put code into the interp loop at the 202/breakpoint opcode.  Bingo, we know it has been reached and can act.

  Typically, the BREAKPOINT event will have a suspend policy of SUSPEND_THREAD, or SUSPEND_ALL.  At any rate, it is likely
  the debugger has requested the running thread to suspend when the breakpoint is hit.  (note, not always though - if a
  breakpoint has a 'count' on it, the breakpoint will be hit and only when the count is satisfied will the event be sent
  and the thread suspended).

  Have a look at the \c exec.c handling of \c OPCODE_breakpoint.  It sends the breakpoint event, then flags the thread as
  being 'at breakpoint' and stores the original opcode with the thread.  This original opcode will be the next opcode the
  threads executes.  Look at the top of the interp loop.  When debugging, the opcode is not determined simply by
  looking at the PC pointer - it is determined each time, and in this instance, it will be set to the original opcode
  displaced by the breakpoint opcode.  So the original opcode gets the opportunity to run *after* the breakpoint
  opcode.

  As breakpoint events are set and cleared, the opcode at the breakpoint location is displaced and re-placed.  You'll
  notice that if the breakpoint logic encounters unexpected things it will cause a BVM_VM_EXIT.  Breakpoints MUST be
  correct as opcodes are displaced and replaced and thus affect the actual correctness of the compiled code.  It is safer
  to consider a breakpoint anomaly a VM-level disaster than try to figure out why code is running funny ....

  Although a bit complicated to explain, breakpoints are rather simple in practice.  More complicated is how single
  stepping is achieved ...

  @subsection debug-babe-impl-singlestep Single-Stepping

  Single stepping is what happens after a breakpoint is reached and the debugger instructs the VM to move - but
  only by a particular degree.  This manifests itself as a SINGLE-STEP event request.  The debugger is expecting the VM
  to sent a SINGLE-STEP event when it has 'stepped' by the requested degree.  The 'degree' is define by two metric -
  the 'size' and the 'depth'.

  Some background:

  JDWP defines two step 'sizes'.  MIN and LINE.  The MIN size means 'move on by a single bytecode'.  The LINE size means 'move
  on by one source line'.

  Additionally, JDWP defines three different step 'depths'.  INTO, OVER, and OUT.  Interestingly, and very cleverly I think, in
  practice the three depths are not mutually exclusive. Initially they seem to be, but consider - what does an INTO do if you
  are on a line that does not actually call any methods?  In that instance it must act like an OVER.  But wait.  What if you are
  actually on the last line of a method or a method's 'return' and therefore will not step INTO, or OVER any more lines
  within that method at all?  In this case it must act like an OUT.  See?  The absence of one type of step means another
  must be checked and so on.  An INTO step on the last line of a method that does not call any other methods is satisfied by
  the same conditions as an OUT.

  This can be summarised as follows: an INTO can also be satisfied by an OVER or OUT.  An OVER can also be satisfied by an OUT.
  An OUT can only be satisfied by an OUT.  Clever.

  Determining whether the program has 'stepped' or not is about measuring 'displacement'.  "Has the PC for the current thread
  moved to a position that satisfies the step event requirements?".  Displacement is about measuring against INTO, OVER, OUT
  as above.

  To measure displacement we must reference a starting point - so when the debugger sends an event request command for a
  SINGLE-STEP, the current execution position (class/method/'position'/stack-depth) is stored for the thread on the thread
  itself in the 'dbg_step' struct. We use these details to determine 'displacement'.  'Displacement' is the measure of how the
  execution point has moved since the SINGLE-STEP event request was received.

  (Note: The stored 'position' depends on whether the step size in MIN or LINE. If it is MIN, we store the offset of the current
  executing bytecode within the executing method's bytecodes.  If it is LINE, we store the current executing line number.)

  While single-stepping, displacement is measured every executed bytecode at the top of the interp loop.

  There are only three displacements that actually can be measured against the execution location when the
  SINGLE-STEP event request was received.  Simply put, these are:

  @li Has the stack depth increased relative to when the single step was requested?  If so, we have an INTO.
  @li Has the stack depth decreased relative to when the single step was requested?  If so, we have an OUT.
  @li has the stack depth remained the same (same method is still executing), and the 'position' within the method
  moved.  We have an OVER.

  If none of those, then no step has occurred.

  Now recall, an INTO step-size can also be satisfied by an OVER, or OUT displacement.  An OVER step-size can also be satisfied by
  an OUT displacement, and an OUT step-size can only be satisfied by an OUT displacement.

  If we measure the displacement between the current execution location and the location at the time the single step event was
  requested using the above logic, we know whether we need to send a SINGLE-STEP event back to the debugger.

  That is, if the debugger has requested an INTO step size, we test for INTO, OVER, and OUT displacements.  If the debugger has
  requested an OVER step size, we test for OVER and OUT displacements.  And lastly, the if the debugger has requested an OUT, we
  test only for an OUT displacement.

  One last note:  If the next opcode to execute after a step has been reached is actually an \c OPCODE_breakpoint opcode, the
  stepping logic detects this and checks for breakpoint events to send as well.  The #bvmd_event_SingleStep method can do single steps
  events as well as breakpoint events that occur at the same location.

  @subsection debug-babe-impl-exception Exception Breakpoints

  Debuggers can request the VM for event notification when an exception occurs.  Typically, using a debugger, a developer
  somehow informs the debugger to break when an exception for a particular class is thrown.  The exception can be filtered
  further by specifying whether to break on caught/uncaught occurrences, or both.

  A debugger then sends an EXCEPTION event request and often specifies a number of filters to narrow down the type of
  the exception.  In a way, this is just another breakpoint.  Debuggers use a thread suspend policy to ensure the
  thread (or all of them) is suspended as the actual event is sent from the VM to the debugger.

  The debugger needs to be informed of an exception *before* the executing thread's stack is altered in any way to
  cater for the exception.  During normal VM execution, when an exception is encountered, the stack is unwound down to
  where exception hander code is (if caught), or (if uncaught) to the very bottom of the stack.  This usually just a single
  set of operations that happen as part of the VM's processing of an exception.

  However, debugging complicates this by wanting to be informed of the exception *before* the stack unwind is done - and then to
  also have sent as part of the event the location where the exception is caught (or not caught ...).  This means that the
  VM has to break up exception handing into two parts.  The first part locates the position of the caught exception
  (if any), and the second part unwinds the stack.  If an exception event is to be sent, it is sent as a sandwich between
  these two arts.

  @subsection debug-babe-impl-unloading Class Unloading

  During GC, Babe will unload unused classes if their classloader is unreachable.  If a debugger has requested to be
  notified of class unloads events, cool - this is supported.  However, it is worth a discussion to explain how it is done
  and why it is done in that manner.

  Firstly, sending events to the debugger allocates heap memory, but class unloading happens during GC, so the events
  cannot be send during a GC cycle - we do not allocate memory during a GC.  So, classes to be unloaded must be stored
  somewhere and unload events sent after the GC has finished.

  When debugging, if the GC attempts to unload a class it will park it at the end of the #bvmd_gl_unloaded_clazzes list.  The GC
  cleans up everything except the clazz's basic \c bvm_clazz_t struct.  The clazz is removed from the clazz pool and is placed onto
  this 'to be unloaded' list.  The list is checked each thread switch.

  The implementation gets over some subtle error conditions.  Consider, when traversing the parked clazzes and sending
  events, sending the event allocates heap memory and therefore may cause a GC - we have to be very careful with how the
  list is used.  After an event send the parked clazz list may actually have more unloaded clazzes on the end that were placed
  there by a GC.

  More insidiously, a GC will actually find the *same* clazzes still in the heap and therefore want to add them *again*
  to the list (the clazz is, after all, not reachable but still in the heap).  To solve this, as a clazz is parked its heap
  allocation type is changed to #BVM_ALLOC_TYPE_STATIC to ensure any subsequent GC cycles ignore it.

  After sending the unload event for a parked classes, the clazz memory is returned to the heap via #bvm_heap_free.

  @subsection debug-babe-impl-perf Debugging performance and memory usage

  A final note about debugging and the performance of the VM when the debugging capability is compiled in.

  The VM will run slower when debugging code is compiled in.  It will only run marginally slower when not actually
  debugging - that is, if the VM is not connected to a debugger stuff should run only a bit slower.  When connected to a
  debugger it would be reasonable to expect a greater drop in performance.

  While connected to a debugger, more things happen each thread switch and more things happen while loading classes
  and so on.  A large part of the additional work is checking if any packets from the debugger are waiting to be read
  and processed.

  Single-stepping while debugging takes a fair bit of processing to accomplish.  Each bytecode execution requires a) examining the
  stack depth, and b) examining the current execution position (normally this means figuring the executing source code line
  number). Perhaps not mountains of it, but when you compare the effort the VM goes through to check for that 'next step' with
  the simple bytecode switch when not debugging, there is a pretty big difference.

  Compiling in debugger support also means extra heap is used to load stuff like clazz method line numbers and local
  variable tables.  During a debugging session, extra heap is also used to keep track of events and their modifiers, as well
  as ID/address mapping and so on.  (Having said this, all debugging and development was done using the 128k default heap).

  So why say all of this?  Just to say that unless you're happy with the performance of the VM with debugging support compiled
  in, and can spare the extra memory required to have the extra class info loaded - you really shouldn't, unless you have
  specific reasons to do so, ship the VM with debugging support compiled in.  Nuff said.

  ... except also also that the debug code may actually double the executable program size - depending on platform.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * Enable debug at startup.  This variable is only inspected as the VM start to see if debug should be
 * enabled, and during shutdown to see if cleanup should be done.  Can be set with command line option -Xdebug.
 */
bvm_bool_t bvmd_gl_enabledebug = BVM_FALSE;

/**
 * Specifies whether the VM is in debug server mode (which means it listens for a connection from a debugger).
 * Set using the server=y/n setting on the \c Xrunjdwp command line option.
 */
bvm_bool_t bvmd_gl_server = BVM_FALSE;

/**
 * Flag to determine if the VM suspends all thread on startup pending connection with a debugger.
 * Can be set on command line as the suspend=y/n setting of the \c Xrunjdwp option.  Only valid
 * if \c server=y setting on the \c Xrunjdwp option.
 */
bvm_bool_t bvmd_gl_suspendonstart = BVM_FALSE;

/**
 * Flag telling whether all threads are currently in a suspended state.  Not for use by developers.  As a
 * thread is suspended or terminated or resumed this flag is reset.
 */
bvm_bool_t bvmd_gl_all_suspended = BVM_FALSE;

/**
 * Default timeout for connections to/from debugger.  Can be set with command line -dbg_timeout.  Defaults to
 * compile-time variable #BVM_DEBUGGER_TIMEOUT.
 */
bvm_uint32_t bvmd_gl_timeout = BVM_DEBUGGER_TIMEOUT;

/**
 * Flag indicating whether the debugger has requested suspension of all events from the VM to the debugger.  The
 * VM considers this as a 'suspension' and cause the entire VM to suspend waiting for the debugger to
 * resume event sending again.
 */
bvm_bool_t bvmd_gl_holdevents = BVM_FALSE;

/**
 * The address a debugger transport (#bvmd_transport_t) will use to connect or listen.  Can be set using command
 * line \c -Xrunjdwp as the 'address'.  The address may mean different things depending on whether the VM is attaching
 * to the debugger, or listening for a connection.
 */
char *bvmd_gl_address = NULL;

bvm_string_obj_t *bvmd_nosupp_tostring_obj = NULL;

/**
 * Provides an ID to identify objects and packets.  Counting starts at 1.
 *
 * @return a unique ID.
 */
int bvmd_nextid() {

	static int counter = 1;

	return counter++;
}

/**
 * Reports whether the VM is current suspended.
 */
bvm_bool_t bvmd_is_vm_suspended() {
	return bvmd_gl_all_suspended || bvmd_gl_holdevents;
}

/**
 * Causes the VM to spin waiting on a command from the debugger that will unsuspend the VM.  Has no effect if
 * the VM is current not suspended.
 */
void bvmd_spin_on_debugger() {
	while (bvmd_is_vm_suspended()) {
		bvmd_interact(BVMD_TIMEOUT_FOREVER);
	}
}

/**
 * Performs one-time startup initialisation of the VMs debugging facilities.
 */
void bvmd_init() {
	bvmd_io_init();
}

/**
 * Perform one-time shutdown and finalisation of the VM debug facilities.
 */
void bvmd_shutdown() {
	bvmd_io_shutdown();
}

/**
 * Determine the JDWP ReferenceType for a given clazz.
 *
 * @param clazz a give clazz
 *
 * @return the JDWP refernce type of the given clazz.
 */
bvm_uint8_t bvmd_clazz_reftype(bvm_clazz_t *clazz) {

	bvm_uint8_t ret = JDWP_TagType_CLASS;

	if (BVM_CLAZZ_IsInterface(clazz)) ret = JDWP_TagType_INTERFACE;
	else if (BVM_CLAZZ_IsArrayClazz(clazz)) ret = JDWP_TagType_ARRAY;

	return ret;
}

/**
 * Determines the JDWP status of a given clazz.  JDWP Note: Clazz states are cumulative - the
 * status reflected here is *all* the statuses that the clazz has passed through so far.  Weird,
 * but true.
 *
 * @param clazz a given clazz.
 * @return the JDWP status of the given clazz
 */
bvm_int32_t bvmd_clazz_status(bvm_clazz_t *clazz) {

	bvm_int32_t ret = 0;

	if (BVM_CLAZZ_IsArrayClazz(clazz)) return JDWP_ClassStatus_INITIALIZED;

	switch (clazz->state) {
		case(BVM_CLAZZ_STATE_ERROR) :
			/* easy one.  If in error, just say so */
			return JDWP_ClassStatus_ERROR;

		/* note that the remainder of these statuses are in descending order, with INITIALISED being
		 * the highest.  Note the fall-through, this is so that a status is actually the accumulation
		 * of all lower states as well.  Figure that out from the spec! */
		case(BVM_CLAZZ_STATE_INITIALISED) :
			ret |= JDWP_ClassStatus_INITIALIZED;
			/* fall through intentional */
		case(BVM_CLAZZ_STATE_INITIALISING) :
		case(BVM_CLAZZ_STATE_LOADING) :
		case(BVM_CLAZZ_STATE_LOADED) :
			ret |= JDWP_ClassStatus_PREPARED;
			ret |= JDWP_ClassStatus_VERIFIED;
			break;
	}

	return ret;
}

/**
 * Determine the JDWP tagtype from the first character of a given JNI signature.  Note that this will
 * only determine whether the signature specifies a primitive or an array. If the signature describes
 * an object type, this function will return zero.
 *
 * @param ch the first character of a given JNI signature.
 *
 * @return the JDWP tagtype if the signature represents a primitive or an array.  Zero will be
 * returned for JNI signatures for object types.
 */
bvm_uint8_t bvmd_get_jdwptag_from_char(char ch) {

	switch (ch) {
		case 'B':
			return JDWP_Tag_BYTE;
		case 'C':
			return JDWP_Tag_CHAR;
		case 'D':
			return JDWP_Tag_DOUBLE;
		case 'F':
			return JDWP_Tag_FLOAT;
		case 'I':
			return JDWP_Tag_INT;
		case 'J':
			return JDWP_Tag_LONG;
		case 'S':
			return JDWP_Tag_SHORT;
		case 'Z':
			return JDWP_Tag_BOOLEAN;
		case '[':
			return JDWP_Tag_ARRAY;
		default :
			return 0;
	}
}

/**
 * Determines the JDWP tag of a given JVMS type.  The JVMS type is a enumeration up to 11, whereas the
 * JDWP tag is a single char representation of a type.  This function provides a mapping between
 * the two.
 *
 * @param type a given JVMS type.
 */
bvm_uint8_t bvmd_get_jdwptag_from_jvmstype(bvm_jtype_t type) {

	static bvm_uint8_t tags[] = {0,
				     JDWP_Tag_OBJECT,
				     JDWP_Tag_ARRAY,
					 0,  // no jtype for 3
				     JDWP_Tag_BOOLEAN,
				     JDWP_Tag_CHAR,
				     JDWP_Tag_FLOAT,
				     JDWP_Tag_DOUBLE,
				     JDWP_Tag_BYTE,
				     JDWP_Tag_SHORT,
				     JDWP_Tag_INT,
				     JDWP_Tag_LONG
				     };

	return tags[type];
}

/**
 * Determines the JDWP tag of a given clazz.  JDWP note: If the clazz is for an array or primitive
 * type then the tag reflects this.  If the clazz is for an object type, then JDWP has that further broken
 * down to the type of object it is - like String, Thread, Class, or ClassLoader.  If the object is none of
 * these the return tag is simply for an object.
 *
 * @param clazz a given clazz.
 * @return the JDWP tag for the given clazz.
 */
bvm_uint8_t bvmd_get_jdwptag_from_clazz(bvm_clazz_t *clazz) {

	/* test for primitive and array tags first */
	bvm_uint8_t jdwptag = bvmd_get_jdwptag_from_char(clazz->jni_signature->data[0]);

	/* if not primitive or array, test for specific object type */
	if (!jdwptag) {
		if (clazz == (bvm_clazz_t *) BVM_STRING_CLAZZ) jdwptag = JDWP_Tag_STRING;
		else if (clazz == (bvm_clazz_t *) BVM_THREAD_CLAZZ) jdwptag = JDWP_Tag_THREAD;
		else if (clazz == (bvm_clazz_t *) BVM_CLASS_CLAZZ) jdwptag = JDWP_Tag_CLASS_OBJECT;
		else if (clazz == (bvm_clazz_t *) BVM_CLASSLOADER_CLAZZ) jdwptag = JDWP_Tag_CLASS_LOADER;
		else jdwptag = JDWP_Tag_OBJECT;
	}

	return jdwptag;
}

/**
 * Determines the JDWP tag for a given object.
 *
 * @param obj a given object
 * @return the JDWP tag for the object, or #JDWP_Tag_OBJECT if the object is \c NULL.
 */
bvm_uint8_t bvmd_get_jdwptag_from_object(bvm_obj_t *obj) {
	return (obj == NULL) ? JDWP_Tag_OBJECT : bvmd_get_jdwptag_from_clazz(obj->clazz);
}

#endif

