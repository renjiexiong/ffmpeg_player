#ifndef SLIDER_H
#define SLIDER_H

#include <QObject>
#include <QMouseEvent>
#include <QSlider>

class Slider : public QSlider
{
    Q_OBJECT
public:
    explicit Slider(QWidget *parent = nullptr);
    void mousePressEvent(QMouseEvent *e);
signals:

};

#endif // SLIDER_H
