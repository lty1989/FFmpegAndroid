
#include <iostream>

using namespace std;

#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

extern "C" {
#include "libavutil/time.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libswresample/swresample.h"
//};


#define  LOG_TAG    "ffmpegdemo"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)


int avError(int errNum) {
    char *buf;
    //获取错误信息
    LOGD("avError %d", errNum);
    av_strerror(errNum, buf, sizeof(buf));
    LOGD("%s", buf);
    return -1;
}


JNIEXPORT jint JNICALL
Java_com_ffmpegdemo_MainActivity_play
        (JNIEnv *env, jclass clazz, jobject surface) {
    LOGD("play");

    // sd卡中的视频文件地址,可自行修改或者通过jni传入
    //char *file_name = "/storage/emulated/0/test.mp4";
    char *file_name = "/storage/emulated/0/test.mp4";

    av_register_all();

    AVFormatContext *pFormatCtx = avformat_alloc_context();

    // Open video file
    if (avformat_open_input(&pFormatCtx, file_name, NULL, NULL) != 0) {

        LOGD("Couldn't open file:%s\n", file_name);
        return -1; // Couldn't open file
    }

    // Retrieve stream information
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOGD("Couldn't find stream information.");
        return -1;
    }

    // Find the first video stream
    int videoStream = -1, i;
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO
            && videoStream < 0) {
            videoStream = i;
        }
    }
    if (videoStream == -1) {
        LOGD("Didn't find a video stream.");
        return -1; // Didn't find a video stream
    }

    // Get a pointer to the codec context for the video stream
    AVCodecContext *pCodecCtx = pFormatCtx->streams[videoStream]->codec;

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

    // 获取native window
    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);

    // 获取视频宽高
    int videoWidth = pCodecCtx->width;
    int videoHeight = pCodecCtx->height;

    // 设置native window的buffer大小,可自动拉伸
    ANativeWindow_setBuffersGeometry(nativeWindow, videoWidth, videoHeight,
                                     WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer windowBuffer;

    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOGD("Could not open codec.");
        return -1; // Could not open codec
    }

    // Allocate video frame
    AVFrame *pFrame = av_frame_alloc();

    // 用于渲染
    AVFrame *pFrameRGBA = av_frame_alloc();
    if (pFrameRGBA == NULL || pFrame == NULL) {
        LOGD("Could not allocate video frame.");
        return -1;
    }

    // Determine required buffer size and allocate buffer
    // buffer中数据就是用于渲染的,且格式为RGBA
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, pCodecCtx->width, pCodecCtx->height,
                                            1);
    uint8_t *buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(pFrameRGBA->data, pFrameRGBA->linesize, buffer, AV_PIX_FMT_RGBA,
                         pCodecCtx->width, pCodecCtx->height, 1);

    // 由于解码出来的帧格式不是RGBA的,在渲染之前需要进行格式转换
    struct SwsContext *sws_ctx = sws_getContext(pCodecCtx->width,
                                                pCodecCtx->height,
                                                pCodecCtx->pix_fmt,
                                                pCodecCtx->width,
                                                pCodecCtx->height,
                                                AV_PIX_FMT_RGBA,
                                                SWS_BILINEAR,
                                                NULL,
                                                NULL,
                                                NULL);

    int frameFinished;
    AVPacket packet;
    while (av_read_frame(pFormatCtx, &packet) >= 0) {
        // Is this a packet from the video stream?
        if (packet.stream_index == videoStream) {

            // Decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

            // 并不是decode一次就可解码出一帧
            if (frameFinished) {

                // lock native window buffer
                ANativeWindow_lock(nativeWindow, &windowBuffer, 0);

                // 格式转换
                sws_scale(sws_ctx, (uint8_t const *const *) pFrame->data,
                          pFrame->linesize, 0, pCodecCtx->height,
                          pFrameRGBA->data, pFrameRGBA->linesize);

                // 获取stride
                uint8_t *dst = (uint8_t *) windowBuffer.bits;
                int dstStride = windowBuffer.stride * 4;
                uint8_t *src = (pFrameRGBA->data[0]);
                int srcStride = pFrameRGBA->linesize[0];

                // 由于window的stride和帧的stride不同,因此需要逐行复制
                int h;
                for (h = 0; h < videoHeight; h++) {
                    memcpy(dst + h * dstStride, src + h * srcStride, srcStride);
                }

                ANativeWindow_unlockAndPost(nativeWindow);
            }

        }
        av_packet_unref(&packet);
    }

    av_free(buffer);
    av_free(pFrameRGBA);

    // Free the YUV frame
    av_free(pFrame);

    // Close the codecs
    avcodec_close(pCodecCtx);

    // Close the video file
    avformat_close_input(&pFormatCtx);
    return 0;
}


