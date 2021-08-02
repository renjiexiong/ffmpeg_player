#include "decode.h"
#include <iostream>
#include <QDebug>
extern "C"
{
#include<libavcodec/avcodec.h>
}
using namespace std;
void XFreePacket(AVPacket **pkt)
{
    if (!pkt || !(*pkt))return;
    av_packet_free(pkt);
}
void XFreeFrame(AVFrame **frame)
{
    if (!frame || !(*frame))return;
    av_frame_free(frame);
}
void Decode::Close()
{
    mux.lock();
    if (codec)
    {
        avcodec_close(codec);
        avcodec_free_context(&codec);
    }
    pts = 0;
    mux.unlock();
}

void Decode::Clear()
{
    mux.lock();
    pts = 0;
    //清理解码缓冲
    if (codec)
        avcodec_flush_buffers(codec);
    mux.unlock();
}

//打开解码器
bool Decode::Open(AVCodecContext *para)
{

    if (!para) return false;
    Close();
    //////////////////////////////////////////////////////////
    codec = para;
    return true;
}

bool Decode::Open(AVCodecContext *para, AVCodecContext *para2)
{

    if (!para  ) return false;
    Close();

    codec = para;
    if (para2)
        videoSoftCodec = para2;
    return true;
}
//发送到解码线程，不管成功与否都释放pkt空间（对象和媒体内容）
bool Decode::Send(AVPacket *pkt)
{
    //容错处理
    if (!pkt || pkt->size <= 0 || !pkt->data)return false;
    mux.lock();
    if (!codec)
    {        

        mux.unlock();
        return false;
    }

    int re = avcodec_send_packet(codec, pkt);
//    if (re != 0)
//    {
//        if( videoSoftCodec!=nullptr && codec!=videoSoftCodec)
//        {
//            Close();
//            codec = videoSoftCodec;
//            if( avcodec_send_packet(codec, pkt)!=0)
//            {
//                mux.unlock();
//                av_packet_free(&pkt);
//                return false;
//            }
//        }
//    }

    mux.unlock();
    av_packet_free(&pkt);
    if (re != 0)return false;
    return true;
}

//获取解码数据，一次send可能需要多次Recv，获取缓冲中的数据Send NULL在Recv多次
//每次复制一份，由调用者释放 av_frame_free
AVFrame* Decode::Recv()
{
    mux.lock();
    if (!codec)
    {
        mux.unlock();
        return NULL;
    }
    AVFrame *frame = av_frame_alloc();
    int re = avcodec_receive_frame(codec, frame);
    mux.unlock();
    if (re != 0)
    {
        av_frame_free(&frame);
        return NULL;
    }
    //cout << "["<<frame->linesize[0] << "] " << flush;
    qDebug()<<"avcodec_receive_frame gegegegege"<<re;
    pts = frame->pts;
    return frame;
}

Decode::Decode()
{
}


Decode::~Decode()
{
}


