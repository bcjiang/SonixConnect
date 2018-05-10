#include "StdAfx.h"

bool selectProbe(int connector);
bool createSequence();
bool setup();
bool run();
bool stop();
int newImage(void*, unsigned char*, int);
bool setupMotor();
void setMotorPos(double angle);
void stepMotor(int steps, int direction);
void printStats();
bool saveData();
void wait();

class win32timer
{
public:
    win32timer();

    void start();
    double end();

private:
    LARGE_INTEGER _tstart;
    LARGE_INTEGER _freq;
};

win32timer::win32timer()
{
    ::QueryPerformanceFrequency(&_freq);
}

void win32timer::start()
{
    ::QueryPerformanceCounter(&_tstart);
}

double win32timer::end()
{
    LARGE_INTEGER tend;
    ::QueryPerformanceCounter(&tend);
    return ((double)(tend.QuadPart - _tstart.QuadPart) / _freq.QuadPart) * 1000.0;
}

#ifdef COMPILEDAQ

#define DAQDATA   "D:/DAQDATA/"
#define DAQRTDATA "D:/DAQRT/"

bool initDAQ();
bool sequenceDAQ(bool showStats);
bool saveDAQData();
void daqCallback(void*, int, ECallbackSources);

bool sampling_80MHz = false;
bool daqRealtime = false;
bool daqDisplayTiming = false;
int daqFrCount = 0;
char daqfwpath[1024];
char rtPath[1024] = DAQRTDATA;
UINT channelsDAQ[4] = { 0, 0, 0, 0 };

win32timer acqTimer;
win32timer dlTimer;
win32timer seqTimer;

#endif

// status variables
bool running = false;
bool validprobe = false;
bool validsequence = false;

// sequencing flags
bool singleTx = false;
bool phasedArray = false;

// global settings
int depth = 50;
int power = 10;
double gain = 0.80;
int channels = 64;
double motorPos = 0;

void usage(char* prog)
{
    printf("\n");
    printf("usage: %s [options]\n", prog);
    printf("options:\n");
    printf("-fw PATH   set the firmware path as specified by PATH\n");
#ifdef COMPILEDAQ
    printf("-dfw PATH  set DAQ firmware path as specified by PATH\n");
#endif
    printf("-v2        Sonix RP\n");
    printf("-v3        SonixTouch Research\n");
    printf("-v4        SonixTouch or SonixTablet Research\n");
    printf("-v5        SonixTouch or SonixTablet Research with pciv4\n");
    printf("-help      display this help\n");
    printf("notes:\n");
    printf("-default firmware path is '%s'\n", FIRMWARE_PATH);
}

// program entry point
int main(int argc, char* argv[])
{
    int i, pci = 3, usm = 4;
    char sel;
    char fwpath[1024];

    strcpy(fwpath, FIRMWARE_PATH);
#ifdef COMPILEDAQ
    strcpy(daqfwpath, DAQ_FIRMWARE_PATH);
#endif

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-v2") == 0)
        {
            pci = 2;
            usm = 2;
            channels = 32;
        }
        else if (strcmp(argv[i], "-v3") == 0)
        {
            pci = 3;
            usm = 3;
            channels = 64;
        }
        else if (strcmp(argv[i], "-v4") == 0)
        {
            pci = 3;
            usm = 4;
            channels = 64;
        }
        else if (strcmp(argv[i], "-v5") == 0)
        {
            pci = 4;
            usm = 4;
            channels = 64;
        }
        else if (strcmp(argv[i], "-fw") == 0)
        {
            strcpy(fwpath, argv[i + 1]);
            i++;
        }
#ifdef COMPILEDAQ
        else if (strcmp(argv[i], "-dfw") == 0)
        {
            strcpy(fwpath, argv[i + 1]);
            i++;
        }
#endif
        else
        {
            usage(argv[0]);
            return 0;
        }
    }

    strcat(fwpath, "\\");

    // initialize and set the data file path
    printf("initializing Texo\n");
    if (!texoInit(fwpath, pci, usm, 0, channels))
    {
        return -1;
    }

    // setup callback
    texoSetCallback(newImage, 0);
#ifdef COMPILEDAQ
    // setup callback
    daqSetCallback(daqCallback, 0);
