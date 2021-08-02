#include "extractwidget.h"
#include "ui_extractwidget.h"

#include "inputitem.h"
#include "outputitem.h"
#include "userinfomanager.h"
#include "confirmdialog.h"
//#include "limitdialog.h"
#include "AtempoWidget/AtempoAskVip.h"
#include "buttonagent.h"
#include "clueframe.h"
#include "menudialog.h"

#if     defined(Q_OS_WIN)

#include "SensorsTracker.h"
#endif
#include "demux.h"
#include <QFileDialog>
#include <QDebug>
#include <QDesktopServices>

#include <thread>
#include <functional>
#include <chrono>
#include "demuxthread.h"
using namespace std;
std::mutex mux;
ExtractWidget::ExtractWidget(QWidget *parent) :
    QWidget(parent),
    is_slider_moving(false), is_playing(false), volumeFrame(nullptr), m_videoPlayer(nullptr), undo_timer_id(-1), clueFrame(nullptr), clue_timer_id(-1), clue_pos(0),
    is_loading(false),
    ui(new Ui::ExtractWidget)
{
    ui->setupUi(this);
    ui->countSelect->setView(new QListView());
    ui->countSelect->addItem("2", 2);
    ui->countSelect->addItem("3", 3);
    ui->countSelect->addItem("4", 4);
    ui->countSelect->addItem("5", 5);
    ui->countSelect->addItem("6", 6);
    ui->countSelect->addItem("7", 7);
    ui->countSelect->addItem("8", 8);
    ui->countSelect->addItem("9", 9);
    ui->countSelect->addItem("10", 10);

    ui->countSelect->view()->window()->setWindowFlags( Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint );

    ui->stackedWidget->setCurrentIndex(0);
    ui->progressSlider->setRange(0, 100);
    ui->progressSlider->SetLowerValue(0);
    ui->progressSlider->SetUpperValue(0);
    ui->progressSlider->SetNone();

    ui->widget_17->setDisabled(true);
    ui->progressSlider->setFixedHeight(26);

    connect(ui->progressSlider, SIGNAL(currentValueChanged(int)), this, SLOT(slot_progessRangeSlider_valueChange(int)));
    connect(ui->progressSlider, SIGNAL(rangesliderPress()), this, SLOT(slot_progessRangeSlider_sliderPressed()));
    connect(ui->progressSlider, SIGNAL(rangesliderRelease()), this, SLOT(slot_progessRangeSlider_sliderReleased()));
    connect(ui->progressSlider, SIGNAL(lowerValueChanged(int)), this, SLOT(slot_lowerValueChanged(int)));
    connect(ui->progressSlider, SIGNAL(upperValueChanged(int)), this, SLOT(slot_upperValueChanged(int)));
    connect(ui->progressSlider, SIGNAL(dragValueChange()), this, SLOT(slot_dragValueChange()));


    playPixmap.load(":/Res/clip/pic_moren_02.png");
    ui->labelShow->setPixmap(playPixmap);
    ui->stackedWidget_4->setCurrentIndex(1);
    //playPixmap.load(":/Res/clip/pic_moren_02.png");
    //ui->videoShow->setStyleSheet(":/Res/clip/pic_moren_02.png");


    ButtonAgent* buttonPlay = new ButtonAgent(ui->labelShow);
    connect(buttonPlay, SIGNAL(clicked(bool)), this, SLOT(buttonPlay_clicked(bool)));

    auto userinfo = UserInfoManager::Instance();
    ui->outputPathLabel->setText(userinfo->GetTargetPath());

    ui->checkBox->setChecked(true);

    volume_value = 50;

    qRegisterMetaType<MediaProfile>("MediaProfile");

    connect(this, SIGNAL(asyn_add_start()), this, SLOT(asyn_add_start_impl()));
    connect(this, SIGNAL(asyn_add_item(MediaProfile)), this, SLOT(asyn_add_item_impl(MediaProfile)));
    connect(this, SIGNAL(asyn_add_finished(bool)), this, SLOT(asyn_add_finished_impl(bool)));

    connect(UserInfoManager::Instance() , &UserInfoManager::change_targetpath,this,[=]()
    {
        ui->outputPathLabel->setText(UserInfoManager::Instance()->GetTargetPath());
    });

}


ExtractWidget::~ExtractWidget()
{
    if(mediaPlayer)
    {
        delete mediaPlayer;
        mediaPlayer = nullptr;
    }
    if(m_videoPlayer )
    {
        m_videoPlayer->SetPause(true);
        m_videoPlayer->Clear();
        m_videoPlayer->Close();
        delete  m_videoPlayer ;
        m_videoPlayer = nullptr;
    }

    delete ui;
}



bool ExtractWidget::isWorking()
{
    if(ui->stackedWidget_3->currentIndex() == 0)
    {
        int count = ui->outputList->count();
        for(int i = 0; i < count; ++i)
        {
            OutputItem *item = qobject_cast<OutputItem*>(ui->outputList->itemWidget(ui->outputList->item(i)));
            if(item->isWorking())
            {
                return true;
            }
        }
        return false;
    }
    else
    {
        return true;
    }
}

void ExtractWidget::updateTargetPath()
{
    auto userinfo = UserInfoManager::Instance();
    ui->outputPathLabel->setText(userinfo->GetTargetPath());
}

void ExtractWidget::showEvent(QShowEvent *event)
{
    setAttribute(Qt::WA_Mapped);
    QWidget::showEvent(event);
}

void ExtractWidget::timerEvent(QTimerEvent *event)
{
    int timerid = event->timerId();
    if(timerid == player_timer_id)
    {
        if(m_videoPlayer != nullptr && !is_slider_moving)
        {
            ui->currentTime->setTime(QTime(0,0).addMSecs(m_videoPlayer->pts));
            ui->progressSlider->setValue(m_videoPlayer->pts);

        }
        if(m_audition)
        {
            if(m_videoPlayer->pts>=ui->progressSlider->GetUpperValue())
            {
//                int value = ui->progressSlider->GetLowerValue();
//                ui->currentTime->setTime(QTime(0,0).addMSecs(value));
                ui->playCheck->setChecked(false);
                on_playCheck_clicked(false);

                //m_videoPlayer->Seek((double)value/m_videoPlayer->totalMs);
                m_audition = false;
            }
        }
    }
    else if(timerid == undo_timer_id)
    {
        std::vector<std::pair<OutputItem*, int>> temps;
        int now = QTime::currentTime().msecsSinceStartOfDay();
        for(int i = 0; i < undos.size(); ++i)
        {
            if(now - undos[i].second < 5000)
            {
                temps.push_back(undos[i]);
            }
        }

        undos.swap(temps);

        if(undos.size() <= 0)
        {
            killTimer(undo_timer_id);
            undo_timer_id = -1;
            emit hide_undo();
        }
        else
        {
            emit show_undo(undos.size());
        }
    }
    else if(timerid == clue_timer_id)
    {
        if(clueFrame != nullptr && !clueFrame->isHidden())
        {
            QPoint point = ui->progressSlider->mapTo(this, QPoint(0, 0));
            int pos = 0;
            if(clue_pos < 10)
            {
                pos = clue_pos;
            }
            else
            {
                pos = 20 - clue_pos;
            }
            clueFrame->setGeometry(point.x() - 3, point.y() + ui->progressSlider->height() + pos, clueFrame->width(), clueFrame->height());
            ++clue_pos;
            if(clue_pos > 20)
            {
                clue_pos = 0;
            }
        }
    }
}

