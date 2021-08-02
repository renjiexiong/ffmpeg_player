#ifndef DEMUXTHREAD_H
#define DEMUXTHREAD_H

#include <QObject>
#include <QThread>
#include "IVideoCall.h"
#include "imagedisplay.h"
#include <mutex>
#include <QObject>
class Demux;
class VideoThread;
class AudioThread;
class DemuxThread : public QThread
{
    Q_OBJECT

public:
    explicit DemuxThread(QObject* parent = nullptr);
    ~DemuxThread();
    //创建对象并打开
    bool Open(const char *url, ImageDisplay *call);
    void setVideoCodec();

    //启动所有线程
    void Start();

    //关闭线程清理资源
    void Close();
    void Clear();

    void Seek(double pos);

    void setVolume(float volume);
    float getVolume();
    void run();


    bool isExit = false;
    long long pts = 0;
    long long totalMs = 0;
    void SetPause(bool isPause);
    bool isPause = false;

    VideoThread *vt = 0;
signals:
    void sendEnd(void);
protected:
    std::mutex mux;
    Demux *demux = 0;

    AudioThread *at = 0;

};

#endif // DEMUXTHREAD_H
