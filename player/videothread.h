#ifndef VIDEOTHREAD_H
#define VIDEOTHREAD_H

#include <QObject>
struct AVPacket;
struct AVCodecParameters;
class XDecode;
#include <list>
#include <mutex>
#include <QThread>
#include "IVideoCall.h"
#include "decodethread.h"
#include "imagedisplay.h"
#include "demux.h"
struct AVCodecContext;
struct SwsContext;
class VideoThread : public DecodeThread
{
    Q_OBJECT

public:
    //解码pts，如果接收到的解码数据pts >= seekpts return true 并且显示画面
    virtual bool RepaintPts(AVPacket *pkt, long long seekpts);
    //打开，不管成功与否都清理
    virtual bool Open(AVCodecContext *para,AVCodecContext *para2,ImageDisplay *call,int width,int height,bool hard=true);
    void run();
    void SwsScale(AVFrame *frame);
    VideoThread();
    virtual ~VideoThread();
    //同步时间，由外部传入
    long long synpts = 0;

    void SetPause(bool isPause);
    bool isPause = false;
    Demux *demux=nullptr;

signals:
        //void sendData(unsigned char *rgb,int width,int height);
protected:
    ImageDisplay *call = 0;

    std::mutex vmux;
    SwsContext *pSwsCtx = nullptr;
    uint8_t *pRgbBuffer = nullptr;
    AVFrame *pAVFrameRGB;
    unsigned char *rgb=nullptr;
    bool m_hard = true;
    //QImage *image = ;

};

#endif // VIDEOTHREAD_H
