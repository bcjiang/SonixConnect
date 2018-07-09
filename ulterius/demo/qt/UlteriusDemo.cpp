#include "stdafx.h"
#include "UlteriusDemo.h"
#include "fft.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <time.h>
#include <stdlib.h>
using namespace std;

#define MSG_TIMEOUT 3000     // ms
#define MAX_LIST    1024     // chars
//#define N 8 // length of the complex array for test program
#define N_LINES 128 // Number of transducer elements
#define N_SAMPLES 2336 // Number of samples for each scan line
#define N_SAMPLES_FFT 2048 // reduce the number of samples for FFT conversion
#define N_SAMPLES_BPRE 520 // 128*520 B pre-scan data size
#define BWIDTH 768 //BMode pre scan image width
#define BHEIGHT 701 //BMode pre scan image height

char uList[MAX_LIST];
UlteriusDemo* mainWindow = 0;
double lineTable[128];
bool frame_processed = true;

//Display frame rate if set to true.
bool dispFrameRate = true;
std::clock_t previous = std::clock();

// nice way to implement sleep since Qt typically requires setup of threads to use usleep(), etc.
void QSleep(int time)
{
    QMutex mtx;
    mtx.lock();
    QWaitCondition wc;
    wc.wait(&mtx, time);
}

// used to parse words from retreived lists
char* getWord(char* in, char sep, char* out, int sz)
{
    int pos = 0;

    strcpy_s(out, sz, "");

    while (in[pos] != sep)
    {
        if (in[pos] == '\0')
        {
            return 0;
        }

        out[pos] = in[pos];

        if (++pos >= sz)
        {
            return 0;
        }
    }

    out[pos] = '\0';

    return in + (pos + 1);
}

UlteriusDemo::UlteriusDemo(QWidget* parent) : QMainWindow(parent)
{
    setupUi(this);
    setupControls();

    //m_server = "localhost";
	m_server = "10.162.34.191";
    m_ulterius = new ulterius;

    m_ulterius->setCallback(onNewData);
    m_ulterius->setParamCallback(paramCallback);

    wStatus->showMessage(tr("Server address configured to: ") + m_server, MSG_TIMEOUT);

    mainWindow = this;

	// Create line table
	for (int i = 0; i < 128; ++i){
		if (i<16){lineTable[i] = 0.4*i;}
		if (i>=16 && i<48){
			lineTable[i] = lineTable[i-1] + 0.4 - 0.00625*(i-15);
		}
		if (i>=48 && i<80){
			lineTable[i] = lineTable[i-1] + 0.2;
		}
		if (i>=80 && i<112){
			lineTable[i] = lineTable[i-1] + 0.2 + 0.00625*(i-79);
		}
		if (i>=112 && i<128){
			lineTable[i] = lineTable[i-1] + 0.4;
		}
	}
	for (int i = 0; i< 128; ++i){
		lineTable[i] = lineTable[i] / 38.0;
	}
}

UlteriusDemo::~UlteriusDemo()
{
    if (m_ulterius)
    {
        m_ulterius->disconnect();
        delete m_ulterius;
    }
}

// setup various controls
void UlteriusDemo::setupControls()
{
    int i, j = 0;

    wModes->blockSignals(true);
    for (i = 0; i < wModes->count(); i++)
    {
        wModes->setItemData(i, BMode + i);
    }
    wModes->blockSignals(false);

    wParams->setColumnWidth(0, 240);
    wParams->setColumnWidth(1, 120);

    wAcquire->blockSignals(true);
    wAcquire->setColumnWidth(0, 240);
    wAcquire->setColumnWidth(1, 120);

    for (i = 0; i < wAcquire->rowCount(); i++)
    {
        j = (i > 10) ? 1 : 0;
        wAcquire->item(i, 0)->setData(Qt::UserRole, udtBPre << (i + j));
        wAcquire->item(i, 1)->setData(Qt::UserRole, udtBPre << (i + j));
    }

    wAcquire->blockSignals(false);
}


