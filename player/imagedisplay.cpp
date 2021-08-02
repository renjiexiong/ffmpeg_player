#include "imagedisplay.h"
#include <QPainter>
#include <QDebug>
ImageDisplay::ImageDisplay(QWidget *parent) : QOpenGLWidget(parent)
{

}

void ImageDisplay::Repaint(QImage &&img, int size)
{
    mux.lock();
    m_img = img.copy();
    mux.unlock();
    update();
}


void ImageDisplay::paintEvent(QPaintEvent *event)
{

    QPainter p(this);
    mux.lock();
    if(m_img.isNull())
    {
        qDebug()<<"m_img is no data!"<<endl;
    }
    else
    {

        p.drawImage(this->rect(),m_img);

    }
    mux.unlock();
}
