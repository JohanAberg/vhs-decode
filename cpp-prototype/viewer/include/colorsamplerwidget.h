#pragma once

#include <QWidget>
#include <QImage>
#include <QPoint>

class ColorSamplerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ColorSamplerWidget(QWidget *parent = nullptr);
    ~ColorSamplerWidget() override = default;

    void setSamplePosition(const QPoint &pos);
    void updateFrame(const QImage &frame);
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void updateSample();
    
    QImage currentFrame_;
    QPoint samplePos_;
    
    // Current pixel values
    int pixelX_;
    int pixelY_;
    uint8_t valueR_;
    uint8_t valueG_;
    uint8_t valueB_;
    uint8_t valueLuma_;
    bool hasValidSample_;
};
