#include "slider.h"

Slider::Slider(QWidget *parent) : QSlider(parent)
{

}

void Slider::mousePressEvent(QMouseEvent *e)
{
    double pos = (double)e->pos().x() / (double)width();
    setValue(pos * this->maximum());
    //原有事件处理
    //QSlider::mousePressEvent(e);
    QSlider::sliderReleased();
}
