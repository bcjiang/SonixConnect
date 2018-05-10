#pragma once

#include <QMutex>
#include <QThread>
#include <daq_def.h>
#include "ui_texo.h"

class texoParams
{
public:
    // gather data from parameters control
    int numLines;
    int angle;
    int vsound;
    int freq;
    int txaperture;
    int power;
    QString pulse;
    int focus;

    int rxaperture;
    int gain;
    int depth;
    int delay;
    int decim;
    bool bfdelay;
    int planeWave;

    int windowType;
    int fnumber;

    int demoInd;
};

class amplioParams
{
public:
    // IQ demodulation params
    int samplingFreq;
    int startFreq;
    int stopFreq;
    int cutoffFreq;
    // Compression table params
    int useCompression;
    int reject;
    int dynamicRg;
    int decimation;
};

class daqParams
{
public:
    // sequencer params
    unsigned int channels[4];
    int gainDelay;
    int gainOffset;
    int rxDelay;
    int decimation;
    int freeRun;
    int hpfBypass;
    int divisor;           // data size = 16GB / 2^divisor
    int externalTrigger;    // sync with Texo transmits
    int externalClock;      // set to true if external clock is provided
    int fixedTGC;
    int lnaGain;            // 0:16dB, 1:18dB, 2:21dB
    int pgaGain;            // 0:21dB, 1:24dB, 2:27dB, 3:30dB
    int biasCurrent;        // 0,1,2,...,7
    // beamforming params
    int pitch;
    int SOS;
    int Fs;
    double Fnumber;
    int maxApr;
    int digitalGain;
};

class TexoDemo : public QMainWindow, private Ui::MainWindow
{
    Q_OBJECT

public:
    TexoDemo(QWidget* parent = 0);
    virtual ~TexoDemo();
    static int onData(void*, unsigned char*, int);
    static void daqCallback(void*, int, ECallbackSources);

private:
    void setupControls();
    void readProbes();
    bool programSequence();
    bool programSequenceDAQ();
    int loadDaqData(char* path, int frame);
    void displayDAQ();
    void incDaqCounter();
    bool initAmplio();
    bool initDAQ();
    bool initTexo();
    void imageFormerTexo();
    void imageFormerDAQ();
    void allocateTexoBuffers(int szRF, int szB);
    void allocateDAQBuffers(int szRF, int szB);
    void reloadParams();
    void displayTexo();
    void beamformDAQ();
    class Processor : public QThread
    {
public:
        Processor(TexoDemo* inp) { m_txo = inp; }
        void run();

private:
        TexoDemo* m_txo;
    };

private:
    int m_cineSize;
    int m_scanlines;
    int m_samplesPerLine;
    int m_BsamplesPerLine;
    int m_display;
    int m_channels;
    QString m_texoFwPath;
    QString m_daqFwPath;
    int m_daqFrCount;
    char m_daqpath[256];
    int m_daqchannels;
    int m_daqsamplesPerLine;
    bool m_daq80MHzSampling;

    bool m_daqShouldBeRunning;

    short* m_rf;    // acq thread
    short* m_daqdata;
    short* m_bfdaq;

    unsigned char* m_bTexo;
    unsigned char* m_bDaq;

    bool m_runBeamforming;
    bool m_runAmplio;

    bool m_requiresUpdate;

    // Acq/worker threads params
    QMutex m_dispMutex;
    QMutex m_amplioMutex;

    bool m_texoIsRunning;
    Processor m_workerProcessor;

    int m_amp;
    texoParams m_txoParams;
    amplioParams m_amplioParams;
    daqParams m_daqParams;
    int m_daqDownloadingMode;

private slots:
    void initHardware();
    void runImage();
    void stopImage();
    void getTexoFirmwarePath();
    void getDaqFirmwarePath();
    void setCineSize();
    void selectProbe1();
    void selectProbe2();
    void cineScroll(int);
    void onApplyParams();
    void onSave();
    void onAmp(int);
    void onChannelDisplay(int);
    void runDAQ();
};

// taken from uFileHeader.h
class uFileHeader
{
public:
    /// data type - data types can also be determined by file extensions
    int type;
    /// number of frames in file
    int frames;
    /// width - number of vectors for raw data, image width for processed data
    int w;
    /// height - number of samples for raw data, image height for processed data
    int h;
    /// data sample size in bits
    int ss;
    /// roi - upper left (x)
    int ulx;
    /// roi - upper left (y)
    int uly;
    /// roi - upper right (x)
    int urx;
    /// roi - upper right (y)
    int ury;
    /// roi - bottom right (x)
    int brx;
    /// roi - bottom right (y)
    int bry;
    /// roi - bottom left (x)
    int blx;
    /// roi - bottom left (y)
    int bly;
    /// probe identifier - additional probe information can be found using this id
    int probe;
    /// transmit frequency
    int txf;
    /// sampling frequency
    int sf;
    /// data rate - frame rate or pulse repetition period in Doppler modes
    int dr;
    /// line density - can be used to calculate element spacing if pitch and native # elements is known
    int ld;
    /// extra information - ensemble for color RF
    int extra;
};
