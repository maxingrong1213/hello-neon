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


static AVCodecContext *c = nullptr;
static AVFrame* frame;
static AVPacket pkt;
struct SwsContext* sws_context = nullptr;
static uint8_t* m_yuv_data = nullptr;
static FILE *file;

void rgb2yuv(uint8_t *rgb_data){
#if 0

#else
    static int y_size = 720 * 1280;
    int y_index = 0;
    int u_index = y_size;
    int v_index = u_index + y_size / 4;
    int R, G, B, Y, U, V;

    for(int i=0; i<frame->height; i++){
        for(int j=0; j<frame->width; j++){
            R = rgb_data[(i*frame->width + j) * 4 + 0];
            G = rgb_data[(i*frame->width + j) * 4 + 1];
            B = rgb_data[(i*frame->width + j) * 4 + 2];
            Y = ((66*R + 129*G + 25*B) >> 8 ) + 16;
            U = ((-38*R - 74*G + 112*B) >> 8 ) + 128;
            V = ((112*R - 94*G - 18*B) >> 8 ) + 128;
            m_yuv_data[y_index++] = (uint8_t)(Y);
            if(i%2 == 0 && j%2 == 0){
                m_yuv_data[u_index++] = (uint8_t)(U);
                m_yuv_data[v_index++] = (uint8_t)(V);
            }
        }
    }
    frame->data[0] = m_yuv_data;
    frame->data[1] = m_yuv_data + y_size;
    frame->data[2] = m_yuv_data + y_size * 5 / 4;
#endif
}


void RGBtoNV12(uint8_t* pNV12, uint8_t* pRGB, int width, int height)
{
    int frameSize = width * height;
    int yIndex = 0;
    int uvIndex = frameSize;

    int R, G, B, Y, U, V;
    int index = 0;
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            R = pRGB[index++];
            G = pRGB[index++];
            B = pRGB[index++];

            Y = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
            U = ((-38 * R - 74 * G + 112 * B + 128) >> 8) + 128;
            V = ((112 * R - 94 * G - 18 * B + 128) >> 8) + 128;

            // NV12  YYYYYYYY UVUV
            // NV21  YYYYYYYY VUVU
            pNV12[yIndex++] = (uint8_t)((Y < 0) ? 0 : ((Y > 255) ? 255 : Y));
            if (j % 2 == 0 && index % 2 == 0)
            {
                pNV12[uvIndex++] = (uint8_t)((U < 0) ? 0 : ((U > 255) ? 255 : U));
                pNV12[uvIndex++] = (uint8_t)((V < 0) ? 0 : ((V > 255) ? 255 : V));
            }
        }
    }
//    frame->data[0] = pNV12;
//    frame->data[1] = pNV12 + frameSize;
//    frame->data[2] = pNV12 + frameSize * 5 / 4;
}

extern "C" void rgb888_2_nv12_neon_asm(uint8_t * nv12, uint8_t * rgb, int width, int height);

