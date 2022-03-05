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

#ifndef BVM_DEBUGGERCOMMANDS_H_
#define BVM_DEBUGGERCOMMANDS_H_

/**

 @file

 Definitions for JDWP command handling.

 @author Greg McCreath
 @since 0.0.10

*/

typedef void (*bvmd_cmdhandler_t)(bvmd_instream_t* in, bvmd_outstream_t* out);

void bvmd_cmdhandler_default(bvmd_instream_t* in, bvmd_outstream_t* out);

/* 1.  VirtualMachine */
void bvmd_ch_VM_Version(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_VM_AllClasses(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_VM_ClassesBySignature(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_VM_AllThreads(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_VM_TopLevelThreadGroups(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_VM_Dispose(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_VM_IDSizes(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_VM_Suspend(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_VM_Resume(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_VM_Exit(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_VM_Capabilities(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_VM_ClassPaths(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_VM_DisposeObjects(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_VM_CreateString(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_VM_CapabilitiesNew(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_VM_HoldEvents(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_VM_ReleaseEvents(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_VM_SetDefaultStratum(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_VM_AllClassesWithGeneric(bvmd_instream_t* in, bvmd_outstream_t* out);

/* 2.  ReferenceType */
void bvmd_ch_RT_Signature(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_RT_ClassLoader(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_RT_Modifiers(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_RT_Fields(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_RT_Methods(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_RT_GetSetValues(bvmd_instream_t* in, bvmd_outstream_t* out, bvm_bool_t do_set);
void bvmd_ch_RT_GetValues(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_RT_SourceFile(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_RT_Status(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_RT_Interfaces(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_RT_ClassObject(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_RT_SourceDebugExtension(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_RT_SignatureWithGeneric(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_RT_FieldsWithGeneric(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_RT_MethodsWithGeneric(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_RT_ClassFileVersion(bvmd_instream_t* in, bvmd_outstream_t* out);

/* 3. ClassType */
void bvmd_ch_CT_Superclass(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_CT_SetValues(bvmd_instream_t* in, bvmd_outstream_t* out);

/* 4. ArrayType */
void bvmd_ch_AT_NewInstance(bvmd_instream_t* in, bvmd_outstream_t* out);

/* 6. Method */
void bvmd_ch_M_LineTable(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_M_VariableTable(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_M_Bytecodes(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_M_VariableTableWithGeneric(bvmd_instream_t* in, bvmd_outstream_t* out);

/* 9.  ObjectReference 	*/
void bvmd_ch_OR_ReferenceType(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_OR_GetValues(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_OR_SetValues(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_OR_MonitorInfo(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_OR_InvokeMethod(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_OR_DisableCollection(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_OR_EnableCollection(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_OR_IsCollected(bvmd_instream_t* in, bvmd_outstream_t* out);

/* 10. String Reference */
void bvmd_ch_SR_Value(bvmd_instream_t* in, bvmd_outstream_t* out);

/* 11. ThreadReference */
void bvmd_ch_TR_Name(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_TR_Suspend(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_TR_Resume(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_TR_Status(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_TR_ThreadGroup(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_TR_Frames(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_TR_OwnedMonitors(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_TR_CurrentContendedMonitor(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_TR_FrameCount(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_TR_Interrupt(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_TR_SuspendCount(bvmd_instream_t* in, bvmd_outstream_t* out);

/* 12. ThreadGroupReference */
void bvmd_ch_TGR_Name(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_TGR_Parent(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_TGR_Children(bvmd_instream_t* in, bvmd_outstream_t* out);

/* 13. Array Reference */
void bvmd_ch_AR_Length(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_AR_GetValues(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_AR_SetValues(bvmd_instream_t* in, bvmd_outstream_t* out);

/* 14. Classloader Reference  */
void bvmd_ch_CLR_VisibleClasses(bvmd_instream_t* in, bvmd_outstream_t* out);

/* 15. EventRequest */
void bvmd_ch_ER_Set(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_ER_Clear(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_ER_ClearAllBreakpoints(bvmd_instream_t* in, bvmd_outstream_t* out);

/* 16. Stack Frame */
void bvmd_ch_SF_GetValues(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_SF_SetValues(bvmd_instream_t* in, bvmd_outstream_t* out);
void bvmd_ch_SF_ThisObject(bvmd_instream_t* in, bvmd_outstream_t* out);

/* 17. ClassObjectReference */
void bvmd_ch_COR_ReflectedType(bvmd_instream_t* in, bvmd_outstream_t* out);

static int bvmd_gl_commandset_sizes[] = {
		0,  /* unused */
		21, /* 1.  VirtualMachine */
		18, /* 2.  ReferenceType */
		3,  /* 3.  ClassType */
		1,  /* 4.  ArrayType */
		0,  /* 5.  InterfaceType 	*/
		5,  /* 6.  Method */
		0,  /* 7   unused */
		0,  /* 8.  Field */
		10, /* 9.  ObjectReference 	*/
		1,  /* 10. StringReference */
		14, /* 11. ThreadReference */
		3,  /* 12. ThreadGroupReference */
		3,  /* 13. ArrayReference */
		1,  /* 14. ClassLoaderReference */
		3,  /* 15. EventRequest */
		3,  /* 16. StackFrame */
		1   /* 17. ClassObjectReference */
};

static bvmd_cmdhandler_t bvmd_VM_commands[] = {
	NULL,				  /* unused */
	bvmd_ch_VM_Version, /* 1.  JDWP_VirtualMachine_Version */
	bvmd_ch_VM_ClassesBySignature, /* 2.  JDWP_VirtualMachine_ClassesBySignature */
	bvmd_ch_VM_AllClasses, /* 3.  JDWP_VirtualMachine_AllClasses */
    bvmd_ch_VM_AllThreads, /* 4.  JDWP_VirtualMachine_AllThreads */
    bvmd_ch_VM_TopLevelThreadGroups, /* 5.  JDWP_VirtualMachine_TopLevelThreadGroups */
    bvmd_ch_VM_Dispose, /* 6.  JDWP_VirtualMachine_Dispose */
    bvmd_ch_VM_IDSizes, /* 7.  JDWP_VirtualMachine_IDSizes */
    bvmd_ch_VM_Suspend, /* 8.  JDWP_VirtualMachine_Suspend */
    bvmd_ch_VM_Resume, /* 9.  JDWP_VirtualMachine_Resume */
    bvmd_ch_VM_Exit, /* 10. JDWP_VirtualMachine_Exit */
    bvmd_ch_VM_CreateString, /* 11. JDWP_VirtualMachine_CreateString */
    bvmd_ch_VM_Capabilities, /* 12. JDWP_VirtualMachine_Capabilities */
    bvmd_ch_VM_ClassPaths, /* 13. JDWP_VirtualMachine_ClassPaths */
    bvmd_ch_VM_DisposeObjects, /* 14. JDWP_VirtualMachine_DisposeObjects */
    bvmd_ch_VM_HoldEvents, /* 15. JDWP_VirtualMachine_HoldEvents */
    bvmd_ch_VM_ReleaseEvents, /* 16. JDWP_VirtualMachine_ReleaseEvents */
    bvmd_ch_VM_CapabilitiesNew, /* 17. JDWP_VirtualMachine_CapabilitiesNew */
    bvmd_cmdhandler_default, /* 18. JDWP_VirtualMachine_RedefineClasses */

#if BVM_DEBUGGER_JSR045_ENABLE
    bvmd_ch_VM_SetDefaultStratum, /* 19. JDWP_VirtualMachine_SetDefaultStratum */
#else
    bvmd_cmdhandler_default, /* 19. JDWP_VirtualMachine_SetDefaultStratum */
#endif

    bvmd_ch_VM_AllClassesWithGeneric, /* 20. JDWP_VirtualMachine_AllClassesWithGeneric */
    bvmd_cmdhandler_default  /* 21. JDWP_VirtualMachine_InstanceCounts */
};

static bvmd_cmdhandler_t bvmd_RT_commands[] = {
	NULL,				   /* unused */
	bvmd_ch_RT_Signature,  /* 1.  JDWP_ReferenceType_Signature */
	bvmd_ch_RT_ClassLoader,  /* 2.  JDWP_ReferenceType_ClassLoader */
	bvmd_ch_RT_Modifiers,  /* 3.  JDWP_ReferenceType_Modifiers */
	bvmd_ch_RT_Fields,  /* 4.  JDWP_ReferenceType_Fields */
	bvmd_ch_RT_Methods,  /* 5.  JDWP_ReferenceType_Methods */
	bvmd_ch_RT_GetValues,  /* 6.  JDWP_ReferenceType_GetValues */
	bvmd_ch_RT_SourceFile,  /* 7.  JDWP_ReferenceType_SourceFile 	*/
	bvmd_cmdhandler_default,  /* 8.  JDWP_ReferenceType_NestedTypes	*/
	bvmd_ch_RT_Status,  /* 9.  JDWP_ReferenceType_Status 	*/
	bvmd_ch_RT_Interfaces,  /* 10. JDWP_ReferenceType_Interfaces */
	bvmd_ch_RT_ClassObject,  /* 11. JDWP_ReferenceType_ClassObject */

#if BVM_DEBUGGER_JSR045_ENABLE
	bvmd_ch_RT_SourceDebugExtension,  /* 12. JDWP_ReferenceType_SourceDebugExtension 	*/
#else
	bvmd_cmdhandler_default,  /* 12. JDWP_ReferenceType_SourceDebugExtension 	*/
#endif

	bvmd_ch_RT_SignatureWithGeneric,  /* 13. JDWP_ReferenceType_SignatureWithGeneric */
	bvmd_ch_RT_FieldsWithGeneric,  /* 14. JDWP_ReferenceType_FieldsWithGeneric 	*/
	bvmd_ch_RT_MethodsWithGeneric,  /* 15. JDWP_ReferenceType_MethodsWithGeneric */
	bvmd_cmdhandler_default,  /* 16. JDWP_ReferenceType_Instances 	*/
	bvmd_ch_RT_ClassFileVersion,  /* 17. JDWP_ReferenceType_ClassFileVersion */
	bvmd_cmdhandler_default   /* 18. JDWP_ReferenceType_ConstantPool */
};

static bvmd_cmdhandler_t bvmd_CT_commands[] = {
	NULL,				   /* unused */
	bvmd_ch_CT_Superclass,  /* 1. JDWP_ClassType_Superclass */
	bvmd_ch_CT_SetValues,  /* 2. JDWP_ClassType_SetValues 	*/
	bvmd_cmdhandler_default,  /* 3. JDWP_ClassType_InvokeMethod */
	bvmd_cmdhandler_default,  /* 4. JDWP_ClassType_NewInstance */
};

static bvmd_cmdhandler_t bvmd_AT_commands[] = {
	NULL,				   /* unused */
	bvmd_ch_AT_NewInstance,  /* 1. JDWP_ArrayType_NewInstance */
};

static bvmd_cmdhandler_t bvmd_M_commands[] = {
	NULL,				   /* unused */
	bvmd_ch_M_LineTable,  /* 1. JDWP_Method_LineTable */
	bvmd_ch_M_VariableTable,  /* 2. JDWP_Method_VariableTable */

#if BVM_DEBUGGER_BYTECODES_ENABLE
	bvmd_ch_M_Bytecodes,  /* 3. JDWP_Method_Bytecodes */
#else
	bvmd_cmdhandler_default,  /* 3. JDWP_Method_Bytecodes */
#endif

	bvmd_cmdhandler_default,  /* 4. JDWP_Method_IsObsolete */
	bvmd_ch_M_VariableTableWithGeneric,  /* 5. JDWP_Method_VariableTableWithGeneric */
};

static bvmd_cmdhandler_t bvmd_OR_commands[] = {
	NULL,				   /* unused */
	bvmd_ch_OR_ReferenceType,  /* 1. JDWP_ObjectReference_ReferenceType */
	bvmd_ch_OR_GetValues,  /* 2. JDWP_ObjectReference_GetValues */
	bvmd_ch_OR_SetValues,  /* 3. JDWP_ObjectReference_SetValues */
	bvmd_cmdhandler_default,  /* 4. unused */
#if BVM_DEBUGGER_MONITORINFO_ENABLE
	bvmd_ch_OR_MonitorInfo,  /* 5. JDWP_ObjectReference_MonitorInfo */
#else
	bvmd_cmdhandler_default,  /* 5. JDWP_ObjectReference_MonitorInfo */
#endif
	bvmd_ch_OR_InvokeMethod,  /* 6. JDWP_ObjectReference_InvokeMethod */
	bvmd_ch_OR_DisableCollection,  /* 7. JDWP_ObjectReference_DisableCollection */
	bvmd_ch_OR_EnableCollection,  /* 8. JDWP_ObjectReference_EnableCollection */
	bvmd_ch_OR_IsCollected,  /* 9. JDWP_ObjectReference_IsCollected 	*/
	bvmd_cmdhandler_default,  /* 10. JDWP_ObjectReference_ReferringObjects */
};

static bvmd_cmdhandler_t bvmd_SR_commands[] = {
	NULL,				   /* unused */
	bvmd_ch_SR_Value,  /* 1. JDWP_StringReference_Value */
};

static bvmd_cmdhandler_t bvmd_TR_commands[] = {
	NULL,				   /* unused */
	bvmd_ch_TR_Name,  /* 1.  JDWP_ThreadReference_Name */
	bvmd_ch_TR_Suspend,  /* 2.  JDWP_ThreadReference_Suspend */
	bvmd_ch_TR_Resume,  /* 3.  JDWP_ThreadReference_Resume */
	bvmd_ch_TR_Status,  /* 4.  JDWP_ThreadReference_Status */
	bvmd_ch_TR_ThreadGroup,  /* 5.  JDWP_ThreadReference_ThreadGroup */
	bvmd_ch_TR_Frames,  /* 6.  JDWP_ThreadReference_Frames */
	bvmd_ch_TR_FrameCount,  /* 7.  JDWP_ThreadReference_FrameCount */
#if BVM_DEBUGGER_MONITORINFO_ENABLE
	bvmd_ch_TR_OwnedMonitors,  /* 8.  JDWP_ThreadReference_OwnedMonitors */
	bvmd_ch_TR_CurrentContendedMonitor,  /* 9.  JDWP_ThreadReference_CurrentContendedMonitor */
#else
	bvmd_cmdhandler_default,  /* 8.  JDWP_ThreadReference_OwnedMonitors */
	bvmd_cmdhandler_default,  /* 9.  JDWP_ThreadReference_CurrentContendedMonitor */
#endif
	bvmd_cmdhandler_default,  /* 10. JDWP_ThreadReference_Stop */
	bvmd_ch_TR_Interrupt,  /* 11. JDWP_ThreadReference_Interrupt */
	bvmd_ch_TR_SuspendCount,  /* 12. JDWP_ThreadReference_SuspendCount */
	bvmd_cmdhandler_default,  /* 13. JDWP_ThreadReference_OwnedMonitorsStackDepthInfo */
	bvmd_cmdhandler_default,  /* 14. JDWP_ThreadReference_ForceEarlyReturn */
};

static bvmd_cmdhandler_t bvmd_TGR_commands[] = {
	NULL,				   /* unused */
	bvmd_ch_TGR_Name,  /* 1.JDWP_ThreadGroupReference_Name */
	bvmd_ch_TGR_Parent,  /* 2. JDWP_ThreadGroupReference_Parent */
	bvmd_ch_TGR_Children,  /* 3. JDWP_ThreadGroupReference_Children */
};

static bvmd_cmdhandler_t bvmd_AR_commands[] = {
	NULL,				   /* unused */
	bvmd_ch_AR_Length,  /* 1. JDWP_ArrayReference_Length */
	bvmd_ch_AR_GetValues,  /* 2. JDWP_ArrayReference_GetValues */
	bvmd_ch_AR_SetValues,  /* 3. JDWP_ArrayReference_SetValues */
};

static bvmd_cmdhandler_t bvmd_CLR_commands[] = {
	NULL,				   /* unused */
	bvmd_ch_CLR_VisibleClasses,  /* 1. JDWP_ClassLoaderReference_VisibleClasses */
};

static bvmd_cmdhandler_t bvmd_ER_commands[] = {
	NULL,				   /* unused */
	bvmd_ch_ER_Set,  /* 1. JDWP_EventRequest_Set */
	bvmd_ch_ER_Clear,  /* 2. JDWP_EventRequest_Clear */
	bvmd_ch_ER_ClearAllBreakpoints,  /* 3. JDWP_EventRequest_ClearAllBreakpoints */
};

static bvmd_cmdhandler_t bvmd_SF_commands[] = {
	NULL,				   /* unused */
	bvmd_ch_SF_GetValues,  /* 1. JDWP_StackFrame_GetValues 	*/
	bvmd_ch_SF_SetValues,  /* 2. JDWP_StackFrame_SetValues 	*/
	bvmd_ch_SF_ThisObject,  /* 3. JDWP_StackFrame_ThisObject 	*/
};

static bvmd_cmdhandler_t bvmd_COR_commands[] = {
	NULL,				   /* unused */
	bvmd_ch_COR_ReflectedType,  /* 1. JDWP_ClassObjectReference_ReflectedType */
};

static bvmd_cmdhandler_t* bvmd_gl_command_sets[] = {
	NULL,									/* unused */
	bvmd_VM_commands,			/* 1 */
	bvmd_RT_commands,				/* 2 */
	bvmd_CT_commands,					/* 3 */
	bvmd_AT_commands,					/* 4 */
	NULL,									/* 5 - unused */
	bvmd_M_commands,				/* 6 */
	NULL,									/* 7 - unused */
	NULL,									/* 8 - unused */
	bvmd_OR_commands,			/* 10 */
	bvmd_SR_commands,			/* 10 */
	bvmd_TR_commands,			/* 11 */
	bvmd_TGR_commands,		/* 12 */
	bvmd_AR_commands,			/* 13 */
	bvmd_CLR_commands,		/* 14 */
	bvmd_ER_commands,				/* 15 */
	bvmd_SF_commands,				/* 16 */
	bvmd_COR_commands	 	/* 17 */
};

#endif /* BVM_DEBUGGERCOMMANDS_H_ */
