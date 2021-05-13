    .text
    .align
    .global rgb888_2_nv12_neon_asm
    .type   rgb888_2_nv12_neon_asm, %function

/*the param of the function*/
#define Dst_NV12_Ptr    r0
#define Src_RGB_Ptr     r1
#define width           r2
#define height          r3
#define pitch           r4 /*width>>4*/

/*the param using in function*/
#define XCount          r5
#define YCount          r6

#define UV_Ptr          r7

#define CData0          q10
#define CData16         d22
#define CData66         d23
#define CData129        d24
#define CData25         d25
#define CData128        d26
#define CData38         d27
#define CData74         d28
#define CData112        d29
#define CData94         d30
#define CData18         d31

rgb888_2_nv12_neon_asm:
        PUSH            {r4-r12, lr}                    /* 10 words */
        VPUSH           {q0-q7}                         /* 8Q -> 32 words */
        VPUSH           {q8-q15}                        /* 8Q -> 32 words */

        /* 向量加载常数 */
        VMOV.s16    CData0  ,   #0
        VMOV.u8     CData16 ,   #16
        VMOV.u8     CData66 ,   #66
        VMOV.u8     CData129,   #129
        VMOV.u8     CData25 ,   #25
        VMOV.u8     CData128,   #128
        VMOV.u8     CData38 ,   #38
        VMOV.u8     CData74 ,   #74
        VMOV.u8     CData112,   #112
        VMOV.u8     CData94 ,   #94
        VMOV.u8     CData18 ,   #18

        /* char* UVPtr = Dst_NV12_Ptr + width*height; */
        MUL         UV_Ptr, width, height
        ADD         UV_Ptr, Dst_NV12_Ptr

        MOV YCount, height                              /*  int YCount = height;   */
        CMP YCount, #0                                  /*  if (YCount == 0) return;*/
        BEQ endColNormal
    beginColNormal:                                     /*  do
                                                            {*/
        MOV     XCount, pitch                           /*  int XCount = pitch;*/
        BEQ     endRowNormal                            /*  if (XCount == 0) continue;*/
    beginRowNormal:                                     /*  do
                                                            {*/
        /* d0 d1 d2 as RGB, d3 d4 d5 as RGB */
        VLD3.8  {d0-d2}, [Src_RGB_Ptr]!
        VLD3.8  {d3-d5}, [Src_RGB_Ptr]!

        #------------------------------------------
        /* 计算Y值 Y = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16; */

        VMULL.u8 q3, d0, CData66
        VMULL.u8 q4, d3, CData66
        VMLAL.u8 q3, d1, CData129
        VMLAL.u8 q4, d4, CData129
        VMLAL.u8 q3, d2, CData25
        VMLAL.u8 q4, d5, CData25

        VADDW.u8 q3 , q3, CData128
        VADDW.u8 q4, q4, CData128
        VSHRN.u16 d10, q3, #8
        VSHRN.u16 d11, q4, #8
        VADDL.u8 q3,d10, CData16
        VADDL.u8 q4,d11, CData16

        /* vqmovn 16 to 8 [0~255] */
        VQMOVN.u16 d10, q3
        VQMOVN.u16 d11, q4

        VST1.u8 {d10} , [Dst_NV12_Ptr]!
        VST1.u8 {d11} , [Dst_NV12_Ptr]!

        TST YCount, #1
        BNE skipUV
        #------------------------------------------
        /* UV预处理，去奇数, 存入d0(R)，d1(G)，d2(B) */
        VUZP.u8 d0 , d3
        VUZP.u8 d1 , d4
        VUZP.u8 d2 , d5

        /*  计算UV
            U = ((-38 * R - 74 * G + 112 * B + 128) >> 8) + 128;
            V = ((112 * R - 94 * G -  18 * B + 128) >> 8) + 128;
            U:q3q5q7, V:q4q6q8
        */
        VMULL.u8 q4, d0, CData112
        VMULL.u8 q3, d0, CData38
        VMULL.u8 q6, d1, CData18
        VMULL.u8 q5, d1, CData112
        VMLAL.u8 q3, d2, CData74
        VMLAL.u8 q6, d2, CData94

        VADDW.u8 q7, q5, CData128
        VADDW.u8 q8, q4, CData128
        VSUB.s16 q2, q7 , q3
        VSUB.s16 q3, q8 , q6

        VSHRN.s16 d0, q2, #8
        VSHRN.s16 d1, q3, #8
        VADDL.u8 q2, d0, CData128
        VADDL.u8 q3, d1, CData128

        VMAX.s16 q4, CData0,    q2
        VMAX.s16 q5, CData0,    q3

        /* vqmovn 16 to 8 [0~255] */
        VQMOVN.u16 d0, q4
        VQMOVN.u16 d1, q5
        #------------------------------------------

        VST2.u8  {d0-d1}, [UV_Ptr]!

    skipUV:
        SUBS    XCount, #1
        BNE     beginRowNormal                  /*      }while(--XCount);*/
    endRowNormal:
        SUBS    YCount, #1                      /*  }while(--YCount);*/
        BNE     beginColNormal
    endColNormal:

        VPOP           {q8-q15}
        VPOP           {q0-q7}
        POP            {r4-r12, pc}