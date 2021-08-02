#ifndef DECODETHREAD_H
#define DECODETHREAD_H

#include <QObject>
#include <list>
#include <mutex>
#include <QThread>
struct AVPacket;
class Decode;
class DecodeThread : public QThread
{
public:
    DecodeThread();
    virtual ~DecodeThread();
    virtual bool Push(AVPacket *pkt);

    //清理队列
    virtual void Clear();

    //清理资源，停止线程
    virtual void Close();

    //取出一帧数据，并出栈，如果没有返回NULL
    virtual AVPacket *Pop();
    //最大队列
    int maxList = 100;
    bool isExit = false;
    std::list <AVPacket *> packs;

    Decode *decode = 0;
    std::mutex mux;
protected:




};

#endif // DECODETHREAD_H
