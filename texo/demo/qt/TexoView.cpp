#include "stdafx.h"
#include "TexoView.h"

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

TexoView::TexoView(QWidget* parent) : QGraphicsView(parent)
{
    QGraphicsScene* sc = new QGraphicsScene(this);
    setScene(sc);

    m_scanLines = 0;
    m_startChDisplay = 0;
    m_startSampleDisplay = 0;
    m_numChDisplay = 8;
    m_samples = 0;
    m_ssize = 2;
    m_scale = 1.0;
    m_zoom = 1.0;
    m_amp = 1.0;
}

TexoView::~TexoView()
{
}

bool TexoView::init(int scanLines, int samples)
{
    m_scanLines = scanLines;
    m_samples = samples;
    m_ssize = 2;

    m_buffer.reset();

    if (m_samples)
    {
        m_buffer.reset(new int16_t[m_scanLines * m_samples]);
        memset(m_buffer.get(), 0, m_scanLines * m_samples * m_ssize);
    }

    adjustScale();

    setSceneRect(0, 0, width(), height());

    return true;
}

void TexoView::adjustScale()
{
    if (m_samples)
    {
        m_scale = (double)width() / (double)m_samples;
    }
}

void TexoView::setZoomFactor(double zoom)
{
    if (zoom > 0)
    {
        m_zoom = zoom;
    }
}

void TexoView::setAmp(double amp)
{
    m_amp = amp;
    scene()->invalidate();
}

void TexoView::setStartChDisplay(int start)
{
    if (start > m_scanLines)
    {
        return;
    }

    m_startChDisplay = start;
    scene()->invalidate();
}

void TexoView::setNumChDisplay(int numDisplay)
{
    if (numDisplay < 1)
    {
        return;
    }

    m_numChDisplay = numDisplay;
    scene()->invalidate();
}

void TexoView::setStartSmpleDisplay(int smd)
{
    if (smd < 1)
    {
        return;
    }

    m_startSampleDisplay = smd;
    scene()->invalidate();
}

void TexoView::display(const void* data)
{
    if (m_buffer)
    {
        memcpy(m_buffer.get(), data, m_scanLines * m_samples * m_ssize);
        scene()->invalidate();
    }
}

void TexoView::resizeEvent(QResizeEvent* e)
{
    setSceneRect(0, 0, e->size().width(), e->size().height());

    adjustScale();

    QGraphicsView::resizeEvent(e);
}

void TexoView::drawBackground(QPainter* painter, const QRectF& r)
{
    QGraphicsView::drawBackground(painter, r);

    painter->fillRect(0, 0, width(), height(), Qt::black);
}

void TexoView::drawForeground(QPainter* painter, const QRectF& r)
{
    QGraphicsView::drawForeground(painter, r);

    if (m_buffer)
    {
        int numDisplay = m_numChDisplay;  // avoid race

        int h = height() / numDisplay;
        double amp = m_amp;
        // data pointer
        const auto* data = m_buffer.get();
        // set the background
        painter->fillRect(0, 0, width(), height(), Qt::darkGray);
        // set the font
        painter->setFont(QFont("Arial", (h / 4 < 10) ? 10 : h / 4));
        // find channel indices
        int startCh = m_startChDisplay;
        int endCh = startCh + numDisplay;
        if (endCh > m_scanLines)
        {
            endCh = m_scanLines;
        }
        // find start of sample data
        int nSamples = static_cast< int >(m_samples / m_zoom);
        int startSample = m_startSampleDisplay;
        int endSample = startSample + nSamples;
        if (endSample > m_samples)
        {
            endSample = m_samples;
        }

        // display channel
        for (int i = startCh; i < endCh; i++)
        {
            // 1) set the scale
            painter->scale(m_scale * m_zoom, 1);
            // 2) plot
            int x0 = 0;
            int y0 = (i - startCh) * h + (h / 2);
            painter->setPen(QColor(40, 50, 130));
            QPainterPath pp(QPoint(x0, static_cast< int >(y0 + (data[i * m_samples + startSample] * amp))));
            for (int x = startSample + 1; x < endSample; x++)
            {
                int y = bound(static_cast< int >(data[i * m_samples + x] * amp), (h / 2));
                pp.lineTo(x0 + (x - startSample), y0 + y);
            }
            painter->drawPath(pp);
            // 3) reset the scale
            painter->resetTransform();

            // box on
            painter->setPen(Qt::black);
            painter->drawRect(x0, y0 - h / 2, width(), h);
            // draw center line
            painter->setPen(Qt::black);
            painter->drawLine(x0, y0, width(), y0);
            // write label
            painter->setPen(Qt::white);
            painter->drawText(x0, y0 + h / 2, QString("%1").arg(i));
        }
    }
}