void ExtractWidget::undo_impl()
{
    for(int i = 0; i < undos.size(); ++i)
    {
        OutputItem *item = undos[i].first;
        QListWidgetItem *listitem = new QListWidgetItem();
        listitem->setSizeHint(QSize(ui->outputList->width(), 33));
        ui->outputList->addItem(listitem);
        ui->outputList->setItemWidget(listitem, item);
    }
    undos.clear();
    if(ui->outputList->count() > 0)
    {
        ui->startStaticButton->setDisabled(false);
    }
    else
    {
        ui->startStaticButton->setDisabled(true);
    }
}

void ExtractWidget::undo_cancel_impl()
{
    if(undo_timer_id != -1)
    {
        killTimer(undo_timer_id);
        undo_timer_id= -1;
    }
    undos.clear();
}

bool ExtractWidget::isNeedShowUndo()
{
    return undos.size() > 0;
}

void ExtractWidget::buttonPlay_clicked(bool is)
{
    /*
    if(is_playing)
    {
        playMovie->stop();
    }
    else
    {
        playMovie->start();
    }
    is_playing = !is_playing;
    */
}

void ExtractWidget::SetClipType(const ClipType type)
{
    m_eClipType = type;

    if(m_eClipType == ClipType::VIDEO)
    {
        ui->stackedWidget_2->setCurrentIndex(1);
        ui->startStaticButton->setDisabled(true);
        ui->startStaticButton->setStyleSheet("QPushButton{border-image: url(:/Res/footer/btn_kstq_01.png);} QPushButton:hover{ border-image: url(:/Res/footer/btn_kstq_02.png);} QPushButton:pressed{ border-image: url(:/Res/footer/btn_kstq_03.png);} QPushButton:disabled{ border-image: url(:/Res/footer/btn_kstq_04.png);}");
        ui->startGifButton->SetGifPath(":/Res/footer/extract.gif");
        ui->startGifButton->SetAuto(true);
        ui->addButton->setFixedSize(QSize(280, 152));
        ui->addButton->setStyleSheet(
                    "QPushButton{border-image: url(:/open/Res/open/btn_extract_01.png);}"
                    "QPushButton:hover{ border-image: url(:/open/Res/open/btn_extract_02.png);}"
                    "QPushButton:pressed{ border-image: url(:/open/Res/open/btn_extract_03.png);}");


    }
}

void ExtractWidget::SetShowDrag(bool show)
{
    if(show)
    {
        ui->progressSlider->SetSelected(true);
    }
    else
    {
        ui->progressSlider->SetNone();
    }
}

void ExtractWidget::fsdssf()
{

}

void ExtractWidget::addDrap(QList<QUrl> &urls)
{
    if(this->is_loading)
    {
        return;
    }
    if(urls.size() > 0)
    {
        std::thread th(std::bind(&ExtractWidget::asyn_add_drop, this, urls));
        th.detach();
    }
    return;
    foreach(QUrl url, urls)
    {
        QDir tdir(url.toLocalFile());
        if(tdir.exists())
        {

            if(m_eClipType == ClipType::VIDEO)
            {
                QString filterstring = "*.swf; *.wtv; *.ogv; *.mxf; *.vro; *.webm; *.divx; *.avi; *.mp4; *.mpg; *.mka; *.mpeg; *.vob; *.3gp; *.3g2; *.wmv; *.asf; *.rm; *.rmvb; *.dat; *.mov; *.flv; *.f4v; *.m4v; *.mkv";
                filterstring.remove(" ");
                QStringList filters = filterstring.split(";",QString::SkipEmptyParts);
                QDir dir(tdir.path());
                dir.setFilter(QDir::Dirs | QDir::Files);
                dir.setNameFilters(filters);
                dir.setSorting(QDir::Name | QDir::Reversed);
                QFileInfoList list = dir.entryInfoList();
                for(int i = 0; i < list.size(); ++i)
                {
                    QFileInfo fileInfo = list.at(i);
                    QString path = fileInfo.absoluteFilePath();
                    //qDebug() << path;
                    MediaProfile profile;
                    bool ret = Media::GetVedioProfile(path, profile);
                    if(ret)
                    {
                        ui->stackedWidget->setCurrentIndex(1);
                        QListWidgetItem *listitem = new QListWidgetItem();
                        listitem->setSizeHint(QSize(ui->inputList->width(), 40));
                        InputItem *item = new InputItem();
                        connect(item, SIGNAL(remove()), this, SLOT(input_item_remove()));
                        item->setFixedSize(listitem->sizeHint());
                        item->SetMediaProfile(profile);
                        ui->inputList->addItem(listitem);
                        ui->inputList->setItemWidget(listitem, item);
                    }
                }
            }
        }
        else
        {
            QFileInfo file(url.toLocalFile());
            if(file.exists())
            {
                QString suf = file.suffix().toLower();
                if(m_eClipType == ClipType::VIDEO)
                {
                    QString filterstring = "*.swf; *.wtv; *.ogv; *.mxf; *.vro; *.webm; *.divx; *.avi; *.mp4; *.mpg; *.mka; *.mpeg; *.vob; *.3gp; *.3g2; *.wmv; *.asf; *.rm; *.rmvb; *.dat; *.mov; *.flv; *.f4v; *.m4v; *.mkv";
                    filterstring.remove(" ");
                    QStringList filters = filterstring.split(";",QString::SkipEmptyParts);
                    for(QString filter : filters)
                    {
                        if(filter.right(filter.length() - 2) == suf)
                        {
                            MediaProfile profile;
                            bool ret = Media::GetVedioProfile(file.filePath(), profile);
                            if(ret)
                            {
                                ui->stackedWidget->setCurrentIndex(1);
                                QListWidgetItem *listitem = new QListWidgetItem();
                                listitem->setSizeHint(QSize(ui->inputList->width(), 40));
                                InputItem *item = new InputItem();
                                connect(item, SIGNAL(remove()), this, SLOT(input_item_remove()));
                                item->setFixedSize(listitem->sizeHint());
                                item->SetMediaProfile(profile);
                                ui->inputList->addItem(listitem);
                                ui->inputList->setItemWidget(listitem, item);
                            }
                        }
                    }
                }
            }
        }
    }

    if(ui->inputList->count() > 0)
    {
        ui->stackedWidget->setCurrentIndex(1);
        if(ui->inputList->currentRow() < 0)
        {
            ui->inputList->setCurrentRow(0);
        }
    }
}

void ExtractWidget::on_addFileButton_clicked()
{
    emit show_shadow();

    if(m_eClipType == ClipType::VIDEO)
    {
        QFileDialog fileDialog(this);
        fileDialog.setWindowTitle(tr("请选择视频文件"));
        QStringList filters;
        filters << "All Supported Formats(*.swf; *.wtv; *.ogv; *.mxf; *.vro; *.webm; *.divx; *.avi; *.mp4; *.mpg; *.mka; *.mpeg; *.vob; *.3gp; *.3g2; *.wmv; *.asf; *.rm; *.rmvb; *.dat; *.mov; *.flv; *.f4v; *.m4v; *.mkv;)";
        //filters << "All files (*.*)";
        fileDialog.setNameFilters(filters);
        fileDialog.setFileMode(QFileDialog::ExistingFiles);
        fileDialog.setDirectory(".");
        if (fileDialog.exec() == QDialog::Accepted)
        {
            QStringList pathlist = fileDialog.selectedFiles();
            if(pathlist.size() > 0)
            {
                std::thread th(std::bind(&ExtractWidget::asyn_add_files, this, pathlist));
                th.detach();
            }
            /*
            for(auto const& path : pathlist)
            {
                MediaProfile profile;
                bool ret = Media::GetVedioProfile(path, profile);
                if(ret)
                {
                    ui->stackedWidget->setCurrentIndex(1);
                    QListWidgetItem *listitem = new QListWidgetItem();
                    listitem->setSizeHint(QSize(ui->inputList->width(), 40));
                    InputItem *item = new InputItem();
                    connect(item, SIGNAL(remove()), this, SLOT(input_item_remove()));
                    item->setFixedSize(listitem->sizeHint());
                    item->SetMediaProfile(profile);
                    ui->inputList->addItem(listitem);
                    ui->inputList->setItemWidget(listitem, item);
                }
            }
            */
        }
    }

    emit hide_shadow();
    /*
    if(ui->inputList->count() > 0)
    {
        ui->stackedWidget->setCurrentIndex(1);
        if(ui->inputList->currentRow() < 0)
        {
            ui->inputList->setCurrentRow(0);
        }
    }
    */
}

