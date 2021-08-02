#include "videothread.h"

#include "decode.h"
#include <iostream>
#include <QDebug>
#include <QImage>
#include "demux.h"
extern "C"
{
    #include "libswscale/swscale.h"
    #include <libavcodec/avcodec.h>
    #include <libavutil/imgutils.h>
}

using namespace std;
//打开，不管成功与否都清理
bool VideoThread::Open(AVCodecContext *para,AVCodecContext *para2, ImageDisplay *call,int width,int height,bool hard)
{
    if (!para)return false;
    Clear();

    vmux.lock();
    synpts = 0;
    m_hard = hard;
    //初始化显示窗口
    this->call = call;
//    if (call)
//    {
//        call->Init(width, height);
//    }
    vmux.unlock();
    int re = true;
//    if (!decode->Open(para))
//    {
//        cout << "video XDecode open failed!" << endl;
//        re = false;
//    }
    if (!decode->Open(para,para2))
    {
        cout << "video XDecode open failed!" << endl;
        re = false;
    }
    pAVFrameRGB = av_frame_alloc();
    int size = av_image_get_buffer_size(AVPixelFormat(AV_PIX_FMT_RGB32), width, height, 1);
    pRgbBuffer = (uint8_t *)(av_malloc(size));
    av_image_fill_arrays(pAVFrameRGB->data, pAVFrameRGB->linesize, pRgbBuffer, AV_PIX_FMT_RGB32,
                              width, height, 1);
    cout << "XAudioThread::Open :" << re << endl;
    return re;
}
void VideoThread::SetPause(bool isPause)
{
    vmux.lock();
    this->isPause = isPause;
    vmux.unlock();
}
void VideoThread::run()
{
    while (!isExit)
    {
        vmux.lock();
        if (this->isPause)
        {
            vmux.unlock();
            msleep(5);
            continue;
        }
        qDebug()<<__LINE__<<" video pop "<<packs.size() <<endl;
        qDebug()<<"synpts:"<<synpts<<endl;
        //音视频同步
        if ( synpts < decode->pts)
        {
            vmux.unlock();
            msleep(1);
            continue;
        }
        //qDebug()<<__LINE__<<" video pop "<<packs.size() <<endl;
        AVPacket *pkt = Pop();
        //qDebug()<<"video pop "<<packs.size() <<endl;


        bool re = decode->Send(pkt);
        if(!re)
        {
            vmux.unlock();
            msleep(1);
            continue;
        }

        //一次send 多次recv
        while (!isExit)
        {
            AVFrame * frame = decode->Recv();
            if (!frame)
            {

//                qDebug()<<__LINE__<<" decode->Recv() fail " <<endl;

                break;
            }
            //显示视频
//            if (call)
//            {
//                call->Repaint(frame);

//            }

            SwsScale(frame);
        }
        vmux.unlock();
    }
}

void VideoThread::SwsScale(AVFrame *frame)
{

    qDebug()<<"hard "<<m_hard<<endl;
    if(m_hard)
    {
        qDebug()<<"====硬解码转码开始";
        AVFrame *sw_frame = NULL;
        AVFrame *tmp_frame = NULL;
        if (!(sw_frame = av_frame_alloc())) {
            fprintf(stderr, "Can not alloc frame\n");
            return ;
        }
        if (frame->format == Demux::hw_pix_fmt)
        {

            if (av_hwframe_transfer_data(sw_frame, frame, 0) < 0)
            {
                fprintf(stderr, "Error transferring the data to system memory\n");
            }
            else
            {
                tmp_frame = sw_frame;
            }
        }
        else
        {
           tmp_frame = frame;
           qDebug()<<"frame img";
        }
        if(!pSwsCtx)
        {
          pSwsCtx = sws_getContext(tmp_frame->width, tmp_frame->height, (AVPixelFormat)tmp_frame->format,
                                               tmp_frame->width, tmp_frame->height, AV_PIX_FMT_RGB32,
                                               SWS_BILINEAR, NULL, NULL, NULL);
        }
        int ret = sws_scale(pSwsCtx, (uint8_t const * const *) tmp_frame->data, tmp_frame->linesize, 0,
        tmp_frame->height, pAVFrameRGB->data, pAVFrameRGB->linesize);


        QImage img((uint8_t *)pAVFrameRGB->data[0],frame->width,frame->height,QImage::Format_RGB32);
        call->Repaint(std::move(img),frame->width*frame->height);


        av_frame_free(&sw_frame);
        qDebug()<<"====硬解码转码完成";
    }

    else
    {
        qDebug()<<"====软解码转码完成";
        SwsContext *vctx = NULL;
        vctx = sws_getCachedContext(
            vctx,	//传NULL会新创建
            frame->width,frame->height,		//输入的宽高
            (AVPixelFormat)frame->format,	//输入格式 YUV420p
            frame->width, frame->height,	//输出的宽高
            AV_PIX_FMT_RGB32,				//输入格式RGBA
            SWS_BILINEAR,					//尺寸变化的算法
            0,0,0);
        //if(vctx)
            //cout << "像素格式尺寸转换上下文创建或者获取成功！" << endl;
        //else
        //	cout << "像素格式尺寸转换上下文创建或者获取失败！" << endl;
        if (vctx)
        {
            if (!rgb) rgb = new unsigned char[frame->width*frame->height * 4];
            uint8_t *data[2] = { 0 };
            data[0] = rgb;
            int lines[2] = { 0 };
            lines[0] = frame->width * 4;
            int re = sws_scale(vctx,
                frame->data,		//输入数据
                frame->linesize,	//输入行大小
                0,
                frame->height,		//输入高度
                data,				//输出数据和大小
                lines
            );
            {
                QImage img(data[0],frame->width,frame->height,QImage::Format_RGB32);
                call->Repaint(std::move(img),frame->width*frame->height);
            }

        }

    }
    qDebug()<<"====软解码转码完成";
    av_frame_free(&frame);






}

//解码pts，如果接收到的解码数据pts >= seekpts return true 并且显示画面
bool VideoThread::RepaintPts(AVPacket *pkt, long long seekpts)
{
    vmux.lock();
    bool re = decode->Send(pkt);
    if (!re)
    {
        vmux.unlock();
        return true; //表示结束解码
    }
    AVFrame *frame = decode->Recv();
    if (!frame)
    {
        vmux.unlock();
        return false;
    }
    //到达位置
    if (decode->pts >= seekpts)
    {
        if(call)
            //call->Repaint(frame);
            SwsScale(frame);
        vmux.unlock();
        return true;
    }
    XFreeFrame(&frame);
    vmux.unlock();
    //emit sendData(rgb,frame->width,frame->height);;
    return false;
}
VideoThread::VideoThread()
{
}


VideoThread::~VideoThread()
{
    av_frame_free(&pAVFrameRGB);
    av_free(pRgbBuffer);
}
