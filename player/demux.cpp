#include "demux.h"

#include <iostream>
#include <QDebug>
using namespace std;

enum AVPixelFormat Demux::hw_pix_fmt = AV_PIX_FMT_NONE;

extern "C"
{
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"

    #include "libavdevice/avdevice.h"
    #include <libavutil/pixdesc.h>
    #include <libavutil/opt.h>
    #include <libavutil/avassert.h>
    #include <libavutil/imgutils.h>
}
static double r2d(AVRational r)
{
    return r.den == 0 ? 0 : (double)r.num / (double)r.den;
}

bool Demux::HasAudioVideo(const char *file)
{
    AVFormatContext *ic = NULL;
    int re = avformat_open_input(
                &ic,
                file,
                0,  // 0表示自动选择解封器
                0 //参数设置，比如rtsp的延时时间
            );
    if(re<0)
    {
        return false;
    }
    int videoStream = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    int audioStream = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if(videoStream<0 || audioStream<0)
    {
        return false;
    }
    return true;


}

bool Demux::Open(const char *url)
{
    Close();
    hard = true;
    const AVCodec *videoCodec;
    //const AVCodec *audioAVCodec;
    //参数设置
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);
    av_dict_set(&opts, "max_delay", "500", 0);
    mux.lock();
    int re = avformat_open_input(
        &ic,
        url,
        0,  // 0表示自动选择解封器
        &opts //参数设置，比如rtsp的延时时间
    );
    if (re != 0)
    {
        mux.unlock();
        char buf[1024] = { 0 };
        av_strerror(re, buf, sizeof(buf) - 1);
        qDebug() << "open " << url << " failed! :" << buf << endl;
        mux.unlock();
        return false;
    }
    qDebug() << "open " << url << " success! " << endl;

    //获取流信息
    re = avformat_find_stream_info(ic, 0);

    //总时长 毫秒
    this->totalMs = ic->duration / (AV_TIME_BASE / 1000);
    //qDebug() << "totalMs = " << totalMs << endl;

    //打印视频流详细信息
    av_dump_format(ic, 0, url, 0);

    //获取视频流
    videoStream = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, &videoCodec, 0);
    if (videoStream < 0) {
        qDebug()<< "av_find_best_stream faliture";
        avformat_close_input(&ic);
        mux.unlock();
        return false;
    }
    AVStream *as = ic->streams[videoStream];
    width = as->codecpar->width;
    height = as->codecpar->height;

    qDebug() << "=======================================================" << endl;
    qDebug() << videoStream << "视频信息" << endl;
    qDebug() << "codec_id = " << as->codecpar->codec_id << endl;
    qDebug() << "format = " << as->codecpar->format << endl;
    qDebug() << "width=" << as->codecpar->width << endl;
    qDebug() << "height=" << as->codecpar->height << endl;
    //帧率 fps 分数转换
    qDebug() << "video fps = " << r2d(as->avg_frame_rate) << endl;

    qDebug() << "=======================================================" << endl;
    qDebug() << audioStream << "音频信息" << endl;
    audioStream = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audioStream < 0) {
        qDebug()<< "av_find_best_stream faliture";
        avformat_close_input(&ic);
        mux.unlock();
        return false;
    }
    as = ic->streams[audioStream];
    sampleRate = as->codecpar->sample_rate;
    channels = as->codecpar->channels;
    sampleSize = as->codecpar->sample_rate;

    qDebug() << "codec_id = " << as->codecpar->codec_id << endl;
    qDebug() << "format = " << as->codecpar->format << endl;
    qDebug() << "sample_rate = " << as->codecpar->sample_rate << endl;
    qDebug() << "channels = " << as->codecpar->channels << endl;
    qDebug() << "frame_size = " << as->codecpar->frame_size << endl;
    mux.unlock();

    vcodec =  configHardcodec(videoCodec);
    if(!vcodec)
    {
        vcodec = configSoftcodec();
    }
    if(!acodec)
    {
        acodec = configAudiocodec();
    }
    return true;
}


//清空读取缓存
void Demux::Clear()
{
    mux.lock();
    if (!ic)
    {
        mux.unlock();
        return ;
    }
    //清理读取缓冲
    avformat_flush(ic);
    mux.unlock();
}
void Demux::Close()
{
    mux.lock();
    if (!ic)
    {
        mux.unlock();
        return;
    }
    avformat_close_input(&ic);
    //媒体总时长（毫秒）
    totalMs = 0;
    mux.unlock();
}

