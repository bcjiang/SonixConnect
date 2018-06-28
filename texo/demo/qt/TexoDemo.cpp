#include "stdafx.h"
#include "TexoDemo.h"
#include <QAtomicPointer>
#include <vector>

#define MSG_TIMEOUT 3000     // ms
#define CINE_SIZE   128     // MB

namespace
{
    template< typename T >
    class DoubleBuffer
    {
public:
        DoubleBuffer(T* first, T* second) : read(first), dirty(second)
        {
        }
        T* startRead()
        {
            return written.fetchAndStoreOrdered(nullptr);
        }
        void doneRead(T* pointer)
        {
            read.fetchAndStoreOrdered(pointer);
        }
        T* startWrite()
        {
            auto writeBuf = dirty;
            dirty = read.fetchAndStoreOrdered(nullptr);
            if (!writeBuf)
            {
                std::swap(writeBuf, dirty);
            }
            return writeBuf;
        }
        bool commitWrite(T* pointer)
        {
            if (written.testAndSetOrdered(nullptr, pointer))
            {
                return true;
            }
            dirty = pointer;
            return false;
        }
private:
        QAtomicPointer< T > written;
        QAtomicPointer< T > read;
        T* dirty;
    };
    std::vector< unsigned char > g_buffer1;
    std::vector< unsigned char > g_buffer2;
    DoubleBuffer< std::vector< unsigned char > > g_buffer(&g_buffer1, &g_buffer2);
    // callback for new frames
    int texoCallback(void* prm, unsigned char* data, int)
    {
        auto writeBuf = g_buffer.startWrite();
        if (writeBuf)
        {
            memcpy(writeBuf->data(), data, writeBuf->size());
            if (g_buffer.commitWrite(writeBuf))
            {
                QMetaObject::invokeMethod(static_cast< TexoDemo* >(prm), "newData", Qt::QueuedConnection);
            }
        }
        return 1;
    }
}

TexoDemo::TexoDemo(QWidget* parent) : QMainWindow(parent)
{
    setupUi(this);

    m_dataPath = DATA_PATH;
    m_cineSize = CINE_SIZE;
    m_scanlines = 128;
    m_channels = 32;
    m_samplesPerLine = 0;

    setupControls();
}

TexoDemo::~TexoDemo()
{
    texoSetCallback(nullptr, nullptr);
    texoShutdown();
}

// create action groups so menus can be checkable
void TexoDemo::setupControls()
{
    QActionGroup* agMain = new QActionGroup(this);
    agMain->addAction(mMain2);
    agMain->addAction(mMain3);
    agMain->addAction(mMain4);

    QActionGroup* agPCI = new QActionGroup(this);
    agPCI->addAction(mPCI2);
    agPCI->addAction(mPCI3);
    agPCI->addAction(mPCI4);
}

void TexoDemo::initDisplay(int scanLines, int samples)
{
    g_buffer1.resize(scanLines * samples * 2);
    g_buffer2.resize(scanLines * samples * 2);

    // 1) init channel display
    wDisplay->init(scanLines, samples);

    // 2) init image display
    bool transpose = (wParams->item(15, 0)->checkState() == Qt::Unchecked) ? false : true;
    iDisplay->init(scanLines, samples, transpose);

    // init amplification for both display
    onAmp(wAmp->value());
    // set the channel slider
    wChannelDisplay->setRange(0, scanLines - 1);
    wChannelDisplay->setValue(0);
    wNumChannels->setRange(1, scanLines);
    wNumChannels->setValue(8);
    // set the sample slider
    wSampleDisplay->setRange(0, samples - 1);
    wSampleDisplay->setValue(0);
}

// allow user to change data path
void TexoDemo::getDataPath()
{
    bool ok = false;
    QString text = QInputDialog::getText(this, tr("Enter Data Path"), tr("Data Path:"), QLineEdit::Normal, m_dataPath, &ok);

    if (ok && !text.isEmpty())
    {
        m_dataPath = text;
    }
}

// allow user to change cine size
void TexoDemo::setCineSize()
{
    bool ok = false;
    int cine = QInputDialog::getInt(this, tr("Enter Cine Size"), tr("Cine Size (MB):"), m_cineSize, 32, 512, 1, &ok);

    if (ok)
    {
        m_cineSize = cine;
    }
}