//Takes input image data (stored in qbr) and displays image on screen.
void UlteriusDemo::processFrame(QImage BModeImage, const int& type, const int& sz, const int& frmnum)
{	
	mainWindow->labelDisplay->setPixmap(QPixmap::fromImage(BModeImage));
}

bool UlteriusDemo::onNewData(void* data, int type, int sz, bool cine, int frmnum)
{
    int i, count = mainWindow->wAcquire->rowCount();
    QTableWidgetItem* item;
    QString text;

    for (i = 0; i < count; i++)
    {
        item = mainWindow->wAcquire->item(i, 1);
        if (item->data(Qt::UserRole) == type)
        {
            text = QString("%1").arg(frmnum);
            item->setText(text);
            break;
        }
    }
	// Above is original code from demo //

	unsigned char* char_data = reinterpret_cast<unsigned char*>(data);
	int * int_data = new int[sz];
	for(i = 0; i<sz; ++i){
		int_data[i] = char_data[i];
	}

	std::vector<std::vector<int>> RFlineScaled;
	RFlineScaled.resize(N_LINES);
	for(int i = 0; i < N_LINES; i++){
		RFlineScaled[i].resize(N_SAMPLES_BPRE);
		for(int j = 0; j < N_SAMPLES_BPRE; j++){
			RFlineScaled[i][j] = int_data[i*N_SAMPLES_BPRE+j];
		}
	}

	// Scan conversion with image companding
	// The true image size is: (0.4*64 + 0.2*64 = 38.4mm) width : (40mm * 2048/2336 = 35.068mm) height
	// The display image is: 768 * 701, i.e., scan conversion from 128 * 2048 to 768 * 701
	// The aperture size is set to 16 (minimal aperture in EXAM software).
	QImage BModeImage(BWIDTH, BHEIGHT, QImage::Format_RGB32);
	QRgb PixelValue;
	int ValueOne, index_i, index_j; 
	double line_loc;
	bool not_done;
	index_i = 0;
	for (int i = 0; i < BWIDTH; ++i){
		for (int j = 0; j < BHEIGHT; ++j){
			index_j = int(std::floor(double(j)/double(BHEIGHT)*N_SAMPLES_BPRE+0.5));
			line_loc = double(i)/double(BWIDTH);
			not_done = true;
			while(not_done){
				if(index_i == 127){
					ValueOne = RFlineScaled[index_i-1][index_j];
					not_done = false;
				}
				else if(line_loc >= lineTable[index_i] && line_loc < lineTable[index_i+1]){
					//ValueOne = RFlineScaled[index_i][index_j]; //always use the left value
					ValueOne = int((line_loc - lineTable[index_i])/(lineTable[index_i+1]-lineTable[index_i]) * \
						RFlineScaled[index_i+1][index_j] + (lineTable[index_i+1] - line_loc)/(lineTable[index_i+1]-lineTable[index_i]) * \
						RFlineScaled[index_i][index_j] + 0.5); //Linear interpolation
					not_done = false;
				}
				else
				{
					index_i++;
				}
			}
			PixelValue = qRgb(ValueOne,ValueOne,ValueOne);
			BModeImage.setPixel(i,j,PixelValue);
		}
	}

	QMetaObject::invokeMethod(mainWindow, "processFrame", Qt::QueuedConnection, Q_ARG(QImage, BModeImage),Q_ARG(int, type), Q_ARG(int, sz),Q_ARG(int, frmnum));
	
	delete int_data;

	//Display frame rate information to ulterious interface.
	if(dispFrameRate){
		double frames_sec = 1.0/((std::clock()-previous)/ (double)CLOCKS_PER_SEC);
		std::cout << "Frame rate = " << frames_sec << "Hz\n";
		previous = std::clock();
	}
	
    return true;
}

// parameter was changed, update interface
bool UlteriusDemo::paramCallback(void*, int, int)
{
    /*char* id = (char*)paramID;
       QString text(id);
       text.append(" changed");
       mainWindow->wStatus->showMessage(text, MSG_TIMEOUT);*/

    return true;
}

