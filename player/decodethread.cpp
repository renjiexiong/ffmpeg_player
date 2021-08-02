#include "decodethread.h"
#include "decode.h"
#include <QDebug>
void DecodeThread::Close()
{
    Clear();

    //等待线程退出
    isExit = true;
    wait();
    decode->Close();

    mux.lock();
    delete decode;
    decode = NULL;
    mux.unlock();
}
void DecodeThread::Clear()
{
    mux.lock();
    decode->Clear();
    while (!packs.empty())
    {
        AVPacket *pkt = packs.front();
        XFreePacket(&pkt);
        packs.pop_front();
    }

    mux.unlock();
}


//取出一帧数据，并出栈，如果没有返回NULL
AVPacket *DecodeThread::Pop()
{
    mux.lock();
    if (packs.empty())
    {
        mux.unlock();
        return nullptr;
    }
    AVPacket *pkt = packs.front();
    packs.pop_front();
    mux.unlock();
    return pkt;
}
bool DecodeThread::Push(AVPacket *pkt)
{
    mux.lock();
    if (packs.size() < maxList)
    {
        packs.push_back(pkt);
        qDebug()<<"packs size"<<packs.size();
        mux.unlock();
        return true;
    }
    mux.unlock();
    return false;
}


DecodeThread::DecodeThread()
{
    //打开解码器
    if (!decode) decode = new Decode();
}


DecodeThread::~DecodeThread()
{	//等待线程退出
    isExit = true;
    wait();
}
