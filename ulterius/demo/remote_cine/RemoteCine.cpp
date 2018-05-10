#include "stdafx.h"
#include "RemoteCine.h"

#define MSG_TIMEOUT 5000     // ms

RemoteCine* mainWindow = 0;

// Construtor
RemoteCine::RemoteCine(QWidget* parent) : QMainWindow(parent)
{
    setupUi(this);
    
    m_cine   = NULL;
    m_server = "localhost";
    m_ulterius = new ulterius;
    
    m_firstFrame = 0;
    m_lastFrame  = 0;
    
    m_selRow     = -1;
    m_fileName   = "";

    m_frameSize  = 0;
    m_blankFrame = NULL;

    m_ulterius->setCallback(onNewData);
    m_ulterius->setParamCallback(paramCallback);

    wStatus->showMessage(tr("Server address configured to: ") + m_server, MSG_TIMEOUT);
    
    tbl_dataList->setColumnWidth(0, 320);
    QObject::connect(tbl_dataList, SIGNAL(itemClicked(QTableWidgetItem *)), this, SLOT(onDataSelect(QTableWidgetItem *)));

    mainWindow = this;
}

// Destructor
RemoteCine::~RemoteCine()
{
    if (m_ulterius)
    {
        m_ulterius->disconnect();
        delete m_ulterius;
    }
    if (m_cine)
    {
        fclose(m_cine);
        m_cine = NULL;
    }
}


bool RemoteCine::onNewData(void* data, int, int sz, bool, int ifrmnum)
{
    size_t frmnum = static_cast< size_t >(ifrmnum);
    if (static_cast< size_t >(sz) != mainWindow->m_frameSize)
    {
        QMetaObject::invokeMethod(mainWindow, "onHandleSizeMismatch");
        return true;
    }
    if (mainWindow->m_cine)
    {
        if (mainWindow->m_lastFrame == 0)
        {
            mainWindow->m_firstFrame = frmnum;
            mainWindow->m_lastFrame  = frmnum;
            fwrite(data, 1, sz, mainWindow->m_cine);
        }
        else
        {
            if (frmnum == mainWindow->m_lastFrame + 1)
            {
                mainWindow->m_lastFrame  = frmnum;
                fwrite(data, 1, sz, mainWindow->m_cine);
            }
            else if (frmnum > mainWindow->m_lastFrame + 1) // dropped frames - Ali B, Feb 2014
            {
                for (size_t i = 0; i + mainWindow->m_lastFrame + 1  < frmnum; i++)
                {
                    fwrite(mainWindow->m_blankFrame, 1, sz, mainWindow->m_cine);
                }
                mainWindow->m_lastFrame  = frmnum;
                fwrite(data, 1, sz, mainWindow->m_cine);
            }
        }
    }
    return true;
}

// parameter was changed, update interface
bool RemoteCine::paramCallback(void* paramID, int, int)
{
    char* id = (char*)paramID;
    QString text(id);
    if (!text.startsWith("freezestatus"))
    {
        QMetaObject::invokeMethod(mainWindow, "onRefresh");
    }
    else
    {
        QMetaObject::invokeMethod(mainWindow, "onFreezeUnfreeze");
    }
    return true;
}

// allow user to configure server name
void RemoteCine::onServerAddress()
{
    bool ok = false;
    QString text = QInputDialog::getText(this, tr("Enter Server Address"), tr("Server Address:"), QLineEdit::Normal, m_server, &ok);

    if (ok && !text.isEmpty())
    {
        m_server = text;
    }
}

// user connected/disconnected
void RemoteCine::onConnect(bool state)
{
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
        closeFile();
        onRefresh();
    }
}

