#pragma once

#include "qgraphicsview.h"
#include <cstdint>
#include <memory>

class TexoView : public QGraphicsView
{
public:
    TexoView(QWidget* parent = 0);
    virtual ~TexoView();

    bool init(int channels, int samples);
    void setStartChDisplay(int chd);
    void setStartSmpleDisplay(int smd);
    void setNumChDisplay(int numDisplay);
    void display(const void* data);
    void setZoomFactor(double zoom);
    void setAmp(double amp);

protected:
    void adjustScale();
    virtual void drawForeground(QPainter* painter, const QRectF& r);
    virtual void drawBackground(QPainter*, const QRectF&);
    virtual void resizeEvent(QResizeEvent*);

protected:

    int m_scanLines;
    int m_samples;
    int m_ssize;
    int m_numChDisplay;
    int m_startChDisplay;
    int m_startSampleDisplay;
    double m_amp;
    double m_scale;
    double m_zoom;
    std::unique_ptr< int16_t[] > m_buffer;
};
