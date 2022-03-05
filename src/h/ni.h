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

/**
  @file

  Constants/Macros/Functions/Types for the Native Interface.

  @author Greg McCreath
  @since 0.0.10

*/

#ifndef BVM_NI_H_
#define BVM_NI_H_

#define NI_OK           BVM_OK
#define NI_ERR          BVM_ERR

#define NI_VERSION 0x00010000

jint 	 NI_GetVersion();

jclass 	 NI_FindClass(const char *name);
jclass 	 NI_GetSuperclass(jclass clazz);
jboolean NI_IsAssignableFrom(jclass clazz1, jclass clazz2);
jclass 	 NI_GetCurrentClass();

jint 	 NI_Throw(jthrowable obj);
jint     NI_ThrowNew(jclass clazz, const char *msg);
void 	 NI_FatalError(char *msg);

jboolean NI_ExceptionCheck();
void 	 NI_ExceptionClear();
jthrowable NI_ExceptionOccurred();

jclass   NI_GetObjectClass(jobject obj);
jboolean NI_IsInstanceOf(jobject obj, jclass clazz);

jfieldID NI_GetFieldID(jclass clazz, const char *name, const char *sig);

jobject  NI_GetObjectField(jobject obj, jfieldID fieldID);
jboolean NI_GetBooleanField(jobject obj, jfieldID fieldID);
jbyte    NI_GetByteField(jobject obj, jfieldID fieldID);
jchar    NI_GetCharField(jobject obj, jfieldID fieldID);
jshort   NI_GetShortField(jobject obj, jfieldID fieldID);
jint     NI_GetIntField(jobject obj, jfieldID fieldID);
jlong    NI_GetLongField(jobject obj, jfieldID fieldID);
#if BVM_FLOAT_ENABLE
jfloat   NI_GetFloatField(jobject obj, jfieldID fieldID);
jdouble  NI_GetDoubleField(jobject obj, jfieldID fieldID);
#endif

void 	 NI_SetObjectField(jobject obj, jfieldID fieldID, jobject val);
void 	 NI_SetBooleanField(jobject obj, jfieldID fieldID, jboolean val);
void 	 NI_SetByteField(jobject obj, jfieldID fieldID, jbyte val);
void 	 NI_SetCharField(jobject obj, jfieldID fieldID, jchar val);
void 	 NI_SetShortField(jobject obj, jfieldID fieldID, jshort val);
void 	 NI_SetIntField(jobject obj, jfieldID fieldID, jint val);
void 	 NI_SetLongField(jobject obj, jfieldID fieldID, jlong val);
#if BVM_FLOAT_ENABLE
void 	 NI_SetFloatField(jobject obj, jfieldID fieldID, jfloat val);
void 	 NI_SetDoubleField(jobject obj, jfieldID fieldID, jdouble val);
#endif

jfieldID NI_GetStaticFieldID(jclass clazz, const char *name, const char *sig);

jobject  NI_GetStaticObjectField(jclass clazz, jfieldID fieldID);
jboolean NI_GetStaticBooleanField(jclass clazz, jfieldID fieldID);
jbyte    NI_GetStaticByteField(jclass clazz, jfieldID fieldID);
jchar    NI_GetStaticCharField(jclass clazz, jfieldID fieldID);
jshort   NI_GetStaticShortField(jclass clazz, jfieldID fieldID);
jint     NI_GetStaticIntField(jclass clazz, jfieldID fieldID);
jlong    NI_GetStaticLongField(jclass clazz, jfieldID fieldID);
#if BVM_FLOAT_ENABLE
jfloat 	 NI_GetStaticFloatField(jclass clazz, jfieldID fieldID);
jdouble  NI_GetStaticDoubleField(jclass clazz, jfieldID fieldID);
#endif

void 	 NI_SetStaticObjectField(jclass clazz, jfieldID fieldID, jobject value);
void 	 NI_SetStaticBooleanField(jclass clazz, jfieldID fieldID, jboolean value);
void 	 NI_SetStaticByteField(jclass clazz, jfieldID fieldID, jbyte value);
void 	 NI_SetStaticCharField(jclass clazz, jfieldID fieldID, jchar value);
void 	 NI_SetStaticShortField(jclass clazz, jfieldID fieldID, jshort value);
void 	 NI_SetStaticIntField(jclass clazz, jfieldID fieldID, jint value);
void 	 NI_SetStaticLongField(jclass clazz, jfieldID fieldID, jlong value);
#if BVM_FLOAT_ENABLE
void 	 NI_SetStaticFloatField(jclass clazz, jfieldID fieldID, jfloat value);
void 	 NI_SetStaticDoubleField(jclass clazz, jfieldID fieldID, jdouble value);
#endif