void rgb888_2_nv12_intrinsic(uint8_t * nv12, uint8_t * rgb, int width, int height)
{
    const uint8x8_t u8_zero = vdup_n_u8(0);
    const uint8x8_t u8_16 = vdup_n_u8(16);
    const uint16x8_t u16_rounding = vdupq_n_u16(128);

    const int16x8_t s16_zero = vdupq_n_s16(0);
    const int8x8_t s8_rounding = vdup_n_s8(-128);
    const int16x8_t s16_rounding = vdupq_n_s16(128);
    int frameSize = width * height;
    uint8_t* UVPtr = nv12 + frameSize;
    int pitch = width >> 4;

    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < pitch; ++i)
        {
            // Load rgb 16 pixel
            uint8x16x3_t pixel_rgb = vld3q_u8(rgb);

            uint8x8_t high_r = vget_high_u8(pixel_rgb.val[0]);
            uint8x8_t low_r = vget_low_u8(pixel_rgb.val[0]);
            uint8x8_t high_g = vget_high_u8(pixel_rgb.val[1]);
            uint8x8_t low_g = vget_low_u8(pixel_rgb.val[1]);
            uint8x8_t high_b = vget_high_u8(pixel_rgb.val[2]);
            uint8x8_t low_b = vget_low_u8(pixel_rgb.val[2]);

            // NOTE:
            // declaration may not appear after executable statement in block
            uint16x8_t high_y;
            uint16x8_t low_y;

            // 1. Multiply transform matrix (Y′: unsigned, U/V: signed)
            // 2. Scale down (">>8") to 8-bit values with rounding ("+128") (Y′: unsigned, U/V: signed)
            // 3. Add an offset to the values to eliminate any negative values (all results are 8-bit unsigned)
            uint8x8_t scalar = vdup_n_u8(66);
            high_y = vmull_u8(high_r, scalar);
            low_y = vmull_u8(low_r, scalar);

            scalar = vdup_n_u8(129);
            high_y = vmlal_u8(high_y, high_g, scalar);
            low_y = vmlal_u8(low_y, low_g, scalar);

            scalar = vdup_n_u8(25);
            high_y = vmlal_u8(high_y, high_b, scalar);
            low_y = vmlal_u8(low_y, low_b, scalar);

            high_y = vaddq_u16(high_y, u16_rounding);
            low_y = vaddq_u16(low_y, u16_rounding);

            uint8x8_t u8_low_y = vshrn_n_u16(low_y, 8);
            uint8x8_t u8_high_y = vshrn_n_u16(high_y, 8);

            low_y = vaddl_u8(u8_low_y, u8_16);
            high_y = vaddl_u8(u8_high_y, u8_16);

            uint8x16_t pixel_y = vcombine_u8(vqmovn_u16(low_y), vqmovn_u16(high_y));

            // Store
            vst1q_u8(nv12, pixel_y);

            if (j % 2 == 0)
            {
                uint8x8x2_t mix_r = vuzp_u8(low_r, high_r);
                uint8x8x2_t mix_g = vuzp_u8(low_g, high_g);
                uint8x8x2_t mix_b = vuzp_u8(low_b, high_b);

                int16x8_t signed_r = vreinterpretq_s16_u16(vaddl_u8(mix_r.val[0], u8_zero));
                int16x8_t signed_g = vreinterpretq_s16_u16(vaddl_u8(mix_g.val[0], u8_zero));
                int16x8_t signed_b = vreinterpretq_s16_u16(vaddl_u8(mix_b.val[0], u8_zero));

                int16x8_t signed_u;
                int16x8_t signed_v;

                int16x8_t signed_scalar = vdupq_n_s16(-38);
                signed_u = vmulq_s16(signed_r, signed_scalar);

                signed_scalar = vdupq_n_s16(112);
                signed_v = vmulq_s16(signed_r, signed_scalar);

                signed_scalar = vdupq_n_s16(-74);
                signed_u = vmlaq_s16(signed_u, signed_g, signed_scalar);

                signed_scalar = vdupq_n_s16(-94);
                signed_v = vmlaq_s16(signed_v, signed_g, signed_scalar);

                signed_scalar = vdupq_n_s16(112);
                signed_u = vmlaq_s16(signed_u, signed_b, signed_scalar);

                signed_scalar = vdupq_n_s16(-18);
                signed_v = vmlaq_s16(signed_v, signed_b, signed_scalar);

                signed_u = vaddq_s16(signed_u, s16_rounding);
                signed_v = vaddq_s16(signed_v, s16_rounding);

                int8x8_t s8_u = vshrn_n_s16(signed_u, 8);
                int8x8_t s8_v = vshrn_n_s16(signed_v, 8);

                signed_u = vsubl_s8(s8_u, s8_rounding);
                signed_v = vsubl_s8(s8_v, s8_rounding);

                signed_u = vmaxq_s16(signed_u, s16_zero);
                signed_v = vmaxq_s16(signed_v, s16_zero);

                uint16x8_t unsigned_u = vreinterpretq_u16_s16(signed_u);
                uint16x8_t unsigned_v = vreinterpretq_u16_s16(signed_v);

                uint8x8x2_t result;
                result.val[0] = vqmovn_u16(unsigned_u);
                result.val[1] = vqmovn_u16(unsigned_v);

                vst2_u8(UVPtr, result);
                UVPtr += 16;
            }

            rgb += 3 * 16;
            nv12 += 16;
        }
    }
