#pragma once

#include <QOpenGLWidget>
#include <QImage>
#include <QPoint>
#include <QOpenGLTexture>
#include <memory>

class VideoWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget() override;

    void setFrame(const QImage &frame);
    void clearFrame();
    
    void setZoom(float zoom);
    float getZoom() const { return zoom_; }
    
    void setPan(const QPointF &pan);
    QPointF getPan() const { return pan_; }
    
    void resetView();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void updateTexture();
    QPointF screenToImage(const QPointF &screenPos) const;
    
    QImage currentFrame_;
    std::unique_ptr<QOpenGLTexture> texture_;
    
    float zoom_;
    QPointF pan_;
    
    bool isPanning_;
    QPoint lastMousePos_;
    
    int viewportWidth_;
    int viewportHeight_;
};
