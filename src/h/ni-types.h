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

#ifndef BVM_NI_TYPES_H_
#define BVM_NI_TYPES_H_

/**
  @file

  Types for the native interface (NI).

  @author Greg McCreath
  @since 0.0.10

*/

typedef bvm_uint8_t    jboolean;
typedef bvm_int8_t     jbyte;
typedef bvm_uint16_t   jchar;
typedef bvm_int16_t    jshort;
typedef bvm_int32_t    jint;

#if BVM_FLOAT_ENABLE
typedef bvm_float_t    jfloat;
typedef bvm_double_t   jdouble;
#endif

typedef jint           jsize;
typedef bvm_int64_t    jlong;

typedef void *vmhandle;

typedef vmhandle jfieldID;
typedef vmhandle jmethodID;
typedef vmhandle jobject;

typedef jobject  jclass;
typedef jobject  jthrowable;
typedef jobject  jstring;
typedef jobject  jarray;

typedef jarray   jbooleanArray;
typedef jarray   jbyteArray;
typedef jarray   jcharArray;
typedef jarray   jshortArray;
typedef jarray   jintArray;
typedef jarray   jlongArray;

#if BVM_FLOAT_ENABLE
typedef jarray   jfloatArray;
typedef jarray   jdoubleArray;
#endif

typedef jarray   jobjectArray;


#endif /* BVM_NI_TYPES_H_ */