//    frame->data[0] = nv12;
//    frame->data[1] = nv12 + frameSize;
//    frame->data[2] = nv12 + frameSize * 5 / 4;
}


uint8_t* generate_rgb(int width, int height, int pts, uint8_t *rgb) {
    int x, y, cur;
    rgb = (uint8_t *)realloc(rgb, 3 * sizeof(uint8_t) * height * width);
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            cur = 3 * (y * width + x);
            rgb[cur + 0] = 0;
            rgb[cur + 1] = 0;
            rgb[cur + 2] = 0;
            if ((frame->pts / 25) % 2 == 0) {
                if (y < height / 2) {
                    if (x < width / 2) {
                        /* Black. */
                    }
                    else {
                        rgb[cur + 0] = 255;
                    }
                }
                else {
                    if (x < width / 2) {
                        rgb[cur + 1] = 255;
                    }
                    else {
                        rgb[cur + 2] = 255;
                    }
                }
            }
            else {
                if (y < height / 2) {
                    rgb[cur + 0] = 255;
                    if (x < width / 2) {
                        rgb[cur + 1] = 255;
                    }
                    else {
                        rgb[cur + 2] = 255;
                    }
                }
                else {
                    if (x < width / 2) {
                        rgb[cur + 1] = 255;
                        rgb[cur + 2] = 255;
                    }
                    else {
                        rgb[cur + 0] = 255;
                        rgb[cur + 1] = 255;
                        rgb[cur + 2] = 255;
                    }
                }
            }
        }
    }
    return rgb;
}

void encoder_start(const char*filename, int codec_id, int fps, int width, int height){

    //测试minicap函数
    int32_t displayid=3;
    //Minicap* minicap_ma = minicap_create(displayid);

    AVCodec* codec;
    int ret;
    codec = avcodec_find_encoder((AVCodecID)codec_id);
    LOGD("调用编码卡avcodec_find_encoder_by_name()\n");
    //codec = avcodec_find_encoder_by_name("h264_ni_enc");
    if(!codec){
        LOGD("can not find h264_ni_enc\n");
        exit(1);
    }
    LOGD("调用编码卡avcodec_alloc_context3()\n");
    c = avcodec_alloc_context3(codec);
    if(!c){
        LOGD("can not allocate video codec context\n");
        exit(1);
    }
    c->bit_rate = 2048000;
    c->width = width;
    c->height = height;
    c->time_base.num = 1;
    c->time_base.den = fps;
    c->framerate.num = fps;
    c->framerate.den = 1;
    c->profile = 66;
//	c->keyint_min = 60;
    c->pix_fmt = AV_PIX_FMT_YUV420P;
    if(codec_id == AV_CODEC_ID_H264){
        av_opt_set(c->priv_data, "preset", "slow", 0);
        av_opt_set(c->priv_data, "tune", "zerolatency", 0);
    }
    LOGD("调用编码卡avcodec_open2\n");
    if(avcodec_open2(c, codec, NULL) < 0){
        LOGD("can not open codec\n");
        exit(1);
    }

    file = fopen(filename, "wb");
    if (!file) {
        LOGD("Could not open %s\n",filename);
        exit(1);
    }
    LOGD("调用编码卡av_frame_alloc()\n");
    frame = av_frame_alloc();
    if(!frame){
        LOGD("can not allocate frame\n");
        exit(1);
    }
    frame->format = c->pix_fmt;
    frame->width = c->width;
    frame->height = c->height;
//	ret = av_image_alloc(frame->data, frame->linesize, c->width, c->height, c->pix_fmt, 32);
//	if(!ret){
//		std::cout << "can not allocate image buffer" << std::endl;
//		exit(1);
//	}
}

