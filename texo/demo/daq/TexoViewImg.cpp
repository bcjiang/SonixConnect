#include "stdafx.h"
#include "TexoViewImg.h"

#define bound(y, h) (y < 0 ? ((-y > h) ? -h : y) : ((y > h) ? h : y))

TexoViewImg::TexoViewImg(QWidget* parent) : QGraphicsView(parent)
{
    QGraphicsScene* sc = new QGraphicsScene(this);
    setScene(sc);

    m_scanLines = 32;
    m_samples = 0;
    m_buffer = 0;
    m_bufferImg = 0;
    m_image = 0;
    m_amp = 100;
    m_map = 0;
}

TexoViewImg::~TexoViewImg()
{
    if (m_buffer)
    {
        delete[] m_buffer;
    }

    if (m_bufferImg)
    {
        delete[] m_bufferImg;
    }

    if (m_map)
    {
        delete[] m_map;
    }
}

bool TexoViewImg::init(int scanLines, int samples)
{
    m_scanLines = scanLines;
    m_samples = samples;

    if (m_buffer)
    {
        delete[] m_buffer;
    }

    if (m_bufferImg)
    {
        delete[] m_bufferImg;
    }

    int sz = m_scanLines * m_samples;
    if (sz)
    {
        // RGB buffer for the image
        m_buffer = new char[sz * sizeof(short)];
        m_bufferImg = new unsigned char[sz * 4];
    }

    int w, h;
    getOptDims(w, h);
    setupBuffer(w, h);
    setSceneRect(0, 0, width(), height());

    if (m_map)
    {
        delete[] m_map;
    }
    // create the RGB colormap
    m_map = new int[256];
    for (int i = 0; i < 256; i++)
    {
        m_map[i] = ((3 * i / 4) + (i << 8) + ((3 * i / 4) << 16));
    }

    return true;
}

void TexoViewImg::resizeEvent(QResizeEvent* e)
{
    setSceneRect(0, 0, e->size().width(), e->size().height());
    int w, h;
    getOptDims(w, h);
    setupBuffer(w, h);

    QGraphicsView::resizeEvent(e);
}

void TexoViewImg::drawBackground(QPainter* painter, const QRectF& r)
{
    QGraphicsView::drawBackground(painter, r);

    if (m_image)
    {
        QRect r1(0, 0, width(), height());
        int w, h;
        getOptDims(w, h);
        QRect r2(0, 0, w, h);
        painter->drawImage(r1, *m_image, r2, Qt::MonoOnly);
    }
    else
    {
        painter->fillRect(0, 0, width(), height(), Qt::black);
    }
}

void TexoViewImg::convertRF2Img(char* buffer, unsigned char* bufferImg)
{
    short* data = (short*)(buffer);
    int* img = (int*)(bufferImg);

    for (int j = 0; j < m_scanLines; j++)
    {
        for (int i = 0; i < m_samples; i++)
        {
            int rf = data[j * m_samples + i];
            int val = 127 + bound(rf * m_amp / 100, 127);
            //transpose and apply colormap
            img[i * m_scanLines + j] = m_map[(unsigned char)val];
        }
    }
}

bool TexoViewImg::setImgData(char* data)
{
    if (!m_buffer || !m_bufferImg || !m_image)
    {
        return false;
    }

    memcpy(m_buffer, data, m_scanLines * m_samples * sizeof(short));
    convertRF2Img(m_buffer, m_bufferImg);

    scene()->invalidate();
    return true;
}

void TexoViewImg::getOptDims(int& w, int& h)
{
    //transpose the image
    w = m_scanLines;
    h = m_samples;
}

void TexoViewImg::setupBuffer(int w, int h)
{
    if (m_image)
    {
        delete m_image;
    }

    m_image = new QImage(m_bufferImg, w, h, QImage::Format_RGB32);
}

void TexoViewImg::setAmp(int amp)
{
    if (amp > 0)
    {
        m_amp = amp;
    }
    scene()->invalidate();
}