#endif

    for (;;)
    {
        printf("make a selection:\n");
        printf("\n");
        printf("(1) select probe connector 1\n");
        printf("(2) select probe connector 2\n");
        printf("(3) select probe connector 3\n");
        printf("\n");
        printf("(L) load sequence\n");
        printf("\n");
        printf("(P) toggle phased array transmit/receive\n");
        printf("(T) toggle single element transmit\n");
        printf("\n");

#ifdef COMPILEDAQ
        printf("(D) initialize DAQ\n");
        printf("(V) load DAQ sequence\n");
        printf("(Q) set DAQ in real-time mode\n");
        printf("(E) set real-time download folder\n");
        printf("\n");
#endif
        if (texoGetProbeHasMotor())
        {
            printf("(M) setup motor\n");
            printf("(H) set 3D motor position to home\n");
            printf("(>) step 3D motor forward\n");
            printf("(<) step 3D motor backward\n");
            printf("\n");
        }

        printf("(G) run sequence\n");
        printf("(S) stop sequence\n");
        printf("\n");
        printf("(Y) store RF data to disk\n");
#ifdef COMPILEDAQ
        printf("(Z) store DAQ data to disk\n");
#endif
        printf("(X) exit\n");
        printf("\n");
        scanf("%c", &sel);

        switch (sel)
        {
        case '1': selectProbe(0);
            break;
        case '2': selectProbe(1);
            break;
        case '3': selectProbe(2);
            break;
        case 'l': case 'L':
            setup();
            break;
        case 'p': case 'P':
            phasedArray = !phasedArray;
            printf("phased array transmit/receive is now %s (requires reload of sequence)\n", phasedArray ? "ON" : "OFF");
            break;
        case 't': case 'T':
            singleTx = !singleTx;
            printf("single element transmit is now %s (requires reload of sequence)\n", singleTx ? "ON" : "OFF");
            break;
#ifdef COMPILEDAQ
        case 'd': case 'D': initDAQ();
            break;
        case 'v': case 'V': sequenceDAQ(true);
            break;
        case 'q': case 'Q':
            daqRealtime = !daqRealtime;
            printf("real-time DAQ is now %s\n", daqRealtime ? "ON" : "OFF");
            break;
        case 'e': case 'E':
            printf("enter a folder name: ");
            scanf("%s", rtPath);
            CreateDirectoryA(rtPath, 0);
            strcat(rtPath, "/");
            break;
#endif
        case 'm': case 'M':
            if (texoGetProbeHasMotor())
            {
                setupMotor();
            }
            break;
        case 'h': case 'H':
            if (texoGetProbeHasMotor())
            {
                setMotorPos(0);
            }
            break;
        case '.': case '>':
            if (texoGetProbeHasMotor())
            {
                stepMotor(8, 1);
            }
            break;
        case ',': case '<':
            if (texoGetProbeHasMotor())
            {
                stepMotor(8, -1);
            }
            break;
        case 'g': case 'G': run();
            break;
        case 's': case 'S': stop();
            break;
        case 'y': case 'Y': saveData();
            break;
#ifdef COMPILEDAQ
        case 'z': case 'Z': saveDAQData();
            break;
#endif
        case 'x': case 'X': goto goodbye;
        }

        wait();
    }

goodbye:

    if (running)
    {
        stop();
    }

    // clean up
    texoShutdown();
#ifdef COMPILEDAQ
    daqShutdown();
#endif

    return 0;
}

void wait()
{
    printf("\npress any key to continue");
    while (!_kbhit())
    {
    }
    _getch();
    fflush(stdin);
    system("cls");
}

// statistics printout for after sequence has been loaded
void printStats()
{
    // print out sequence statistics
    printf("sequence statistics:\n");
    printf("frame size = %d bytes\n", texoGetFrameSize());
    printf("frame rate= %.1f fr/sec\n", texoGetFrameRate());
    printf("buffer size = %d frames\n\n", texoGetMaxFrameCount());
}

// selects a probe, the function will fail if the connector is invalid or if there is no probe
// on the specified connector.
bool selectProbe(int connector)
{
    if (texoSelectProbe(texoGetProbeCode(connector)))
    {
        if (!texoActivateProbeConnector(connector))
        {
            fprintf(stderr, "could not activate connector %d\n", connector);
            return false;
        }
    }

#ifdef COMPILEDAQ
    printf("opening 3rd port for DAQ collection\n");
    texoForceConnector(3);
#endif

    validprobe = true;
    return true;
}