// initialize the ultrasound engine, populate probes, load lists
void TexoDemo::initHardware()
{
    int i;
    int usm = mMain2->isChecked() ? 2 : (mMain3->isChecked() ? 3 : 4);
    int pci = mPCI2->isChecked() ? 2 : (mPCI3->isChecked() ? 3 : 4);
    m_channels = (usm == 2) ? 32 : 64;
    bool activated = false;

    wStatus->showMessage(tr("Initializing ultrasound"), MSG_TIMEOUT);

    if (!texoInit(m_dataPath.toAscii(), pci, usm, 0, m_channels, 0, m_cineSize))
    {
        QMessageBox mb;
        mb.setText(tr("Texo could not initialize ultrasound"));
        mb.exec();
        return;
    }
    else
    {
        readProbes();
        wStatus->showMessage(tr("Successfully intitialized ultrasound"), MSG_TIMEOUT);

        texoSetCallback(texoCallback, this);
    }

    for (i = 0; i < 3; i++)
    {
        if (texoGetProbeCode(i) != 31)
        {
            texoActivateProbeConnector(i);
            activated = true;
            break;
        }
    }

    if (activated)
    {
        //if (!programSequence())
        if (!programSequenceFromLookUpTable())
        {
            wStatus->showMessage(tr("Error Programming Sequence"));
        }
    }
}

void TexoDemo::newData()
{
    auto latest = g_buffer.startRead();
    if (latest)
    {
        wDisplay->display(latest->data());
        iDisplay->setImgData(latest->data());
        g_buffer.doneRead(latest);
    }
}

// start imaging
void TexoDemo::runImage()
{
    if (texoIsInitialized() && texoRunImage())
    {
        wStatus->showMessage(tr(QString("Imaging Running @ %1Hz").arg(texoGetFrameRate()).toAscii()));
    }
}

// stop imaging
void TexoDemo::stopImage()
{
    if (texoIsInitialized() && texoStopImage())
    {
        wStatus->showMessage(tr("Imaging Stopped"));
        wCine->setRange(0, texoGetCollectedFrameCount() - 1);
        wCine->setValue(wCine->maximum());
    }
}

// read probes and setup controls
void TexoDemo::readProbes()
{
    int i;
    char name[80];
    QAbstractButton* wProbe = 0;

    for (i = 0; i < 3; i++)
    {
        wProbe = (i == 0) ? wProbe1 : ((i == 2) ? wProbe2 : wProbe3);

        if (texoGetProbeName(i, name, 80))
        {
            wProbe->setText(name);
            wProbe->setEnabled(true);
        }
        else
        {
            wProbe->setText("");
            wProbe->setEnabled(false);
        }
    }
}

// select first probe
void TexoDemo::selectProbe1()
{
    if (texoActivateProbeConnector(0))
    {
        wStatus->showMessage(tr("Selected first probe connector"), MSG_TIMEOUT);
        //if (!programSequence())
        if (!programSequenceFromLookUpTable())
        {
            wStatus->showMessage(tr("Error Programming Sequence"));
        }
    }
}

// select second probe
void TexoDemo::selectProbe2()
{
    if (texoActivateProbeConnector(2))
    {
        wStatus->showMessage(tr("Selected second probe connector"), MSG_TIMEOUT);
        //if (!programSequence())
        if (!programSequenceFromLookUpTable())
        {
            wStatus->showMessage(tr("Error Programming Sequence"));
        }
    }
}

// select third probe
void TexoDemo::selectProbe3()
{
    if (texoActivateProbeConnector(1))
    {
        wStatus->showMessage(tr("Selected third probe connector"), MSG_TIMEOUT);
        //if (!programSequence())
        if (!programSequenceFromLookUpTable())
        {
            wStatus->showMessage(tr("Error Programming Sequence"));
        }
    }
}

void TexoDemo::cineScroll(int index)
{
    bool running = texoIsImaging() != 0;

    if (running)
    {
        fprintf(stderr, "cine scroll is only available when imaging is stopped.\n");
        return;
    }

    unsigned char* cine = texoGetCineStart(0);
    cine += (texoGetFrameSize() * index);
    wDisplay->display(reinterpret_cast< char* >(cine));
    iDisplay->setImgData(reinterpret_cast< char* >(cine));
}

void TexoDemo::onAmp(int value)
{
    wDisplay->setAmp(value / 255.0);
    iDisplay->setAmp(value / 255.0);
}

void TexoDemo::onChannelDisplay(int start)
{
    wDisplay->setStartChDisplay(start);
}

void TexoDemo::onSampleDisplay(int start)
{
    wDisplay->setStartSmpleDisplay(start);
}