void ExtractWidget::on_addFolderButton_clicked()
{
    emit show_shadow();

    if(m_eClipType == ClipType::VIDEO)
    {
        QFileDialog fileDialog(this);
        fileDialog.setWindowTitle(tr("浏览文件夹"));
        QString filterstring = "*.swf; *.wtv; *.ogv; *.mxf; *.vro; *.webm; *.divx; *.avi; *.mp4; *.mpg; *.mka; *.mpeg; *.vob; *.3gp; *.3g2; *.wmv; *.asf; *.rm; *.rmvb; *.dat; *.mov; *.flv; *.f4v; *.m4v; *.mkv";
        filterstring.remove(" ");
        QStringList filters = filterstring.split(";",QString::SkipEmptyParts);
        fileDialog.setFileMode(QFileDialog::DirectoryOnly);
        fileDialog.setDirectory(".");
        if (fileDialog.exec() == QDialog::Accepted)
        {
            QStringList pathlist = fileDialog.selectedFiles();
            if(!pathlist.isEmpty())
            {
                std::thread th(std::bind(&ExtractWidget::asyn_add_folders, this, pathlist));
                th.detach();
                /*
                for(auto const& path : pathlist)
                {
                    QDir dir(path);
                    dir.setFilter(QDir::Dirs | QDir::Files);
                    dir.setNameFilters(filters);
                    dir.setSorting(QDir::Name | QDir::Reversed);
                    QFileInfoList list = dir.entryInfoList();
                    for(int i = 0; i < list.size(); ++i)
                    {
                        QFileInfo fileInfo = list.at(i);
                        QString path = fileInfo.absoluteFilePath();
                        //qDebug() << path;
                        MediaProfile profile;
                        bool ret = Media::GetVedioProfile(path, profile);
                        if(ret)
                        {
                            ui->stackedWidget->setCurrentIndex(1);
                            QListWidgetItem *listitem = new QListWidgetItem();
                            listitem->setSizeHint(QSize(ui->inputList->width(), 40));
                            InputItem *item = new InputItem();
                            connect(item, SIGNAL(remove()), this, SLOT(input_item_remove()));
                            item->setFixedSize(listitem->sizeHint());
                            item->SetMediaProfile(profile);
                            ui->inputList->addItem(listitem);
                            ui->inputList->setItemWidget(listitem, item);
                        }
                    }
                }
                */
            }
        }
    }

    emit hide_shadow();

    if(ui->inputList->count() > 0)
    {
        ui->stackedWidget->setCurrentIndex(1);
        if(ui->inputList->currentRow() < 0)
        {
            ui->inputList->setCurrentRow(0);
        }
    }
}

void ExtractWidget::on_changeFolderButton_clicked()
{
    QString workDir = UserInfoManager::Instance()->GetTargetPath();
    QDir dir(workDir);
    if(!dir.exists())
    {
        workDir="";
    }

    QFileDialog fileDialog(this);
    fileDialog.setWindowTitle("浏览文件夹");
    fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    fileDialog.setFileMode(QFileDialog::DirectoryOnly);
    if(workDir.isEmpty())
    {
        fileDialog.setDirectory(".");
    }
    else
    {
        fileDialog.setDirectory(workDir);
    }

    emit show_shadow();

    if (fileDialog.exec() == QDialog::Accepted)
    {
        QStringList pathlist = fileDialog.selectedFiles();
        if(!pathlist.isEmpty() && !pathlist.first().isEmpty())
        {
            {
                QString text = pathlist.first();//.replace("/","\\");
                QFontMetrics fontWidth(ui->outputPathLabel->font());
                int width = fontWidth.width(text);
                int maxwidth = ui->outputPathLabel->width() - 50;
                if(width >= maxwidth)
                {
                    text = fontWidth.elidedText(text, Qt::ElideMiddle, maxwidth);
                }
                ui->outputPathLabel->setText(text);
            }
            UserInfoManager::Instance()->SetTargetPath(QString(pathlist.first()));
        }
    }
    emit hide_shadow();
}