// runs a sequence
bool run()
{
    if (!validsequence)
    {
        fprintf(stderr, "cannot run, no sequence selected\n");
        return false;
    }

    if (running)
    {
        fprintf(stderr, "sequence is already running\n");
        return false;
    }

#ifdef COMPILEDAQ
    if (daqRealtime)
    {
        daqFrCount = 0;
        sequenceDAQ(false);
    }
#endif

    if (texoRunImage())
    {
        running = true;
        return true;
    }

    return false;
}

// stops a sequence
bool stop()
{
    if (texoStopImage())
    {
        running = false;
        fprintf(stdout, "acquired (%d) frames\n", texoGetCollectedFrameCount());

#ifdef COMPILEDAQ
        if (daqIsDownloading()) // daq is donwloading
        {
            daqStopDownload();
        }
        if (daqIsRunning()) // daq is collecting data
        {
            daqStop();
        }
#endif
        return true;
    }

    return false;
}

bool setup()
{
    if (!validprobe)
    {
        fprintf(stderr, "cannot create sequence, no probe selected\n");
        return false;
    }

    printf("enter desired depth in mm (10 - 300):\n");
    scanf("%d", &depth);
    if (depth < 10 || depth > 300)
    {
        fprintf(stderr, "invalid depth entered\n");
        return false;
    }

    createSequence();

    printStats();

    validsequence = true;
    return true;
}

// transmits and receives across the entire probe to acquire focused RF data from each centered aperture.
// this is the sequence that would be used to generate B mode images
bool createSequence()
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
    texoAddTGCFixed(gain);

    // setup power
    texoSetPower(power, power, power);
#ifdef COMPILEDAQ
    // output both sync and clock for the DAQ
    texoSetSyncSignals(0, 1, 3);
#endif

    //setup VCA (analog gains)
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
    tx.centerElement = 0;
    if (singleTx)
    {
        tx.aperture = 0;    // single element Tx
        tx.focusDistance = 300000;  //set focus to 30cm to nullify delay
    }
    else
    {
        tx.aperture = 64;
        tx.focusDistance = depth * 1000 / 2;    // half the imaging depth
    }
    tx.angle = 0;
    tx.frequency = texoGetProbeCenterFreq();
    strcpy(tx.pulseShape, "+-");
    tx.txRepeat = 0;
    tx.txDelay = 100;
    tx.speedOfSound = 1540;
    tx.sync = 0;    // per scan line, 0 means no trigger
    tx.useManualDelays = 0;
    tx.useMask = 0;
    tx.tableIndex = -1;

    rx.centerElement = 0;
    rx.aperture = channels;
    rx.angle = 0;
    rx.maxApertureDepth = 30000;    // in mm i.e. 30 cm
    rx.acquisitionDepth = depth * 1000;    // in mm
    rx.saveDelay = 1 * 1000;
    rx.rxBeamFormingDelay = 2500; // in ns
    rx.speedOfSound = 1540;
    rx.channelMask[0] = rx.channelMask[1] = 0xFFFFFFFF;
    rx.applyFocus = 1;
    rx.useCustomWindow = 0;
    rx.decimation = 0;  // 0 collects at 40MHz, 1 collects at 20MHz, ...
    rx.lgcValue = 0;
    rx.tgcSel = 0;
    rx.tableIndex = -1;
    rx.customLineDuration = 0;
    rx.numChannels = 64;
    rx.weightType = 0;  // fix apodization for all 64 channels: 0 means no apodization
    rx.fnumber = 20;    // Fnumber x10 i.e. 20 mean Fnumber = 2.0, increase the Fnumber to reduce the number of effective elements

    // get number of elements
    int elements = texoGetProbeNumElements();

    for (int i = 0; i < elements; i++)
    {
        // Add 0.5 to center the delays, to make symmetrical time delays
        // we should do this because the aperture values must be even for now
        if (phasedArray)
        {
            // always center the aperture and just change the steering angle
            int min = -45000;   // starting angle x1000
            int max = 45000;    // ending angle x1000
            tx.centerElement = (elements / 2 + 0.5);
            rx.centerElement = (elements / 2 + 0.5);
            rx.angle = tx.angle = min + (((max - min) * i) / (elements - 1));
        }
        else
        {
            // for linear and curved, start from one side and go to the other side
            tx.centerElement = (i + 0.5);
            rx.centerElement = (i + 0.5);
        }

        if (!texoAddLine(tx, rx, li))
        {
            return false;
        }
    }

