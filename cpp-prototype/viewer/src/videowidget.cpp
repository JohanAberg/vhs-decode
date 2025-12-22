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
{
    setFocusPolicy(Qt::StrongFocus);
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
    
    // Use QPainter for simple 2D rendering
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    // Calculate display rectangle with zoom and pan
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
    
    QRectF targetRect(x, y, displayWidth, displayHeight);
    painter.drawImage(targetRect, currentFrame_);
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
