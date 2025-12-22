#include "vectorscopewidget.h"
#include <QMouseEvent>
#include <QPainter>
#include <cmath>

VectorscopeWidget::VectorscopeWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , dataReady_(false)
    , overlayMode_(false)
    , intensity_(1.0f)
    , showTargets_(true)
    , isDragging_(false)
    , widgetPos_(10, 10)
{
    setMinimumSize(256, 256);
    if (!overlayMode_) {
        setFocusPolicy(Qt::StrongFocus);
    }
    
    // Initialize standard color bar targets (75% amplitude)
    // Rec. 709 color space
    targets_ = {
        {0.0f, 0.436f, 0.75f, 0.75f, 0.75f},    // White
        {-0.291f, 0.291f, 0.75f, 0.75f, 0.0f},  // Yellow
        {-0.291f, -0.145f, 0.0f, 0.75f, 0.75f}, // Cyan
        {0.0f, -0.436f, 0.0f, 0.75f, 0.0f},     // Green
        {0.291f, 0.145f, 0.75f, 0.0f, 0.75f},   // Magenta
        {0.291f, -0.291f, 0.75f, 0.0f, 0.0f},   // Red
        {0.0f, 0.436f, 0.0f, 0.0f, 0.75f}       // Blue
    };
}

VectorscopeWidget::~VectorscopeWidget() {
    makeCurrent();
    vbo_.reset();
    vao_.reset();
    pointShader_.reset();
    targetShader_.reset();
    doneCurrent();
}

void VectorscopeWidget::setOverlayMode(bool overlay) {
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

void VectorscopeWidget::updateVectorscope(const QImage &frame) {
    if (frame.isNull()) {
        return;
    }
    
    computeVectorscope(frame);
    dataReady_ = true;
    update();
}

void VectorscopeWidget::clear() {
    vectorData_.clear();
    dataReady_ = false;
    update();
}

void VectorscopeWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, overlayMode_ ? 0.7f : 1.0f);
    
    // Shader for point rendering
    pointShader_ = std::make_unique<QOpenGLShaderProgram>();
    
    const char *vertexShader = R"(
        #version 330 core
        layout(location = 0) in vec2 position;
        uniform float intensity;
        void main() {
            gl_Position = vec4(position, 0.0, 1.0);
            gl_PointSize = 2.0;
        }
    )";
    
    const char *fragmentShader = R"(
        #version 330 core
        uniform float intensity;
        out vec4 outColor;
        void main() {
            outColor = vec4(0.0, 1.0, 0.0, intensity);
        }
    )";
    
    pointShader_->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
    pointShader_->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader);
    pointShader_->link();
    
    vao_ = std::make_unique<QOpenGLVertexArrayObject>();
    vao_->create();
    
    vbo_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    vbo_->create();
}

void VectorscopeWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void VectorscopeWidget::paintGL() {
    if (overlayMode_) {
        glClearColor(0.0f, 0.0f, 0.0f, 0.7f);
    } else {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT);
    
    if (overlayMode_) {
        renderOverlay();
    } else {
        renderVectorscope();
    }
}

void VectorscopeWidget::computeVectorscope(const QImage &frame) {
    vectorData_.clear();
    
    QImage rgb32 = frame.convertToFormat(QImage::Format_RGB32);
    
    // Sample every 4th pixel for performance
    for (int y = 0; y < rgb32.height(); y += 4) {
        const uint32_t *line = reinterpret_cast<const uint32_t*>(rgb32.constScanLine(y));
        for (int x = 0; x < rgb32.width(); x += 4) {
            uint32_t pixel = line[x];
            float r = ((pixel >> 16) & 0xFF) / 255.0f;
            float g = ((pixel >> 8) & 0xFF) / 255.0f;
            float b = (pixel & 0xFF) / 255.0f;
            
            // RGB to YUV (Rec. 709)
            float y_comp = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            float u = (b - y_comp) / 1.8556f;  // Cb component
            float v = (r - y_comp) / 1.5748f;  // Cr component
            
            // Normalize to -1 to +1 range for display
            vectorData_.push_back(u * 2.0f);
            vectorData_.push_back(v * 2.0f);
        }
    }
}

void VectorscopeWidget::renderVectorscope() {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    int w = width();
    int h = height();
    int centerX = w / 2;
    int centerY = h / 2;
    int radius = std::min(w, h) / 2 - 10;
    
    // Draw circular graticule
    painter.setPen(QPen(QColor(50, 50, 50), 1));
    for (int i = 1; i <= 4; ++i) {
        int r = (radius * i) / 4;
        painter.drawEllipse(centerX - r, centerY - r, r * 2, r * 2);
    }
    
    // Draw crosshairs
    painter.drawLine(centerX - radius, centerY, centerX + radius, centerY);
    painter.drawLine(centerX, centerY - radius, centerX, centerY + radius);
    
    // Draw target boxes if enabled
    if (showTargets_) {
        renderTargets();
    }
    
    // Draw vector data
    if (dataReady_ && !vectorData_.empty()) {
        painter.setPen(QPen(QColor(0, 255, 0, static_cast<int>(intensity_ * 100)), 1));
        
        for (size_t i = 0; i < vectorData_.size(); i += 2) {
            float u = vectorData_[i];
            float v = vectorData_[i + 1];
            
            // Clamp to -1 to +1
            u = std::max(-1.0f, std::min(1.0f, u));
            v = std::max(-1.0f, std::min(1.0f, v));
            
            int x = centerX + static_cast<int>(u * radius);
            int y = centerY - static_cast<int>(v * radius);  // Invert Y
            
            painter.drawPoint(x, y);
        }
    }
    
    // Draw labels
    painter.setPen(Qt::white);
    painter.drawText(5, 15, "Vectorscope");
    painter.drawText(centerX + radius - 10, centerY - 5, "R");
    painter.drawText(centerX - radius + 5, centerY - 5, "Cy");
    painter.drawText(centerX - 5, centerY - radius + 15, "G");
    painter.drawText(centerX - 5, centerY + radius - 5, "Mg");
}

void VectorscopeWidget::renderTargets() {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    int w = width();
    int h = height();
    int centerX = w / 2;
    int centerY = h / 2;
    int radius = std::min(w, h) / 2 - 10;
    
    painter.setPen(QPen(QColor(255, 255, 255, 100), 1));
    
    for (const auto &target : targets_) {
        int x = centerX + static_cast<int>(target.u * radius);
        int y = centerY - static_cast<int>(target.v * radius);  // Invert Y
        
        // Draw small box
        int boxSize = 8;
        painter.drawRect(x - boxSize/2, y - boxSize/2, boxSize, boxSize);
    }
}

void VectorscopeWidget::renderOverlay() {
    renderVectorscope();
    
    // Draw border for overlay mode
    QPainter painter(this);
    painter.setPen(QPen(QColor(255, 255, 255, 150), 2));
    painter.drawRect(1, 1, width() - 2, height() - 2);
}

void VectorscopeWidget::mousePressEvent(QMouseEvent *event) {
    if (overlayMode_ && event->button() == Qt::LeftButton) {
        isDragging_ = true;
        dragStartPos_ = event->globalPosition().toPoint() - pos();
        event->accept();
    }
}

void VectorscopeWidget::mouseMoveEvent(QMouseEvent *event) {
    if (overlayMode_ && isDragging_) {
        move(event->globalPosition().toPoint() - dragStartPos_);
        event->accept();
    }
}

void VectorscopeWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (overlayMode_ && event->button() == Qt::LeftButton) {
        isDragging_ = false;
        event->accept();
    }
}
