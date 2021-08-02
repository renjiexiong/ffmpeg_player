#ifndef RESAMPLE_H
#define RESAMPLE_H

struct AVCodecContext;
struct AVFrame;
struct SwrContext;
#include <mutex>
class Resample
{
public:
    //输出参数和输入参数一致除了采样格式，输出为S16 ,会释放para
    virtual bool Open(AVCodecContext *para,int channel, bool isClearPara = false);
    virtual void Close();

    //返回重采样后大小,不管成功与否都释放indata空间
    virtual int RResample(AVFrame *indata,unsigned char *data);
    Resample();
    ~Resample();

    //AV_SAMPLE_FMT_S16
    int outFormat = 1;
    int m_channel = 1;
protected:
    std::mutex mux;
    SwrContext *actx = 0;
};

#endif // RESAMPLE_H