void ExtractWidget::on_openFolderButton_clicked()
{
    QString path = UserInfoManager::Instance()->GetTargetPath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void ExtractWidget::tip_impl()
{

    emit show_shadow();
//    LimitDialog *limit = new LimitDialog(this);
//    limit->setAttribute(Qt::WA_DeleteOnClose, true);
//    limit->SetTip(tr("非VIP用户仅支持2M以内大小文件转换，开通VIP后无限制"));
    AtempoAskVip *pAskVip = new AtempoAskVip(this);
    pAskVip->setAttribute(Qt::WA_DeleteOnClose, true);

    if(pAskVip->exec()== QDialog::Accepted)
    {

        emit hide_shadow();
        emit purchase();
        return;
    }
    emit hide_shadow();

}

bool ExtractWidget::getIs_currentpage() const
{
    return is_currentpage;
}

void ExtractWidget::setIs_currentpage(bool value)
{
    is_currentpage = value;
    qDebug()<<"NewConvertWidget"<<value;

    if(value)
    {
        if(m_videoPlayer != nullptr)
        {
            if(is_playing)
            {
                ui->playCheck->setChecked(true);
                on_playCheck_clicked(true);
                is_playing =false;
            }
        }
    }
    else
    {

        if(m_videoPlayer != nullptr)
        {
            if(ui->playCheck->isChecked())
            {
                is_playing = true;
                ui->playCheck->setChecked(false);
                on_playCheck_clicked(false);

            }
            else
            {
                is_playing = false;
            }
        }
    }
}




void ExtractWidget::item_start_clicked()
{
    QObject* sender = this->sender();
    OutputItem* target = qobject_cast<OutputItem*>(sender);
    if(target != nullptr)
    {

    }
}

void ExtractWidget::item_succ(bool succ)
{
    QObject* sender = this->sender();
    OutputItem* target = qobject_cast<OutputItem*>(sender);
    if(target != nullptr)
    {
        if(UserInfoManager::Instance()->isAudioTip())
        {
            if(mediaPlayer == nullptr)
            {
                mediaPlayer = new QMediaPlayer();
            }
            if(!succ)
            {
                mediaPlayer->setMedia(QUrl(UserInfoManager::Instance()->GetResourcePath() + "/fail.wav"));
            }
            else
            {
                mediaPlayer->setMedia(QUrl(UserInfoManager::Instance()->GetResourcePath() + "/succ.wav"));
            }

            mediaPlayer->setVolume(100);
            mediaPlayer->play();
        }

        auto itor = outputItemCache.find(target);
        if(itor != outputItemCache.end())
        {
            outputItemCache.erase(itor);
        }



        if(target->IsSingle())
        {
            mux.lock();
            m_isRunning--;
            mux.unlock();
            if(m_isRunning<=0)
            {
                ui->clearoutButton->setDisabled(false);
            }
            return;
        }
        else
        {
            int count = ui->outputList->count();
            for(int i = 0; i < count; ++i)
            {
                OutputItem *item = qobject_cast<OutputItem*>(ui->outputList->itemWidget(ui->outputList->item(i)));
                if(item->isSelected() && item->isNeedStart() && (outputItemCache.count(item) > 0))
                {
                    item->SetStart();
                    return;
                }
            }

            ui->clearoutButton->setDisabled(false);
            ui->stackedWidget_3->setCurrentIndex(0);
        }
    }
}

void ExtractWidget::item_remove()
{
    QObject* sender = this->sender();
    OutputItem* target = qobject_cast<OutputItem*>(sender);
    if(target != nullptr)
    {
        auto itor = outputItemCache.find(target);
        if(itor != outputItemCache.end())
        {
            outputItemCache.erase(itor);
        }

        int count = ui->outputList->count();
        for(int i = 0; i < count; ++i)
        {
            OutputItem *item = qobject_cast<OutputItem*>(ui->outputList->itemWidget(ui->outputList->item(i)));
            if(item != nullptr && target == item)
            {
                {
                    OutputItem *undoitem = new OutputItem();
                    connect(undoitem, SIGNAL(start_clicked()), this, SLOT(item_start_clicked()));
                    connect(undoitem, SIGNAL(succ(bool)), this, SLOT(item_succ(bool)));
                    connect(undoitem, SIGNAL(remove()), this, SLOT(item_remove()));
                    connect(undoitem, SIGNAL(tip()), this, SLOT(tip_impl()));
                    item->Clone(undoitem);
                    undos.push_back(std::pair<OutputItem*, int>(undoitem, QTime::currentTime().msecsSinceStartOfDay()));
                    if(!undos.empty())
                    {
                        if(undo_timer_id == -1)
                        {
                            undo_timer_id = startTimer(1000);
                        }
                        emit show_undo(undos.size());
                    }
                }
                disconnect(item, SIGNAL(succ(bool)), this, SLOT(item_succ(bool)));
                disconnect(item, SIGNAL(remove()), this, SLOT(item_remove()));
                auto listitem = ui->outputList->takeItem(i);
                delete target;
                delete listitem;
                break;
            }
        }
    }
    if(ui->outputList->count() <= 0)
    {
        ui->startStaticButton->setDisabled(true);
    }
}

void ExtractWidget::input_item_remove()
{
    QObject* sender = this->sender();
    InputItem* target = qobject_cast<InputItem*>(sender);
    if(target != nullptr)
    {
        int count = ui->inputList->count();
        for(int i = 0; i < count; ++i)
        {
            InputItem *item = qobject_cast<InputItem*>(ui->inputList->itemWidget(ui->inputList->item(i)));
            if(item != nullptr && target == item)
            {
                disconnect(item, SIGNAL(remove()), this, SLOT(input_item_remove()));
                auto listitem = ui->inputList->takeItem(i);
                delete target;
                delete listitem;
                break;
            }
        }
    }
    if(ui->inputList->count() <= 0)
    {
        ui->stackedWidget->setCurrentIndex(0);
        m_inputlist_is_empty =true;
        if(m_videoPlayer)
        {
            m_videoPlayer->SetPause(true);
            m_videoPlayer->Clear();
            m_videoPlayer->Close();
            delete  m_videoPlayer ;
            m_videoPlayer = nullptr;
        }

    }
}

void ExtractWidget::on_inputList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    if(previous != nullptr)
    {
        InputItem *item = qobject_cast<InputItem*>(ui->inputList->itemWidget(previous));
        if(item != nullptr)
        {
            item->SetSelected(false);
        }
        ui->tabWidget->setCurrentIndex(0);
        ui->currentTime->setTime(QTime(0,0).addMSecs(0));
        ui->totalTime->setTime(QTime(0,0).addMSecs(0));
        ui->progressSlider->setValue(0);
        ui->progressSlider->setLowerValue(0);
        ui->progressSlider->setUpperValue(0);

        ui->lineEdit->setText("");
        ui->lineEdit_2->setText("");

        ui->timeEdit->setTime(QTime(0,0).addMSecs(0));
        ui->timeEdit->setTimeRange(QTime(0,0).addMSecs(0), QTime(0,0).addMSecs(0));
        if(m_videoPlayer)
        {
            m_videoPlayer->SetPause(true);
            //m_inputlist_is_empty = true;
            m_videoPlayer->Clear();
            m_videoPlayer->Close();
            delete  m_videoPlayer ;
            m_videoPlayer = nullptr;

        }
        ui->progressSlider->SetNone();

        ui->stackedWidget_4->setCurrentIndex(1);
        ui->videoShow->Repaint(std::move(QImage(":/Res/clip/pic_moren_02.png")));
    }

    if(current != nullptr)
    {
        InputItem *item = qobject_cast<InputItem*>(ui->inputList->itemWidget(current));
        if(!item->isExist())
        {
            emit show_notexist(item->GetMediaProfile().filename+"不存在");
            item->on_deleteButton_clicked();
            return ;
        }
        item->SetSelected(true);

        MediaProfile profile = item->GetMediaProfile();
        ui->currentTime->setTime(QTime(0,0).addMSecs(0));
        ui->totalTime->setTime(QTime(0,0).addMSecs(profile.duration));
        ui->progressSlider->setRange(0, profile.duration);
        ui->progressSlider->setValue(0);

        ui->progressSlider->setLowerValue(0);
        ui->progressSlider->SetUpperValue(profile.duration);

        ui->timeEdit->setTime(QTime(0,0).addMSecs(profile.duration * 0.25));
        ui->timeEdit->setTimeRange(QTime(0,0).addMSecs(profile.duration * 0.1), QTime(0,0).addMSecs(profile.duration));

        if(ui->tabWidget->currentIndex() == 0)
        {
            ui->progressSlider->SetSelected(true);
        }
        else if(ui->tabWidget->currentIndex() == 1)
        {
            ui->progressSlider->SetBlock(ui->countSelect->currentData().toInt());
        }
        else if(ui->tabWidget->currentIndex() == 2)
        {
            ui->progressSlider->SetSlice(ui->timeEdit->time().msecsSinceStartOfDay());
        }
        else if(m_eClipType == ClipType::VIDEO)
        {
            QString temp = profile.filename;
            temp = temp.left(temp.lastIndexOf('.')) + "_1.MP3";
            ui->lineEdit->setText(temp);
        }

        ui->widget_17->setDisabled(false);
        if(!startPlay(profile.filepath))
        {
            emit show_notexist(profile.filepath+"不能播放或者没有音频");
            item->on_deleteButton_clicked();
            return ;
        }
        ui->stackedWidget_4->setCurrentIndex(0);
        if(!UserInfoManager::Instance()->isClueTip() && clueFrame == nullptr)
        {
            if(clueFrame != nullptr)
            {
                clueFrame->show();
            }
            else
            {
                clueFrame = new ClueFrame(ClueType::CLUE,"拖动指针选择片段。",this);
                connect(clueFrame , &ClueFrame::closeframe,this,&ExtractWidget::closeTimeEditTip);
                QPoint point = ui->progressSlider->mapTo(this, QPoint(0, 0));
                clueFrame->setGeometry(point.x(), point.y() + ui->progressSlider->height(), clueFrame->width(), clueFrame->height());
                clueFrame->show();
                clue_timer_id = startTimer(100);
            }
        }
        if(!UserInfoManager::Instance()->getIs_time_edittip() && timeTipFrame == nullptr)
        {
            {
                if(timeTipFrame != nullptr)
                {
                    QPoint point = ui->rbtnStart->mapTo(this, QPoint(0, 0));
                    timeTipFrame->setLabelStr("点击选择当前时间");
                    timeTipFrame->move(point.x() - timeTipFrame->width()+30 , point.y() + ui->rbtnStart->height());
                    timeTipFrame->show();
                    timetip_timer_id = startTimer(100);

                }
                else
                {

                    timeTipFrame = new ClueFrame(ClueType::TIMETIP,"点击选择当前时间",this);
                    connect(timeTipFrame , &ClueFrame::closeframe,this,&ExtractWidget::closeTimeEditTip);
                    QPoint point = ui->rbtnStart->mapTo(this, QPoint(0, 0));
                    timeTipFrame->move(point.x() - timeTipFrame->width()+30, point.y() + ui->rbtnStart->height());
                    timeTipFrame->show();
                    timetip_timer_id = startTimer(100);

                }
            }

        }

    }
    else
    {
        if(clueFrame != nullptr)
        {
            clueFrame->hide();
        }
        ui->widget_17->setDisabled(true);
    }
}

