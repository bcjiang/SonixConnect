#include "stdafx.h"
#include "TexoViewImg.h"
#include "math.h"

namespace
{
    template< typename T >
    T bound(T y, T h)
    {
        if (y < 0)
        {
            return (-y > h) ? -h : y;
        }
        else
        {
            return (y > h) ? h : y;
        }
    }
}

TexoViewImg::TexoViewImg(QWidget* parent) : QGraphicsView(parent)
{
    QGraphicsScene* sc = new QGraphicsScene(this);
    setScene(sc);

    m_width = 0;
    m_height = 0;
    m_amp = 1.0;
    m_transpose = false;
}

TexoViewImg::~TexoViewImg()
{
}

bool TexoViewImg::init(int X, int Y, bool transpose)
{
    if (X * Y == 0)
    {
        return false;
    }

    m_width = X;
    m_height = Y;
    m_transpose = transpose;

    m_buffer.reset();
    m_bufferImg.reset();

    // RGB buffer for the image
    int sz = m_width * m_height;
    m_buffer.reset(new int16_t[sz]);
    m_bufferImg.reset(new uint32_t[sz]);

    if (!m_transpose)
    {
        m_image.reset(new QImage(reinterpret_cast< const uchar* >(m_bufferImg.get()), m_height, m_width, QImage::Format_RGB32));
    }
    else
    {
        m_image.reset(new QImage(reinterpret_cast< const uchar* >(m_bufferImg.get()), m_width, m_height, QImage::Format_RGB32));
    }

    setSceneRect(0, 0, width(), height());

    return true;
}

void TexoViewImg::resizeEvent(QResizeEvent* e)
{
    setSceneRect(0, 0, e->size().width(), e->size().height());
    QGraphicsView::resizeEvent(e);
}

void TexoViewImg::drawBackground(QPainter* painter, const QRectF& r)
{
    QGraphicsView::drawBackground(painter, r);

    if (m_image)
    {
        if (!m_transpose)
        {
            painter->drawImage(QRect(0, 0, width(), height()), *m_image, QRect(0, 0, m_height, m_width), Qt::MonoOnly);
        }
        else
        {
            painter->drawImage(QRect(0, 0, width(), height()), *m_image, QRect(0, 0, m_width, m_height), Qt::MonoOnly);
            painter->setPen(Qt::white);
            painter->setFont(QFont("Arial", 12));
            // lateral lines
            for (int i = 1; i < 4; i++)
            {
                painter->drawText(i * width() / 4 + 1, height() - 10, QString("%1").arg(i * m_width / 4));
            }
            for (int i = 3; i < m_width; i += 5)
            {
                painter->drawEllipse(QPoint(i * width() / m_width, height() - 5), 1, 1);
            }
            // axial samples
            for (int i = 1; i < 4; i++)
            {
                painter->drawText(0, i * height() / 4, QString("%1").arg(i * m_height / 4));
            }
            for (int i = 100; i < m_height; i += 50)
            {
                painter->drawEllipse(QPoint(2, i * height() / m_height), 1, 1);
            }
        }
    }
    else
    {
        painter->fillRect(0, 0, width(), height(), Qt::black);
    }
}

void TexoViewImg::convertRF2Img(const int16_t* in, uint32_t* out) const
{
    for (int j = 0; j < m_width; j++)
    {
        for (int i = 0; i < m_height; i++)
        {
            int rf = in[j * m_height + i];
            int val = 127 + bound(int(rf * m_amp), 127);
            if (!m_transpose)
            {
                out[j * m_height + i] = (val + (val << 8) + (val << 16));
            }
            else
            {
                out[i * m_width + j] = (val + (val << 8) + (val << 16));
            }
        }
    }
}

bool TexoViewImg::setImgData(const void* data)
{
    if (!m_buffer || !m_bufferImg || !m_image)
    {
        return false;
    }

    memcpy(m_buffer.get(), data, m_width * m_height * sizeof(short));
    convertRF2Img(m_buffer.get(), m_bufferImg.get());

    scene()->invalidate();
    return true;
}

void TexoViewImg::setAmp(double amp)
{
    m_amp = amp;
    convertRF2Img(m_buffer.get(), m_bufferImg.get());
    scene()->invalidate();
}
