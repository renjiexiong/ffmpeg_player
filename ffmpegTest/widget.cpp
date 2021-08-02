#include "widget.h"
#include "ui_widget.h"

#include <QFileDialog>
#include <QDebug>
#include "demuxthread.h"
#include <QMessageBox>
static DemuxThread dt;
Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    dt.Start();
    //char *url = "rtmp://live.hkstv.hk.lxdns.com/live/hks";
    startTimer(40);

    if (!dt.Open("C:/Users/Admin/Desktop/music/1.mp4", ui->video))
    {
        QMessageBox::information(0, "error", "open file failed!");
        return;
    }
}

Widget::~Widget()
{
    delete ui;
}


void Widget::on_pushButton_clicked()
{
    bool isPause = !dt.isPause;
    //SetPause(isPause);
    dt.SetPause(isPause);
}


void Widget::timerEvent(QTimerEvent *e)
{
    if (isSliderPress)return;
    long long total = dt.totalMs;
    if (total > 0)
    {
        double pos = (double)dt.pts / (double)total;
        int v = ui->horizontalSlider->maximum() * pos;
        ui->horizontalSlider->setValue(v);
    }
}

void Widget::on_horizontalSlider_sliderReleased()
{
    isSliderPress = false;
    double pos = 0.0;
    pos = (double)ui->horizontalSlider->value() / (double)ui->horizontalSlider->maximum();
    dt.Seek(pos);
}

void Widget::on_horizontalSlider_sliderPressed()
{
    isSliderPress = true;
}
