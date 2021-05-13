#include <jni.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <android/log.h>
#include <arm_neon.h>
#include <cpu-features.h>

extern "C"{
#include "helloneon-intrinsics.h"
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libswscale/swscale.h"
}


#define DEBUG 0

#if DEBUG
#include <android/log.h>
#  define  D(x...)  __android_log_print(ANDROID_LOG_INFO,"helloneon",x)
#else
#  define  D(...)  do {} while (0)
#endif
#define LOG_TAG "TEST_NEON"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C" int armFunction(int);




extern "C" void neon_asm_convert(uint8_t * dest, uint8_t * src,int n);

void test()
{
    int16_t result[8];
    int8x8_t a = vdup_n_s8(121);
    int8x8_t b = vdup_n_s8(2);
    int16x8_t c;
    c = vmull_s8(a,b);
    vst1q_s16(result,c);
    for(int i=0;i<8;i++){
        LOGD("data[%d] is %d ",i,result[i]);
    }
}

void normal_convert (uint8_t * __restrict dest, uint8_t * __restrict src, int n)
{
    int i;
    for (i=0; i<n; i++)
    {
        int r = *src++; // load red
        int g = *src++; // load green
        int b = *src++; // load blue

        // build weighted average:
        int y = (r*77)+(g*151)+(b*28);

        // undo the scale by 256 and write to memory:
        *dest++ = (y>>8);
    }
}

void neon_convert (uint8_t * __restrict dest, uint8_t * __restrict src, int n)
{
    int i;
    uint8x8_t rfac = vdup_n_u8 (77);
    uint8x8_t gfac = vdup_n_u8 (151);
    uint8x8_t bfac = vdup_n_u8 (28);
    n/=8;

    for (i=0; i<n; i++)
    {
        uint16x8_t  temp;
        uint8x8x3_t rgb  = vld3_u8 (src);
        uint8x8_t result;

        temp = vmull_u8 (rgb.val[0],      rfac);
        temp = vmlal_u8 (temp,rgb.val[1], gfac);
        temp = vmlal_u8 (temp,rgb.val[2], bfac);

        result = vshrn_n_u16 (temp, 8);
        vst1_u8 (dest, result);
        src  += 8*3;
        dest += 8;
    }
}

void test1()
{
    //准备一张图片，使用软件模拟生成，格式为rgb rgb ..
    uint32_t const array_size = 2048*2048;
    uint8_t * rgb = new uint8_t[array_size*3];
    for(int i=0;i<array_size;i++){
        rgb[i*3]=234;
        rgb[i*3+1]=94;
        rgb[i*3+2]=23;
    }
    //灰度图大小为rgb的1/3
    uint8_t * gray_cpu = new uint8_t[array_size];
    uint8_t * gray_neon = new uint8_t[array_size];
    uint8_t * gray_neon_asm = new uint8_t[array_size];

    struct timeval tv1,tv2;
    gettimeofday(&tv1,NULL);
    normal_convert(gray_cpu,rgb,array_size);
    gettimeofday(&tv2,NULL);
    LOGD("pure cpu cost time:%ld",(tv2.tv_sec-tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec));

    gettimeofday(&tv1,NULL);
    neon_convert(gray_neon,rgb,array_size);
    gettimeofday(&tv2,NULL);
    LOGD("neon c cost time:%ld",(tv2.tv_sec-tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec));

    gettimeofday(&tv1,NULL);
    neon_asm_convert(gray_neon_asm,rgb,array_size);
    gettimeofday(&tv2,NULL);
    LOGD("neon asm cost time:%ld",(tv2.tv_sec-tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec));

    delete[] rgb;
    delete[] gray_cpu;
    delete[] gray_neon;
    delete[] gray_neon_asm;
}

extern "C"
void
Java_com_example_helloneon_HelloNeon_rgb2gray(JNIEnv *env, jobject thiz) {
    // TODO: implement rgb2gray()
    test1();
    LOGD("汇编语言执行结果为：%d",armFunction(2));
}