void ExtractWidget::on_clearinButton_clicked()
{
    if(ui->inputList->count() > 0)
    {
        emit show_shadow();
        ConfirmDialog *confirm = new ConfirmDialog(this);
        if(confirm->exec() == QDialog::Accepted)
        {
            ui->inputList->clear();
            ui->stackedWidget->setCurrentIndex(0);
            m_inputlist_is_empty = true;
            if(m_videoPlayer)
            {
                m_videoPlayer->SetPause(true);
                m_videoPlayer->Clear();
                m_videoPlayer->Close();
                delete  m_videoPlayer ;
                m_videoPlayer = nullptr;
            }

        }
        delete confirm;

        emit hide_shadow();
    }
}

void ExtractWidget::on_clearoutButton_clicked()
{
    mux.lock();
    m_isRunning=0;
    mux.unlock();
    int count = ui->outputList->count();
    for(int i = 0; i < count; ++i)
    {
        OutputItem *citem = qobject_cast<OutputItem*>(ui->outputList->itemWidget(ui->outputList->item(i)));
        OutputItem *item = new OutputItem();
        connect(item, SIGNAL(succ(bool)), this, SLOT(item_succ(bool)));
        connect(item, SIGNAL(remove()), this, SLOT(item_remove()));
        connect(item, SIGNAL(tip()), this, SLOT(tip_impl()));
        connect(item, &OutputItem::isRunning ,this,[=](){
            ui->clearoutButton->setDisabled(true);
            mux.lock();
            m_isRunning++;
            mux.unlock();
        });
        citem->Clone(item);
        undos.push_back(std::pair<OutputItem*, int>(item, QTime::currentTime().msecsSinceStartOfDay()));
    }
    ui->outputList->clear();
    if(!undos.empty())
    {
        if(undo_timer_id == -1)
        {
            undo_timer_id = startTimer(1000);
        }
        emit show_undo(undos.size());
    }
    ui->startStaticButton->setDisabled(true);
}

void ExtractWidget::on_timeEdit_timeChanged(const QTime &time)
{
    int length = time.msecsSinceStartOfDay();
    if(length <= 0)
    {
        return;
    }
    ui->progressSlider->SetSlice(length);
}

void ExtractWidget::on_tabWidget_currentChanged(int index)
{
    if(ui->inputList->currentRow() < 0)
    {
        ui->progressSlider->SetNone();
    }
    else
    {
        if(ui->tabWidget->currentIndex() == 0)
        {
            ui->progressSlider->SetSelected(true);
        }
        else if(ui->tabWidget->currentIndex() == 1)
        {
            ui->progressSlider->SetBlock(ui->countSelect->currentData().toInt());
        }
        else if(ui->tabWidget->currentIndex() == 2)
        {
            ui->progressSlider->SetSlice(ui->timeEdit->time().msecsSinceStartOfDay());
        }
    }
}

void ExtractWidget::on_outputButton_clicked()
{
    if(ui->inputList->currentItem() == nullptr) return;
    int index = ui->tabWidget->currentIndex();
    InputItem *item = qobject_cast<InputItem*>(ui->inputList->itemWidget(ui->inputList->currentItem()));
    MediaProfile profile = item->GetMediaProfile();
    QFileInfo fileinfo(profile.filepath);
    if (!fileinfo.isFile())
    {
        return;
    }
    if(index == 0)
    {
        QListWidgetItem *listitem = new QListWidgetItem();
        listitem->setSizeHint(QSize(ui->outputList->width(), 33));
        OutputItem *item = new OutputItem();
        item->setFixedSize(listitem->sizeHint());
        item->SetType((int)m_eClipType);
        QString temp = profile.filename;
        temp = temp.left(temp.lastIndexOf('.')) + ".MP3";

        if(m_eClipType == ClipType::VIDEO)
        {
            if(ui->lineEdit->text().isEmpty())
            {
                item->SetOutputArgs(profile, temp, ui->progressSlider->GetLowerValue(), ui->progressSlider->GetUpperValue());
            }
            else
            {
                QString strSuffix;
                {
                    QFileInfo file(ui->lineEdit->text());
                    {
                        strSuffix = file.suffix().toUpper();
                    }
                }
                if(0 != strSuffix.compare("MP3"))
                    temp = ui->lineEdit->text() + ".MP3";
                else
                    temp = ui->lineEdit->text();
                item->SetOutputArgs(profile, temp, ui->progressSlider->GetLowerValue(), ui->progressSlider->GetUpperValue());
            }
        }
        connect(item, SIGNAL(start_clicked()), this, SLOT(item_start_clicked()));
        connect(item, SIGNAL(succ(bool)), this, SLOT(item_succ(bool)));
        connect(item, SIGNAL(remove()), this, SLOT(item_remove()));
        connect(item, SIGNAL(tip()), this, SLOT(tip_impl()));
        connect(item, &OutputItem::isRunning ,this,[=](){
            ui->clearoutButton->setDisabled(true);
            mux.lock();
            m_isRunning++;
            mux.unlock();
        });
        ui->outputList->addItem(listitem);
        ui->outputList->setItemWidget(listitem, item);
    }
    else if(index == 1)
    {
        int count = ui->countSelect->currentData().toInt();
        int average = profile.duration / count;
        for(int i = 1; i <= count; ++i)
        {
            int start = average * (i - 1);
            int end = 0;
            if(i == count)
            {
                end = profile.duration;
            }
            else
            {
                end = average * i;
            }

            QListWidgetItem *listitem = new QListWidgetItem();
            listitem->setSizeHint(QSize(ui->outputList->width(), 33));
            OutputItem *item = new OutputItem();
            item->setFixedSize(listitem->sizeHint());
            item->SetType((int)m_eClipType);
            QString temp = ui->lineEdit_2->text();
            if(temp.isEmpty())
            {
                temp = profile.filename;
            }
            temp = temp.left(temp.lastIndexOf('.')) + "-" + QString::number(i) + ".MP3";

            item->SetOutputArgs(profile, temp, start, end);
            connect(item, SIGNAL(start_clicked()), this, SLOT(item_start_clicked()));
            connect(item, SIGNAL(succ(bool)), this, SLOT(item_succ(bool)));
            connect(item, SIGNAL(remove()), this, SLOT(item_remove()));
            connect(item, SIGNAL(tip()), this, SLOT(tip_impl()));
            connect(item, &OutputItem::isRunning ,this,[=](){
                ui->clearoutButton->setDisabled(true);
                mux.lock();
                m_isRunning++;
                mux.unlock();
            });
            ui->outputList->addItem(listitem);
            ui->outputList->setItemWidget(listitem, item);
        }
    }
    else if(index == 2)
    {
        int duration = ui->timeEdit->time().msecsSinceStartOfDay();
        int count = profile.duration / duration;
        int over = profile.duration % duration;
        if(over > 0)
        {
            ++count;
        }
        for(int i = 1; i <= count; ++i)
        {
            int start = duration * (i - 1);
            int end = 0;
            if(i == count)
            {
                end = profile.duration;
            }
            else
            {
                end = duration * i;
            }

            QListWidgetItem *listitem = new QListWidgetItem();
            listitem->setSizeHint(QSize(ui->outputList->width(), 33));
            OutputItem *item = new OutputItem();
            item->setFixedSize(listitem->sizeHint());
            item->SetType((int)m_eClipType);
            QString temp = ui->lineEdit_2->text();
            if(temp.isEmpty())
            {
                temp = profile.filename;
            }
            temp = temp.left(temp.lastIndexOf('.')) + "-" + QString::number(i) + ".MP3";

            item->SetOutputArgs(profile, temp, start, end);
            connect(item, SIGNAL(start_clicked()), this, SLOT(item_start_clicked()));
            connect(item, SIGNAL(succ(bool)), this, SLOT(item_succ(bool)));
            connect(item, SIGNAL(remove()), this, SLOT(item_remove()));
            connect(item, SIGNAL(tip()), this, SLOT(tip_impl()));
            connect(item, &OutputItem::isRunning ,this,[=](){
                ui->clearoutButton->setDisabled(true);
                mux.lock();
                m_isRunning++;
                mux.unlock();
            });
            ui->outputList->addItem(listitem);
            ui->outputList->setItemWidget(listitem, item);
        }
    }

    if(ui->outputList->count() > 0)
    {
        ui->startStaticButton->setDisabled(false);
    }
}

