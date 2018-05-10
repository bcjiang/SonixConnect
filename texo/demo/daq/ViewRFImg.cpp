#include "stdafx.h"
#include "ViewRFImg.h"
#include "math.h"

#define bound(y, h) (y < 0 ? ((-y > h) ? -h : y) : ((y > h) ? h : y))

ViewRFImg::ViewRFImg(QWidget* parent) : QGraphicsView(parent)
{
    QGraphicsScene* sc = new QGraphicsScene(this);
    setScene(sc);

    m_scanLines = 32;
    m_samples = 0;
    m_buffer = 0;
    m_bufferImg = 0;
    m_image = 0;
    m_colorMap = 0;
}

ViewRFImg::~ViewRFImg()
{
    if (m_buffer)
    {
        delete[] m_buffer;
    }

    if (m_bufferImg)
    {
        delete[] m_bufferImg;
    }

    if (m_colorMap)
    {
        delete[] m_colorMap;
    }
}

bool ViewRFImg::init(int scanLines, int samples)
{
    m_scanLines = scanLines;
    m_samples = samples;
    int sz = m_scanLines * m_samples;

    if (!sz)
    {
        return false;
    }

    allocateBuffer(sz);
    assignBuffer(m_scanLines, m_samples);
    setSceneRect(0, 0, width(), height());

    createRGBColorMap();

    return true;
}

void ViewRFImg::resizeEvent(QResizeEvent* e)
{
    setSceneRect(0, 0, e->size().width(), e->size().height());
    assignBuffer(m_scanLines, m_samples);

    QGraphicsView::resizeEvent(e);
}

void ViewRFImg::drawBackground(QPainter* painter, const QRectF& r)
{
    QGraphicsView::drawBackground(painter, r);

    if (m_image)
    {
        QRect r1(0, 0, width(), height());
        QRect r2(0, 0, m_scanLines, m_samples);
        painter->drawImage(r1, *m_image, r2, Qt::MonoOnly);
    }
    else
    {
        painter->fillRect(0, 0, width(), height(), Qt::black);
    }
}

void ViewRFImg::convert2RGB(unsigned char* buffer, unsigned char* bufferImg)
{
    int* img = (int*)(bufferImg);

    //transpose and apply colormap
    for (int j = 0; j < m_scanLines; j++)
    {
        for (int i = 0; i < m_samples; i++)
        {
            img[i * m_scanLines + j] = m_colorMap[buffer[j * m_samples + i]];
        }
    }
}

bool ViewRFImg::setImgData(unsigned char* data)
{
    if (!m_buffer || !m_bufferImg || !m_image || !data)
    {
        return false;
    }

    memcpy(m_buffer, data, m_scanLines * m_samples);
    convert2RGB(m_buffer, m_bufferImg);

    scene()->invalidate();
    return true;
}

void ViewRFImg::assignBuffer(int w, int h)
{
    if (m_image)
    {
        delete m_image;
    }

    m_image = new QImage(m_bufferImg, w, h, QImage::Format_RGB32);
}

void ViewRFImg::allocateBuffer(int sz)
{
    if (m_buffer)
    {
        delete[] m_buffer;
    }
    if (m_bufferImg)
    {
        delete[] m_bufferImg;
    }

    m_buffer = new unsigned char[sz];
    m_bufferImg = new unsigned char[sz * sizeof(int)];  // RGB
}

void ViewRFImg::createRGBColorMap()
{
    // create RGB colormap
    if (m_colorMap)
    {
        delete[] m_colorMap;
    }
    m_colorMap = new int[256];
    for (int i = 0; i < 256; i++)
    {
        m_colorMap[i] = (i + (i << 8) + (i << 16));
    }
}
