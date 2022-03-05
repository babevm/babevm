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

#ifndef BVM_EXEC_H_
#define BVM_EXEC_H_

/**
  @file

  Constants/Macros/Functions for the interpreter loop.

  @author Greg McCreath
  @since 0.0.10

*/

void bvm_exec_run();

/* The list of opcodes in all their naked glory. */
#define OPCODE_nop             0
#define OPCODE_aconst_null     1
#define OPCODE_iconst_m1       2
#define OPCODE_iconst_0        3
#define OPCODE_iconst_1        4
#define OPCODE_iconst_2        5
#define OPCODE_iconst_3        6
#define OPCODE_iconst_4        7
#define OPCODE_iconst_5        8
#define OPCODE_lconst_0        9
#define OPCODE_lconst_1        10
#define OPCODE_fconst_0        11
#define OPCODE_fconst_1        12
#define OPCODE_fconst_2        13
#define OPCODE_dconst_0        14
#define OPCODE_dconst_1        15
#define OPCODE_bipush          16
#define OPCODE_sipush          17
#define OPCODE_ldc             18
#define OPCODE_ldc_w           19
#define OPCODE_ldc2_w          20
#define OPCODE_iload           21
#define OPCODE_lload           22
#define OPCODE_fload           23
#define OPCODE_dload           24
#define OPCODE_aload           25
#define OPCODE_iload_0         26
#define OPCODE_iload_1         27
#define OPCODE_iload_2         28
#define OPCODE_iload_3         29
#define OPCODE_lload_0         30
#define OPCODE_lload_1         31
#define OPCODE_lload_2         32
#define OPCODE_lload_3         33
#define OPCODE_fload_0         34
#define OPCODE_fload_1         35
#define OPCODE_fload_2         36
#define OPCODE_fload_3         37
#define OPCODE_dload_0         38
#define OPCODE_dload_1         39
#define OPCODE_dload_2         40
#define OPCODE_dload_3         41
#define OPCODE_aload_0         42
#define OPCODE_aload_1         43
#define OPCODE_aload_2         44
#define OPCODE_aload_3         45
#define OPCODE_iaload          46
#define OPCODE_laload          47
#define OPCODE_faload          48
#define OPCODE_daload          49
#define OPCODE_aaload          50
#define OPCODE_baload          51
#define OPCODE_caload          52
#define OPCODE_saload          53
#define OPCODE_istore          54
#define OPCODE_lstore          55
#define OPCODE_fstore          56
#define OPCODE_dstore          57
#define OPCODE_astore          58
#define OPCODE_istore_0        59
#define OPCODE_istore_1        60
#define OPCODE_istore_2        61
#define OPCODE_istore_3        62
#define OPCODE_lstore_0        63
#define OPCODE_lstore_1        64
#define OPCODE_lstore_2        65
#define OPCODE_lstore_3        66
#define OPCODE_fstore_0        67
#define OPCODE_fstore_1        68
#define OPCODE_fstore_2        69
#define OPCODE_fstore_3        70
#define OPCODE_dstore_0        71
#define OPCODE_dstore_1        72
#define OPCODE_dstore_2        73
#define OPCODE_dstore_3        74
#define OPCODE_astore_0        75
#define OPCODE_astore_1        76
#define OPCODE_astore_2        77
#define OPCODE_astore_3        78
#define OPCODE_iastore         79
#define OPCODE_lastore         80
#define OPCODE_fastore         81
#define OPCODE_dastore         82
#define OPCODE_aastore         83
#define OPCODE_bastore         84
#define OPCODE_castore         85
#define OPCODE_sastore         86
#define OPCODE_pop             87
#define OPCODE_pop2            88
#define OPCODE_dup             89
#define OPCODE_dup_x1          90
#define OPCODE_dup_x2          91
#define OPCODE_dup2            92
#define OPCODE_dup2_x1         93
#define OPCODE_dup2_x2         94
#define OPCODE_swap            95
#define OPCODE_iadd            96
#define OPCODE_ladd            97
#define OPCODE_fadd            98
#define OPCODE_dadd            99
#define OPCODE_isub            100
#define OPCODE_lsub            101
#define OPCODE_fsub            102
#define OPCODE_dsub            103
#define OPCODE_imul            104
#define OPCODE_lmul            105
#define OPCODE_fmul            106
#define OPCODE_dmul            107
#define OPCODE_idiv            108
#define OPCODE_ldiv            109
#define OPCODE_fdiv            110
#define OPCODE_ddiv            111
#define OPCODE_irem            112
#define OPCODE_lrem            113
#define OPCODE_frem            114
#define OPCODE_drem            115
#define OPCODE_ineg            116
#define OPCODE_lneg            117
#define OPCODE_fneg            118
#define OPCODE_dneg            119
#define OPCODE_ishl            120
#define OPCODE_lshl            121
#define OPCODE_ishr            122
#define OPCODE_lshr            123
#define OPCODE_iushr           124
#define OPCODE_lushr           125
#define OPCODE_iand            126
#define OPCODE_land            127
#define OPCODE_ior             128
#define OPCODE_lor             129
#define OPCODE_ixor            130
#define OPCODE_lxor            131
#define OPCODE_iinc            132
#define OPCODE_i2l             133
#define OPCODE_i2f             134
#define OPCODE_i2d             135
#define OPCODE_l2i             136
#define OPCODE_l2f             137
#define OPCODE_l2d             138
#define OPCODE_f2i             139
#define OPCODE_f2l             140
#define OPCODE_f2d             141
#define OPCODE_d2i             142
#define OPCODE_d2l             143
#define OPCODE_d2f             144
#define OPCODE_i2b             145
#define OPCODE_i2c             146
#define OPCODE_i2s             147
#define OPCODE_lcmp            148
#define OPCODE_fcmpl           149
#define OPCODE_fcmpg           150
#define OPCODE_dcmpl           151
#define OPCODE_dcmpg           152
#define OPCODE_ifeq            153
#define OPCODE_ifne            154
#define OPCODE_iflt            155
#define OPCODE_ifge            156
#define OPCODE_ifgt            157
#define OPCODE_ifle            158
#define OPCODE_if_icmpeq       159
#define OPCODE_if_icmpne       160
#define OPCODE_if_icmplt       161
#define OPCODE_if_icmpge       162
#define OPCODE_if_icmpgt       163
#define OPCODE_if_icmple       164
#define OPCODE_if_acmpeq       165
#define OPCODE_if_acmpne       166
#define OPCODE_goto            167
#define OPCODE_jsr             168
#define OPCODE_ret             169
#define OPCODE_tableswitch     170
#define OPCODE_lookupswitch    171
#define OPCODE_ireturn         172
#define OPCODE_lreturn         173
#define OPCODE_freturn         174
#define OPCODE_dreturn         175
#define OPCODE_areturn         176
#define OPCODE_return          177
#define OPCODE_getstatic       178
#define OPCODE_putstatic       179
#define OPCODE_getfield        180
#define OPCODE_putfield        181
#define OPCODE_invokevirtual   182
#define OPCODE_invokespecial   183
#define OPCODE_invokestatic    184
#define OPCODE_invokeinterface 185
#define OPCODE_186             186
#define OPCODE_new             187
#define OPCODE_newarray        188
#define OPCODE_anewarray       189
#define OPCODE_arraylength     190
#define OPCODE_athrow          191
#define OPCODE_checkcast       192
#define OPCODE_instanceof      193
#define OPCODE_monitorenter    194
#define OPCODE_monitorexit     195
#define OPCODE_wide            196
#define OPCODE_multianewarray  197
#define OPCODE_ifnull          198
#define OPCODE_ifnonnull       199
#define OPCODE_goto_w          200
#define OPCODE_jsr_w           201
#define OPCODE_breakpoint      202