void ExtractWidget::slot_progessRangeSlider_valueChange(int value)
{
}

void ExtractWidget::slot_dragValueChange()
{
    if(clueFrame != nullptr && !clueFrame->isHidden())
    {
        clueFrame->hide();
        UserInfoManager::Instance()->SetClueTip(true);
    }
}

void ExtractWidget::slot_progessRangeSlider_sliderPressed()
{
    QObject* sender = this->sender();
    RangeSlider* slider = qobject_cast<RangeSlider*>(sender);
    if(ui->progressSlider == slider)
    {
        is_slider_moving = true;
    }
}

void ExtractWidget::slot_progessRangeSlider_sliderReleased()
{
    QObject* sender = this->sender();
    RangeSlider* slider = qobject_cast<RangeSlider*>(sender);
    if(ui->progressSlider == slider)
    {
        is_slider_moving = false;
        if(m_videoPlayer != nullptr)
        {

            m_videoPlayer->Seek((double)slider->getCurrentValue()/m_videoPlayer->totalMs);
            ui->playCheck->setChecked(true);
            if(player_timer_id == -1)
            {
                player_timer_id = startTimer(500);
            }
        }
    }
}

void ExtractWidget::slot_lowerValueChanged(int value)
{

    QObject* sender = this->sender();
    RangeSlider* slider = qobject_cast<RangeSlider*>(sender);
    if(ui->progressSlider == slider)
    {

        if(m_eClipType == ClipType::VIDEO)
        {
            ui->handStartTime->setTime(QTime(0,0).addMSecs(value));
        }

    }
    {
        if(m_videoPlayer != nullptr)
        {
            //ui->playCheck->setChecked(true);

            if(player_timer_id == -1)
            {
                player_timer_id = startTimer(500);
            }
        }

    }

}

void ExtractWidget::slot_upperValueChanged(int value)
{
    QObject* sender = this->sender();
    RangeSlider* slider = qobject_cast<RangeSlider*>(sender);
    if(ui->progressSlider == slider)
    {

        if(m_eClipType == ClipType::VIDEO)
        {
            ui->handEndTime->setTime(QTime(0,0).addMSecs(value));
        }
    }
}

void ExtractWidget::on_checkBox_clicked(bool checked)
{
    int count = ui->outputList->count();
    for(int i = 0; i < count; ++i)
    {
        OutputItem *item = qobject_cast<OutputItem*>(ui->outputList->itemWidget(ui->outputList->item(i)));
        item->SetChecked(checked);
    }
}

void ExtractWidget::on_countSelect_currentIndexChanged(int index)
{
    //qDebug() << ui->countSelect->itemData(index);
    ui->progressSlider->SetBlock(ui->countSelect->currentData().toInt());
}

bool ExtractWidget::startPlay(QString path)
{
    if(!Demux::HasAudioVideo(path.toUtf8().data()))
    {
        return false;
    }
    if(m_videoPlayer == nullptr)
    {
        m_videoPlayer = new DemuxThread();
        connect(m_videoPlayer,&DemuxThread::sendEnd,this,[=]()
        {
            if(m_audition)
            {

//                    int value = ui->progressSlider->GetLowerValue();
//                    ui->currentTime->setTime(QTime(0,0).addMSecs(value));
//                    m_videoPlayer->Seek((double)value/m_videoPlayer->totalMs);
                    ui->playCheck->setChecked(false);
                    on_playCheck_clicked(false);
                    m_audition = false;

            }
        });
    }
    m_videoPlayer->Start();
    if(!m_videoPlayer->Open(path.toUtf8().data(),ui->videoShow))
    {
        delete m_videoPlayer;
        m_videoPlayer = nullptr;
        qDebug()<<"该视频不能打开或则没有音频"<<endl;
        return false;
    }
    //m_videoPlayer->play(path);

    m_videoPlayer->setVolume(volume_value);
    update_volume(volume_value);
    ui->playCheck->setChecked(true);
    player_timer_id = startTimer(500);

    return true;
}

void ExtractWidget::stopPlay()
{
    if(m_videoPlayer != nullptr)
    {
        m_videoPlayer->SetPause(true);
        ui->playCheck->setChecked(false);
        killTimer(player_timer_id);
        player_timer_id = -1;
    }
}

void ExtractWidget::on_volumeButton_clicked()
{
    if(volumeFrame == nullptr)
    {
        volumeFrame = new VolumeFrame();
        //volumeFrame->setAttribute(Qt::WA_DeleteOnClose, true);
        connect(volumeFrame, SIGNAL(volume_change(int)), this, SLOT(update_volume(int)));
    }
    QPoint point = ui->volumeButton->mapToGlobal(QPoint(0, 0));
    QSize svolume = volumeFrame->size();
    QSize sbutton = ui->volumeButton->size();
    volumeFrame->SetVolume(m_videoPlayer->getVolume());
    volumeFrame->move(QPoint(point.x() + sbutton.width() / 2 - svolume.width() / 2, point.y() - svolume.height() - 5));
    volumeFrame->showNormal();
    volumeFrame->setFocus();
    volumeFrame->activateWindow();
}

void ExtractWidget::update_volume(int value)
{
    //qDebug() << __FUNCTION__ << value;
    if(m_videoPlayer != nullptr)
    {
        m_videoPlayer->setVolume(value);
        volume_value = value;
    }
}

void ExtractWidget::on_stopButton_clicked()
{
    m_videoPlayer->Seek(0);
    ui->progressSlider->setValue(0);
    stopPlay();

}

