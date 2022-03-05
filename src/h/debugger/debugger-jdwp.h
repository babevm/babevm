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

#ifndef BVM_DEBUGGER_JDWP_H_
#define BVM_DEBUGGER_JDWP_H_

/**

 @file

 JDWP Constants and codes.

 @author Greg McCreath
 @since 0.0.10

*/

#define JDWP_Error_NONE								0	/* No error has occurred. */
#define JDWP_Error_INVALID_THREAD					10	/* Passed thread is null, is not a valid thread or has exited. */
#define JDWP_Error_INVALID_THREAD_GROUP				11	/* Thread group invalid. */
#define JDWP_Error_INVALID_PRIORITY					12	/* Invalid priority. */
#define JDWP_Error_THREAD_NOT_SUSPENDED				13	/* If the specified thread has not been suspended by an event. */
#define JDWP_Error_THREAD_SUSPENDED					14	/* Thread already suspended. */
#define JDWP_Error_THREAD_NOT_ALIVE					15	/* Thread has not been started or is now dead. */
#define JDWP_Error_INVALID_OBJECT					20	/* If this reference type has been unloaded and garbage collected. */
#define JDWP_Error_INVALID_CLASS					21	/* Invalid class. */
#define JDWP_Error_CLASS_NOT_PREPARED				22	/* Class has been loaded but not yet prepared. */
#define JDWP_Error_INVALID_METHODID					23	/* Invalid method. */
#define JDWP_Error_INVALID_LOCATION					24	/* Invalid location. */
#define JDWP_Error_INVALID_FIELDID					25	/* Invalid field. */
#define JDWP_Error_INVALID_FRAMEID					30	/* Invalid jframeID. */
#define JDWP_Error_NO_MORE_FRAMES					31	/* There are no more Java or JNI frames on the call stack. */
#define JDWP_Error_OPAQUE_FRAME						32	/* Information about the frame is not available. */
#define JDWP_Error_NOT_CURRENT_FRAME				33	/* Operation can only be performed on current frame. */
#define JDWP_Error_TYPE_MISMATCH					34	/* The variable is not an appropriate type for the function used. */
#define JDWP_Error_INVALID_SLOT						35	/* Invalid slot. */
#define JDWP_Error_DUPLICATE						40	/* Item already set. */
#define JDWP_Error_NOT_FOUND						41	/* Desired element not found. */
#define JDWP_Error_INVALID_MONITOR					50	/* Invalid monitor. */
#define JDWP_Error_NOT_MONITOR_OWNER				51	/* This thread doesn't own the monitor. */
#define JDWP_Error_INTERRUPT						52	/* The call has been interrupted before completion. */
#define JDWP_Error_INVALID_CLASS_FORMAT				60	/* The virtual machine attempted to read a class file and determined that the file is malformed or otherwise cannot be interpreted as a class file.  */
#define JDWP_Error_CIRCULAR_CLASS_DEFINITION		61	/* A circularity has been detected while initializing a class. */
#define JDWP_Error_FAILS_VERIFICATION				62	/* The verifier detected that a class file, though well formed, contained some sort of internal inconsistency or security problem. */
#define JDWP_Error_ADD_METHOD_NOT_IMPLEMENTED		63	/* Adding methods has not been implemented. */
#define JDWP_Error_SCHEMA_CHANGE_NOT_IMPLEMENTED	64	/* Schema change has not been implemented. */
#define JDWP_Error_INVALID_TYPESTATE				65	/* The state of the thread has been modified, and is now inconsistent. */
#define JDWP_Error_HIERARCHY_CHANGE_NOT_IMPLEMENTED	66	/* A direct superclass is different for the new class version, or the set of directly implemented interfaces is different and canUnrestrictedlyRedefineClasses is false. */
#define JDWP_Error_DELETE_METHOD_NOT_IMPLEMENTED	67	/* The new class version does not declare a method declared in the old class version and canUnrestrictedlyRedefineClasses is false. */
#define JDWP_Error_UNSUPPORTED_VERSION				68	/* A class file has a version number not supported by this VM. */
#define JDWP_Error_NAMES_DONT_MATCH					69	/* The class name defined in the new class file is different from the name in the old class object. */
#define JDWP_Error_CLASS_MODIFIERS_CHANGE_NOT_IMPLEMENTED	70	/* The new class version has different modifiers and canUnrestrictedlyRedefineClasses is false. */
#define JDWP_Error_METHOD_MODIFIERS_CHANGE_NOT_IMPLEMENTED	71	/* A method in the new class version has different modifiers than its counterpart in the old class version and and canUnrestrictedlyRedefineClasses is false. */
#define JDWP_Error_NOT_IMPLEMENTED					99	/* The functionality is not implemented in this virtual machine. */
#define JDWP_Error_NULL_POINTER						100	/* Invalid pointer. */
#define JDWP_Error_ABSENT_INFORMATION				101	/* Desired information is not available. */
#define JDWP_Error_INVALID_EVENT_TYPE				102	/* The specified event type id is not recognized. */
#define JDWP_Error_ILLEGAL_ARGUMENT					103	/* Illegal argument. */
#define JDWP_Error_OUT_OF_MEMORY					110	/* The function needed to allocate memory and no more memory was available for allocation. */
#define JDWP_Error_ACCESS_DENIED					111	/* Debugging has not been enabled in this virtual machine. JVMTI cannot be used. */
#define JDWP_Error_VM_DEAD							112	/* The virtual machine is not running. */
#define JDWP_Error_INTERNAL							113	/* An unexpected internal error has occurred. */
#define JDWP_Error_UNATTACHED_THREAD				115	/* The thread being used to call this function is not attached to the virtual machine. Calls must be made from attached threads. */
#define JDWP_Error_INVALID_TAG						500	/* object type id or class tag. */
#define JDWP_Error_ALREADY_INVOKING					502	/* Previous invoke not complete. */
#define JDWP_Error_INVALID_INDEX					503	/* Index is invalid. */
#define JDWP_Error_INVALID_LENGTH					504	/* The length is invalid. */
#define JDWP_Error_INVALID_STRING					506	/* The string is invalid. */
#define JDWP_Error_INVALID_CLASS_LOADER				507	/* The class loader is invalid. */
#define JDWP_Error_INVALID_ARRAY					508	/* The array is invalid. */
#define JDWP_Error_TRANSPORT_LOAD					509	/* Unable to load the transport. */
#define JDWP_Error_TRANSPORT_INIT					510	/* Unable to initialize the transport. */
#define JDWP_Error_NATIVE_METHOD					511
#define JDWP_Error_INVALID_COUNT					512	/* The count is invalid. */