void TexoDemo::onApplyParams()
{
    bool running = texoIsImaging() != 0;

    if (running)
    {
        texoStopImage();
    }

    //if (!programSequence())
    if (!programSequenceFromLookUpTable())
    {
        wStatus->showMessage(tr("Error Programming Sequence"));
    }

    if (running)
    {
        texoRunImage();
        wStatus->showMessage(tr(QString("Imaging Running @ %1Hz").arg(texoGetFrameRate()).toAscii()));
    }
}

bool TexoDemo::programLookUpTable(int* o_numProgrammedLines)
{
    _texoTransmitParams tx;
    _texoReceiveParams rx;

    // gather data from parameters control
    // Tx only
    int txaperture = wParams->item(4, 0)->text().toInt();
    int focus = wParams->item(7, 0)->text().toInt();
    int freq = wParams->item(3, 0)->text().toInt();
    QString pulse = wParams->item(6, 0)->text();
    // shared between Tx and Rx
    int numLines = wParams->item(0, 0)->text().toInt();
    int angle = wParams->item(1, 0)->text().toInt();
    int vsound = wParams->item(2, 0)->text().toInt();
    // Rx only
    int rxaperture = wParams->item(8, 0)->text().toInt();
    bool bfdelay = (wParams->item(13, 0)->checkState() == Qt::Unchecked) ? false : true;

    int numelements = texoGetProbeNumElements();
    double virtualElmStep = double(numelements) / double(numLines);

    // create the sequence
    *o_numProgrammedLines = 0;
    for (int i = 0; i < numLines; i++)
    {
        // set the transmit parameters in the LUT
        tx.centerElement = (i + 0.5) * virtualElmStep;
        tx.angle = angle;
        tx.aperture = txaperture;
        tx.focusDistance = focus * 1000;
        tx.frequency = freq * 1000 * 1000;
        tx.speedOfSound = vsound;
        tx.useManualDelays = 0;
        tx.useMask = 0;
        strcpy_s(tx.pulseShape, pulse.toLocal8Bit());
        // Programming the LUT's of transmit
        if (!texoAddTransmit(tx))
        {
            return false;
        }

        // set the receive parameters in the LUT
        rx.centerElement = (i + 0.5) * virtualElmStep;
        rx.aperture = rxaperture;
        rx.angle = angle * 1000;
        rx.speedOfSound = vsound;
        rx.numChannels = m_channels;
        rx.applyFocus = bfdelay;
        rx.useCustomWindow = 0;
        rx.weightType = 0;
        rx.fnumber = 0;
        rx.maxApertureDepth = 30000;
        rx.rxAprCrv.top = 0;
        rx.rxAprCrv.mid = 0;
        rx.rxAprCrv.vmid = 0;
        rx.rxAprCrv.btm = 0;

        // Programming the LUT's of receive
        if (!texoAddReceive(rx))
        {
            return false;
        }

        *o_numProgrammedLines = *o_numProgrammedLines + 1;
    }
    return true;
}

