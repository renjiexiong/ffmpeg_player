#ifndef IMAGEDISPLAY_H
#define IMAGEDISPLAY_H

#include <QWidget>
#include <QOpenGLWidget>
#include <mutex>
class ImageDisplay :public QOpenGLWidget
{
    Q_OBJECT
public:
    explicit ImageDisplay(QWidget *parent = nullptr);
    void Repaint(QImage &&img,int size=0) ;

protected:
    void paintEvent(QPaintEvent *event);


private:
    unsigned char *rgb;
    QImage m_img;
    std::mutex mux;
};

#endif // IMAGEDISPLAY_H