#define JDWP_EventKind_NONE								0
#define JDWP_EventKind_SINGLE_STEP						1
#define JDWP_EventKind_BREAKPOINT						2
#define JDWP_EventKind_FRAME_POP						3
#define JDWP_EventKind_EXCEPTION						4
#define JDWP_EventKind_USER_DEFINED						5
#define JDWP_EventKind_THREAD_START						6
#define JDWP_EventKind_THREAD_DEATH						7
#define JDWP_EventKind_CLASS_PREPARE					8
#define JDWP_EventKind_CLASS_UNLOAD						9
#define JDWP_EventKind_CLASS_LOAD						10
#define JDWP_EventKind_FIELD_ACCESS						20
#define JDWP_EventKind_FIELD_MODIFICATION				21
#define JDWP_EventKind_EXCEPTION_CATCH					30
#define JDWP_EventKind_METHOD_ENTRY						40
#define JDWP_EventKind_METHOD_EXIT						41
#define JDWP_EventKind_METHOD_EXIT_WITH_RETURN_VALUE	42
#define JDWP_EventKind_MONITOR_CONTENDED_ENTER			43
#define JDWP_EventKind_MONITOR_CONTENDED_ENTERED		44
#define JDWP_EventKind_MONITOR_WAIT						45
#define JDWP_EventKind_MONITOR_WAITED					46
#define JDWP_EventKind_VM_START							90
#define JDWP_EventKind_VM_DEATH							99
#define JDWP_EventKind_VM_DISCONNECTED					100

#define JDWP_ThreadStatus_ZOMBIE		0
#define JDWP_ThreadStatus_RUNNING		1
#define JDWP_ThreadStatus_SLEEPING		2
#define JDWP_ThreadStatus_MONITOR		3
#define JDWP_ThreadStatus_WAIT			4

