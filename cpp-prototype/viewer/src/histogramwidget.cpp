#include "histogramwidget.h"
#include <QMouseEvent>
#include <QPainter>
#include <cmath>
#include <algorithm>
#include <execution>

HistogramWidget::HistogramWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , maxValue_(0)
    , dataReady_(false)
    , overlayMode_(false)
    , showRGB_(true)
    , showLuma_(true)
    , logScale_(false)
    , isDragging_(false)
    , widgetPos_(10, 10)
{
    setMinimumSize(256, 150);
    if (!overlayMode_) {
        setFocusPolicy(Qt::StrongFocus);
    }
}

HistogramWidget::~HistogramWidget() {
    makeCurrent();
    vbo_.reset();
    vao_.reset();
    shaderProgram_.reset();
    doneCurrent();
}

void HistogramWidget::setOverlayMode(bool overlay) {
    overlayMode_ = overlay;
    if (overlay) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground);
    } else {
        setWindowFlags(Qt::Widget);
        setAttribute(Qt::WA_TranslucentBackground, false);
    }
    update();
}

void HistogramWidget::updateHistogram(const QImage &frame) {
    if (frame.isNull()) {
        return;
    }
    
    computeHistogram(frame);
    dataReady_ = true;
    update();
}

void HistogramWidget::clear() {
    histogramR_.fill(0);
    histogramG_.fill(0);
    histogramB_.fill(0);
    histogramLuma_.fill(0);
    maxValue_ = 0;
    dataReady_ = false;
    update();
}

void HistogramWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, overlayMode_ ? 0.7f : 1.0f);
    
    // Simple shader for histogram rendering
    shaderProgram_ = std::make_unique<QOpenGLShaderProgram>();
    
    const char *vertexShader = R"(
        #version 330 core
        layout(location = 0) in vec2 position;
        layout(location = 1) in vec3 color;
        out vec3 fragColor;
        void main() {
            gl_Position = vec4(position, 0.0, 1.0);
            fragColor = color;
        }
    )";
    
    const char *fragmentShader = R"(
        #version 330 core
        in vec3 fragColor;
        out vec4 outColor;
        void main() {
            outColor = vec4(fragColor, 1.0);
        }
    )";
    
    shaderProgram_->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
    shaderProgram_->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader);
    shaderProgram_->link();
    
    vao_ = std::make_unique<QOpenGLVertexArrayObject>();
    vao_->create();
    
    vbo_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    vbo_->create();
}

void HistogramWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void HistogramWidget::paintGL() {
    if (overlayMode_) {
        glClearColor(0.0f, 0.0f, 0.0f, 0.7f);
    } else {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT);
    
    if (!dataReady_) {
        return;
    }
    
    if (overlayMode_) {
        renderOverlay();
    } else {
        renderHistogram();
    }
}

void HistogramWidget::computeHistogram(const QImage &frame) {
    histogramR_.fill(0);
    histogramG_.fill(0);
    histogramB_.fill(0);
    histogramLuma_.fill(0);
    
    QImage rgb32 = frame.convertToFormat(QImage::Format_RGB32);
    
    // Compute histogram
    for (int y = 0; y < rgb32.height(); ++y) {
        const uint32_t *line = reinterpret_cast<const uint32_t*>(rgb32.constScanLine(y));
        for (int x = 0; x < rgb32.width(); ++x) {
            uint32_t pixel = line[x];
            uint8_t r = (pixel >> 16) & 0xFF;
            uint8_t g = (pixel >> 8) & 0xFF;
            uint8_t b = pixel & 0xFF;
            
            histogramR_[r]++;
            histogramG_[g]++;
            histogramB_[b]++;
            
            // Rec. 709 luma
            uint8_t luma = static_cast<uint8_t>(
                0.2126f * r + 0.7152f * g + 0.0722f * b
            );
            histogramLuma_[luma]++;
        }
    }
    
    // Find max value for normalization
    maxValue_ = 0;
    for (int i = 0; i < 256; ++i) {
        maxValue_ = std::max(maxValue_, histogramR_[i]);
        maxValue_ = std::max(maxValue_, histogramG_[i]);
        maxValue_ = std::max(maxValue_, histogramB_[i]);
        maxValue_ = std::max(maxValue_, histogramLuma_[i]);
    }
}

void HistogramWidget::renderHistogram() {
    if (maxValue_ == 0) {
        return;
    }
    
    // Use QPainter for simpler rendering
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    int w = width();
    int h = height();
    
    // Draw background grid
    painter.setPen(QPen(QColor(50, 50, 50), 1));
    for (int i = 0; i <= 4; ++i) {
        int y = h * i / 4;
        painter.drawLine(0, y, w, y);
    }
    for (int i = 0; i <= 4; ++i) {
        int x = w * i / 4;
        painter.drawLine(x, 0, x, h);
    }
    
    // Draw histograms
    auto drawChannel = [&](const std::array<uint32_t, 256> &hist, const QColor &color) {
        painter.setPen(QPen(color, 1));
        for (int i = 0; i < 256; ++i) {
            float value = static_cast<float>(hist[i]);
            if (logScale_ && value > 0) {
                value = std::log10(value + 1.0f);
                float maxLog = std::log10(static_cast<float>(maxValue_) + 1.0f);
                value = (value / maxLog) * h;
            } else {
                value = (value / static_cast<float>(maxValue_)) * h;
            }
            
            int x = (i * w) / 256;
            int barHeight = static_cast<int>(value);
            painter.drawLine(x, h, x, h - barHeight);
        }
    };
    
    if (showRGB_) {
        drawChannel(histogramR_, QColor(255, 100, 100, 180));
        drawChannel(histogramG_, QColor(100, 255, 100, 180));
        drawChannel(histogramB_, QColor(100, 100, 255, 180));
    }
    
    if (showLuma_) {
        drawChannel(histogramLuma_, QColor(200, 200, 200, 220));
    }
    
    // Draw labels
    painter.setPen(Qt::white);
    painter.drawText(5, 15, "Histogram");
    painter.drawText(5, h - 5, "0");
    painter.drawText(w - 25, h - 5, "255");
}

void HistogramWidget::renderOverlay() {
    renderHistogram();
    
    // Draw border for overlay mode
    QPainter painter(this);
    painter.setPen(QPen(QColor(255, 255, 255, 150), 2));
    painter.drawRect(1, 1, width() - 2, height() - 2);
}

void HistogramWidget::mousePressEvent(QMouseEvent *event) {
    if (overlayMode_ && event->button() == Qt::LeftButton) {
        isDragging_ = true;
        dragStartPos_ = event->globalPosition().toPoint() - pos();
        event->accept();
    }
}

void HistogramWidget::mouseMoveEvent(QMouseEvent *event) {
    if (overlayMode_ && isDragging_) {
        move(event->globalPosition().toPoint() - dragStartPos_);
        event->accept();
    }
}

void HistogramWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (overlayMode_ && event->button() == Qt::LeftButton) {
        isDragging_ = false;
        event->accept();
    }
}
