#pragma once

#include "qgraphicsview.h"

class TexoViewImg : public QGraphicsView
{
public:
    TexoViewImg(QWidget* parent = 0);
    virtual ~TexoViewImg();

    bool init(int channels, int samples);
    void TexoViewImg::setAmp(int amp);
    virtual bool setImgData(char* buffer);

protected:
    virtual void drawBackground(QPainter*, const QRectF&);
    virtual void resizeEvent(QResizeEvent*);

protected:
    void setupBuffer(int w, int h);
    void convertRF2Img(char* buffer, unsigned char* bufferImg);
    void getOptDims(int& w, int& h);

protected:

    int m_scanLines;
    int m_samples;
    int m_amp;
    char* m_buffer;
    unsigned char* m_bufferImg;
    QImage* m_image;
    int* m_map;
};
