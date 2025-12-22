#include "scanlineplotwidget.h"
#include <QMouseEvent>
#include <QPainter>
#include <cmath>

ScanlinePlotWidget::ScanlinePlotWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , scanlineY_(0)
    , dataReady_(false)
    , overlayMode_(false)
    , showRGB_(true)
    , showLuma_(true)
    , isDragging_(false)
    , widgetPos_(10, 10)
{
    setMinimumSize(400, 150);
    if (!overlayMode_) {
        setFocusPolicy(Qt::StrongFocus);
    }
}

ScanlinePlotWidget::~ScanlinePlotWidget() {
    makeCurrent();
    vboR_.reset();
    vboG_.reset();
    vboB_.reset();
    vboLuma_.reset();
    vao_.reset();
    shaderProgram_.reset();
    doneCurrent();
}

void ScanlinePlotWidget::setOverlayMode(bool overlay) {
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

void ScanlinePlotWidget::updateScanline(const QImage &frame, int scanlineY) {
    if (frame.isNull()) {
        return;
    }
    
    scanlineY_ = scanlineY;
    extractScanline(frame, scanlineY);
    dataReady_ = true;
    update();
}

void ScanlinePlotWidget::clear() {
    scanlineR_.clear();
    scanlineG_.clear();
    scanlineB_.clear();
    scanlineLuma_.clear();
    dataReady_ = false;
    update();
}

void ScanlinePlotWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, overlayMode_ ? 0.7f : 1.0f);
    
    vao_ = std::make_unique<QOpenGLVertexArrayObject>();
    vao_->create();
    
    vboR_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    vboR_->create();
    
    vboG_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    vboG_->create();
    
    vboB_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    vboB_->create();
    
    vboLuma_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    vboLuma_->create();
}

void ScanlinePlotWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void ScanlinePlotWidget::paintGL() {
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
        renderScanline();
    }
}

void ScanlinePlotWidget::extractScanline(const QImage &frame, int scanlineY) {
    if (scanlineY < 0 || scanlineY >= frame.height()) {
        return;
    }
    
    scanlineR_.clear();
    scanlineG_.clear();
    scanlineB_.clear();
    scanlineLuma_.clear();
    
    QImage rgb32 = frame.convertToFormat(QImage::Format_RGB32);
    const uint32_t *line = reinterpret_cast<const uint32_t*>(rgb32.constScanLine(scanlineY));
    
    for (int x = 0; x < rgb32.width(); ++x) {
        uint32_t pixel = line[x];
        float r = ((pixel >> 16) & 0xFF) / 255.0f;
        float g = ((pixel >> 8) & 0xFF) / 255.0f;
        float b = (pixel & 0xFF) / 255.0f;
        
        scanlineR_.push_back(r);
        scanlineG_.push_back(g);
        scanlineB_.push_back(b);
        
        // Rec. 709 luma
        float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        scanlineLuma_.push_back(luma);
    }
}

void ScanlinePlotWidget::renderScanline() {
    if (scanlineR_.empty()) {
        return;
    }
    
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
    for (int i = 0; i <= 8; ++i) {
        int x = w * i / 8;
        painter.drawLine(x, 0, x, h);
    }
    
    // Draw scanline data
    auto drawChannel = [&](const std::vector<float> &data, const QColor &color) {
        if (data.empty()) return;
        
        painter.setPen(QPen(color, 2));
        
        for (size_t i = 1; i < data.size(); ++i) {
            float x1 = ((i - 1) * w) / static_cast<float>(data.size());
            float y1 = h - (data[i - 1] * h);
            float x2 = (i * w) / static_cast<float>(data.size());
            float y2 = h - (data[i] * h);
            
            painter.drawLine(
                static_cast<int>(x1), static_cast<int>(y1),
                static_cast<int>(x2), static_cast<int>(y2)
            );
        }
    };
    
    if (showRGB_) {
        drawChannel(scanlineR_, QColor(255, 100, 100, 200));
        drawChannel(scanlineG_, QColor(100, 255, 100, 200));
        drawChannel(scanlineB_, QColor(100, 100, 255, 200));
    }
    
    if (showLuma_) {
        drawChannel(scanlineLuma_, QColor(220, 220, 220, 230));
    }
    
    // Draw labels
    painter.setPen(Qt::white);
    painter.drawText(5, 15, QString("Scanline: %1").arg(scanlineY_));
    painter.drawText(5, h - 5, "0");
    painter.drawText(w - 30, h - 5, QString("%1").arg(scanlineR_.size()));
}

void ScanlinePlotWidget::renderOverlay() {
    renderScanline();
    
    // Draw border for overlay mode
    QPainter painter(this);
    painter.setPen(QPen(QColor(255, 255, 255, 150), 2));
    painter.drawRect(1, 1, width() - 2, height() - 2);
}

void ScanlinePlotWidget::mousePressEvent(QMouseEvent *event) {
    if (overlayMode_ && event->button() == Qt::LeftButton) {
        isDragging_ = true;
        dragStartPos_ = event->globalPosition().toPoint() - pos();
        event->accept();
    }
}

void ScanlinePlotWidget::mouseMoveEvent(QMouseEvent *event) {
    if (overlayMode_ && isDragging_) {
        move(event->globalPosition().toPoint() - dragStartPos_);
        event->accept();
    }
}

void ScanlinePlotWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (overlayMode_ && event->button() == Qt::LeftButton) {
        isDragging_ = false;
        event->accept();
    }
}