void RemoteCine::onRefresh()
{
    tbl_dataList->blockSignals(true);
    auto dType = udtScreen;
    for (int cntr = 0; cntr < tbl_dataList->rowCount(); cntr++)
    {
        if (m_ulterius->isConnected() && m_ulterius->isDataAvailable(dType))
        {
            tbl_dataList->item(cntr, 0)->setFlags(Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
            if (!tbl_dataList->item(cntr, 0)->checkState())
            {
                tbl_dataList->item(cntr, 0)->setCheckState(Qt::Unchecked);
            }
            else
            {
                tbl_dataList->item(cntr, 0)->setCheckState(Qt::Checked);
            }
            tbl_dataList->item(cntr, 1)->setText("yes");
        }
        else
        {
            tbl_dataList->item(cntr, 0)->setFlags(Qt::ItemIsEnabled);
            tbl_dataList->item(cntr, 0)->setData(Qt::CheckStateRole, QVariant());
            tbl_dataList->item(cntr, 1)->setText("no");
        }
        dType = static_cast< uData> (static_cast< long int >(dType) << 1);
    }
    tbl_dataList->blockSignals(false);
}

void RemoteCine::onFreezeUnfreeze()
{
    tbl_dataList->blockSignals(true);
    if (m_ulterius->isConnected())
    {
        if (!m_ulterius->getFreezeState()) // Unfreeze
        {
            for (int cntr = 0; cntr < tbl_dataList->rowCount(); cntr++)
            {
                tbl_dataList->item(cntr, 3)->setText("0");
            }
            m_firstFrame = 0;
            m_lastFrame = 0;
        }
        else // Freeze
        {
            // Opening a new cine file - Ali B, Feb 2014
            createFile();
            for (int cntr = 0; cntr < tbl_dataList->rowCount(); cntr++)
            {
                if (tbl_dataList->item(cntr, 0)->checkState() == Qt::Checked)
                {
                    tbl_dataList->item(cntr, 3)->setText(QString("%1").arg(m_lastFrame - m_firstFrame + 1));
                }
            }
        }
    }
    tbl_dataList->blockSignals(false);
}

void RemoteCine::onDataSelect(QTableWidgetItem *itm)
{
    tbl_dataList->blockSignals(true);
    if (itm->checkState() == Qt::Checked)
    {
        // Enforce a single data type at a time because of bandwidth limitations - Ali B, Feb 2014
        for (int cntr = 0; cntr < tbl_dataList->rowCount(); cntr++)
        {
            if (cntr != itm->row() && (tbl_dataList->item(cntr, 0)->flags() & Qt::ItemIsUserCheckable))
            {
                tbl_dataList->item(cntr, 0)->setCheckState(Qt::Unchecked);
            }
        }
        // Updating the data type - Ali B, Feb 2014
        m_selRow = itm->row();
        // Opening a new cine file - Ali B, Feb 2014
        createFile();
        // Start listening for frames - Ali B, Feb 2014
        int dMask = static_cast< int >(udtScreen) << m_selRow;
        if (m_ulterius->isConnected())
        {
            m_ulterius->setDataToAcquire(dMask);
        }
    }
    else
    {
        // If none of the data types are selected, the signal was a result of deselcting the data type
        // therefore unregister from the server - Ali B, Feb 2014
        bool dataSelected = false;
        for (int cntr = 0; cntr < tbl_dataList->rowCount(); cntr++)
        {
            if (tbl_dataList->item(cntr, 0)->checkState() && (tbl_dataList->item(cntr, 0)->flags() & Qt::ItemIsUserCheckable))
            {
                dataSelected = true;
                break;
            }
        }
        if (!dataSelected)
        { 
            // No data type selected - Ali B, Feb 2014
            m_selRow = -1;
            // Stop listening for frames - Ali B, Feb 2014
            m_ulterius->setDataToAcquire(0);
            // Closing the cine file - Ali B, Feb 2014
            closeFile();
        }
    }
    tbl_dataList->blockSignals(false);
}


void RemoteCine::onStatusUpdate(QString msg)
{
    wStatus->showMessage(msg, MSG_TIMEOUT);
}

void RemoteCine::createFile()
{
    closeFile();

    QString fileExtenstion = ".dat";
    if (m_selRow > 0)
    {
        fileExtenstion = tbl_dataList->item(m_selRow, 4)->text();
    }
    QDateTime current = QDateTime::currentDateTime();
    m_fileName = current.time().toString().replace(':', '-') + fileExtenstion;

    m_cine = fopen(m_fileName.toStdString().c_str(), "wb+");
    writeHeader();
}

void RemoteCine::closeFile()
{
    if (m_cine)
    {
        long int sz = ftell(m_cine);
        fclose(m_cine);
        m_cine = NULL;
        if (sz <= sizeof(uFileHeader))
        {
            remove(m_fileName.toStdString().c_str());
        }
        else
        {
            int numFrames = static_cast< int >(m_lastFrame - m_firstFrame + 1);
            FILE* tmpFile = fopen(m_fileName.toStdString().c_str(), "r+b");
            fseek(tmpFile, sizeof(int), 0);
            fwrite(&numFrames, sizeof(int), 1, tmpFile);
            fclose(tmpFile);
        }
        m_fileName = "";
    }
    if (m_blankFrame)
    {
        delete [] m_blankFrame;
        m_blankFrame = NULL;
        m_frameSize = 0;
    }
}

void RemoteCine::writeHeader()
{
    uFileHeader hdr;
    uDataDesc   dataDesc;
    
    auto dType = static_cast< uData >(udtScreen << m_selRow);
    if (m_ulterius->isConnected())
    {
        m_ulterius->getDataDescriptor(dType, dataDesc);
    }

    hdr.type   = static_cast< int >(dataDesc.type);
    hdr.frames = 0;
    hdr.w      = dataDesc.w;
    hdr.h      = dataDesc.h;
    hdr.ss     = tbl_dataList->item(m_selRow, 2)->text().toInt();
    hdr.ulx    = dataDesc.roi.ulx;
    hdr.uly    = dataDesc.roi.uly;
    hdr.urx    = dataDesc.roi.urx;
    hdr.ury    = dataDesc.roi.ury;
    hdr.brx    = dataDesc.roi.brx;
    hdr.bry    = dataDesc.roi.bry;
    hdr.blx    = dataDesc.roi.blx;
    hdr.bly    = dataDesc.roi.bly;
    hdr.probe  = 2;
    hdr.txf    = 10 * 1000 * 1000;
    hdr.sf     = 40 * 1000 * 1000;
    hdr.dr     = 0;
    hdr.ld     = 128;
    hdr.extra  = 0;

    if (m_cine)
    {
        fwrite(&hdr, sizeof(uFileHeader), 1, m_cine);
    }

    m_frameSize = static_cast< size_t >(hdr.w * hdr.h * (hdr.ss / 8));
    if (m_blankFrame)
    {
        delete [] m_blankFrame;
    }
    m_blankFrame = new char[m_frameSize];
    memset(m_blankFrame, 0, m_frameSize);
}

void RemoteCine::onHandleSizeMismatch()
{
    if (m_ulterius->isConnected() && m_selRow >= 0)
    {
        // Stop listening for frames - Ali B, Feb 2014
        m_ulterius->setDataToAcquire(0);
        // Opening a new cine file - Ali B, Feb 2014
        createFile();
        // Start listening for frames - Ali B, Feb 2014
        int dMask = static_cast< int >(udtScreen) << m_selRow;
        m_ulterius->setDataToAcquire(dMask);
    }
}