// allow user to configure server name
void UlteriusDemo::onServerAddress()
{
    bool ok = false;
    QString text = QInputDialog::getText(this, tr("Enter Server Address"), tr("Server Address:"), QLineEdit::Normal, m_server, &ok);

    if (ok && !text.isEmpty())
    {
        m_server = text;
    }
}

// user connected/disconnected
void UlteriusDemo::onConnect(bool state)
{
    int i, j, val = 0;
    //QTableWidgetItem* item;

    if (state)
    {
        if (m_ulterius->connect(m_server.toAscii()))
        {
            wStatus->showMessage(tr("Successfully made connection"), MSG_TIMEOUT);
            onRefresh();
        }
        else
        {
            wStatus->showMessage(tr("Could not connect to specified server address"), MSG_TIMEOUT);
            aConnect->setChecked(false);
        }
    }
    else
    {
        if (m_ulterius->disconnect())
        {
            wStatus->showMessage(tr("Disconnected"), MSG_TIMEOUT);
        }

        aConnect->setChecked(false);
    }

    if (m_ulterius->isConnected())
    {
        // load parameters list
        //uParam prm;
        i = j = 0;

        wParams->blockSignals(true);
        //while (m_ulterius->getParam(i, prm))
        //{
        //    if (prm.source == 1)
        //    {
        //        wParams->setRowCount(wParams->rowCount() + 1);
        //        item = new QTableWidgetItem(prm.name);
        //        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        //        item->setData(Qt::UserRole, prm.id);
        //        wParams->setItem(j, 0, item);

        //        val = 0;
        //        m_ulterius->getParamValue(prm.id, val);
        //        item = new QTableWidgetItem(QString("%1").arg(val));
        //        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        //        item->setData(Qt::UserRole, prm.id);
        //        wParams->setItem(j, 1, item);
        //        j++;
        //    }
        //    i++;
        //}
        wParams->blockSignals(false);

		uDataDesc qbr_descriptor;
		m_ulterius->getDataDescriptor(udtBPre, qbr_descriptor);
		std::cout << "Image width: "<< qbr_descriptor.w <<" height: " << qbr_descriptor.h << " size: "<<qbr_descriptor.ss<<endl;
    }
}

// user toggled freeze
void UlteriusDemo::onFreeze(bool)
{
    m_ulterius->toggleFreeze();

    QSleep(500);

    aFreeze->blockSignals(true);
    aFreeze->setChecked(m_ulterius->getFreezeState() != 0);
    aFreeze->blockSignals(false);
}

// user selected first probe
void UlteriusDemo::onProbe1()
{
    m_ulterius->selectProbe(0);
    onRefresh();
}

// user selected second probe
void UlteriusDemo::onProbe2()
{
    m_ulterius->selectProbe(1);
    onRefresh();
}

// user selected third probe
void UlteriusDemo::onProbe3()
{
    m_ulterius->selectProbe(2);
    onRefresh();
}

// user changed modes
void UlteriusDemo::onSelectMode(int index)
{
    if (m_ulterius->selectMode(wModes->itemData(index).toInt()))
    {
        wStatus->showMessage(tr("Successfully changed modes"), MSG_TIMEOUT);
    }
}

// user changed presets
void UlteriusDemo::onSelectPreset(QString preset)
{
    if (m_ulterius->selectPreset(preset.toAscii()))
    {
        wStatus->showMessage(tr("Successfully changed presets"), MSG_TIMEOUT);
    }
}

// user selected a new parameter
void UlteriusDemo::onSelectParam()
{
    refreshParamValue();
}

// user incremented parameter
void UlteriusDemo::onIncParam()
{
    if (!wParams->currentItem())
    {
        return;
    }

    QString prm = wParams->currentItem()->data(Qt::UserRole).toString();
    if (m_ulterius->incParam(prm.toAscii()))
    {
        refreshParamValue();
    }
}

