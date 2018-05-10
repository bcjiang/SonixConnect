#pragma once

#include "ui_texo.h"

class TexoDemo : public QMainWindow, private Ui::MainWindow
{
    Q_OBJECT

public:
    TexoDemo(QWidget* parent = 0);
    virtual ~TexoDemo();

private:
    void setupControls();
    void initDisplay(int scanLines, int samples);
    void readProbes();
    bool programSequence();
    bool programLookUpTable(int* o_numProgrammedLines);
    bool programSequenceFromLookUpTable();

private:
    int m_cineSize;
    int m_scanlines;
    int m_samplesPerLine;
    int m_channels;

    QString m_dataPath;

private slots:
    void initHardware();
    void runImage();
    void stopImage();
    void getDataPath();
    void setCineSize();
    void selectProbe1();
    void selectProbe2();
    void selectProbe3();
    void cineScroll(int);
    void onApplyParams();
    void onSave();
    void onZoom();
    void onAmp(int);
    void onChannelDisplay(int);
    void onSampleDisplay(int);
    void onChannelStart(int);
    void newData();
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