#define OPCODE_203_pop_for_exception 203
#define OPCODE_204             204
#define OPCODE_205             205
#define OPCODE_206             206
#define OPCODE_207             207
#define OPCODE_208             208
#define OPCODE_209             209
#define OPCODE_210             210
#define OPCODE_211             211
#define OPCODE_212             212
#define OPCODE_213             213
#define OPCODE_214             214
#define OPCODE_215             215

/* plus some non-standard ones to help us processes run quicker */
#define OPCODE_ldc_fast_1           216
#define OPCODE_ldc_fast_2           217
#define OPCODE_ldc_w_fast_1         218
#define OPCODE_ldc_w_fast_2         219
#define OPCODE_getstatic_fast       220
#define OPCODE_getstatic_fast_long  221
#define OPCODE_putstatic_fast  		222
#define OPCODE_putstatic_fast_long  223
#define OPCODE_getfield_fast   		224
#define OPCODE_getfield_fast_long   225
#define OPCODE_putfield_fast   		226
#define OPCODE_putfield_fast_long   227
#define OPCODE_new_fast				228

#define OPCODE_229_invokestatic_fast    229
#define OPCODE_230_invokespecial_fast   230
#define OPCODE_231_invokeinterface_fast 231
#define OPCODE_232_invokevirtual_fast   232
#define OPCODE_233 					233
#define OPCODE_234             		234
#define OPCODE_235             		235
#define OPCODE_236             		236
#define OPCODE_237             		237
#define OPCODE_238             		238
#define OPCODE_239             		239

#define OPCODE_240             		240
#define OPCODE_241             		241
#define OPCODE_242             		242
#define OPCODE_243             		243
#define OPCODE_244             		244
#define OPCODE_245             		245
#define OPCODE_246             		246
#define OPCODE_247             		247
#define OPCODE_248             		248
#define OPCODE_249             		249

#define OPCODE_250             		250
#define OPCODE_251             		251
#define OPCODE_252             		252
#define OPCODE_253             		253

#define OPCODE_impdep1         		254
#define OPCODE_impdep2         		255


#endif /*BVM_EXEC_H_*/