#define JDWP_SuspendStatus_SUSPEND_STATUS_SUSPENDED	0x1

#define JDWP_ClassStatus_VERIFIED		1
#define JDWP_ClassStatus_PREPARED		2
#define JDWP_ClassStatus_INITIALIZED	4
#define JDWP_ClassStatus_ERROR			8

#define JDWP_TagType_CLASS				1	/* ReferenceType is a class.  */
#define JDWP_TagType_INTERFACE			2	/* ReferenceType is an interface.  */
#define JDWP_TagType_ARRAY				3	/* ReferenceType is an array.   */

#define JDWP_Tag_BYTE					66	/* 'B' - a byte value (1 byte).   */
#define JDWP_Tag_CHAR					67	/* 'C' - a character value (2 bytes).   */
#define JDWP_Tag_DOUBLE					68	/* 'D' - a double value (8 bytes).   */
#define JDWP_Tag_FLOAT					70	/* 'F' - a float value (4 bytes).   */
#define JDWP_Tag_INT					73	/* 'I' - an int value (4 bytes).   */
#define JDWP_Tag_LONG					74	/* 'J' - a long value (8 bytes).   */
#define JDWP_Tag_OBJECT					76	/* 'L' - an object (objectID size).   */
#define JDWP_Tag_SHORT					83	/* 'S' - a short value (2 bytes).   */
#define JDWP_Tag_VOID					86	/* 'V' - a void value (no bytes).   */
#define JDWP_Tag_BOOLEAN				90	/* 'Z' - a boolean value (1 byte).   */
#define JDWP_Tag_ARRAY					91	/* '[' - an array object (objectID size). */
#define JDWP_Tag_CLASS_OBJECT			99	/* 'c' - a class object object (objectID size).    */
#define JDWP_Tag_THREAD_GROUP			103	/* 'g' - a ThreadGroup object (objectID size).   */
#define JDWP_Tag_CLASS_LOADER			108	/* 'l' - a ClassLoader object (objectID size).   */
#define JDWP_Tag_STRING					115	/* 's' - a String object (objectID size).   */
#define JDWP_Tag_THREAD					116	/* 't' - a Thread object (objectID size).   */

#define JDWP_StepDepth_INTO				0	/* Step into any method calls that occur before the end of the step.  */
#define JDWP_StepDepth_OVER				1	/* Step over any method calls that occur before the end of the step.  */
#define JDWP_StepDepth_OUT				2	/* Step out of the current method. */

#define JDWP_StepSize_MIN				0	/* Step by the minimum possible amount (often a bytecode instruction). */
#define JDWP_StepSize_LINE				1	/* Step to the next source line unless there is no line number information in which case a JDWP_StepSize_MIN step is done instead. */

#define JDWP_SuspendPolicy_NONE			0	/* Suspend no threads when this event is encountered. */
#define JDWP_SuspendPolicy_EVENT_THREAD	1	/* Suspend the event thread when this event is encountered.  */
#define JDWP_SuspendPolicy_ALL			2	/* Suspend all threads when this event is encountered.   */

#define JDWP_InvokeOptions_INVOKE_SINGLE_THREADED	0x01	/* otherwise, all threads started. */
#define JDWP_InvokeOptions_INVOKE_NONVIRTUAL		0x02	/* otherwise, normal virtual invoke (instance methods only) */

#define JDWP_VirtualMachine 						1
#define JDWP_VirtualMachine_Version 				1
#define JDWP_VirtualMachine_ClassesBySignature 		2
#define JDWP_VirtualMachine_AllClasses 				3
#define JDWP_VirtualMachine_AllThreads 				4
#define JDWP_VirtualMachine_TopLevelThreadGroups 	5
#define JDWP_VirtualMachine_Dispose 				6
#define JDWP_VirtualMachine_IDSizes 				7
#define JDWP_VirtualMachine_Suspend 				8
#define JDWP_VirtualMachine_Resume 					9
#define JDWP_VirtualMachine_Exit 					10
#define JDWP_VirtualMachine_CreateString 			11
#define JDWP_VirtualMachine_Capabilities 			12
#define JDWP_VirtualMachine_ClassPaths 				13
#define JDWP_VirtualMachine_DisposeObjects 			14
#define JDWP_VirtualMachine_HoldEvents 				15
#define JDWP_VirtualMachine_ReleaseEvents 			16
#define JDWP_VirtualMachine_CapabilitiesNew 		17
#define JDWP_VirtualMachine_RedefineClasses 		18
#define JDWP_VirtualMachine_SetDefaultStratum 		19
#define JDWP_VirtualMachine_AllClassesWithGeneric 	20
#define JDWP_VirtualMachine_InstanceCounts 			21

