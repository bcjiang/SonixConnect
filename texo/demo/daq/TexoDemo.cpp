#include "stdafx.h"
#include "TexoDemo.h"

#define MSG_TIMEOUT   3000          // ms
#define CINE_SIZE     128           // MB
#define MAXDAQFOLDERS 30            // number of DAQ downloads
#define DAQFOLDER     "D:/DAQRT/"

template< class T >
inline T Saturate(T value, T low, T high)
{
    if (value < low)
    {
        return low;
    }
    else if (value > high)
    {
        return high;
    }
    else
    {
        return value;
    }
}

TexoDemo::TexoDemo(QWidget* parent) : QMainWindow(parent), m_workerProcessor(this)
{
    setupUi(this);

    m_texoFwPath = FIRMWARE_PATH;
    m_daqFwPath = DAQ_FIRMWARE_PATH;
    m_cineSize = CINE_SIZE;
    m_scanlines = 64;
    m_channels = 32;
    m_display = m_scanlines;
    m_samplesPerLine = 0;
    m_BsamplesPerLine = 0;
    m_rf = 0;   // used in the Acq thread
    m_bTexo = 0;
    m_bDaq = 0;

    m_texoIsRunning = false;    // used in the worker thread
    m_runAmplio = true;         // turns the Texo image former on or off
    m_runBeamforming = false;    // turns the DAQ beamformer on or off
    m_amp = 100;                // scale rf data for display

    // daq params
    m_daqFrCount = 0;
    strcpy(m_daqpath, DAQFOLDER);
    CreateDirectoryA(m_daqpath, 0);
    m_daqchannels = 128;
    m_daqsamplesPerLine = 2080;
    m_daq80MHzSampling = false;
    m_daqdata = 0;
    m_bfdaq = 0;
    m_daqDownloadingMode = 0;
    m_requiresUpdate = true;

    setupControls();

    daqSetCallback(daqCallback, this);
}

TexoDemo::~TexoDemo()
{
    // texo
    if (m_rf)
    {
        delete[] m_rf;
    }

    if (m_bTexo)
    {
        delete[] m_bTexo;
    }
    texoShutdown();

    // daq
    if (m_daqdata)
    {
        delete[] m_daqdata;
    }

    if (m_bfdaq)
    {
        delete[] m_bfdaq;
    }

    if (m_bDaq)
    {
        delete[] m_bDaq;
    }

    if (daqIsRunning())
    {
        daqStop();
    }

    daqReleaseMem();
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

    // progress bar
    wProgress->setRange(0, 100);
    wProgress->setValue(0);
}

// allow user to change data path
void TexoDemo::getTexoFirmwarePath()
{
    bool ok = false;
    QString text = QInputDialog::getText(this, tr("Enter Texo Firmware Path"), tr("Path:"), QLineEdit::Normal, m_texoFwPath, &ok);

    if (ok && !text.isEmpty())
    {
        m_texoFwPath = text;
    }
}