#ifdef COMPILEDAQ
    // set plane wave transmit without focusing with full aperture
    tx.aperture = 128;
    tx.angle = 0;
    tx.useManualDelays = 1;
    memset(tx.manualDelays, 0, sizeof(int) * 128);
    // center the aperture
    tx.centerElement = (elements / 2 + 0.5);
    // turn on the sync for the plane wave to trigger DAQ collection
    tx.sync = 1;
    // do not acquire texo data during the plane wave
    rx.acquisitionDepth = 0;
    // adjust the line duration if triggering DAQ in plane wave mode
    rx.customLineDuration = 100000; // 100 usec

    if (!texoAddLine(tx, rx, li))
    {
        return false;
    }
#endif

    // tell program to finish sequence
    if (texoEndSequence() == -1)
    {
        return false;
    }

    return true;
}

bool setupMotor()
{
    int status, fpv, spf;

    printf("enable (1) or disable (0) motor?:\n");
    scanf("%d", &status);

    if (status == 0)
    {
        texoSetupMotor(false, 0, 0);
        return true;
    }

    printf("enter # frames per volume (must be odd):\n");
    scanf("%d", &fpv);
    if (fpv % 2 == 0)
    {
        fprintf(stderr, "invalid entry\n");
        return false;
    }

    printf("enter # steps per frame (must be one of: 2,4,8,16):\n");
    scanf("%d", &spf);
    if (spf != 2 && spf != 4 && spf != 8 && spf != 16)
    {
        fprintf(stderr, "invalid entry\n");
        return false;
    }

    printf("-- a sequence must be loaded in order for changes to take effect --\n");

    texoSetupMotor(1, fpv, spf);

    return true;
}

// manually set the home position
void setMotorPos(double angle)
{
    double pos;

    if (running)
    {
        stop();
    }

    pos = texoGoToPosition(angle);

    if (running)
    {
        run();
    }

    motorPos = pos;
}

// manually step the motor
void stepMotor(int steps, int direction)
{
    // for testing purpose the action is repeated ten times
    // remove the for loop for exact positioning
    for (int i = 0; i < 10; i++)
    {
        if (running)
        {
            stop();
        }

        double stepamt = texoStepMotor((direction > 0) ? false : true, steps);

        if (running)
        {
            run();
        }

        motorPos = motorPos + stepamt;

        // hitting the edge
        if (motorPos < 0)
        {
            motorPos = 0;
        }
        if (motorPos > texoGetProbeFOV())
        {
            motorPos = texoGetProbeFOV();
        }

        printf("current motor position = %f degree \n", motorPos);
    }
}

// store data to disk
bool saveData()
{
    char path[100];
    int numFrames, frameSize;
    FILE* fp;

    numFrames = texoGetCollectedFrameCount();
    frameSize = texoGetFrameSize();

    if (numFrames < 1)
    {
        fprintf(stderr, "no frames have been acquired\n");
        return false;
    }

    printf("enter a filename: ");
    scanf("%s", path);

    fp = fopen(path, "wb+");
    if (!fp)
    {
        fprintf(stderr, "could not store data to specified path\n");
        return false;
    }

    fwrite(texoGetCineStart(0), frameSize, numFrames, fp);

    fclose(fp);

    fprintf(stdout, "successfully stored data\n");

    return true;
}

// called when a new frame is received
int newImage(void*, unsigned char* /*data*/, int /*frameID*/)
{
    // print data
    // fprintf(stdout, "new frame received addr=(0x%08x) id=(%d)\n", data, frameID);
    return 1;
}

#ifdef COMPILEDAQ

bool initDAQ()
{
    char err[256];
    printf("initializing DAQ...\n");

    daqSetFirmwarePath(DAQ_FIRMWARE_PATH);
    if (!daqDriverInit())
    {
        fprintf(stderr, "DAQ init failed\n");
        return false;
    }

    if (!daqIsConnected())
    {
        fprintf(stderr, "DAQ not connected or off\n");
        return false;
    }
    else
    {
        printf("found DAQ module\n");
    }

    if (!daqIsInitialized())
    {
        printf("programming DAQ...\n");

        if (!daqInit(sampling_80MHz))
        {
            daqGetLastError(err, 256);
            fprintf(stderr, err);
            return false;
        }

        printf("finished programming DAQ module\n");
    }
    else
    {
        printf("DAQ is already initialized\n");
    }

    // set the daq transfer method
    daqTransferMode(STORE_TO_DISK_ONLY);

    return true;
}