#define JDWP_ReferenceType 							2
#define JDWP_ReferenceType_Signature 				1
#define JDWP_ReferenceType_ClassLoader 				2
#define JDWP_ReferenceType_Modifiers 				3
#define JDWP_ReferenceType_Fields 					4
#define JDWP_ReferenceType_Methods 					5
#define JDWP_ReferenceType_GetValues 				6
#define JDWP_ReferenceType_SourceFile 				7
#define JDWP_ReferenceType_NestedTypes				8
#define JDWP_ReferenceType_Status 					9
#define JDWP_ReferenceType_Interfaces 				10
#define JDWP_ReferenceType_ClassObject 				11
#define JDWP_ReferenceType_SourceDebugExtension 	12
#define JDWP_ReferenceType_SignatureWithGeneric 	13
#define JDWP_ReferenceType_FieldsWithGeneric 		14
#define JDWP_ReferenceType_MethodsWithGeneric 		15
#define JDWP_ReferenceType_Instances 				16
#define JDWP_ReferenceType_ClassFileVersion 		17
#define JDWP_ReferenceType_ConstantPool 			18

#define JDWP_ClassType 				3
#define JDWP_ClassType_Superclass 	1
#define JDWP_ClassType_SetValues 	2
#define JDWP_ClassType_InvokeMethod 3
#define JDWP_ClassType_NewInstance 	4

#define JDWP_ArrayType 				4
#define JDWP_ArrayType_NewInstance 	1

#define JDWP_InterfaceType 	5

#define JDWP_Method 						6
#define JDWP_Method_LineTable 				1
#define JDWP_Method_VariableTable 			2
#define JDWP_Method_Bytecodes 				3
#define JDWP_Method_IsObsolete 				4
#define JDWP_Method_VariableTableWithGeneric 5

#define JDWP_Field 8

#define JDWP_ObjectReference 					9
#define JDWP_ObjectReference_ReferenceType 		1
#define JDWP_ObjectReference_GetValues 			2
#define JDWP_ObjectReference_SetValues 			3
#define JDWP_ObjectReference_MonitorInfo 		5
#define JDWP_ObjectReference_InvokeMethod 		6
#define JDWP_ObjectReference_DisableCollection 	7
#define JDWP_ObjectReference_EnableCollection 	8
#define JDWP_ObjectReference_IsCollected 		9
#define JDWP_ObjectReference_ReferringObjects 	10

#define JDWP_StringReference 		10
#define JDWP_StringReference_Value 	1

#define JDWP_ThreadReference 								11
#define JDWP_ThreadReference_Name 							1
#define JDWP_ThreadReference_Suspend 						2
#define JDWP_ThreadReference_Resume 						3
#define JDWP_ThreadReference_Status 						4
#define JDWP_ThreadReference_ThreadGroup 					5
#define JDWP_ThreadReference_Frames 						6
#define JDWP_ThreadReference_FrameCount 					7
#define JDWP_ThreadReference_OwnedMonitors 					8
#define JDWP_ThreadReference_CurrentContendedMonitor 		9
#define JDWP_ThreadReference_Stop 							10
#define JDWP_ThreadReference_Interrupt 						11
#define JDWP_ThreadReference_SuspendCount 					12
#define JDWP_ThreadReference_OwnedMonitorsStackDepthInfo 	13
#define JDWP_ThreadReference_ForceEarlyReturn 				14

#define JDWP_ThreadGroupReference 			12
#define JDWP_ThreadGroupReference_Name 		1
#define JDWP_ThreadGroupReference_Parent 	2
#define JDWP_ThreadGroupReference_Children 	3

#define JDWP_ArrayReference 			13
#define JDWP_ArrayReference_Length 		1
#define JDWP_ArrayReference_GetValues 	2
#define JDWP_ArrayReference_SetValues 	3