bool TexoDemo::programSequenceFromLookUpTable()
{
    _texoTransmitParams tx;
    _texoReceiveParams rx;
    _texoLineInfo li = { 0 };

    if (!texoIsInitialized())
    {
        return false;
    }

    // gather data from parameters control
    int power = wParams->item(5, 0)->text().toInt();
    int gain = wParams->item(9, 0)->text().toInt();

    int depth = wParams->item(10, 0)->text().toInt();
    int vsound = wParams->item(2, 0)->text().toInt();
    int decim = wParams->item(12, 0)->text().toInt();
    int delay = wParams->item(11, 0)->text().toInt();
    bool channelMaskOn = (wParams->item(14, 0)->checkState() == Qt::Unchecked) ? false : true;

    bool bfdelay = (wParams->item(13, 0)->checkState() == Qt::Unchecked) ? false : true;

    // setup gain and power
    texoClearTGCs();
    texoAddTGCFixed(gain / 100.0);
    texoSetPower(power, 15, 15);
    texoSetSyncSignals(0, 1, 3);

    //setup VCA
    _vcaInfo vcaInfo;
    vcaInfo.amplification = 1;
    vcaInfo.activetermination = 4;
    vcaInfo.inclamp = 1600;
    vcaInfo.LPF = 4;
    vcaInfo.lnaIntegratorEnable = 0;
    vcaInfo.pgaIntegratorEnable = 0;
    vcaInfo.hpfDigitalEnable = 1;
    vcaInfo.hpfDigitalValue = 2;
    texoSetVCAInfo(vcaInfo);

    // start a new sequence
    if (!texoBeginSequence())
    {
        return false;
    }

    int numLines;
    if (!programLookUpTable(&numLines))
    {
        return false;
    }

    // create the sequence
    for (int i = 0; i < numLines; i++)
    {
        // Setting the transmit parameters, not addressed in the LUT
        tx.tableIndex = i;
        tx.sync = 1;
        tx.txRepeat = 0;
        tx.txDelay = 100;

        // Setting the transmit parameters, not addressed in the LUT
        rx.maxApertureDepth = 30000;    // Note that this parameter needs to be repeated
        rx.acquisitionDepth = depth * 1000;
        rx.saveDelay = delay * 1000;
        rx.rxBeamFormingDelay = 2500; // ns
        rx.channelMask[0] = rx.channelMask[1] = 0xFFFFFFFF;
        rx.applyFocus = bfdelay;        // Note that this parameter needs to be repeated
        rx.lgcValue = 0;
        rx.decimation = decim;
        rx.tgcSel = 0;
        rx.customLineDuration = 0;
        rx.speedOfSound = vsound;       // Note that this parameter needs to be repeated
        rx.tableIndex = i;
        rx.rxAprCrv.top = 0;            // Note that this parameter needs to be repeated
        rx.rxAprCrv.mid = 0;            // Note that this parameter needs to be repeated
        rx.rxAprCrv.vmid = 0;           // Note that this parameter needs to be repeated
        rx.rxAprCrv.btm = 0;            // Note that this parameter needs to be repeated

        if (channelMaskOn)
        {
            int c = i % m_channels;
            rx.channelMask[0] = (c < 32) ? (1 << c) : 0;
            rx.channelMask[1] = (c >= 32) ? (1 << (c - 32)) : 0;
        }

        if (!texoAddLine(tx, rx, li))
        {
            return false;
        }
    }

    // finish the sequence
    if (texoEndSequence() == -1)
    {
        return false;
    }

    // save the sequencer params
    m_scanlines = numLines;
    m_samplesPerLine = li.lineSize / 2;

    // setup the display based on the sequencer parameters
    initDisplay(m_scanlines, m_samplesPerLine);

    return true;
}

bool TexoDemo::programSequence()
{
    _texoTransmitParams tx;
    _texoReceiveParams rx;
    _texoLineInfo li = { 0 };

    if (!texoIsInitialized())
    {
        return false;
    }

    // gather data from parameters control
    int numLines = wParams->item(0, 0)->text().toInt();
    int angle = wParams->item(1, 0)->text().toInt();
    int vsound = wParams->item(2, 0)->text().toInt();
    int freq = wParams->item(3, 0)->text().toInt();
    int txaperture = wParams->item(4, 0)->text().toInt();
    int power = wParams->item(5, 0)->text().toInt();
    QString pulse = wParams->item(6, 0)->text();
    int focus = wParams->item(7, 0)->text().toInt();

    int rxaperture = wParams->item(8, 0)->text().toInt();
    int gain = wParams->item(9, 0)->text().toInt();
    int depth = wParams->item(10, 0)->text().toInt();
    int delay = wParams->item(11, 0)->text().toInt();
    int decim = wParams->item(12, 0)->text().toInt();
    bool bfdelay = (wParams->item(13, 0)->checkState() == Qt::Unchecked) ? false : true;
    bool channelMaskOn = (wParams->item(14, 0)->checkState() == Qt::Unchecked) ? false : true;

    // setup gain and power
    texoClearTGCs();
    texoAddTGCFixed(gain / 100.0);
    texoSetPower(power, 15, 15);
    texoSetSyncSignals(0, 1, 3);

    //setup VCA
    _vcaInfo vcaInfo;
    vcaInfo.amplification = 1;
    vcaInfo.activetermination = 4;
    vcaInfo.inclamp = 1600;
    vcaInfo.LPF = 4;
    vcaInfo.lnaIntegratorEnable = 0;
    vcaInfo.pgaIntegratorEnable = 0;
    vcaInfo.hpfDigitalEnable = 1;
    vcaInfo.hpfDigitalValue = 2;
    texoSetVCAInfo(vcaInfo);

    // start a new sequence
    if (!texoBeginSequence())
    {
        return false;
    }

    // set transmit parameters
    tx.angle = angle;
    tx.aperture = txaperture;
    tx.focusDistance = focus * 1000;
    tx.frequency = freq * 1000 * 1000;
    tx.speedOfSound = vsound;
    tx.useManualDelays = 0;
    tx.useMask = 0;
    tx.tableIndex = -1;
    tx.sync = 1;
    strcpy_s(tx.pulseShape, pulse.toLocal8Bit());
    tx.txRepeat = 0;
    tx.txDelay = 100;

    // set receive parameters
    rx.angle = angle * 1000;
    rx.aperture = rxaperture;
    rx.maxApertureDepth = 30000;
    rx.acquisitionDepth = depth * 1000;
    rx.saveDelay = delay * 1000;
    rx.rxBeamFormingDelay = 2500; // ns
    rx.speedOfSound = vsound;
    rx.channelMask[0] = rx.channelMask[1] = 0xFFFFFFFF;
    rx.applyFocus = bfdelay;
    rx.decimation = decim;
    rx.customLineDuration = 0;
    rx.lgcValue = 0;
    rx.tgcSel = 0;
    rx.tableIndex = -1;
    rx.numChannels = m_channels;
    rx.useCustomWindow = 0;
    rx.weightType = 1;      // hanning
    rx.fnumber = 20;        // fnumber = 2.0

    int numelements = texoGetProbeNumElements();
    double virtualElmStep = double(numelements) / double(numLines);

    // create the sequence
    for (int i = 0; i < numLines; i++)
    {
		// Control the elements to use in forming the line
		if (i> 31 && i<=95 && i%2==0)
			continue;

        tx.centerElement = (i + 0.5) * virtualElmStep;
        rx.centerElement = (i + 0.5) * virtualElmStep;

        if (channelMaskOn)
        {
            int c = i % m_channels;
            rx.channelMask[0] = (c < 32) ? (1 << c) : 0;
            rx.channelMask[1] = (c >= 32) ? (1 << (c - 32)) : 0;
        }

        if (!texoAddLine(tx, rx, li))
        {
            return false;
        }
    }

    // finish the sequence
    if (texoEndSequence() == -1)
    {
        return false;
    }

    // save the sequencer params
    m_scanlines = numLines;
    m_samplesPerLine = li.lineSize / 2;

    // setup the display based on the sequencer parameters
    initDisplay(m_scanlines, m_samplesPerLine);

    return true;
}

