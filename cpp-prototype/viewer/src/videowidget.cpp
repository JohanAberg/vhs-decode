#include "videowidget.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLFunctions>
#include <QPainter>
#include <cmath>

VideoWidget::VideoWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , zoom_(1.0f)
    , pan_(0.0f, 0.0f)
    , isPanning_(false)
    , viewportWidth_(800)
    , viewportHeight_(600)
    , gamma_(2.2f)
    , exposure_(0.0f)
    , colorProfile_(0)  // Rec709 default
    , showHairCross_(false)
    , hairCrossPos_(0, 0)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);  // Track mouse for hair cross
}

VideoWidget::~VideoWidget() {
    makeCurrent();
    texture_.reset();
    doneCurrent();
}

void VideoWidget::setFrame(const QImage &frame) {
    currentFrame_ = frame;
    updateTexture();
    update();
}

void VideoWidget::clearFrame() {
    currentFrame_ = QImage();
    texture_.reset();
    update();
}

void VideoWidget::setZoom(float zoom) {
    zoom_ = std::max(0.1f, std::min(zoom, 10.0f));
    update();
}

void VideoWidget::setPan(const QPointF &pan) {
    pan_ = pan;
    update();
}

void VideoWidget::resetView() {
    zoom_ = 1.0f;
    pan_ = QPointF(0.0f, 0.0f);
    update();
}

void VideoWidget::initializeGL() {
    // Qt6 QOpenGLWidget handles initialization automatically
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
}

void VideoWidget::resizeGL(int w, int h) {
    viewportWidth_ = w;
    viewportHeight_ = h;
    glViewport(0, 0, w, h);
}

void VideoWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);
    
    if (currentFrame_.isNull() || !texture_) {
        return;
    }
    
    // Apply color transforms
    QImage displayFrame = currentFrame_;
    if (gamma_ != 2.2f || exposure_ != 0.0f || colorProfile_ != 0) {
        applyColorTransform(displayFrame);
    }
    
    // Use QPainter for simple 2D rendering
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    // Calculate display rectangle with zoom and pan
    float imageAspect = static_cast<float>(displayFrame.width()) / displayFrame.height();
    float widgetAspect = static_cast<float>(viewportWidth_) / viewportHeight_;
    
    float displayWidth, displayHeight;
    if (imageAspect > widgetAspect) {
        displayWidth = viewportWidth_ * zoom_;
        displayHeight = displayWidth / imageAspect;
    } else {
        displayHeight = viewportHeight_ * zoom_;
        displayWidth = displayHeight * imageAspect;
    }
    
    float x = (viewportWidth_ - displayWidth) / 2.0f + pan_.x();
    float y = (viewportHeight_ - displayHeight) / 2.0f + pan_.y();
    
    QRectF targetRect(x, y, displayWidth, displayHeight);
    painter.drawImage(targetRect, displayFrame);
    
    // Draw hair cross if enabled
    if (showHairCross_) {
        drawHairCross();
    }
}

void VideoWidget::updateTexture() {
    if (currentFrame_.isNull()) {
        return;
    }
    
    makeCurrent();
    
    // Convert to RGB32 format if needed
    QImage image = currentFrame_.convertToFormat(QImage::Format_RGB32);
    
    texture_ = std::make_unique<QOpenGLTexture>(image.mirrored());
    texture_->setMinificationFilter(QOpenGLTexture::Linear);
    texture_->setMagnificationFilter(QOpenGLTexture::Linear);
    
    doneCurrent();
}

void VideoWidget::mousePressEvent(QMouseEvent *event) {
    // Quick hair cross placement: Right-click (or Shift+Left) sets sample position
    if (showHairCross_ && (event->button() == Qt::RightButton ||
                           (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ShiftModifier)))) {
        QPoint screenPos = event->pos();
        QPointF imagePos = screenToImage(QPointF(screenPos));
        if (!currentFrame_.isNull() &&
            imagePos.x() >= 0 && imagePos.x() < currentFrame_.width() &&
            imagePos.y() >= 0 && imagePos.y() < currentFrame_.height()) {
            hairCrossPos_ = imagePos.toPoint();
            emit hairCrossPositionChanged(hairCrossPos_, screenPos);
            emit scanlineChanged(hairCrossPos_.y());
            update();
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        isPanning_ = true;
        lastMousePos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void VideoWidget::mouseMoveEvent(QMouseEvent *event) {
    if (isPanning_) {
        QPoint delta = event->pos() - lastMousePos_;
        pan_ += QPointF(delta.x(), delta.y());
        lastMousePos_ = event->pos();
        update();
    } else if (showHairCross_) {
        // Update hair cross position
        QPoint screenPos = event->pos();
        QPointF imagePos = screenToImage(QPointF(screenPos));
        
        if (imagePos.x() >= 0 && imagePos.x() < currentFrame_.width() &&
            imagePos.y() >= 0 && imagePos.y() < currentFrame_.height()) {
            hairCrossPos_ = imagePos.toPoint();
            emit hairCrossPositionChanged(hairCrossPos_, screenPos);
            emit scanlineChanged(hairCrossPos_.y());
            update();
        }
    }
}

void VideoWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        isPanning_ = false;
        setCursor(Qt::ArrowCursor);
    }
}

