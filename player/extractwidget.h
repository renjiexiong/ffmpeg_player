#ifndef EXTRACTWIDGET_H
#define EXTRACTWIDGET_H

#include <QWidget>
#include "clippingwidget.h"
#if     defined(Q_OS_WIN)


#endif

#include <memory>
#include <vector>
#include <utility>
#include <set>
#include <mutex>

class DemuxThread;
namespace Ui {
class ExtractWidget;
}
class ExtractWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ExtractWidget(QWidget *parent = nullptr);
    ~ExtractWidget();

    bool isWorking();
    void updateTargetPath();
    void SetClipType(const ClipType type = ClipType::AUDIO);
    void addDrap(QList<QUrl> &urls);

    bool isNeedShowUndo();

    void SetShowDrag(bool show);

    bool isLoading() const
    {
        return is_loading;
    }
    ClipType getClipType()const{
        return m_eClipType;
    }
    void fsdssf();


    bool getIs_currentpage() const;
    void setIs_currentpage(bool value);

signals:
    void purchase();
    void show_undo(int count);
    void hide_undo();

    void show_shadow();
    void hide_shadow();

    void show_notsupport();
signals:
    void asyn_add_start();
    void asyn_add_item(const MediaProfile);
    void asyn_add_finished(bool);
    void show_notexist(QString file);
public slots:
    void asyn_add_files(const QStringList filelist);
    void asyn_add_folders(const QStringList filelist);
    void asyn_add_drop(const QList<QUrl> filelist);

    void asyn_add_start_impl();
    void asyn_add_item_impl(const MediaProfile);
    void asyn_add_finished_impl(bool);
protected:
    void showEvent(QShowEvent *event);
    void timerEvent(QTimerEvent *event);

private slots:
    void on_addFileButton_clicked();

    void on_addFolderButton_clicked();

    void on_changeFolderButton_clicked();

    void on_openFolderButton_clicked();

    void on_startStaticButton_clicked();

    void on_inputList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);

    void on_clearinButton_clicked();

    void on_clearoutButton_clicked();

    void on_timeEdit_timeChanged(const QTime &time);

    void on_tabWidget_currentChanged(int index);

    void on_outputButton_clicked();

private slots:
    void slot_progessRangeSlider_valueChange(int value);
    void slot_progessRangeSlider_sliderPressed();
    void slot_progessRangeSlider_sliderReleased();
    void slot_lowerValueChanged(int value);
    void slot_upperValueChanged(int value);
    void slot_dragValueChange();

    void on_checkBox_clicked(bool checked);

    void on_countSelect_currentIndexChanged(int index);

    void buttonPlay_clicked(bool);

    bool startPlay(QString path);
    void stopPlay();
    void on_volumeButton_clicked();
    void update_volume(int value);
    void on_stopButton_clicked();

    void on_playCheck_clicked(bool checked);

    void on_handStartTime_2_timeChanged(const QTime &time);

    void on_handEndTime_2_timeChanged(const QTime &time);

    void on_handStartTime_timeChanged(const QTime &time);

    void on_handEndTime_timeChanged(const QTime &time);

    void on_addButton_clicked();

    void on_lineEdit_2_returnPressed();

    void on_lineEdit_returnPressed();

    void on_btnAudition_clicked();

    void on_rbtnStart_clicked();

    void on_rbtnend_clicked();

    void closeTimeEditTip();
public slots:
    void item_start_clicked();
    void item_succ(bool);
    void item_remove();

    void input_item_remove();

    void undo_impl();
    void undo_cancel_impl();

    void tip_impl();

private:
    ClipType m_eClipType;
    bool is_slider_moving;

    QPixmap playPixmap;

    bool is_playing;
    DemuxThread *m_videoPlayer = nullptr;
    //std::shared_ptr<VideoPlayer> videoPlayer;
    VolumeFrame *volumeFrame;
    int player_timer_id;

    int undo_timer_id;
    std::vector<std::pair<OutputItem*, int>> undos;
    int volume_value = 50;

    ClueFrame *clueFrame;
    int clue_timer_id;
    int clue_pos;

    ClueFrame* timeTipFrame=nullptr;
    int timetip_timer_id ;
    int timetip_pos=0;

    bool is_loading;

    std::set<OutputItem*> outputItemCache;

    QMediaPlayer *mediaPlayer =nullptr;

    bool m_inputlist_is_empty=true;
    int m_isRunning =0;

    bool is_currentpage = false;
    bool m_audition = false;
    Ui::ExtractWidget *ui;

};

#endif // EXTRACTWIDGET_H