void ExtractWidget::on_playCheck_clicked(bool checked)
{
    if(m_videoPlayer != nullptr)
    {
        m_videoPlayer->SetPause(!checked);
        if(!checked)
        {
            killTimer(player_timer_id);
            player_timer_id = -1;
        }
        else
        {
            player_timer_id = startTimer(500);
        }
    }
}

void ExtractWidget::on_handStartTime_2_timeChanged(const QTime &time)
{


    if(ui->inputList->currentRow() >= 0)
    {
        int current = time.msecsSinceStartOfDay();
        ui->progressSlider->SetLowerValue(current);
         on_playCheck_clicked(true);
        if(ui->progressSlider->GetUpperValue() < ui->progressSlider->GetLowerValue())
        {
            ui->progressSlider->SetUpperValue(current);
        }

        m_videoPlayer->Seek((double)current/m_videoPlayer->totalMs);
    }
}

void ExtractWidget::on_handEndTime_2_timeChanged(const QTime &time)
{
    if(ui->inputList->currentRow() >= 0)
    {
        ui->progressSlider->SetUpperValue(time.msecsSinceStartOfDay());
        if(ui->progressSlider->GetLowerValue() > ui->progressSlider->GetUpperValue())
        {
            ui->progressSlider->SetLowerValue(time.msecsSinceStartOfDay());
        }
    }
}

void ExtractWidget::on_handStartTime_timeChanged(const QTime &time)
{
    if(ui->inputList->currentRow() >= 0 && !is_slider_moving)
    {
        int current = time.msecsSinceStartOfDay();
        ui->progressSlider->SetLowerValue(current);
        ui->playCheck->setChecked(true);
        if(ui->progressSlider->GetUpperValue() < ui->progressSlider->GetLowerValue())
        {
            ui->progressSlider->SetUpperValue(current);
        }
        m_videoPlayer->Seek((double)current/m_videoPlayer->totalMs);
    }
}

void ExtractWidget::on_handEndTime_timeChanged(const QTime &time)
{
    if(ui->inputList->currentRow() >= 0)
    {
        ui->progressSlider->SetUpperValue(time.msecsSinceStartOfDay());
        if(ui->progressSlider->GetLowerValue() > ui->progressSlider->GetUpperValue())
        {
            ui->progressSlider->SetLowerValue(time.msecsSinceStartOfDay());
        }
    }
}

void ExtractWidget::on_addButton_clicked()
{
    on_addFileButton_clicked();
}