// user decremented parameter
void UlteriusDemo::onDecParam()
{
    if (!wParams->currentItem())
    {
        return;
    }

    QString prm = wParams->currentItem()->data(Qt::UserRole).toString();
    if (m_ulterius->decParam(prm.toAscii()))
    {
        refreshParamValue();
    }
}

// refreshes the currently selected parameter value
void UlteriusDemo::refreshParamValue()
{
    if (!wParams->currentItem())
    {
        return;
    }

    int val;
    QString prm = wParams->currentItem()->data(Qt::UserRole).toString();

    if (m_ulterius->getParamValue(prm.toAscii(), val))
    {
        wParams->item(wParams->currentRow(), 1)->setText(QString("%1").arg(val));
    }
}

// user changed the acquisition boxes
void UlteriusDemo::onDataClicked(int, int)
{
    int i, mask = 0;
    QTableWidgetItem* item;
    for (i = 0; i < wAcquire->rowCount(); i++)
    {
        item = wAcquire->item(i, 0);
        mask |= (item->checkState() == Qt::Checked) ? item->data(Qt::UserRole).toInt() : 0;
    }

    m_ulterius->setDataToAcquire(mask);
}

// user toggled shared memory mode
void UlteriusDemo::onSharedMemoryToggle(bool state)
{
    m_ulterius->setSharedMemoryStatus(state ? 1 : 0);
}

// user toggled inject mode
void UlteriusDemo::onInjectToggle(bool)
{
    m_ulterius->setInjectMode(m_ulterius->getInjectMode() ? false : true);

    QSleep(500);

    aInjectToggle->blockSignals(true);
    aInjectToggle->setChecked(m_ulterius->getInjectMode() != 0);
    aInjectToggle->blockSignals(false);
}

// injects random data
void UlteriusDemo::onInjectRandom()
{
    uDataDesc desc;
    unsigned char* data = 0;
    int i, sz;

    if (m_ulterius->getDataDescriptor(udtBPost, desc))
    {
        sz = desc.w * desc.h * (desc.ss / 8);
        data = (unsigned char*)malloc(sz);

        if (data)
        {
            for (i = 0; i < sz; i++)
            {
                data[i] = (unsigned char)(rand() % 255);
            }

            m_ulterius->injectImage(data, desc.w, desc.h, desc.ss, false);

            free(data);
        }
    }
}

// refresh the interface
void UlteriusDemo::onRefresh()
{
    int i, sel;
    char* pStr;
    char str[256];
    QString curMode, curProbe, curPreset;

    wPresets->clear();

    // load probes
    if (m_ulterius->getProbes(uList, MAX_LIST))
    {
        i = 0;
        pStr = uList;
        while ((pStr = getWord(pStr, '\n', str, 256)) != 0)
        {
            switch (i++)
            {
            case 0: wProbe1->setText(str);
                break;
            case 1: wProbe2->setText(str);
                break;
            case 2: wProbe3->setText(str);
                break;
            }
        }
    }

    // load presets
    if (m_ulterius->getPresets(uList, MAX_LIST))
    {
        pStr = uList;
        while ((pStr = getWord(pStr, '\n', str, 256)) != 0)
        {
            wPresets->addItem(str);
        }

        wPresets->setCurrentIndex(0);
    }

    // get current mode
    sel = m_ulterius->getActiveImagingMode();
    if (sel != -1)
    {
        for (i = 0; i < wModes->count(); i++)
        {
            if (wModes->itemData(i) == sel)
            {
                curMode = QString("Mode=%1, ").arg(wModes->itemText(i));
                break;
            }
        }
    }

    // get current probe
    if (m_ulterius->getActiveProbe(str, 256))
    {
        curProbe = QString("Probe=%1, ").arg(str);
    }

    // get current preset
    if (m_ulterius->getActivePreset(str, 256))
    {
        curPreset = QString("Preset=%1").arg(str);
    }

    wStatus->showMessage(curMode + curProbe + curPreset, MSG_TIMEOUT);
}