//seek 位置 pos 0.0 ~1.0
bool Demux::Seek(double pos)
{
    mux.lock();
    if (!ic)
    {
        mux.unlock();
        return false;
    }
    //清理读取缓冲
    avformat_flush(ic);

    long long seekPos = 0;
    seekPos = ic->streams[videoStream]->duration * pos;
    int re = av_seek_frame(ic, videoStream, seekPos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
    mux.unlock();
    if (re < 0) return false;
    return true;
}

AVCodecContext *Demux::CopyVcodectex()
{
    mux.lock();
    if (!ic)
    {
        mux.unlock();
        return nullptr;
    }

    mux.unlock();
    return vcodec;
}


AVCodecContext *Demux::CopyAcodectex()
{
    mux.lock();
    if (!ic)
    {
        mux.unlock();
        return NULL;
    }

    mux.unlock();
    return acodec;
}

AVCodecContext *Demux::configSoftcodec()
{
    mux.lock();
    hard = true;
    AVCodecContext * tmp=0;

    if(!ic)
    {
        return nullptr;
    }
    const AVCodec *videoAVCodec = avcodec_find_decoder(ic->streams[videoStream]->codecpar->codec_id);
    if (!videoAVCodec)
    {
        cout << "can't find the codec id " << ic->streams[videoStream]->codecpar->codec_id;
        mux.unlock();
        return nullptr;
    }
    cout << "find the AVCodec " << ic->streams[videoStream]->codecpar->codec_id << endl;
    ///创建解码器上下文呢
    tmp = avcodec_alloc_context3(videoAVCodec);

    ///配置解码器上下文参数
    avcodec_parameters_to_context(tmp, ic->streams[videoStream]->codecpar);
    //八线程解码
    tmp->thread_count = 8;

    ///打开解码器上下文
    int re = avcodec_open2(tmp, 0, 0);
    if (re != 0)
    {
        char buf[1024] = { 0 };
        av_strerror(re, buf, sizeof(buf) - 1);
        cout << "avcodec_open2  failed! :" << buf << endl;
        mux.unlock();
        return nullptr;
    }
    mux.unlock();
    return tmp;
}

AVCodecContext *Demux::configHardcodec(const AVCodec* videoCodec)
{
    mux.lock();
    hard = true;
    AVCodecContext *tmpvcodec = 0;

    qDebug()<<"===================硬解码配置开始"<<endl;
    type = av_hwdevice_find_type_by_name("videotoolbox");
    qDebug()<<"type"<<type<<endl;
    if (type == AV_HWDEVICE_TYPE_NONE)
    {
        qDebug() << "Device type videotoolbox is not supported.\n";
        qDebug() <<"Available device types:";
        while((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
            qDebug() <<av_hwdevice_get_type_name(type);
        fprintf(stderr, "\n");
        mux.unlock();
        return nullptr;
    }
    for (int i = 0;; i++)
    {
        const AVCodecHWConfig *config = avcodec_get_hw_config(videoCodec, i);
        if (!config)
        {
            qDebug() <<"Decoder "<<videoCodec->name <<" does not support device type "<<av_hwdevice_get_type_name(type);
            mux.unlock();
            return nullptr;
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&config->device_type == type)
        {
            hw_pix_fmt = config->pix_fmt;
            break;
        }
    }
    qDebug()<<"hw_pix_fmt"<<hw_pix_fmt<<endl;

    tmpvcodec = avcodec_alloc_context3(videoCodec);
    if (tmpvcodec == NULL)
    {
        qDebug() <<"get pAVCodecCtx fail";
        //avformat_close_input(&ic);
        avcodec_close(tmpvcodec);
        avcodec_free_context(&tmpvcodec);
        mux.unlock();
        return nullptr;

    }
    qDebug()<<"tmpvcodec:"<<tmpvcodec<<endl;
    int re = avcodec_parameters_to_context(tmpvcodec,ic->streams[videoStream]->codecpar);
    if (re < 0)
    {
        qDebug() <<"avcodec_parameters_to_context fail";
        //avformat_close_input(&ic);
        avcodec_close(tmpvcodec);
        avcodec_free_context(&tmpvcodec);
        mux.unlock();
        return nullptr;
    }
     // 配置获取硬件加速器像素格式的函数；该函数实际上就是将AVCodec中AVHWCodecConfig中的pix_fmt返回
    //qDebug()<<"hw_pix_fmthw_pix_fmt"<<hw_pix_fmt;

    tmpvcodec->pix_fmt = hw_pix_fmt;
    //vcodec->get_format  = get_hw_format;

    re = hw_decoder_init(tmpvcodec, type);
    if (re < 0)
    {
        mux.unlock();
        return nullptr;
    }

    if (avcodec_open2(tmpvcodec, videoCodec, NULL) < 0)
    {
        qDebug()<<"avcodec_open2 fail";
        mux.unlock();
        return nullptr;
    }
    mux.unlock();
    qDebug()<<"===================硬解码配置结束"<<endl;
    return tmpvcodec;

}

AVCodecContext *Demux::configAudiocodec()
{
    AVCodecContext *tmpacodec = 0;
    ///音频解码器打开
    mux.lock();
    const AVCodec *audioAVCodec = avcodec_find_decoder(ic->streams[audioStream]->codecpar->codec_id);
    if (!audioAVCodec)
    {

        cout << "can't find the codec id " << ic->streams[audioStream]->codecpar->codec_id;
        mux.unlock();
        return nullptr;
    }
    cout << "find the AVCodec " << ic->streams[audioStream]->codecpar->codec_id << endl;
    ///创建解码器上下文呢
    tmpacodec = avcodec_alloc_context3(audioAVCodec);

    ///配置解码器上下文参数
    avcodec_parameters_to_context(tmpacodec, ic->streams[audioStream]->codecpar);
    //八线程解码
    tmpacodec->thread_count = 8;

    ///打开解码器上下文
    int re = avcodec_open2(tmpacodec, 0, 0);
    if (re != 0)
    {
        char buf[1024] = { 0 };
        av_strerror(re, buf, sizeof(buf) - 1);
        cout << "avcodec_open2  failed! :" << buf << endl;
        avcodec_close(tmpacodec);
        avcodec_free_context(&tmpacodec);

        mux.unlock();
        return nullptr;
    }
    mux.unlock();
    return tmpacodec;
}

bool Demux::IsAudio(AVPacket *pkt)
{
    if (!pkt) return false;
    if (pkt->stream_index == videoStream)
        return false;
    return true;

}

AVPacket *Demux::ReadVideo()
{
    mux.lock();
    if (!ic) //容错
    {
        mux.unlock();
        return 0;
    }
    mux.unlock();

    AVPacket *pkt = NULL;
    //防止阻塞
    for (int i = 0; i < 20; i++)
    {
        pkt = Read();
        if (!pkt)break;
        if (pkt->stream_index == videoStream)
        {
            break;
        }
        av_packet_free(&pkt);
    }
    return pkt;
}
//空间需要调用者释放 ，释放AVPacket对象空间，和数据空间 av_packet_free
AVPacket *Demux::Read()
{
    mux.lock();
    if (!ic) //容错
    {
        mux.unlock();
        return 0;
    }
    AVPacket *pkt = av_packet_alloc();
    //读取一帧，并分配空间
    int re = av_read_frame(ic, pkt);
    if (re != 0)
    {
        mux.unlock();
        av_packet_free(&pkt);
        return 0;
    }
    //pts转换为毫秒
    pkt->pts = pkt->pts*(1000 * (r2d(ic->streams[pkt->stream_index]->time_base)));
    pkt->dts = pkt->dts*(1000 * (r2d(ic->streams[pkt->stream_index]->time_base)));
    mux.unlock();
    //cout << pkt->pts << " "<<flush;
    return pkt;

}

enum AVPixelFormat Demux::get_hw_format(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt)
            return *p;
    }

    fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}

int Demux::hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type)
{
    int err = 0;
    //初始化硬件，打开硬件，绑定到具体硬件的指针函数上
    //创建硬件设备相关的上下文信息AVHWDeviceContext，包括分配内存资源、对硬件设备进行初始化
    if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type,
                                      NULL, NULL, 0)) < 0) {
        fprintf(stderr, "Failed to create specified HW device.\n");
        return err;
    }
 /* 需要把这个信息绑定到AVCodecContext
     * 如果使用软解码则默认有一个软解码的缓冲区(获取AVFrame的)，而硬解码则需要额外创建硬件解码的缓冲区
     *  这个缓冲区变量为hw_frames_ctx，不手动创建，则在调用avcodec_send_packet()函数内部自动创建一个
     *  但是必须手动赋值硬件解码缓冲区引用hw_device_ctx(它是一个AVBufferRef变量)
     */
    ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    return err;
}

Demux::Demux()
{
    static bool isFirst = true;
    static std::mutex dmux;
    dmux.lock();
    if (isFirst)
    {
        //初始化封装库
        //av_register_all();

        //初始化网络库 （可以打开rtsp rtmp http 协议的流媒体视频）
        avformat_network_init();
        isFirst = false;
    }
    dmux.unlock();
}


Demux::~Demux()
{
}



