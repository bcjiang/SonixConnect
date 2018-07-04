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

char uList[MAX_LIST];
UlteriusDemo* mainWindow = 0;
double lineTable[128];
bool frame_processed = true;

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
void UlteriusDemo::processFrame(const QByteArray& qbr, const int& type, const int& sz, const int& frmnum)
{
	/*-------------------------- Test part for FFT ------------------------*/
	//const int N_TEST = 2048;
	//double xtest[N_TEST]; //input
	//double ytest[N_TEST]; //input
	//for (int i = 0; i < N_TEST; ++i) {
	//	xtest[i] = i+1;
	//	ytest[i] = 0;
	//}
	//int sampleN = N_TEST;
	//fft(xtest, ytest, sampleN, 1);
	//std::cout<<"FFT result:"<<std::endl;
	//for (int i = 0; i < sampleN; ++i){
	//	std::cout<<xtest[i]<<" "<<ytest[i]<<std::endl;
	//}
	//fft(xtest, ytest, sampleN, -1);
	//for (int i = 0; i < sampleN; ++i){
	//	std::cout<<xtest[i]<<" "<<ytest[i]<<std::endl;
	//}
	/*------------------------- End of FFT test ---------------------------*/

	/*-------------------------- Test part for envelope detection ------------------------*/
	//const int N_TEST = 128;
	//double xtest[N_TEST]; //input
	//double ytest[N_TEST]; //input
	//double test_enve[N_TEST]; //output

	//for (int i = 0; i < N_TEST; ++i) {
	//	xtest[i] = i+1;
	//	ytest[i] = 0;
	//}

	//fft(xtest, ytest, N_TEST, 1);

	//for (int i = 1; i < N_TEST/2; ++i){
	//	xtest[i] = 2 * xtest[i];
	//	ytest[i] = 2 * ytest[i];
	//}

	//for (int i = N_TEST/2 + 1; i < N_TEST; ++i){
	//	xtest[i] = 0;
	//	ytest[i] = 0;
	//}

	//fft(xtest, ytest, N_TEST, -1);

	//for (int i = 0; i < N_TEST; ++i){
	//	test_enve[i] =  sqrt(xtest[i]*xtest[i]+ ytest[i]*ytest[i]);
	//	std::cout<<test_enve[i]<<std::endl;
	//}
	//std::cout<<"######## Above is analytic function magnitude, below is its real part. ########"<<std::endl;
	//for (int i = 0; i < N_TEST; ++i){
	//	std::cout<<xtest[i]<<std::endl;
	//}

	/*-------------------------- End of envelope detection test ------------------------*/

	static std::clock_t previous = std::clock();

	//// Write raw data to file
	//QFile saveraw("rawRF.txt");
	//saveraw.open(QIODevice::WriteOnly);
	//saveraw.write(qbr);
	//saveraw.close();

	// From qbytearray to short array
	QByteArray qbr_new = QByteArray(qbr);
	char * datapointer = qbr_new.data();
	short * shortpointer = reinterpret_cast<short *>(datapointer);
	short * RFlines = new short[N_LINES*N_SAMPLES];
	for(int i = 0; i < N_LINES*N_SAMPLES; ++i){
		RFlines[i] = shortpointer[i];
	}

	// Form a 2D array for RF lines
	int sizeRF = N_LINES*N_SAMPLES;
	double (*RFlines2D)[N_SAMPLES_FFT] = new double[N_LINES][N_SAMPLES_FFT];
	for (int i=0; i<sizeRF; ++i){
		int col = i / N_SAMPLES;
		int row = i % N_SAMPLES;
		if (row < N_SAMPLES_FFT){
			RFlines2D[col][row] = RFlines[i];
		}
	}

	//// Save the raw RFlines to file
	//std::ofstream out("rflines2.txt");
	//for(int i = 0; i < N_LINES; ++i){
	//	for (int j = 0; j<N_SAMPLES_FFT; ++j){
	//		out<<RFlines2D[i][j]<<" ";
	//	}
	//	out<<"\n";
	//}
	//out.close();

	// Compute hilbert transform on RF lines
	double (*RFlinesHilbertXp)[N_SAMPLES_FFT] = new double[N_LINES][N_SAMPLES_FFT];
	double (*RFlinesHilbertYp)[N_SAMPLES_FFT] = new double[N_LINES][N_SAMPLES_FFT];
	double (*RFlineEnvelope)[N_SAMPLES_FFT] = new double[N_LINES][N_SAMPLES_FFT];
	
	for (int i = 0; i<N_LINES; ++i)
	{
		for (int j = 0; j<N_SAMPLES_FFT; ++j){
			RFlinesHilbertXp[i][j] = RFlines2D[i][j];
			RFlinesHilbertYp[i][j] = 0;
		}

		fft(RFlinesHilbertXp[i], RFlinesHilbertYp[i], N_SAMPLES_FFT, 1);

		for (int j = 1; j < N_SAMPLES_FFT/2; ++j){
			RFlinesHilbertXp[i][j] = 2 * RFlinesHilbertXp[i][j];
			RFlinesHilbertYp[i][j] = 2 * RFlinesHilbertYp[i][j];
		}

		for (int j = N_SAMPLES_FFT/2 + 1; j < N_SAMPLES_FFT; ++j){
			RFlinesHilbertXp[i][j] = 0;
			RFlinesHilbertYp[i][j] = 0;
		}

		fft(RFlinesHilbertXp[i], RFlinesHilbertYp[i], N_SAMPLES_FFT, -1);

		for (int j=0; j<N_SAMPLES_FFT; ++j){
			RFlineEnvelope[i][j] = sqrt(RFlinesHilbertXp[i][j]*RFlinesHilbertXp[i][j] + RFlinesHilbertYp[i][j]*RFlinesHilbertYp[i][j]);
		}

	}

	static bool dispHilbertTime = false;

	//Display frame rate information to ulterious interface.
	if(dispHilbertTime){
		double frames_processing_time = (std::clock()-previous)/ (double)CLOCKS_PER_SEC;
		std::cout << "Hilbert transform processing time = " << frames_processing_time << "seconds\n";
		previous = std::clock();
	}

	// Scale the response to 0~255 value
	std::vector<double> max_values;
	for (int i = 0; i<N_LINES; ++i){
		max_values.push_back(*(std::max_element(std::begin(RFlineEnvelope[i]), std::end(RFlineEnvelope[i]))));
	}
	double max_value = *(std::max_element(max_values.begin(), max_values.end()));
	std::vector<std::vector<double>>RFlineBeforeScale;
	std::vector<std::vector<int>> RFlineScaled;
	RFlineScaled.resize(N_LINES);
	RFlineBeforeScale.resize(N_LINES);
	for (int i = 0; i<N_LINES; ++i){
		RFlineScaled[i].resize(N_SAMPLES_FFT);
		RFlineBeforeScale[i].resize(N_SAMPLES_FFT);
		for (int j = 0; j<N_SAMPLES_FFT; ++j){
			RFlineBeforeScale[i][j] = RFlineEnvelope[i][j]/max_value;
			RFlineBeforeScale[i][j] = 20*log10(RFlineBeforeScale[i][j]);
			if(RFlineBeforeScale[i][j] >= -30.0){
				RFlineScaled[i][j] = int((30+RFlineBeforeScale[i][j])/30.0*255);
			}
			else{RFlineScaled[i][j] = 0;}
		}
	}
	
	//for (int i = 0; i<N_LINES; ++i){
	//	for (int j = 0; j<N_SAMPLES_FFT; ++j){
	//		RFlineScaled[i][j] = (int)(RFlineBeforeScale[i][j]*255);
	//		//if(RFlineScaled[i][j]>255){RFlineScaled[i][j] = 255;}
	//		//if(RFlineScaled[i][j]<0){RFlineScaled[i][j] = 0;}
	//	}
	//}

	// Get Qt image object for RF line display (i.e., no scan conversion)
	//QImage BModeImage(N_LINES, N_SAMPLES_FFT, QImage::Format_RGB32);
	//QRgb PixelValue;
	//for (int i = 0; i<N_LINES; ++i){
	//	for (int j = 0; j<N_SAMPLES_FFT; ++j){
	//		PixelValue = qRgb(RFlineScaled[i][j],RFlineScaled[i][j],RFlineScaled[i][j]);
	//		BModeImage.setPixel(i, j, PixelValue);
	//	}
	//}

	// Scan conversion and image creation (no companding)
	// The true image size is: (0.4*64 + 0.2*64 = 38.4mm) width : (40mm * 2048/2336 = 35.068mm) height
	// The display image is: 384 * 350, i.e., scan conversion from 128 * 2048 to 384 * 350
	//const int BModeWidth = 384;
	//const int BModeHeight = 350;
	//QImage BModeImage(BModeWidth, BModeHeight, QImage::Format_RGB32);
	//QRgb PixelValue;
	//int index_i, index_j; 
	//for (int i = 0; i < BModeWidth; ++i){
	//	for (int j = 0; j < BModeHeight; ++j){
	//		index_i = int(std::floor(double(i)/double(BModeWidth)*N_LINES+0.5));
	//		index_j = int(std::floor(double(j)/double(BModeHeight)*N_SAMPLES_FFT+0.5));
	//		if(index_i<N_LINES && index_j<N_SAMPLES_FFT){
	//			PixelValue = qRgb(RFlineScaled[index_i][index_j],RFlineScaled[index_i][index_j],RFlineScaled[index_i][index_j]);
	//			BModeImage.setPixel(i,j,PixelValue);
	//		}
	//		else{
	//			PixelValue = qRgb(0,0,0);
	//			BModeImage.setPixel(i,j,PixelValue);
	//		}
	//	}
	//}

	// Scan conversion with image companding
	// The true image size is: (0.4*64 + 0.2*64 = 38.4mm) width : (40mm * 2048/2336 = 35.068mm) height
	// The display image is: 768 * 701, i.e., scan conversion from 128 * 2048 to 768 * 701
	// The aperture size is set to 16 (minimal aperture in EXAM software).
	const int BModeWidth = 768;
	const int BModeHeight = 701;
	QImage BModeImage(BModeWidth, BModeHeight, QImage::Format_RGB32);
	QRgb PixelValue;
	int ValueOne, index_i, index_j; 
	double line_loc;
	bool not_done;
	index_i = 0;
	for (int i = 0; i < BModeWidth; ++i){
		for (int j = 0; j < BModeHeight; ++j){
			index_j = int(std::floor(double(j)/double(BModeHeight)*N_SAMPLES_FFT+0.5));
			line_loc = double(i)/double(BModeWidth);
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
	
	mainWindow->labelDisplay->setPixmap(QPixmap::fromImage(BModeImage));

	delete RFlines;
	delete []RFlines2D;
	delete []RFlinesHilbertXp;
	delete []RFlinesHilbertYp;
	delete []RFlineEnvelope;

	//Display frame rate if set to true.
	static bool dispFrameProcessingTime = true;

	//Display frame rate information to ulterious interface.
	if(dispFrameProcessingTime){
		double frames_processing_time = (std::clock()-previous)/ (double)CLOCKS_PER_SEC;
		std::cout << " Frame processing time = " << frames_processing_time << "seconds\n";
	}

	frame_processed = true;
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

	//Display frame rate if set to true.
	static bool dispFrameRate = true;
	static std::clock_t previous = std::clock();
	int lines = 128;
	int sample = sz/2/lines;

	//Display frame rate information to ulterious interface.
	if(dispFrameRate){
		double frames_sec = 1.0/((std::clock()-previous)/ (double)CLOCKS_PER_SEC);
		std::cout << "\nSample Depth: "<< sample <<" . Frame rate = " << frames_sec << "Hz\n";
		previous = std::clock();
	}

	QByteArray qbr(reinterpret_cast<const char*>(data),sz);

	//const char* testdata = reinterpret_cast<const char*>(data);
	if (frame_processed = true){
		frame_processed = false;
		QMetaObject::invokeMethod(mainWindow, "processFrame", Qt::BlockingQueuedConnection, Q_ARG(QByteArray, qbr),Q_ARG(int, type), Q_ARG(int, sz),Q_ARG(int, frmnum));
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