void TexoDemo::onSave()
{
    bool running = texoIsImaging() != 0;

    if (running)
    {
        fprintf(stderr, "saving is only available when imaging is stopped.\n");
        return;
    }

    int numFrames = texoGetCollectedFrameCount();
    int frameSize = texoGetFrameSize();

    if (numFrames < 1)
    {
        fprintf(stderr, "no frame available\n");
        return;
    }

    if (frameSize < 1)
    {
        fprintf(stderr, "frame size is zero\n");
        return;
    }

    FILE* fp = fopen("TexoData.rf", "wb");
    if (!fp)
    {
        fprintf(stderr, "could not open the file\n");
        return;
    }

    fprintf(stdout, "Storing data in TexoData.rf:\n\n");
    fprintf(stdout, "-number of RF scan lines = %d \n", m_scanlines);
    fprintf(stdout, "-number of RF samples = %d \n", m_samplesPerLine);
    fprintf(stdout, "-frame size = %d bytes\n", frameSize);
    fprintf(stdout, "-number of frames = %d \n\n", numFrames);

    // 1) populate the header for .rf file
    int decim = wParams->item(12, 0)->text().toInt();

    uFileHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.frames = numFrames;
    hdr.w = m_scanlines;
    hdr.h = m_samplesPerLine;
    hdr.sf = 40000000 >> decim;  // sampling freq
    hdr.ss = 16;         // short is 2 bytes
    hdr.type = 16;       // rf data
    hdr.ld = m_scanlines;
    fwrite(&hdr, sizeof(hdr), 1, fp);

    // 2) write the actual frames
    fwrite(texoGetCineStart(0), frameSize, numFrames, fp);

    fclose(fp);

    wStatus->showMessage(tr("Successfully stored data"));

    return;
}

void TexoDemo::onZoom()
{
    int ind = wZoomFactor->currentIndex();
    if (ind < 0)
    {
        ind = 0;
    }

    if (ind < 10)
    {
        wDisplay->setZoomFactor(ind + 1);
    }
    else if (ind == 10)
    {
        wDisplay->setZoomFactor(100);
    }
    else
    {
        wDisplay->setZoomFactor(1000);
    }
}

void TexoDemo::onChannelStart(int value)
{
    wDisplay->setNumChDisplay(value);
}