JNIEXPORT jint JNICALL
Java_com_ffmpegdemo_AudioTrackActivity_play
        (JNIEnv *env, jclass clazz) {
    LOGD("play");
    // sd卡中的视频文件地址,可自行修改或者通过jni传入
    char *file_name = "/storage/emulated/0/123.mp4";

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

}


JNIEXPORT jstring JNICALL
Java_com_ffmpegdemo_FFmpegHandle_getAvcodecConfiguration(JNIEnv *env, jobject instance) {

    // TODO


    return env->NewStringUTF("sss");
}

static double r2d(AVRational r) {
    return r.num == 0 || r.den == 0 ? 0. : (double) r.num / (double) r.den;
}

JNIEXPORT jint JNICALL
Java_com_ffmpegdemo_FFmpegHandle_pushRtmpFile(JNIEnv *env, jobject instance, jstring path_) {
    const char *path = env->GetStringUTFChars(path_, 0);
    //所有代码执行之前要调用av_register_all和avformat_network_init //初始化所有的封装和解封装 flv mp4 mp3 mov。不包含编码和解码
    av_register_all();
    //初始化网络库
    avformat_network_init();
    //使用的相对路径，执行文件在bin目录下。test.mp4放到bin目录下即可 const char *inUrl = "test.flv";
    // 输出的地址  172.16.10.46:1935/testlive/1
    const char *outUrl = "rtmp://172.16.10.46:1935/testlive/1";

    int videoindex = -1;

    AVFormatContext *pFormatCtx = avformat_alloc_context();
    // Open video file
    if (avformat_open_input(&pFormatCtx, path, NULL, NULL) != 0) {

        LOGD("Couldn't open file:%s\n", path);
        return -1; // Couldn't open file
    }

    // Retrieve stream information
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOGD("Couldn't find stream information.");
        return -1;
    }

    //打印视频视频信息
    //0打印所有  inUrl 打印时候显示，
    av_dump_format(pFormatCtx, 0, path, 0);



    ////////////////// 输出流处理部分 //////////////////////
    AVFormatContext *octx = NULL;
    AVOutputFormat *ofmt = NULL;
    //如果是输入文件 flv可以不传，可以从文件中判断。如果是流则必须传
    // 创建输出上下文
    if (avformat_alloc_output_context2(&octx, NULL, "flv", outUrl) < 0) {
        LOGD("Couldn't avformat_alloc_output_context2");
        return -1;
    }

    ofmt = octx->oformat;

    //配置输出流
    //AVIOcontext *pb
    // IO上下文
    //AVStream **streams 指针数组，存放多个输出流 视频音频字幕流
    // int nb_streams;
    // duration ,bit_rate
    // AVStream
    // AVRational time_base
    // AVCodecParameters *codecpar 音视频参数
    // AVCodecContext *codec
    // 遍历输入的AVStream
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        //创建一个新的流到octx中
        //获取输入视频流
        AVStream *in_stream = pFormatCtx->streams[i];
        AVStream *out = avformat_new_stream(octx, pFormatCtx->streams[i]->codec->codec);
        if (!out) {
            LOGD("未能成功添加音视频流\\n");
            return -1;
        }
        //复制配置信息 用于mp4 过时的方法
        //ret = avcodec_copy_context(out->codec, ictx->streams[i]->codec);
        //avcodec_parameters_copy(out->codecpar, in_stream->codecpar);
        if (avcodec_parameters_copy(out->codecpar, in_stream->codecpar) < 0) {
            return -1;
        }
        out->codecpar->codec_tag = 0;
        out->codec->codec_tag = 0;
        if (octx->oformat->flags & AVFMT_GLOBALHEADER) {
            out->codec->flags = out->codec->flags | CODEC_FLAG_GLOBAL_HEADER;
        }
    }

    //输入流数据的数量循环
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoindex = i;
            break;
        }
    }


    av_dump_format(octx, 0, outUrl, 1);


    int ret;
    // 准备推流
    //打开IO

    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&octx->pb, outUrl, AVIO_FLAG_WRITE);
        if (ret < 0) {
            avError(ret);
        }
    }

    //写入头部信息
    ret = avformat_write_header(octx, 0);
    if (ret < 0) {
        avError(ret);
    }
    LOGD("avformat_write_header Success!");



    //推流每一帧数据
    // int64_t pts [ pts*(num/den) 第几秒显示]
    // int64_t dts 解码时间 [P帧(相对于上一帧的变化) I帧(关键帧，完整的数据) B帧(上一帧和下一帧的变化)] 有了B帧压缩率更高。
    // uint8_t *data
    // int size
    // int stream_index
    // int flag
    AVPacket avPacket;
    //获取当前的时间戳  微妙
    long startTime = av_gettime();
    long frame_index = 0;
    while (1) {
        //输入输出视频流
        AVStream *in_stream, *out_stream;
        //获取解码前数据
        ret = av_read_frame(pFormatCtx, &avPacket);
        if (ret < 0) {
            LOGD("av_read_frame error,code = %d", ret);
            break;
        }
        /* PTS（Presentation Time Stamp）显示播放时间 DTS（Decoding Time Stamp）解码时间 */
        //没有显示时间（比如未解码的 H.264 ）
        if (avPacket.pts == AV_NOPTS_VALUE) {
            //AVRational time_base：时基。通过该值可以把PTS，DTS转化为真正的时间。
            AVRational time_base1 = pFormatCtx->streams[videoindex]->time_base;
            //计算两帧之间的时间 /* r_frame_rate 基流帧速率 （不是太懂） av_q2d 转化为double类型 */
            int64_t calc_duration = (double) AV_TIME_BASE /
                                    av_q2d(pFormatCtx->streams[videoindex]->r_frame_rate);
            // 配置参数
            avPacket.pts = (double) (frame_index * calc_duration) /
                           (double) (av_q2d(time_base1) * AV_TIME_BASE);
            avPacket.dts = avPacket.pts;
            avPacket.duration =
                    (double) calc_duration / (double) (av_q2d(time_base1) * AV_TIME_BASE);
        }
        //延时
        if (avPacket.stream_index == videoindex) {
            AVRational time_base = pFormatCtx->streams[videoindex]->time_base;
            AVRational time_base_q = {1, AV_TIME_BASE};
            //计算视频播放时间
            int64_t pts_time = av_rescale_q(avPacket.dts, time_base, time_base_q);
            //计算实际视频的播放时间
            int64_t now_time = av_gettime() - startTime;
            AVRational avr = pFormatCtx->streams[videoindex]->time_base;
            cout << avr.num << " " << avr.den << " " << avPacket.dts << " " << avPacket.pts
                 << " "
                 << pts_time << endl;
            if (pts_time > now_time) {
                //睡眠一段时间（目的是让当前视频记录的播放时间与实际时间同步）
                av_usleep((unsigned int) (pts_time - now_time));
            }
        }
        in_stream = pFormatCtx->streams[avPacket.stream_index];
        out_stream = octx->streams[avPacket.stream_index];
        //计算延时后，重新指定时间戳
        /** avPacket.pts = av_rescale_q_rnd(avPacket.pts, in_stream->time_base,
                                         out_stream->time_base,
                                         (AVRounding) (AV_ROUND_NEAR_INF |
                                                       AV_ROUND_PASS_MINMAX));
         avPacket.dts = av_rescale_q_rnd(avPacket.dts, in_stream->time_base,
                                         out_stream->time_base,
                                         (AVRounding) (AV_ROUND_NEAR_INF |
                                                       AV_ROUND_PASS_MINMAX));
         avPacket.duration = (int) av_rescale_q(avPacket.duration, in_stream->time_base,
                                                out_stream->time_base);
         //字节流的位置，-1 表示不知道字节流位置
         avPacket.pos = -1;
         */
        avPacket.pts = av_rescale_q_rnd(avPacket.pts, in_stream->time_base, out_stream->time_base,
                                        (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        avPacket.dts = av_rescale_q_rnd(avPacket.dts, in_stream->time_base, out_stream->time_base,
                                        (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        avPacket.duration = av_rescale_q(avPacket.duration, in_stream->time_base,
                                         out_stream->time_base);
        avPacket.pos = -1;

        if (avPacket.stream_index == videoindex) {
            LOGD("Send %8d video frames to output URL\n", frame_index);
            frame_index++;
        }

        //向输出上下文发送（向地址推送）
        ret = av_interleaved_write_frame(octx, &avPacket);
        if (ret < 0) {
            LOGD("发送数据包出错\n");
            break;
        }
        //释放
        av_packet_unref(&avPacket);
    }
    //写文件尾
    av_write_trailer(octx);
    ret = 0;
    LOGD("finish!");
    //关闭输出上下文，这个很关键。
    if (octx && !(ofmt->flags & AVFMT_NOFILE))
        avio_close(octx->pb);
    avio_close(octx->pb);

    //释放输出封装上下文
    if (octx != NULL)
        avformat_free_context(octx);

    //关闭输入上下文
    if (pFormatCtx != NULL)
        avformat_close_input(&pFormatCtx);
    octx = NULL;
    pFormatCtx = NULL;

    env->ReleaseStringUTFChars(path_, path);
    return ret;
}


JNIEXPORT jint JNICALL
Java_com_ffmpegdemo_FFmpegHandle_setCallback(JNIEnv *env, jobject instance,
                                             jobject pushCallback) {

    // TODO

}

}