// allow user to change data path
void TexoDemo::getDaqFirmwarePath()
{
    bool ok = false;
    QString text = QInputDialog::getText(this, tr("Enter DAQ Firmware Path"), tr("Path:"), QLineEdit::Normal, m_daqFwPath, &ok);

    if (ok && !text.isEmpty())
    {
        m_daqFwPath = text;
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
    // initialize Texo
    bool texoReady = initTexo();
    // initialize DAQ
    bool daqReady = initDAQ();

    // load parameters
    reloadParams();

    // Need to program the texo sequence fist (in case DAQ is waiting for external clk)
    if (texoReady)
    {
        programSequence();
    }

    // load DAQ sequence
    if (daqReady)
    {
        if (!programSequenceDAQ())
        {
            wStatus->showMessage(tr("Loading DAQ Sequence Failed"), MSG_TIMEOUT);
        }
        else
        {
            wStatus->showMessage(tr("Successfully loaded the DAQ"), MSG_TIMEOUT);
        }
    }
}

bool TexoDemo::initTexo()
{
    bool texoReady = false;

    int usm = mMain2->isChecked() ? 2 : (mMain3->isChecked() ? 3 : 4);
    int pci = mPCI2->isChecked() ? 2 : (mPCI3->isChecked() ? 3 : 4);
    m_channels = (usm == 2) ? 32 : 64;

    //////////////////////// initialize Texo  ////////////////////////////
    wStatus->showMessage(tr("Initializing ultrasound ..."), MSG_TIMEOUT);
    if (!texoInit(m_texoFwPath.toAscii(), pci, usm, 0, m_channels, 0, m_cineSize))
    {
        wStatus->showMessage(tr("Could not initialize ultrasound"), MSG_TIMEOUT);
        return texoReady;
    }
    else
    {
        wStatus->showMessage(tr("Successfully initialized ultrasound"), MSG_TIMEOUT);
        readProbes();

        texoSetCallback(onData, this);
    }

    // initially select a probe - assume DAQ plugged in on 3rd port - which is i=1
    for (int i = 0; i < 3; i++)
    {
        if (i == 1)
        {
            continue;
        }

        if (texoGetProbeCode(i) != 31)
        {
            if (texoSelectProbe(texoGetProbeCode(i)))
            {
                texoActivateProbeConnector(i);
            }
            texoReady = true;
            break;
        }
    }

    if (!texoReady)
    {
        wStatus->showMessage(tr("No probe found on the first 2 ports."), MSG_TIMEOUT);
    }

    return texoReady;
}

bool TexoDemo::initDAQ()
{
    bool daqReady = false;
    wStatus->showMessage(tr("Initializing DAQ ... This may take few minutes ..."), MSG_TIMEOUT);
    daqSetFirmwarePath(m_daqFwPath.toAscii().constData());

    char err[256];
    if (!daqDriverInit())
    {
        daqGetLastError(err, 256);
        wStatus->showMessage(QString(tr("Driver initialization error: %1")).arg(err));
        return daqReady;
    }

    if (!daqIsConnected())
    {
        QMessageBox mb;
        mb.setText(tr("DAQ is not connected or is not powered"));
        mb.exec();
    }
    else
    {
        if (daqIsRunning())
        {
            daqStop();
        }

        if (!daqIsInitialized())
        {
            wStatus->showMessage(tr("Programming DAQ ..."), MSG_TIMEOUT);

            //////////////////////// initialize DAQ ////////////////////////////
            if (!daqInit(m_daq80MHzSampling))
            {
                QMessageBox mb;
                mb.setText(tr("Initializing DAQ Failed"));
                mb.exec();
            }
            else
            {
                wStatus->showMessage(tr("Successfully initialized DAQ"), MSG_TIMEOUT);
                wProgress->setValue(wProgress->maximum());
                daqReady = true;
            }
        }
        else
        {
            wStatus->showMessage(tr("DAQ is already initialized"), MSG_TIMEOUT);
            wProgress->setValue(wProgress->maximum());
            daqReady = true;
        }
    }
    return daqReady;
}

// callback for new frames
int TexoDemo::onData(void* prm, unsigned char* data, int)
{
    // copy to local rf frame
    //((TexoDemo*)prm)->m_txoMutex.lock();
    memcpy(((TexoDemo*)prm)->m_rf, data + 4, ((TexoDemo*)prm)->m_scanlines * ((TexoDemo*)prm)->m_samplesPerLine * sizeof(short));
    //((TexoDemo*)prm)->m_txoMutex.unlock();

    return 1;
}

// start imaging
void TexoDemo::runImage()
{
    if (texoIsInitialized() && texoRunImage())
    {
        m_texoIsRunning = true;
        m_workerProcessor.start();
        wStatus->showMessage(tr(QString("Imaging Running @ %1Hz").arg(floor(texoGetFrameRate())).toAscii()));
    }

    if (daqRun())
    {
        m_daqShouldBeRunning = true;
    }
}

// stop imaging
void TexoDemo::stopImage()
{
    if (daqIsRunning())
    {
        daqStop();
    }
    m_daqShouldBeRunning = false;

    if (texoIsInitialized() && texoStopImage())
    {
        m_texoIsRunning = false;
        m_workerProcessor.wait();
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
        if (i == 1)
        {
            continue;
        }

        wProbe = (i == 0) ? wProbe1 : wProbe2;

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
    if (texoSelectProbe(texoGetProbeCode(0)))
    {
        if (texoActivateProbeConnector(0))
        {
            wStatus->showMessage(tr("Selected first probe connector"), MSG_TIMEOUT);
            if (!programSequence())
            {
                wStatus->showMessage(tr("Error Programming Sequence"));
            }
        }
    }
    // open the DAQ connector
    texoForceConnector(3);
}

// select second probe
void TexoDemo::selectProbe2()
{
    if (texoSelectProbe(texoGetProbeCode(2)))
    {
        if (texoActivateProbeConnector(2))
        {
            wStatus->showMessage(tr("Selected second probe connector"), MSG_TIMEOUT);
            if (!programSequence())
            {
                wStatus->showMessage(tr("Error Programming Sequence"));
            }
        }
    }
    // open the DAQ connector
    texoForceConnector(3);
}

void TexoDemo::cineScroll(int index)
{
    if (m_texoIsRunning)
    {
        fprintf(stderr, "cine scroll is only available when imaging is stopped.\n");
        return;
    }

    unsigned char* cine = texoGetCineStart(0);
    cine += (texoGetFrameSize() * index);
    memcpy(m_rf, cine, m_scanlines * m_samplesPerLine * sizeof(short));

    imageFormerTexo();
    displayTexo();
}

void TexoDemo::onAmp(int value)
{
    // set the amplitude
    m_amp = value;
    // regenerate the images
    imageFormerTexo();
    displayTexo();
}

void TexoDemo::onChannelDisplay(int)
{
}

void TexoDemo::onApplyParams()
{
    bool running = texoIsImaging() != 0;

    if (running)
    {
        texoStopImage();
        daqStop();
        m_texoIsRunning = false;
        m_workerProcessor.wait();
    }

    // relaod params
    reloadParams();

    // reload the sequencer
    if (!programSequence())
    {
        wStatus->showMessage(tr("Error Programming Sequence"));
    }

    // reload the DAQ sequencer
    if (!programSequenceDAQ())
    {
        wStatus->showMessage(tr("Error Programming DAQ Sequence"));
    }

    if (running)
    {
        daqRun();
        texoRunImage();
        m_texoIsRunning = true;
        m_workerProcessor.start();
        wStatus->showMessage(tr(QString("Imaging Running @ %1Hz").arg(texoGetFrameRate()).toAscii()));
    }
}

bool TexoDemo::programSequence()
{
    _texoTransmitParams tx;
    _texoReceiveParams rx;
    _texoLineInfo li;

    if (!texoIsInitialized())
    {
        return false;
    }

    // setup a fixed tgc
    texoClearTGCs();
    texoAddTGCFixed((double)m_txoParams.gain / 100.0);
    /*// setup a tgc curve
       _texoCurve *tgccrv = new _texoCurve();
       tgccrv->top=50;
       tgccrv->mid=75;
       tgccrv->btm=100;
       tgccrv->vmid=90;
       texoAddTGC(tgccrv, m_txoParams.depth * 1000, (double)m_txoParams.gain / 100.0);*/

    // setup the power
    texoSetPower(m_txoParams.power, 15, 15);
    // output both sync and clock for the DAQ
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
    tx.angle = m_txoParams.angle * 1000;
    tx.aperture = m_txoParams.txaperture;
    tx.focusDistance = m_txoParams.focus * 1000;
    tx.frequency = m_txoParams.freq * 1000 * 1000;
    tx.speedOfSound = m_txoParams.vsound;
    tx.useManualDelays = 0;
    tx.useMask = 0;
    tx.tableIndex = -1;
    tx.sync = 0;
    strcpy(tx.pulseShape, m_txoParams.pulse.toLocal8Bit());
    tx.txRepeat = 0;
    tx.txDelay = 100;

    // if tx focus is set to zero, use plane wave imaging
    // this can be useful to compare texo images with DAQ images
    if (tx.focusDistance == 0)
    {
        // set plane wave transmit without focusing with full aperture
        tx.aperture = 128;
        tx.useManualDelays = 1;
        memset(tx.manualDelays, 0, sizeof(int) * 128);
    }

    // set receive parameters
    rx.angle = m_txoParams.angle * 1000;
    rx.aperture = (m_txoParams.rxaperture < m_channels) ? m_txoParams.rxaperture : m_channels;
    rx.maxApertureDepth = 30000;
    rx.acquisitionDepth = m_txoParams.depth * 1000;
    rx.saveDelay = m_txoParams.delay * 1000;
    rx.rxBeamFormingDelay = 2500; // ns
    rx.speedOfSound = m_txoParams.vsound;
    rx.channelMask[0] = rx.channelMask[1] = 0xFFFFFFFF;
    rx.applyFocus = m_txoParams.bfdelay;
    rx.useCustomWindow = 0;
    rx.decimation = m_txoParams.decim;
    rx.customLineDuration = 0;
    rx.lgcValue = 0;
    rx.tgcSel = 0;
    rx.tableIndex = -1;
    rx.numChannels = m_channels;
    rx.weightType = m_txoParams.windowType;
    rx.fnumber = m_txoParams.fnumber;

    // line density settings
    m_scanlines = m_txoParams.numLines;

    int numelements = texoGetProbeNumElements();
    double virtualElmStep = double(numelements) / double(m_scanlines);

    // Part 1) create a sequence for standard imaging (no sync no DAQ)
    for (int i = 0; i < m_scanlines; i++)
    {
        tx.centerElement = (i + 0.5) * virtualElmStep;
        rx.centerElement = (i + 0.5) * virtualElmStep;

        if (!texoAddLine(tx, rx, li))
        {
            return false;
        }

        m_samplesPerLine = li.lineSize / 2;
    }

    // Part 2) imaging with the DAQ
    if (m_txoParams.planeWave > 0)
    {
        // turn on the sync for the plane wave to trigger DAQ collection
        tx.sync = 1;
        // do not acquire texo data during the plane wave
        rx.acquisitionDepth = 0;
        // adjust the line duration if triggering DAQ in plane wave mode
        rx.customLineDuration = 100000; // 100 usec

        if (m_txoParams.demoInd == 0)   // set the transmit to plane wave
        {
            // full tx aperture
            tx.aperture = 128;
            tx.useManualDelays = 1;
            memset(tx.manualDelays, 0, sizeof(int) * 128);
            // center the aperture
            tx.centerElement = (numelements / 2) + 0.5;

            for (int i = 0; i < m_txoParams.planeWave; i++)
            {
                if (!texoAddLine(tx, rx, li))
                {
                    return false;
                }
            }
        }
        else if (m_txoParams.demoInd == 1)  // random transmit apertures
        {
            // full tx aperture
            tx.aperture = 128;
            tx.useManualDelays = 1;
            memset(tx.manualDelays, 0, sizeof(int) * 128);
            // center the aperture
            tx.centerElement = (numelements / 2) + 0.5;

            for (int i = 0; i < m_txoParams.planeWave; i++)
            {
                // randomize the transmit
                tx.useMask = 1;
                for (int j = 0; j < numelements; j++)
                {
                    tx.mask[j] = rand() % 2;
                }

                if (!texoAddLine(tx, rx, li))
                {
                    return false;
                }
            }
        }
        else if (m_txoParams.demoInd == 2)  // conventional imaging
        {
            virtualElmStep = double(numelements) / double(m_txoParams.planeWave);
            for (int i = 0; i < m_txoParams.planeWave; i++)
            {
                tx.centerElement = (i + 0.5) * virtualElmStep;
                rx.centerElement = (i + 0.5) * virtualElmStep;

                if (!texoAddLine(tx, rx, li))
                {
                    return false;
                }
            }
        }
        else  // beam steering
        {
            // center the aperture
            tx.centerElement = (numelements / 2) + 0.5;
            tx.aperture = 128;
            tx.useManualDelays = 1;
            float elementPitch = 305.0f;
            float pi = 3.14159f;
            float samplingFreq = 40.0f;

            for (int i = 0; i < m_txoParams.planeWave; i++)
            {
                // steer the transmits
                float angle = (float)(i - m_txoParams.planeWave / 2) * (pi / 180.0f);

                // manualDelays must be all positive, we need to find the minimum value for offsetting - Ali B, May 2014
                int minDelay = 0;
                for (int j = 0; j < 128; j++)
                {
                    tx.manualDelays[j] = (int)((j - tx.centerElement + 1) * elementPitch * sinf(angle) / 1540.0f * samplingFreq);
                    if (j == 0)
                    {
                        minDelay = tx.manualDelays[j];
                    }
                    else
                    {
                        if (tx.manualDelays[j] < minDelay)
                        {
                            minDelay = tx.manualDelays[j];
                        }
                    }
                }
                // offsetting the delays by the minimum value - Ali B, May 2014
                for (int j = 0; j < 128; j++)
                {
                    tx.manualDelays[j] -= minDelay;
                }

                if (!texoAddLine(tx, rx, li))
                {
                    return false;
                }
            }
        }
    }

    // finish the sequence
    if (texoEndSequence() == -1)
    {
        return false;
    }

    m_amplioMutex.lock();
    initAmplio();
    m_requiresUpdate = true;
    m_amplioMutex.unlock();

    // allocate memories
    allocateTexoBuffers(m_scanlines * m_samplesPerLine, m_scanlines * m_BsamplesPerLine);

    // init display
    m_dispMutex.lock();
    texoDisplay->init(m_scanlines, m_BsamplesPerLine);
    m_dispMutex.unlock();

    return true;
}

bool TexoDemo::initAmplio()
{
    // initialize IQ demodulation
    if (!amplioInitRfToB(m_amplioParams.startFreq, m_amplioParams.stopFreq, m_amplioParams.cutoffFreq,
            m_amplioParams.samplingFreq, m_samplesPerLine, m_amplioParams.useCompression, m_amplioParams.dynamicRg, m_amplioParams.reject)) // load the demodulation and compression tables
    {
        fprintf(stderr, "could not initialize Amplio\n");
        return false;
    }

    // rf to bpr conversion parameters
    int decimFactor = 1 << m_amplioParams.decimation;
    m_BsamplesPerLine = m_samplesPerLine / decimFactor;

    return true;
}

void TexoDemo::imageFormerTexo()
{
    short* rfFrame = m_rf;
    unsigned char* bFrame = m_bTexo;

    if (!rfFrame || !bFrame)
    {
        return;
    }

    // image dimension
    int nLines = m_scanlines;
    int inSamples = m_samplesPerLine;
    int outSamples = m_BsamplesPerLine;

    int decimation = m_amplioParams.decimation; // 0: sampling freq, 1:sampling freq/2, 2:sampling freq/4, ...
    int decimFactor = 1 << decimation;

    if (m_runAmplio)
    {
        // generate B image
        m_amplioMutex.lock();
        for (int i = 0; i < nLines; i++)
        {
            // read one RF line
            short* rfline = &rfFrame[i * inSamples];
            unsigned char* bline = &bFrame[i * outSamples];
            amplioProcessRfToB(rfline, decimation, bline);
        }
        m_amplioMutex.unlock();
    }
    else
    {
        // generate RF image
        for (int i = 0; i < nLines; i++)
        {
            short* rfline = &rfFrame[i * inSamples];
            unsigned char* bline = &bFrame[i * outSamples];
            for (int j = 0; j < outSamples; j++)
            {
                bline[j] = static_cast< unsigned char >(127 + Saturate((rfline[j * decimFactor] * m_amp / 100), -127, 127));
            }
        }
    }
}

void TexoDemo::imageFormerDAQ()
{
    short* rfFrame = (m_runBeamforming) ? m_bfdaq : m_daqdata;
    unsigned char* bFrame = m_bDaq;

    if (!rfFrame || !bFrame)
    {
        return;
    }

    // image dimension
    int nLines = m_daqchannels;
    int inSamples = m_daqsamplesPerLine;
    int outSamples = m_BsamplesPerLine;

    int decimation = m_amplioParams.decimation; // 0: sampling freq, 1:sampling freq/2, 2:sampling freq/4, ...
    int decimFactor = 1 << decimation;

    // apply digital gain if requested
    int digitaGain = m_daqParams.digitalGain;
    if (digitaGain > 1)
    {
        int sZ = nLines * inSamples;
        for (int i = 0; i < sZ; i++)
        {
            rfFrame[i] = static_cast< short >(Saturate((rfFrame[i] * digitaGain), SHRT_MIN, SHRT_MAX));
        }
    }

    // generate the image
    if (m_runAmplio)    // generate B image
    {
        // process each line separately
        m_amplioMutex.lock();
        for (int i = 0; i < nLines; i++)
        {
            if (m_requiresUpdate)
            {
                continue;
            }
            // read one RF line
            short* rfline = &rfFrame[i * inSamples];
            unsigned char* bline = &bFrame[i * outSamples];
            amplioProcessRfToB(rfline, decimation, bline);
        }
        m_amplioMutex.unlock();
    }
    else                // generate RF image
    {
        for (int i = 0; i < nLines; i++)
        {
            short* rfline = &rfFrame[i * inSamples];
            unsigned char* bline = &bFrame[i * outSamples];
            for (int j = 0; j < outSamples; j++)
            {
                bline[j] = static_cast< unsigned char >(127 + Saturate((rfline[j * decimFactor] * m_amp / 100), -127, 127));
            }
        }
    }
}

void TexoDemo::runDAQ()
{
    if (m_daqShouldBeRunning)
    {
        daqRun();
    }
}

bool TexoDemo::programSequenceDAQ()
{
    daqRaylinePrms rlprms;
    daqSequencePrms seqprms;

    rlprms.channels = m_daqParams.channels;
    rlprms.gainDelay = m_daqParams.gainDelay;
    rlprms.gainOffset = m_daqParams.gainOffset;
    rlprms.rxDelay = m_daqParams.rxDelay;

    // sampling and decimation
    if (m_daq80MHzSampling)
    {
        rlprms.sampling = 80;   // DAQ sampling frequency 80 -> 80 [MHz]
        rlprms.decimation = 0;  // no decimation for 80MHz sampling
    }
    else
    {
        rlprms.sampling = 40;   // DAQ sampling frequency 40 -> 40 [MHz]
        rlprms.decimation = static_cast< unsigned char >(m_daqParams.decimation);  // Fs = sampling / (1+decimation) e.g. decimation = 1 -> Fs=20 MHz
    }
    rlprms.numSamples = m_daqsamplesPerLine; // at 40MHz
    rlprms.lineDuration = m_daqParams.rxDelay + m_daqsamplesPerLine / 40 + 5;  // line duration in micro seconds
    // This also determines the maximum depth the TGC will be applied therefore it's better
    // that it is calculated based on the numSamples
    // 40 Mega samples per second. 5 microseconds extra to be sure - Ali B, Feb 2014

    seqprms.freeRun = m_daqParams.freeRun;
    seqprms.hpfBypass = m_daqParams.hpfBypass;
    seqprms.divisor = static_cast< unsigned char >(m_daqParams.divisor);                  // data size = 16GB / 2^divisor
    seqprms.externalTrigger = m_daqParams.externalTrigger;  // sync with Texo transmits
    seqprms.externalClock = m_daqParams.externalClock;      // set to true if external clock is provided
    seqprms.fixedTGC = m_daqParams.fixedTGC;
    seqprms.lowPassFilterCutOff = 3;                        // For 40 MHz sampling, it would be 13.3 MHz
    seqprms.highPassFilterCutOff = 0;                       // For 40 MHz sampling, it would be 0.64 MHz
    seqprms.lnaGain = m_daqParams.lnaGain;                  // 0:16dB, 1:18dB, 2:21dB
    seqprms.pgaGain = m_daqParams.pgaGain;                  // 0:21dB, 1:24dB, 2:27dB, 3:30dB
    seqprms.biasCurrent = m_daqParams.biasCurrent;          // 0,1,2,...,7

    if (seqprms.fixedTGC)
    {
        seqprms.fixedTGCLevel = 100;
    }
    else
    {
        // set TGC curve
        daqTgcSetX(2, 1.0f);
        daqTgcSetX(1, 0.5f);
        daqTgcSetX(0, 0.0f);

        daqTgcSetY(2, 1.0f);
        daqTgcSetY(1, 0.95f);
        daqTgcSetY(0, 0.5f);
    }

    if (!daqProgramSequence(seqprms, rlprms))
    {
        return false;
    }

    return true;
}

void TexoDemo::daqCallback(void* prm, int val, ECallbackSources src)
{
    if (src == eCbInit)
    {
        ((TexoDemo*)prm)->wProgress->setValue(val);
    }

    if (src == eCbBuffersFull)
    {
        if (!((TexoDemo*)prm)->m_texoIsRunning)
        {
            return;
        }

        if (((TexoDemo*)prm)->m_requiresUpdate)
        {
            // init display
            ((TexoDemo*)prm)->daqDisplay->init(((TexoDemo*)prm)->m_daqchannels, ((TexoDemo*)prm)->m_BsamplesPerLine);
            // allocate the buffer
            int szRF = ((TexoDemo*)prm)->m_daqchannels * ((TexoDemo*)prm)->m_daqsamplesPerLine;
            int szB = ((TexoDemo*)prm)->m_daqchannels * ((TexoDemo*)prm)->m_BsamplesPerLine;
            ((TexoDemo*)prm)->allocateDAQBuffers(szRF, szB);
        }

        // create a folder for the new DAQ data
        char path[256];
        strcpy(path, ((TexoDemo*)prm)->m_daqpath);
        char subdir[128];
        sprintf(subdir, "%d/", ((TexoDemo*)prm)->m_daqFrCount);
        strcat(path, subdir);
        CreateDirectoryA(path, 0);
        ((TexoDemo*)prm)->incDaqCounter();

        // download the DAQ data into a folder
        daqDownload(path);
        // load the first DAQ frame from the folder to memory
        int numFrames = ((TexoDemo*)prm)->loadDaqData(path, 0);
        // send the first DAQ frame for display
        ((TexoDemo*)prm)->displayDAQ();
        // display the rest of DAQ frames
        for (int i = 1; i < numFrames; i++)
        {
            // load i'th DAQ frame
            ((TexoDemo*)prm)->loadDaqData(path, i);
            // display i'th DAQ frame
            ((TexoDemo*)prm)->displayDAQ();
        }

        // update the number of samples in case depth has been changed
        if (((TexoDemo*)prm)->m_daqsamplesPerLine != ((TexoDemo*)prm)->m_samplesPerLine)
        {
            ((TexoDemo*)prm)->m_daqsamplesPerLine = ((TexoDemo*)prm)->m_samplesPerLine;
            ((TexoDemo*)prm)->m_requiresUpdate = true;
        }
        else
        {
            ((TexoDemo*)prm)->m_requiresUpdate = false;
        }

        // The DAQ should not and cannot be restarted from within the callback
        // The callback is part of the daqMonitoring thread of the SDK
        // A message is posted to the main thread of the demo program to notify that
        // the DAQ callback is finished and DAQ needs to be restarted - Ali B, Oct 2013
        QMetaObject::invokeMethod((TexoDemo*)prm, "runDAQ", Qt::QueuedConnection);
    }
}

// allow data to be loaded from file with a frame offset
int TexoDemo::loadDaqData(char* path, int frame)
{
    int channel = 0;
    int frames = 0;
    int samples = 0;

    if (m_daqDownloadingMode == STORE_TO_DISK_ONLY)
    {
        FILE* fp;
        QString file, msg;

        QDir dir(path);
        QStringList files = dir.entryList(QStringList("*.daq"));

        for (int i = 0; i < files.count(); i++)
        {
            file = dir.path() + "/" + files[i];
            fp = fopen(file.toLocal8Bit(), "rb");
            if (fp)
            {
                fread(&channel, sizeof(int), 1, fp);
                fread(&frames, sizeof(int), 1, fp);
                fread(&samples, sizeof(int), 1, fp);

                // check for error
                if (frames <= 0 || samples <= 0 || samples != m_daqsamplesPerLine || channel >= m_daqchannels)
                {
                    fclose(fp);
                    fprintf(stderr, "Error loading DAQ data");
                    return 0;
                }

                // load channel data
                fseek(fp, (samples * frame * sizeof(short)), SEEK_CUR);
                fread(m_daqdata + (daqDataIndexToChannelIndex(channel) * samples), sizeof(short), samples, fp);

                fclose(fp);
            }
        }
    }
    else
    {
        for (int i = 0; i < 128; i++)
        {
            // read header
            int* header = reinterpret_cast< int* >(daqGetDataPtr(i));
            try
            {
                if (header)
                {
                    channel = header[0];
                    frames = header[1];
                    samples = header[2];

                    // read data
                    short int* data = reinterpret_cast< short int* >(daqGetDataPtr(i));
                    memcpy(reinterpret_cast< void* >(m_daqdata + (daqDataIndexToChannelIndex(channel) * samples)),
                        reinterpret_cast< void* >(data + DAQ_FILE_HEADER_SIZE_BYTES / sizeof(short int) + samples *
                                                  frame), samples * sizeof(short int));
                }
            }
            catch (...)
            {
            }
        }
    }

    return frames;
}

// display DAQ frame
void TexoDemo::displayDAQ()
{
    if (m_runBeamforming)
    {
        beamformDAQ();
        imageFormerDAQ();
    }
    else
    {
        imageFormerDAQ();
    }

    daqDisplay->setImgData(m_bDaq);
}

// display Texo frame
void TexoDemo::displayTexo()
{
    m_dispMutex.lock();
    texoDisplay->setImgData(m_bTexo);
    m_dispMutex.unlock();
}

void TexoDemo::incDaqCounter()
{
    // only keep a limited number of daq folders
    m_daqFrCount = (m_daqFrCount + 1) % MAXDAQFOLDERS;
}

void TexoDemo::onSave()
{
    if (m_texoIsRunning)
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

    FILE* fp = fopen("TexoData.rf", "wb+");
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

    // 1) populate and write the header for .rf file
    uFileHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.frames = numFrames;
    hdr.w = m_scanlines;
    hdr.h = m_samplesPerLine;
    hdr.sf = 40000000 >> m_txoParams.decim;  // sampling freq
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

void TexoDemo::allocateTexoBuffers(int szRF, int szB)
{
    if (m_rf)
    {
        delete[] m_rf;
    }

    if (m_bTexo)
    {
        delete[] m_bTexo;
    }

    m_rf = new short[szRF];
    m_bTexo = new unsigned char[szB];

    memset(m_rf, 0, szRF * sizeof(short));
    memset(m_bTexo, 0, szB);
}

void TexoDemo::allocateDAQBuffers(int szRF, int szB)
{
    if (m_daqdata)
    {
        delete[] m_daqdata;
    }

    if (m_bfdaq)
    {
        delete[] m_bfdaq;
    }

    if (m_bDaq)
    {
        delete m_bDaq;
    }

    m_daqdata = new short[szRF];
    m_bfdaq = new short[szRF];
    m_bDaq = new unsigned char[szB];

    memset(m_daqdata, 0, szRF * sizeof(short));
    memset(m_bfdaq, 0, szRF * sizeof(short));
    memset(m_bDaq, 0, szB);
}

void TexoDemo::reloadParams()
{
    /////////////// load Texo params ///////////////

    m_txoParams.numLines = wParamsTXO->item(0, 0)->text().toInt();
    m_txoParams.angle = wParamsTXO->item(1, 0)->text().toInt();
    m_txoParams.vsound = wParamsTXO->item(2, 0)->text().toInt();
    m_txoParams.freq = wParamsTXO->item(3, 0)->text().toInt();
    m_txoParams.txaperture = wParamsTXO->item(4, 0)->text().toInt();
    m_txoParams.power = wParamsTXO->item(5, 0)->text().toInt();
    m_txoParams.pulse = wParamsTXO->item(6, 0)->text();
    m_txoParams.focus = wParamsTXO->item(7, 0)->text().toInt();
    m_txoParams.rxaperture = wParamsTXO->item(8, 0)->text().toInt();
    m_txoParams.gain = wParamsTXO->item(9, 0)->text().toInt();
    m_txoParams.depth = wParamsTXO->item(10, 0)->text().toInt();
    m_txoParams.delay = wParamsTXO->item(11, 0)->text().toInt();
    m_txoParams.decim = wParamsTXO->item(12, 0)->text().toInt();
    m_txoParams.bfdelay = (wParamsTXO->item(13, 0)->checkState() == Qt::Unchecked) ? false : true;
    m_txoParams.planeWave = wParamsTXO->item(14, 0)->text().toInt();
    m_txoParams.windowType = wParamsTXO->item(15, 0)->text().toInt();
    m_txoParams.fnumber = wParamsTXO->item(16, 0)->text().toInt();
    m_txoParams.demoInd = wParamsTXO->item(17, 0)->text().toInt();
    /////////////// load Amplio params ///////////////

    // run amplio or not
    m_runAmplio = (wParamsAMP->item(0, 0)->checkState() == Qt::Unchecked) ? false : true;
    // IQ demodulation parameters
    m_amplioParams.samplingFreq = 40000000 >> m_txoParams.decim;
    m_amplioParams.startFreq = wParamsAMP->item(1, 0)->text().toInt() * 1000000;
    m_amplioParams.stopFreq = wParamsAMP->item(2, 0)->text().toInt() * 1000000;
    m_amplioParams.cutoffFreq = wParamsAMP->item(3, 0)->text().toInt() * 1000000;
    // initialize Log Compression
    m_amplioParams.useCompression = 1;    // if set to zero no log compression will be applied
    m_amplioParams.reject = wParamsAMP->item(4, 0)->text().toInt(); // rejection in dB
    m_amplioParams.dynamicRg = wParamsAMP->item(5, 0)->text().toInt(); // dynamic range in dB
    m_amplioParams.decimation = 1; // for b image, 0: sampling freq, 1:sampling freq/2, 2:sampling freq/4, ...

    /////////////// load daq param ///////////////

    // turn on/off software beamforming
    m_runBeamforming = (wParamsDAQ->item(0, 0)->checkState() == Qt::Unchecked) ? false : true;

    // turn on all channels
    m_daqParams.channels[0] = 0xffffffff;
    m_daqParams.channels[1] = 0xffffffff;
    m_daqParams.channels[2] = 0xffffffff;
    m_daqParams.channels[3] = 0xffffffff;

    m_daqParams.gainDelay = 0;
    m_daqParams.gainOffset = 0;
    m_daqParams.rxDelay = wParamsDAQ->item(1, 0)->text().toInt();
    m_daqParams.decimation = wParamsDAQ->item(2, 0)->text().toInt();
    m_daqParams.freeRun = 0;
    m_daqParams.hpfBypass = 0;
    m_daqParams.divisor = wParamsDAQ->item(3, 0)->text().toInt();
    // data size = 16GB / 2^divisor
    m_daqParams.externalTrigger = 1;    // sync with Texo transmits
    m_daqParams.externalClock = 0;      // set to true if external clock is provided
    m_daqParams.fixedTGC = (wParamsDAQ->item(4, 0)->checkState() == Qt::Unchecked) ? 0 : 1;
    m_daqParams.lnaGain = wParamsDAQ->item(5, 0)->text().toInt();            // 0:16dB, 1:18dB, 2:21dB
    m_daqParams.pgaGain = wParamsDAQ->item(6, 0)->text().toInt();            // 0:21dB, 1:24dB, 2:27dB, 3:30dB
    m_daqParams.biasCurrent = wParamsDAQ->item(7, 0)->text().toInt();        // 0,1,2,...,7
    // downloading mode
    m_daqDownloadingMode = wParamsDAQ->item(8, 0)->text().toInt();
    daqTransferMode(m_daqDownloadingMode);
    // beamforming params
    m_daqParams.pitch = wParamsDAQ->item(9, 0)->text().toInt();
    m_daqParams.SOS = wParamsDAQ->item(10, 0)->text().toInt();
    m_daqParams.Fs = wParamsDAQ->item(11, 0)->text().toInt();
    m_daqParams.Fnumber = wParamsDAQ->item(12, 0)->text().toInt();
    m_daqParams.maxApr = wParamsDAQ->item(13, 0)->text().toInt();
    m_daqParams.digitalGain = wParamsDAQ->item(14, 0)->text().toInt();
}

void TexoDemo::Processor::run()
{
    while (m_txo->m_texoIsRunning)
    {
        // process the image
        m_txo->imageFormerTexo();
        // display the image
        m_txo->displayTexo();
    }
}

void TexoDemo::beamformDAQ()
{
    int pitch = m_daqParams.pitch;    // microns
    int c = m_daqParams.SOS;       // speed of sound m/s
    int Fs = m_daqParams.Fs;        // sampling freq MHz
    double Fnum = m_daqParams.Fnumber / 10.0;
    int maxAprSz = m_daqParams.maxApr;

    // calculating the sample spacing
    double const sampleSpacing = double(c) / Fs / 2.0;     // microns

    // beamforming
    short* in = m_daqdata;
    short* out = m_bfdaq;
    for (int i = 0; i < m_daqchannels; i++)
    {
        for (int j = 0; j < m_daqsamplesPerLine; j++)
        {
            // find sample location in microns
            double Z = j * sampleSpacing;
            double X = i * pitch;

            // calculate the apr based on the F number
            int a = static_cast< int >(Z / (2 * Fnum));
            int hlfAprSz = a / pitch;

            // check to make sure we do not go beyond max apr
            if ((2 * hlfAprSz + 1) > maxAprSz)
            {
                hlfAprSz = maxAprSz / 2 - 1;
            }

            // calc delays based on sample depth and receive aperture
            int startApr = (i - hlfAprSz);
            int endApr = (i + hlfAprSz);

            if (startApr < 0)
            {
                startApr = 0;
            }
            if (endApr > (m_daqchannels - 1))
            {
                endApr = m_daqchannels - 1;
            }

            // calculate delays and sum
            short sum = 0;
            for (int chInd = startApr; chInd <= endApr; chInd++)    // aperture indices
            {
                int X1 = chInd * pitch;
                int chDelay = static_cast< int >((Z + sqrt(Z * Z + (X - X1) * (X - X1))) / c * Fs);

                if (chDelay >= m_daqsamplesPerLine)
                {
                    continue;
                }

                sum += in[chInd * m_daqsamplesPerLine + chDelay];
            }

            out[i * m_daqsamplesPerLine + j] = sum;
        }
    }
}
