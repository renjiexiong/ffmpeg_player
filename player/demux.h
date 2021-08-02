#ifndef DEMUX_H
#define DEMUX_H
#include <mutex>
struct AVFormatContext;
struct AVPacket;
struct AVCodecParameters;
struct AVCodec;

struct AVCodecContext;
extern "C"
{
    #include "libavutil/pixfmt.h"
    #include <libavutil/hwcontext.h>
}
class Demux
{
public:

    static bool HasAudioVideo(const char * file);
    //打开媒体文件，或者流媒体 rtmp http rstp
    virtual bool Open(const char *url);

    //空间需要调用者释放 ，释放AVPacket对象空间，和数据空间 av_packet_free
    virtual AVPacket *Read();
    //只读视频，音频丢弃空间释放
    virtual AVPacket *ReadVideo();

    virtual bool IsAudio(AVPacket *pkt);


    virtual AVCodecContext *CopyVcodectex();
    virtual AVCodecContext *CopyAcodectex();

    AVCodecContext *configSoftcodec();
    AVCodecContext *configHardcodec(const AVCodec* videoCodec );

    AVCodecContext *configAudiocodec();
    //seek 位置 pos 0.0 ~1.0
    virtual bool Seek(double pos);

    //清空读取缓存
    virtual void Clear();
    virtual void Close();


    Demux();
    virtual ~Demux();

    static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
                                            const enum AVPixelFormat *pix_fmts);
    int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type);



    //媒体总时长（毫秒）
    int totalMs = 0;
    int width = 0;
    int height = 0;

    int sampleRate = 44100;
    int sampleSize = 16;
    int channels = 2;
    static enum AVPixelFormat hw_pix_fmt;
    bool hard = true;  //是否可以硬解码

    AVFormatContext *ic = nullptr;
protected:
    std::mutex mux;
    //解封装上下文

    //音视频索引，读取时区分音视频
    int videoStream = 0;
    int audioStream = 1;

    enum AVHWDeviceType type;
    AVBufferRef *hw_device_ctx = NULL;

    AVCodecContext *vcodec = 0;
    AVCodecContext *acodec = 0;


};

#endif // DEMUX_H
