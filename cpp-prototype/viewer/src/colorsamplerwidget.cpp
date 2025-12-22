#include "colorsamplerwidget.h"
#include <QPainter>
#include <QFont>

ColorSamplerWidget::ColorSamplerWidget(QWidget *parent)
    : QWidget(parent)
    , pixelX_(0)
    , pixelY_(0)
    , valueR_(0)
    , valueG_(0)
    , valueB_(0)
    , valueLuma_(0)
    , hasValidSample_(false)
{
    setMinimumHeight(60);
    setMaximumHeight(60);
}

void ColorSamplerWidget::setSamplePosition(const QPoint &pos) {
    samplePos_ = pos;
    updateSample();
    update();
}

void ColorSamplerWidget::updateFrame(const QImage &frame) {
    currentFrame_ = frame;
    updateSample();
    update();
}

void ColorSamplerWidget::clear() {
    currentFrame_ = QImage();
    hasValidSample_ = false;
    update();
}

void ColorSamplerWidget::updateSample() {
    if (currentFrame_.isNull()) {
        hasValidSample_ = false;
        return;
    }
    
    pixelX_ = samplePos_.x();
    pixelY_ = samplePos_.y();
    
    // Check bounds
    if (pixelX_ < 0 || pixelX_ >= currentFrame_.width() ||
        pixelY_ < 0 || pixelY_ >= currentFrame_.height()) {
        hasValidSample_ = false;
        return;
    }
    
    // Get pixel value
    QImage rgb32 = currentFrame_.convertToFormat(QImage::Format_RGB32);
    uint32_t pixel = rgb32.pixel(pixelX_, pixelY_);
    
    valueR_ = (pixel >> 16) & 0xFF;
    valueG_ = (pixel >> 8) & 0xFF;
    valueB_ = pixel & 0xFF;
    
    // Compute luma (Rec. 709)
    valueLuma_ = static_cast<uint8_t>(
        0.2126f * valueR_ + 0.7152f * valueG_ + 0.0722f * valueB_
    );
    
    hasValidSample_ = true;
}

void ColorSamplerWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Background
    painter.fillRect(rect(), QColor(30, 30, 30));
    
    if (!hasValidSample_) {
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter, "No sample");
        return;
    }
    
    int w = width();
    int h = height();
    
    // Draw color swatch
    int swatchSize = h - 10;
    QRect swatchRect(10, 5, swatchSize, swatchSize);
    painter.fillRect(swatchRect, QColor(valueR_, valueG_, valueB_));
    painter.setPen(Qt::white);
    painter.drawRect(swatchRect);
    
    // Draw pixel info
    QFont font("Monospace", 10);
    painter.setFont(font);
    painter.setPen(Qt::white);
    
    int textX = swatchSize + 20;
    int lineHeight = 15;
    
    painter.drawText(textX, 15, QString("Position: (%1, %2)").arg(pixelX_).arg(pixelY_));
    painter.drawText(textX, 15 + lineHeight, QString("RGB: (%1, %2, %3)")
        .arg(valueR_, 3).arg(valueG_, 3).arg(valueB_, 3));
    painter.drawText(textX, 15 + lineHeight * 2, QString("Luma: %1").arg(valueLuma_, 3));
    
    // Draw color bars
    int barX = textX + 200;
    int barWidth = w - barX - 10;
    int barHeight = 12;
    
    if (barWidth > 50) {
        // Red bar
        painter.fillRect(barX, 5, (barWidth * valueR_) / 255, barHeight, QColor(255, 0, 0));
        painter.setPen(Qt::white);
        painter.drawRect(barX, 5, barWidth, barHeight);
        
        // Green bar
        painter.fillRect(barX, 5 + barHeight + 2, (barWidth * valueG_) / 255, barHeight, QColor(0, 255, 0));
        painter.drawRect(barX, 5 + barHeight + 2, barWidth, barHeight);
        
        // Blue bar
        painter.fillRect(barX, 5 + (barHeight + 2) * 2, (barWidth * valueB_) / 255, barHeight, QColor(0, 0, 255));
        painter.drawRect(barX, 5 + (barHeight + 2) * 2, barWidth, barHeight);
    }
}