void VideoWidget::wheelEvent(QWheelEvent *event) {
    // Zoom in/out with mouse wheel
    float zoomFactor = 1.0f + (event->angleDelta().y() / 1200.0f);
    
    // Get mouse position relative to widget center
    QPointF mousePos = event->position();
    QPointF center(viewportWidth_ / 2.0f, viewportHeight_ / 2.0f);
    QPointF offset = mousePos - center;
    
    // Adjust pan to zoom towards mouse position
    pan_ = (pan_ + offset) * zoomFactor - offset;
    
    setZoom(zoom_ * zoomFactor);
    
    event->accept();
}

QPointF VideoWidget::screenToImage(const QPointF &screenPos) const {
    if (currentFrame_.isNull()) {
        return QPointF();
    }
    
    float imageAspect = static_cast<float>(currentFrame_.width()) / currentFrame_.height();
    float widgetAspect = static_cast<float>(viewportWidth_) / viewportHeight_;
    
    float displayWidth, displayHeight;
    if (imageAspect > widgetAspect) {
        displayWidth = viewportWidth_ * zoom_;
        displayHeight = displayWidth / imageAspect;
    } else {
        displayHeight = viewportHeight_ * zoom_;
        displayWidth = displayHeight * imageAspect;
    }
    
    float x = (viewportWidth_ - displayWidth) / 2.0f + pan_.x();
    float y = (viewportHeight_ - displayHeight) / 2.0f + pan_.y();
    
    float relX = (screenPos.x() - x) / displayWidth;
    float relY = (screenPos.y() - y) / displayHeight;
    
    return QPointF(relX * currentFrame_.width(), relY * currentFrame_.height());
}

void VideoWidget::drawHairCross() {
    if (currentFrame_.isNull()) {
        return;
    }
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Calculate screen position from image position
    float imageAspect = static_cast<float>(currentFrame_.width()) / currentFrame_.height();
    float widgetAspect = static_cast<float>(viewportWidth_) / viewportHeight_;
    
    float displayWidth, displayHeight;
    if (imageAspect > widgetAspect) {
        displayWidth = viewportWidth_ * zoom_;
        displayHeight = displayWidth / imageAspect;
    } else {
        displayHeight = viewportHeight_ * zoom_;
        displayWidth = displayHeight * imageAspect;
    }
    
    float imageX = (viewportWidth_ - displayWidth) / 2.0f + pan_.x();
    float imageY = (viewportHeight_ - displayHeight) / 2.0f + pan_.y();
    
    float screenX = imageX + (hairCrossPos_.x() * displayWidth / currentFrame_.width());
    float screenY = imageY + (hairCrossPos_.y() * displayHeight / currentFrame_.height());
    
    // Draw crosshair
    painter.setPen(QPen(QColor(0, 255, 0), 1));
    
    // Horizontal line
    painter.drawLine(0, static_cast<int>(screenY), viewportWidth_, static_cast<int>(screenY));
    
    // Vertical line
    painter.drawLine(static_cast<int>(screenX), 0, static_cast<int>(screenX), viewportHeight_);
    
    // Draw center circle
    painter.setPen(QPen(QColor(0, 255, 0), 2));
    painter.drawEllipse(QPointF(screenX, screenY), 5, 5);
}

void VideoWidget::applyColorTransform(QImage &image) {
    // Apply exposure and gamma adjustments
    image = image.convertToFormat(QImage::Format_RGB32);
    
    float exposureMult = std::pow(2.0f, exposure_);
    float invGamma = 1.0f / gamma_;
    
    for (int y = 0; y < image.height(); ++y) {
        uint32_t *line = reinterpret_cast<uint32_t*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            uint32_t pixel = line[x];
            float r = ((pixel >> 16) & 0xFF) / 255.0f;
            float g = ((pixel >> 8) & 0xFF) / 255.0f;
            float b = (pixel & 0xFF) / 255.0f;
            
            // Apply exposure
            r *= exposureMult;
            g *= exposureMult;
            b *= exposureMult;
            
            // Apply gamma
            r = std::pow(std::max(0.0f, std::min(1.0f, r)), invGamma);
            g = std::pow(std::max(0.0f, std::min(1.0f, g)), invGamma);
            b = std::pow(std::max(0.0f, std::min(1.0f, b)), invGamma);
            
            // Apply color profile transform (simplified for now)
            // In a real implementation, this would use proper color matrices
            // For now, just clamp values
            
            // Convert back to uint8
            uint8_t rByte = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, r * 255.0f)));
            uint8_t gByte = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, g * 255.0f)));
            uint8_t bByte = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, b * 255.0f)));
            
            line[x] = 0xFF000000 | (rByte << 16) | (gByte << 8) | bByte;
        }
    }
}
