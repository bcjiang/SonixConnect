#pragma once

#include "qgraphicsview.h"
#include <cstdint>
#include <memory>

class TexoViewImg : public QGraphicsView
{
public:
    TexoViewImg(QWidget* parent = 0);
    virtual ~TexoViewImg();

    bool init(int X, int Y, bool transpose = false);
    void setAmp(double amp);
    virtual bool setImgData(const void* buffer);

protected:
    virtual void drawBackground(QPainter*, const QRectF&);
    virtual void resizeEvent(QResizeEvent*);

protected:
    void convertRF2Img(const int16_t* buffer, uint32_t* bufferImg) const;
    QSize getOptDims() const;

protected:

    int m_width;
    int m_height;

    bool m_transpose;

    double m_amp;

    std::unique_ptr< int16_t[] > m_buffer;
    std::unique_ptr< uint32_t[] > m_bufferImg;
    std::unique_ptr< QImage > m_image;
};
