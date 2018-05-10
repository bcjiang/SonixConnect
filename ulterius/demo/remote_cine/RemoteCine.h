#pragma once

#include "ui_ulterius.h"

class ulterius;

class RemoteCine : public QMainWindow, private Ui::MainWindow
{
    Q_OBJECT

public:
    RemoteCine(QWidget* parent = 0);
    virtual ~RemoteCine();

private:
    void createFile();
    void closeFile();
    void writeHeader();

    static bool paramCallback(void* paramID, int x, int y);
    static bool onNewData(void* data, int type, int sz, bool cine, int frmnum);

private:
    FILE*   m_cine;
    QString m_server;
    ulterius* m_ulterius;
    
    size_t  m_firstFrame;
    size_t  m_lastFrame;
    int     m_selRow;
    QString m_fileName;
    
    size_t  m_frameSize;
    char*   m_blankFrame;

private slots:
    void onServerAddress();
    void onConnect(bool);
    void onRefresh();
    void onFreezeUnfreeze();
    void onDataSelect(QTableWidgetItem *);
    void onStatusUpdate(QString);
    void onHandleSizeMismatch();
};