void ExtractWidget::asyn_add_files(const QStringList filelist)
{
    emit asyn_add_start();
    bool has = false;
    for(auto const& path : filelist)
    {

        if(m_eClipType == ClipType::VIDEO)
        {
            MediaProfile profile;
            bool ret = Media::GetVedioProfile(path, profile);
            if(!ret) continue;
            has = true;
            emit asyn_add_item(profile);
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    emit asyn_add_finished(has);
}

void ExtractWidget::asyn_add_folders(const QStringList filelist)
{
    emit asyn_add_start();
    bool has = false;
    for(auto const &path : filelist)
    {

        if(m_eClipType == ClipType::VIDEO)
        {
            QString filterstring = "*.swf; *.wtv; *.ogv; *.mxf; *.vro; *.webm; *.divx; *.avi; *.mp4; *.mpg; *.mka; *.mpeg; *.vob; *.3gp; *.3g2; *.wmv; *.asf; *.rm; *.rmvb; *.dat; *.mov; *.flv; *.f4v; *.m4v; *.mkv";
            filterstring.remove(" ");
            QStringList filters = filterstring.split(";",QString::SkipEmptyParts);
            QDir dir(path);
            dir.setFilter(QDir::Dirs | QDir::Files);
            dir.setNameFilters(filters);
            dir.setSorting(QDir::Name | QDir::Reversed);
            QFileInfoList list = dir.entryInfoList();
            for(int i = 0; i < list.size(); ++i)
            {
                QFileInfo fileInfo = list.at(i);
                QString path = fileInfo.absoluteFilePath();
                MediaProfile profile;
                bool ret = Media::GetVedioProfile(path, profile);
                if(!ret) continue;
                has = true;
                emit asyn_add_item(profile);
            }
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    emit asyn_add_finished(has);
}

void ExtractWidget::asyn_add_drop(const QList<QUrl> urls)
{
    emit asyn_add_start();
    bool has = false;
    bool notsupport = true;
    foreach(QUrl url, urls)
    {
        QDir tdir(url.toLocalFile());
        if(tdir.exists())
        {

            if(m_eClipType == ClipType::VIDEO)
            {
                QString filterstring = "*.swf; *.wtv; *.ogv; *.mxf; *.vro; *.webm; *.divx; *.avi; *.mp4; *.mpg; *.mka; *.mpeg; *.vob; *.3gp; *.3g2; *.wmv; *.asf; *.rm; *.rmvb; *.dat; *.mov; *.flv; *.f4v; *.m4v; *.mkv";
                filterstring.remove(" ");
                QStringList filters = filterstring.split(";",QString::SkipEmptyParts);
                QDir dir(tdir.path());
                dir.setFilter(QDir::Dirs | QDir::Files);
                dir.setNameFilters(filters);
                dir.setSorting(QDir::Name | QDir::Reversed);
                QFileInfoList list = dir.entryInfoList();
                for(int i = 0; i < list.size(); ++i)
                {
                    QFileInfo fileInfo = list.at(i);
                    QString path = fileInfo.absoluteFilePath();
                    //qDebug() << path;
                    MediaProfile profile;
                    bool ret = Media::GetVedioProfile(path, profile);
                    if(!ret) continue;
                    has = true;
                    emit asyn_add_item(profile);
                    notsupport = false;
                    has=true;
                }
            }
        }
        else
        {
            QFileInfo file(url.toLocalFile());
            if(file.exists())
            {
                QString suf = file.suffix().toLower();

                if(m_eClipType == ClipType::VIDEO)
                {
                    QString filterstring = "*.swf; *.wtv; *.ogv; *.mxf; *.vro; *.webm; *.divx; *.avi; *.mp4; *.mpg; *.mka; *.mpeg; *.vob; *.3gp; *.3g2; *.wmv; *.asf; *.rm; *.rmvb; *.dat; *.mov; *.flv; *.f4v; *.m4v; *.mkv";
                    filterstring.remove(" ");
                    QStringList filters = filterstring.split(";",QString::SkipEmptyParts);
                    for(QString filter : filters)
                    {
                        if(filter.right(filter.length() - 2) == suf)
                        {
                            MediaProfile profile;
                            bool ret = Media::GetVedioProfile(file.filePath(), profile);
                            if(!ret) continue;
                            has = true;
                            emit asyn_add_item(profile);
                            notsupport = false;
                            has=true;
                        }
                    }
                }
            }
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    emit asyn_add_finished(has);
    if(notsupport)
    {
        emit show_notexist("当前格式不支持。 ");
    }
}

void ExtractWidget::asyn_add_start_impl()
{
    is_loading = true;
    emit show_shadow();
    MenuDialog *tip = new MenuDialog(this);
    connect(this, SIGNAL(asyn_add_finished(bool)), tip, SLOT(accept()));
    if(tip->exec()== QDialog::Accepted)
    {

    }
    disconnect(this, SIGNAL(asyn_add_finished(bool)), tip, SLOT(accept()));
    emit hide_shadow();
    delete tip;
}

void ExtractWidget::asyn_add_item_impl(const MediaProfile profile)
{
    ui->stackedWidget->setCurrentIndex(1);
    QListWidgetItem *listitem = new QListWidgetItem();
    listitem->setSizeHint(QSize(ui->inputList->width(), 40));
    InputItem *item = new InputItem();
    connect(item, SIGNAL(remove()), this, SLOT(input_item_remove()));
    item->setFixedSize(listitem->sizeHint());
    item->SetMediaProfile(profile);
    ui->inputList->addItem(listitem);
    ui->inputList->setItemWidget(listitem, item);
    if(ui->inputList->count() > 0)
    {
        ui->stackedWidget->setCurrentIndex(1);
//        if(ui->inputList->currentRow() < 0)
//        {
//            ui->inputList->setCurrentRow(0);
//        }
    }
}

void ExtractWidget::asyn_add_finished_impl(bool has)
{
    if(m_inputlist_is_empty)
    {
        ui->inputList->setCurrentRow(0);
        m_inputlist_is_empty = false;
    }
    if(!has)
    {
        emit show_notexist("当前格式不支持。 ");
    }
    is_loading = false;
}
void ExtractWidget::on_lineEdit_2_returnPressed()
{
    ui->lineEdit_2->clearFocus();
}

void ExtractWidget::on_lineEdit_returnPressed()
{
    ui->lineEdit->clearFocus();
}

void ExtractWidget::on_startStaticButton_clicked()
{
#if     defined(Q_OS_WIN)
    if(m_eClipType == ClipType::AUDIO)
    {

        QString value = QString(tr("AC-音频分割-分割全部"));
        hudun::sensors::HdEventTask TaskInfo;
        TaskInfo.name = value.toStdString();
        TaskInfo.elapsed = 0;
        TaskInfo.result = std::make_shared<hudun::sensors::HdResult>(hudun::sensors::HdResult::Success);
        hudun::sensors::HdContextProperties hdContextProperties;
        hudun::sensors::SensorsTracker::getInstance()->trackTask(TaskInfo, hdContextProperties);
    }
    else if(m_eClipType == ClipType::VIDEO)
    {

        QString value = QString(tr("AC-音频提取-提取全部"));
        hudun::sensors::HdEventTask TaskInfo;
        TaskInfo.name = value.toStdString();
        TaskInfo.elapsed = 0;
        TaskInfo.result = std::make_shared<hudun::sensors::HdResult>(hudun::sensors::HdResult::Success);
        hudun::sensors::HdContextProperties hdContextProperties;
        hudun::sensors::SensorsTracker::getInstance()->trackTask(TaskInfo, hdContextProperties);
    }
#endif
    outputItemCache.clear();
    int count = ui->outputList->count();
    bool show_tip = false;
    if(count > 0)
    {
        {
            for(int i = 0; i < count; ++i)
            {
                OutputItem *item = qobject_cast<OutputItem*>(ui->outputList->itemWidget(ui->outputList->item(i)));
                if(item->isSelected() )
                {
                    if(item->isNeedStart())
                    {
                        outputItemCache.insert(item);
                    }
                    else
                    {
                        if(!item->isFinished())
                        {
                            show_tip = true;
#if     defined(Q_OS_WIN)
                            if(m_eClipType == ClipType::AUDIO)
                            {
                                QString value = QString(tr("AC-音频分割-分割"));
                                hudun::sensors::HdEventTask TaskInfo;
                                TaskInfo.name = value.toStdString();
                                TaskInfo.elapsed = 0;
                                TaskInfo.result = std::make_shared<hudun::sensors::HdResult>(hudun::sensors::HdResult::Cancelled);
                                hudun::sensors::HdContextProperties hdContextProperties;
                                hudun::sensors::SensorsTracker::getInstance()->trackTask(TaskInfo, hdContextProperties);
                            }
                            else if(m_eClipType == ClipType::VIDEO)
                            {
                                QString value = QString(tr("AC-音频提取-提取"));
                                hudun::sensors::HdEventTask TaskInfo;
                                TaskInfo.name = value.toStdString();
                                TaskInfo.elapsed = 0;
                                TaskInfo.result = std::make_shared<hudun::sensors::HdResult>(hudun::sensors::HdResult::Cancelled);
                                hudun::sensors::HdContextProperties hdContextProperties;
                                hudun::sensors::SensorsTracker::getInstance()->trackTask(TaskInfo, hdContextProperties);
                            }
#endif
                        }
                    }
                }
            }
        }
    }
    bool has = false;
    for(int i = 0; i < count; ++i)
    {
        OutputItem *item = qobject_cast<OutputItem*>(ui->outputList->itemWidget(ui->outputList->item(i)));
        if(item->isSelected() && item->isNeedStart())
        {
            item->SetStart();
            has = true;
            break;
        }
    }
    if(has)
    {
        ui->clearoutButton->setDisabled(true);
        ui->stackedWidget_3->setCurrentIndex(1);
    }

    if(show_tip)
    {
        if(!UserInfoManager::Instance()->isVip())
        {
            emit show_shadow();
//            LimitDialog *limit = new LimitDialog(this);
//            limit->setAttribute(Qt::WA_DeleteOnClose, true);
//            limit->SetTip(tr("非VIP用户仅支持2M以内大小文件转换，开通VIP后无限制"));
            AtempoAskVip *pAskVip = new AtempoAskVip(this);
            pAskVip->setAttribute(Qt::WA_DeleteOnClose, true);

            if(pAskVip->exec()== QDialog::Accepted)
            {

                emit hide_shadow();
                emit purchase();
                return;
            }
            emit hide_shadow();
        }
    }
}

void ExtractWidget::on_btnAudition_clicked()
{
    if(!m_videoPlayer)
    {
        return;
    }
    m_audition = false;

    int value = ui->progressSlider->GetLowerValue();
    m_videoPlayer->Seek((double)value/m_videoPlayer->totalMs);
    ui->progressSlider->setValue(value);

    if(!ui->playCheck->isChecked())
    {
        ui->playCheck->setChecked(true);
        player_timer_id = startTimer(500);
    }

    m_audition = true;


//    hudun::sensors:: HdEventClick clickInfo;
//    clickInfo.clickType = hudun::sensors:: HdEventClick::ClickType::Button;
//    if(m_eClipType == ClipType::AUDIO)
//    {
//        clickInfo.name = u8"试听";
//        clickInfo.title = u8"音频剪切";
//    }
//    else
//    {
//        clickInfo.name = u8"预览";
//        clickInfo.title = u8"音频提取";
//    }
//    hudun::sensors::HdContextProperties hdContextProperties;
//    hudun::sensors::SensorsTracker::getInstance()->trackClick(clickInfo,hdContextProperties);
}

void ExtractWidget::on_rbtnStart_clicked()
{
    int value = ui->progressSlider->getCurrentValue();
    ui->progressSlider->SetLowerValue(value);
}

void ExtractWidget::on_rbtnend_clicked()
{
    int value = ui->progressSlider->getCurrentValue();
    ui->progressSlider->SetUpperValue(value);
}


void ExtractWidget::closeTimeEditTip()
{
    QObject *obj = sender();
    if(qobject_cast<ClueFrame*>(obj) == clueFrame)
    {
        UserInfoManager::Instance()->SetClueTip(true);
        clueFrame->close();
        clue_timer_id =-1;
        killTimer(clue_timer_id);
    }
    else /*if(qobject_cast<ClueFrame*>(obj) == timeTipFrame)*/
    {
        UserInfoManager::Instance()->setIs_time_edittip(true);
        if(timeTipFrame)
        {
            timeTipFrame->close();
            killTimer(timetip_timer_id);
            timetip_timer_id =-1;
        }
    }


}
