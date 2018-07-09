#pragma once

#include "ui_ulterius.h"
#include <fftw3.h>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <math.h> 

#include <string>
#include <ctime>

class ulterius;
class porta;

class UlteriusDemo : public QMainWindow, private Ui::MainWindow
{
    Q_OBJECT

public:
    UlteriusDemo(QWidget* parent = 0);
    virtual ~UlteriusDemo();

public slots:
	void processFrame(QImage BModeImage, const int& type, const int& sz, const int& frmnum);

private:
    void setupControls();
    void refreshParamValue();

    static bool paramCallback(void* paramID, int x, int y);
    static bool onNewData(void* data, int type, int sz, bool cine, int frmnum);

private:

    QString m_server;
    ulterius* m_ulterius;

private slots:
    void onServerAddress();
    void onConnect(bool);
    void onFreeze(bool);
    void onRefresh();
    void onProbe1();
    void onProbe2();
    void onProbe3();
    void onSelectMode(int);
    void onSelectPreset(QString);
    void onSelectParam();
    void onIncParam();
    void onDecParam();
    void onDataClicked(int, int);
    void onSharedMemoryToggle(bool);
    void onInjectToggle(bool);
    void onInjectRandom();
};