bool sequenceDAQ(bool showStats)
{
    // turn on all channels
    channelsDAQ[0] = 0xffffffff;
    channelsDAQ[1] = 0xffffffff;
    channelsDAQ[2] = 0xffffffff;
    channelsDAQ[3] = 0xffffffff;

    daqRaylinePrms rlprms;
    daqSequencePrms seqprms;

    rlprms.channels = channelsDAQ;
    rlprms.gainDelay = 0;
    rlprms.gainOffset = 0;
    rlprms.rxDelay = 4;

    // sampling and decimation
    if (sampling_80MHz)
    {
        rlprms.sampling = 80;   // DAQ sampling frequency 80 -> 80 [MHz]
        rlprms.decimation = 0;  // no decimation for 80MHz sampling
    }
    else
    {
        rlprms.sampling = 40;   // DAQ sampling frequency 40 -> 40 [MHz]
        rlprms.decimation = 0;  // Fs = sampling / (1+decimation) e.g. decimation = 1 -> Fs=20 MHz
    }
    rlprms.lineDuration = 100;  // line duration in micro seconds
    rlprms.numSamples = 2080; // assuming 2080 samples

    seqprms.freeRun = 0;
    seqprms.hpfBypass = 0;
    seqprms.divisor = 10;           // data size = 16GB / 2^divisor
    seqprms.externalTrigger = 1;
    seqprms.externalClock = 0;  // set to true if external clock is provided
    seqprms.fixedTGC = 1;
    seqprms.lowPassFilterCutOff = 3;
    seqprms.highPassFilterCutOff = 0;
    seqprms.lnaGain = 1;            // 16dB, 18dB, 21dB
    seqprms.pgaGain = 2;            // 21dB, 24dB, 27dB, 30dB
    seqprms.biasCurrent = 1;        // 0,1,2,...,7

    if (seqprms.fixedTGC)
    {
        seqprms.fixedTGCLevel = 100;
    }
    else
    {
        // set TGC curve
        daqTgcSetX(2, 0.75f);
        daqTgcSetX(1, 0.5f);
        daqTgcSetX(0, 0.25f);

        daqTgcSetY(2, 1.0f);
        daqTgcSetY(1, 0.75f);
        daqTgcSetY(0, 0.5f);
    }

    if (!daqRun(seqprms, rlprms))
    {
        char err[256];
        printf("could not upload DAQ sequence\n");
        daqGetLastError(err, 256);
        printf(err);
        return false;
    }

    if (showStats)
    {
        int const numChannls = 128;
        printf("DAQ statistics:\n\n");
        printf("line duration = %d micro sec\n", rlprms.lineDuration);
        printf("number of channels = %d channels\n", numChannls);
        printf("samples per channel = %d samples\n", rlprms.numSamples);
        printf("frame size = %d bytes\n", (rlprms.numSamples * numChannls * 2));
    }

    return true;
}

void daqCallback(void*, int /*param*/, ECallbackSources src)
{
    char path[256], subdir[128];

    if (daqRealtime && src == eCbBuffersFull)
    {
        if (daqDisplayTiming)
        {
            printf("\nAcq = %0.1f ms ", acqTimer.end());
        }

        strcpy(path, rtPath);
        sprintf(subdir, "%d/", daqFrCount++);
        strcat(path, subdir);
        CreateDirectoryA(path, 0);

        dlTimer.start();
        daqDownload(path);
        if (daqDisplayTiming)
        {
            printf("Download = %0.1f ms ", dlTimer.end());
        }

        seqTimer.start();
        sequenceDAQ(false);
        if (daqDisplayTiming)
        {
            printf("DAQ Load = %0.1f ms", seqTimer.end());
        }

        acqTimer.start();
    }
}

// store data to disk
bool saveDAQData()
{
    char path[100] = DAQDATA, inpath[80], err[256];
    CreateDirectoryA(path, 0);

    printf("enter a folder name: ");
    scanf("%s", inpath);

    // create sub-folder
    strcat(path, inpath);
    CreateDirectoryA(path, 0);
    strcat(path, "\\");

    // save DAQ data
    fprintf(stdout, "storing DAQ data...\n");

    if (!daqIsConnected())
    {
        fprintf(stderr, "the device is not connected!\n");
        return false;
    }

    if (!daqDownload(path))
    {
        daqGetLastError(err, 256);
        fprintf(stderr, err);
        return false;
    }

    fprintf(stdout, "successfully stored DAQ data\n");

    return true;
}

#endif
