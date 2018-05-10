#pragma once

#include "qgraphicsview.h"

class ViewBImg : public QGraphicsView
{
public:
    ViewBImg(QWidget* parent = 0);
    virtual ~ViewBImg();

    bool init(int channels, int samples);
    virtual bool setImgData(unsigned char* buffer);

protected:
    virtual void drawBackground(QPainter*, const QRectF&);
    virtual void resizeEvent(QResizeEvent*);

protected:
    void allocateBuffer(int sZ);
    void assignBuffer(int w, int h);
    void convert2RGB(unsigned char* buffer, unsigned char* bufferImg);
    void createRGBColorMap();

protected:

    int m_scanLines;
    int m_samples;
    unsigned char* m_buffer;
    unsigned char* m_bufferImg;
    QImage* m_image;
    int* m_colorMap;
};
