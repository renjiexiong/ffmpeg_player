#ifndef AUDIOTHREAD_H
#define AUDIOTHREAD_H

#include <QThread>
#include <mutex>
#include <list>
struct AVCodecParameters;
class AudioPlay;
class Resample;
#include "decodethread.h"
struct AVCodecContext;
class AudioThread :public DecodeThread
{
    Q_OBJECT
public:
    //当前音频播放的pts
    long long pts = 0;
    //打开，不管成功与否都清理
    virtual bool Open(AVCodecContext *para,int sampleRate,int channels);

    //停止线程，清理资源
    virtual void Close();

    virtual void Clear();
    void setVolume(float volume);
    float GetVolume();
    void run();
    AudioThread();
    virtual ~AudioThread();
    void SetPause(bool isPause);
    bool isPause = false;
protected:
    std::mutex amux;
    AudioPlay *ap = 0;
    Resample *res = 0;
};

#endif // AUDIOTHREAD_H