jstring      NI_NewString(const jchar *unicode, jsize len);
jsize 		 NI_GetStringLength(jstring str);
const jchar *NI_GetStringChars(jstring str);

jstring 	 NI_NewStringUTF(const char *utf);
jsize 		 NI_GetStringUTFLength(jstring str);
const char*  NI_GetStringUTFChars(jstring str);
jint 	     NI_GetStringRegion(jstring str, jsize start, jsize len, jchar *buf);
jint 	     NI_GetStringUTFRegion(jstring str, jsize start, jsize len, char *buf);

jsize 		 NI_GetArrayLength(jarray array);

jobjectArray NI_NewObjectArray(jsize len, jclass clazz, jobject init);
jobject 	 NI_GetObjectArrayElement(jobjectArray array, jsize index);
jint 	     NI_SetObjectArrayElement(jobjectArray array, jsize index, jobject val);

jbooleanArray 	NI_NewBooleanArray(jsize len);
jbyteArray 		NI_NewByteArray(jsize len);
jcharArray 		NI_NewCharArray(jsize len);
jshortArray 	NI_NewShortArray(jsize len);
jintArray 		NI_NewIntArray(jsize len);
jlongArray 		NI_NewLongArray(jsize len);
#if BVM_FLOAT_ENABLE
jfloatArray 	NI_NewFloatArray(jsize len);
jdoubleArray 	NI_NewDoubleArray(jsize len);
#endif

jboolean *NI_GetBooleanArrayElements(jbooleanArray array);
jbyte    *NI_GetByteArrayElements(jbyteArray array);
jchar    *NI_GetCharArrayElements(jcharArray array);
jshort   *NI_GetShortArrayElements(jshortArray array);
jint     *NI_GetIntArrayElements(jintArray array);
jlong    *NI_GetLongArrayElements(jlongArray array);
#if BVM_FLOAT_ENABLE
jfloat   *NI_GetFloatArrayElements(jfloatArray array);
jdouble  *NI_GetDoubleArrayElements(jdoubleArray array);
#endif

void *NI_MallocLocal(jint size);
void *NI_MallocGlobal(jint size);
void NI_Free(void *handle);

#define NI_IsSameObject(obj1, obj2) ( (obj1 == obj2) ? NI_TRUE : NI_FALSE)

#define NI_GetParameterAsObject(x)   ((jobject)  ((bvm_cell_t*)args)[x].ref_value)
#define NI_GetParameterAsInt(x)      ((jint) 	 ((bvm_cell_t*)args)[x].int_value)
#define NI_GetParameterAsByte(x)     ((jbyte) 	 ((bvm_cell_t*)args)[x].int_value)
#define NI_GetParameterAsShort(x)    ((jshort) 	 ((bvm_cell_t*)args)[x].int_value)
#define NI_GetParameterAsChar(x)     ((jchar) 	 ((bvm_cell_t*)args)[x].int_value)
#define NI_GetParameterAsBoolean(x)  ((jboolean) ((bvm_cell_t*)args)[x].int_value)
#if BVM_FLOAT_ENABLE
#define NI_GetParameterAsFloat(x)    ((jfloat) 	 ((bvm_cell_t*)args)[x].float_value)
#define NI_GetParameterAsDouble(x)    (__NI_GetParameterAsDouble(args))
jdouble __NI_GetParameterAsDouble(void *args);
#endif
#define NI_GetParameterAsLong(x)     ((jlong) (BVM_INT64_from_cells( ((bvm_cell_t*)args)+x)))

#define NI_ReturnInt(v) 	(bvm_gl_rx_sp++[0].int_value = (bvm_int32_t)(v))
#define NI_ReturnShort(v)   (NI_ReturnInt(v))
#define NI_ReturnChar(v)    (NI_ReturnInt(v))
#define NI_ReturnByte(v)    (NI_ReturnInt(v))
#define NI_ReturnBoolean(v) (NI_ReturnInt(v))
#define NI_ReturnObject(v) 	(bvm_gl_rx_sp++[0].ref_value = (bvm_obj_t*)(v))
#if BVM_FLOAT_ENABLE
#define NI_ReturnFloat(v) 	(bvm_gl_rx_sp++[0].float_value = (jfloat)(v))
#define NI_ReturnDouble(v) 	BVM_DOUBLE_to_cells(bvm_gl_rx_sp, v); \
                            bvm_gl_rx_sp+=2;
#endif
#define NI_ReturnLong(v) 	BVM_INT64_to_cells(bvm_gl_rx_sp, v); \
                            bvm_gl_rx_sp+=2;
#define NI_ReturnVoid() 	{}

#endif /*BVM_NI_H_*/