#define JDWP_ClassLoaderReference 				14
#define JDWP_ClassLoaderReference_VisibleClasses 1

#define JDWP_EventRequest 15
#define JDWP_EventRequest_Set 											1
#define JDWP_EventRequest_Set_Out_modifiers_Modifier_Count				1
#define JDWP_EventRequest_Set_Out_modifiers_Modifier_Conditional 		2
#define JDWP_EventRequest_Set_Out_modifiers_Modifier_ThreadOnly 		3
#define JDWP_EventRequest_Set_Out_modifiers_Modifier_ClassOnly 			4
#define JDWP_EventRequest_Set_Out_modifiers_Modifier_ClassMatch 		5
#define JDWP_EventRequest_Set_Out_modifiers_Modifier_ClassExclude 		6
#define JDWP_EventRequest_Set_Out_modifiers_Modifier_LocationOnly 		7
#define JDWP_EventRequest_Set_Out_modifiers_Modifier_ExceptionOnly 		8
#define JDWP_EventRequest_Set_Out_modifiers_Modifier_FieldOnly		 	9
#define JDWP_EventRequest_Set_Out_modifiers_Modifier_Step 				10
#define JDWP_EventRequest_Set_Out_modifiers_Modifier_InstanceOnly 		11
#define JDWP_EventRequest_Set_Out_modifiers_Modifier_SourceNameMatch  	12
#define JDWP_EventRequest_Clear 										2
#define JDWP_EventRequest_ClearAllBreakpoints 							3

#define JDWP_StackFrame 16
#define JDWP_StackFrame_GetValues 	1
#define JDWP_StackFrame_SetValues 	2
#define JDWP_StackFrame_ThisObject 	3

#define JDWP_ClassObjectReference 17
#define JDWP_ClassObjectReference_ReflectedType 1

#define JDWP_Event 64

#define JDWP_Event_Composite 100
#define JDWP_Event_Composite_Event_events_Events_VMStart 					JDWP_EventKind_VM_START
#define JDWP_Event_Composite_Event_events_Events_SingleStep 				JDWP_EventKind_SINGLE_STEP
#define JDWP_Event_Composite_Event_events_Events_Breakpoint 				JDWP_EventKind_BREAKPOINT
#define JDWP_Event_Composite_Event_events_Events_MethodEntry 				JDWP_EventKind_METHOD_ENTRY
#define JDWP_Event_Composite_Event_events_Events_MethodExit 				JDWP_EventKind_METHOD_EXIT
#define JDWP_Event_Composite_Event_events_Events_MethodExitWithReturnValue  JDWP_EventKind_METHOD_EXIT_WITH_RETURN_VALUE
#define JDWP_Event_Composite_Event_events_Events_MonitorContendedEnter  	JDWP_EventKind_MONITOR_CONTENDED_ENTER
#define JDWP_Event_Composite_Event_events_Events_MonitorContendedEntered  	JDWP_EventKind_MONITOR_CONTENDED_ENTERED
#define JDWP_Event_Composite_Event_events_Events_MonitorWait  				JDWP_EventKind_MONITOR_WAIT
#define JDWP_Event_Composite_Event_events_Events_MonitorWaited  			JDWP_EventKind_MONITOR_WAITED
#define JDWP_Event_Composite_Event_events_Events_Exception 					JDWP_EventKind_EXCEPTION
#define JDWP_Event_Composite_Event_events_Events_ThreadStart 				JDWP_EventKind_THREAD_START
#define JDWP_Event_Composite_Event_events_Events_ThreadDeath 				JDWP_EventKind_THREAD_DEATH
#define JDWP_Event_Composite_Event_events_Events_ClassPrepare 				JDWP_EventKind_CLASS_PREPARE
#define JDWP_Event_Composite_Event_events_Events_ClassUnload 				JDWP_EventKind_CLASS_UNLOAD
#define JDWP_Event_Composite_Event_events_Events_FieldAccess 				JDWP_EventKind_FIELD_ACCESS
#define JDWP_Event_Composite_Event_events_Events_FieldModification 			JDWP_EventKind_FIELD_MODIFICATION
#define JDWP_Event_Composite_Event_events_Events_VMDeath 					JDWP_EventKind_VM_DEATH

#endif /* BVM_DEBUGGER_JDWP_H_ */
