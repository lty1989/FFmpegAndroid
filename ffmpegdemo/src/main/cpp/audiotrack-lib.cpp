

#include <jni.h>
#include <android/log.h>
#include "libswresample/swresample.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
//};


#define  LOG_TAG    "ffmpegdemo"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

JNIEXPORT jint JNICALL
Java_com_ffmpegdemo_AudioTrackActivity_play
        (JNIEnv *env, jclass clazz) {
    LOGD("play");

    // sd卡中的视频文件地址,可自行修改或者通过jni传入
    //char *file_name = "/storage/emulated/0/test.mp4";
    char *file_name = "/storage/emulated/0/temp.mp3";

    av_register_all();

    AVFormatContext *pFormatCtx = avformat_alloc_context();

    // Open video file
    int ecode = 0;
    if ((ecode = avformat_open_input(&pFormatCtx, file_name, NULL, NULL)) != 0) {

        LOGD("Couldn't open file:%s  code=%d\n", file_name, ecode);
        return -1; // Couldn't open file
    }

    // Retrieve stream information
    if ((ecode = avformat_find_stream_info(pFormatCtx, NULL)) < 0) {
        LOGD("Couldn't find stream information. %d", ecode);
        return -1;
    }

    // Find the first video stream
    int audio_stream_idx = -1, i;
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO
            && audio_stream_idx < 0) {
            audio_stream_idx = i;
        }
    }
    if (audio_stream_idx == -1) {
        LOGD("Didn't find a video stream.");
        return -1; // Didn't find a video stream
    }

    // Get a pointer to the codec context for the video stream
    AVCodecContext *pCodecCtx = pFormatCtx->streams[audio_stream_idx]->codec;

    // Find the decoder for the video stream
    AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL) {
        LOGD("Codec not found.");
        return -1; // Codec not found
    }

    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOGD("Could not open codec.");
        return -1; // Could not open codec
    }


    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOGD("Could not open codec.");
        return -1; // Could not open codec
    }


    //申请avpakcet，装解码前的数据
    AVPacket *packet = (AVPacket *) av_malloc(sizeof(AVPacket));
    // Allocate video frame
    AVFrame *pFrame = av_frame_alloc();

    //得到SwrContext ，进行重采样，具体参考http://blog.csdn.net/jammg/article/details/52688506
    SwrContext *swrContext = swr_alloc();

    //缓存区
    uint8_t *out_buffer = (uint8_t *) av_malloc(44100 * 2);
    //输出的声道布局（立体声）
    uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO;
    //输出采样位数 16位
    enum AVSampleFormat out_formart = AV_SAMPLE_FMT_S16;
    //输出的采样率必须与输入相同
    int out_sample_rate = pCodecCtx->sample_rate;
    //swr_alloc_set_opts将PCM源文件的采样格式转换为自己希望的采样格式
    swr_alloc_set_opts(swrContext, out_ch_layout, out_formart, out_sample_rate,
                       pCodecCtx->channel_layout, pCodecCtx->sample_fmt, pCodecCtx->sample_rate, 0,
                       NULL);
    swr_init(swrContext);

    // 获取通道数 2
    int out_channer_nb = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    // 反射得到Class类型
    jclass david_player = env->GetObjectClass(clazz);
    // 反射得到createAudio方法
    jmethodID createAudio = env->GetMethodID(david_player, "createTrack", "(II)V");
    // 反射调用createAudio
    env->CallVoidMethod(clazz, createAudio, 44100, out_channer_nb);
    jmethodID audio_write = env->GetMethodID(david_player, "playTrack", "([BI)V");
    int got_frame;
    while (av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index ==
            audio_stream_idx) {
            // 解码 mp3 编码格式frame----pcm frame
            avcodec_decode_audio4(pCodecCtx, pFrame, &got_frame, packet);
            if (got_frame) {
                LOGD("解码");
                swr_convert(swrContext, &out_buffer, 44100 * 2, (const uint8_t **) pFrame->data,
                            pFrame->nb_samples);
                // 缓冲区的大小
                int size = av_samples_get_buffer_size(NULL, out_channer_nb, pFrame->nb_samples,
                                                      AV_SAMPLE_FMT_S16, 1);
                jbyteArray audio_sample_array = env->NewByteArray(size);
                env->SetByteArrayRegion(audio_sample_array, 0, size, (const jbyte *) out_buffer);
                env->CallVoidMethod(clazz, audio_write, audio_sample_array, size);
                env->DeleteLocalRef(audio_sample_array);
            }
        }
    }




    // Free the YUV frame
    av_free(pFrame);
    swr_free(&swrContext);
    // Close the codecs
    avcodec_close(pCodecCtx);

    // Close the video file
    avformat_close_input(&pFormatCtx);

    return 0;
}


}

