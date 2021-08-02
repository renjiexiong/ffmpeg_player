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
class VideoThread : public DecodeThread
{
public:
    //解码pts，如果接收到的解码数据pts >= seekpts return true 并且显示画面
    virtual bool RepaintPts(AVPacket *pkt, long long seekpts);
    //打开，不管成功与否都清理
    virtual bool Open(AVCodecParameters *para,IVideoCall *call,int width,int height);
    void run();
    VideoThread();
    virtual ~VideoThread();
    //同步时间，由外部传入
    long long synpts = 0;

    void SetPause(bool isPause);
    bool isPause = false;
protected:
    IVideoCall *call = 0;
    std::mutex vmux;

};

#endif // VIDEOTHREAD_H