void encoder_finish(){
    uint8_t encode[] = {0, 0, 1, 0xb7};
    int got_output, ret;
    do{
        fflush(stdout);
        ret = avcodec_encode_video2(c, &pkt, nullptr, &got_output);
        if(ret < 0){
            LOGD("encode error\n");
            exit(1);
        }
        if(got_output){
            fwrite(pkt.data, 1, pkt.size, file);
            av_packet_unref(&pkt);
        }
    }while(got_output);
    fwrite(encode, 1, sizeof(encode), file);
    fclose(file);
    avcodec_close(c);
    av_free(c);
    av_freep(&frame->data[0]);
    av_frame_free(&frame);
}

void encoder_frame(uint8_t *rgb){
    int ret, got_output;
    struct timeval tv1,tv2;

    gettimeofday(&tv1,NULL);
    rgb2yuv(rgb);
    gettimeofday(&tv2,NULL);
    LOGD("rgb2yuv pure cpu cost time:%ld",(tv2.tv_sec-tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec));


    gettimeofday(&tv1,NULL);
    RGBtoNV12(m_yuv_data,rgb,720,1280);
    gettimeofday(&tv2,NULL);
    LOGD("RGBtoNV12 pure cpu cost time:%ld",(tv2.tv_sec-tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec));


    gettimeofday(&tv1,NULL);
    rgb888_2_nv12_intrinsic(m_yuv_data,rgb,720,1280);
    gettimeofday(&tv2,NULL);
    LOGD("rgb888_2_nv12_intrinsic pure cpu cost time:%ld",(tv2.tv_sec-tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec));


//    gettimeofday(&tv1,NULL);
//    rgb888_2_nv12_neon_asm(m_yuv_data,rgb,720,1080);
//    gettimeofday(&tv2,NULL);
//    LOGD("rgb888_2_nv12_neon_asm pure cpu cost time:%ld",(tv2.tv_sec-tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec));


    LOGD("encode frame:%d\n",frame->pts);
    av_init_packet(&pkt);
//	if(frame->pts == 1){
//		frame->key_frame = 1;
//		frame->pict_type = AV_PICTURE_TYPE_I;
//	}
//	else{
//		frame->key_frame = 0;
//		frame->pict_type = AV_PICTURE_TYPE_P;
//	}
    LOGD("调用编码卡avcodec_encode_video2()\n");
    ret = avcodec_encode_video2(c, &pkt, frame, &got_output);
    if(ret < 0){
        LOGD("encode error\n");
        exit(1);
    }
    if(got_output){
        fwrite(pkt.data, 1, pkt.size, file);
        av_packet_unref(&pkt);
    }
}

void encode_example(const char* filename, int codec_id){
    int pts = 0;
    const int width = 720;
    const int height = 1280;
//	static uint8_t rgb[width*height*4];
    LOGD("开始encoder_start()");
    encoder_start(filename, codec_id, 25, width, height);
    LOGD("结束encoder_start()");
//	while(!filestream.eof()){
//		filestream.read((char*)rgb, width * height * 4);
//		frame->pts = pts++;
//		encoder_frame(rgb);
//	}


    uint8_t* rgb = nullptr;
    LOGD("调用avpicture_get_size()");
    int yuv_size = avpicture_get_size(c->pix_fmt,frame->width, frame->height);
    m_yuv_data = (unsigned char*)av_malloc(yuv_size);
    avpicture_fill((AVPicture*)frame, m_yuv_data,c->pix_fmt, frame->width, frame->height);
    for(pts=0; pts<1; pts++){
        frame->pts = pts;
        rgb = generate_rgb(width, height, pts, rgb);
        LOGD("开始encoder_frame()");
        encoder_frame(rgb);
        LOGD("结束encoder_frame()");
    }
    encoder_finish();
}



extern "C"
void
Java_com_example_helloneon_HelloNeon_rgb2yuv( JNIEnv* env,
                                              jobject thiz ){
    avcodec_register_all();
    encode_example("/data/data/com.example.helloneon/ma.h264", AV_CODEC_ID_H264);
    LOGD("汇编语言执行结果为：%d",armFunction(2));
}