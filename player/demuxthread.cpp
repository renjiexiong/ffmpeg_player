#include "demuxthread.h"

#include "demux.h"
#include "videothread.h"
#include "audiothread.h"
#include <iostream>
#include <QDebug>
extern "C"
{
#include <libavformat/avformat.h>

}
#include "decode.h"

using namespace std;

DemuxThread::DemuxThread(QObject *parent): QThread(parent)
{

}

void DemuxThread::Clear()
{
    //mux.lock();
    if (demux)demux->Clear();
    if (vt) vt->Clear();
    if (at) at->Clear();
    //mux.unlock();
}

void DemuxThread::Seek(double pos)
{
    //清理缓存
    Clear();
    mux.lock();
    //bool status = this->isPause;
    mux.unlock();
    //暂停
    SetPause(true);

    mux.lock();
    if (demux)
        demux->Seek(pos);
    //实际要显示的位置pts
    long long seekPts = pos*demux->totalMs;
    while (!isExit)
    {
        AVPacket *pkt = demux->ReadVideo();
        if (!pkt) break;
        //如果解码到seekPts
        if (vt->RepaintPts(pkt, seekPts))
        {
            this->pts = seekPts;
            break;
        }
        //bool re = vt->decode->Send(pkt);
        //if (!re) break;
        //AVFrame *frame = vt->decode->Recv();
        //if (!frame) continue;
        ////到达位置
        //if (frame->pts >= seekPts)
        //{
        //	this->pts = frame->pts;
        //	vt->call->Repaint(frame);
        //	break;
        //}
        //av_frame_free(&frame);
    }

    mux.unlock();

    //seek是非暂停状态
    SetPause(false);
}

void DemuxThread::setVolume(float volume)
{
    at->setVolume(volume);
}

float DemuxThread::getVolume()
{
     return at->GetVolume();
}

void DemuxThread::SetPause(bool isPause)
{
    mux.lock();
    this->isPause = isPause;
    if (at) at->SetPause(isPause);
    if (vt) vt->SetPause(isPause);
    mux.unlock();
}

void DemuxThread::run()
{
    while (!isExit)
    {
        mux.lock();
        if (isPause)
        {
            mux.unlock();
            msleep(5);
        }
        if (!demux)
        {
            mux.unlock();
            Seek(0);
            continue;
        }

        //音视频同步
        if (vt && at)
        {
            pts = at->pts;
            vt->synpts = at->pts;
        }

        AVPacket *pkt = demux->Read();
        if (!pkt)
        {

            vt->synpts = LONG_LONG_MAX;
            mux.unlock();
            while(!vt->packs.empty() || !at->packs.empty())
            {
                pts = at->pts;
                msleep(5);
            }


            emit sendEnd();
            Seek(0);
            continue;
        }

        //判断数据是音频
        if (demux->IsAudio(pkt))
        {
            //while (at->IsFull())
            //{
            //	vt->synpts = at->pts;
            //}
            if (!pkt)
            {
                return;
            }
            while (!isExit)
            {
                if(at &&at->Push(pkt))
                {
                     break;
                }
                vt->synpts = at->pts;
                msleep(1);
            }

            qDebug()<<"audio push "<<at->packs.size() <<endl;
        }
        else //视频
        {
            //while (vt->IsFull())
            //{
            //	vt->synpts = at->pts;
            //}
            if (!pkt)
            {
                qDebug()<<__LINE__<<" pkt is empty " <<endl;
                return;
            }
            while (!isExit)
            {
                if (vt && vt->Push(pkt))
                {
                    break;
                }
                vt->synpts = at->pts;
                msleep(1);

            }
            qDebug()<<"video push "<<isExit<<!pkt <<vt->packs.size() <<endl;


        }
        mux.unlock();
        msleep(1);
    }
}


bool DemuxThread::Open(const char *url, ImageDisplay *call)
{
    if (url == 0 || url[0] == '\0')
        return false;

    mux.lock();
    if (!demux) demux = new Demux();
    if (!vt) vt = new VideoThread();
    if (!at) at = new AudioThread();

    //打开解封装
    bool re = demux->Open(url);
    if (!re)
    {
        Close();
        mux.unlock();
        cout << "demux->Open(url) failed!" << endl;
        return false;
    }
    //打开视频解码器和处理线程
    if (!vt->Open(demux->CopyVcodectex(),demux->configSoftcodec() ,call, demux->width, demux->height,demux->hard))
    {

        cout << "vt->Open failed!" << endl;
        Close();
        mux.unlock();
        return false;

    }
    //打开音频解码器和处理线程
    if (!at->Open(demux->CopyAcodectex(), demux->sampleRate, demux->channels))
    {

        cout << "at->Open failed!" << endl;
        Close();
        mux.unlock();
        return false;
    }
    totalMs = demux->totalMs;
    mux.unlock();

    vt->demux = demux;
    cout << "XDemuxThread::Open " << re << endl;
    return re;
}

void DemuxThread::setVideoCodec()
{
    if(!vt)
    vt->decode->codec = demux->configSoftcodec();
}

//关闭线程清理资源
void DemuxThread::Close()
{
    isExit = true;

    if (vt) vt->Close();
    if (at) at->Close();
    wait();
    mux.lock();
    delete vt;
    delete at;
    vt = NULL;
    at = NULL;

    mux.unlock();
}
//启动所有线程
void DemuxThread::Start()
{
    mux.lock();
    if (!demux) demux = new Demux();
    if (!vt) vt = new VideoThread();
    if (!at) at = new AudioThread();
    //启动当前线程
    QThread::start();
    if (vt)vt->start();
    if (at)at->start();
    mux.unlock();
}

DemuxThread::~DemuxThread()
{
    isExit = true;
    wait();
}
