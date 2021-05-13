    .text
    .align  2
    .global armFunction
    .type   armFunction, %function
armFunction:
    stmfd   sp!, {fp,ip,lr}
    mov r3, r0, asl #3
    add r0, r3, r0, asl #1
    ldmfd   sp!, {fp,ip,lr}
    bx  lr
    .size   armFunction, .-armFunction


    .global neon_asm_convert
    neon_asm_convert:
          # r0: Ptr to destination data
          # r1: Ptr to source data
          # r2: Iteration count:
          push        {r4-r5,lr}
          lsr         r2, r2, #3
          # build the three constants:
          mov         r3, #77
          mov         r4, #151
          mov         r5, #28
          vdup.8      d3, r3
          vdup.8      d4, r4
          vdup.8      d5, r5
      .loop:
          # load 8 pixels:
          vld3.8      {d0-d2}, [r1]!
          # do the weight average:
          vmull.u8    q3, d0, d3
          vmlal.u8    q3, d1, d4
          vmlal.u8    q3, d2, d5
          # shift and store:
          vshrn.u16   d6, q3, #8
          vst1.8      {d6}, [r0]!
          subs        r2, r2, #1
          bne         .loop
          pop         { r4-r5, pc }