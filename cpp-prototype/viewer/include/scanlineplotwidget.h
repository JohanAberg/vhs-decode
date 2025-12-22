#pragma once

#include <QOpenGLWidget>
#include <QImage>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFunctions>
#include <vector>
#include <memory>

class ScanlinePlotWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit ScanlinePlotWidget(QWidget *parent = nullptr);
    ~ScanlinePlotWidget() override;

    void updateScanline(const QImage &frame, int scanlineY);
    void clear();
    
    void setOverlayMode(bool overlay);
    bool isOverlayMode() const { return overlayMode_; }
    
    void setShowRGB(bool show) { showRGB_ = show; update(); }
    void setShowLuma(bool show) { showLuma_ = show; update(); }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void extractScanline(const QImage &frame, int scanlineY);
    void uploadScanlineData();
    void renderScanline();
    void renderOverlay();
    
    // Scanline data
    std::vector<float> scanlineR_;
    std::vector<float> scanlineG_;
    std::vector<float> scanlineB_;
    std::vector<float> scanlineLuma_;
    int scanlineY_;
    bool dataReady_;
    
    // Display options
    bool overlayMode_;
    bool showRGB_;
    bool showLuma_;
    
    // OpenGL resources
    std::unique_ptr<QOpenGLShaderProgram> shaderProgram_;
    std::unique_ptr<QOpenGLBuffer> vboR_;
    std::unique_ptr<QOpenGLBuffer> vboG_;
    std::unique_ptr<QOpenGLBuffer> vboB_;
    std::unique_ptr<QOpenGLBuffer> vboLuma_;
    std::unique_ptr<QOpenGLVertexArrayObject> vao_;
    
    // Overlay dragging
    bool isDragging_;
    QPoint dragStartPos_;
    QPoint widgetPos_;
};
