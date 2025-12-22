#pragma once

#include <QOpenGLWidget>
#include <QImage>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFunctions>
#include <array>
#include <memory>

class HistogramWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit HistogramWidget(QWidget *parent = nullptr);
    ~HistogramWidget() override;

    void updateHistogram(const QImage &frame);
    void clear();
    
    void setOverlayMode(bool overlay);
    bool isOverlayMode() const { return overlayMode_; }
    
    void setShowRGB(bool show) { showRGB_ = show; update(); }
    void setShowLuma(bool show) { showLuma_ = show; update(); }
    void setLogScale(bool log) { logScale_ = log; update(); }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void computeHistogram(const QImage &frame);
    void uploadHistogramData();
    void renderHistogram();
    void renderOverlay();
    
    // Histogram data (256 bins per channel)
    std::array<uint32_t, 256> histogramR_;
    std::array<uint32_t, 256> histogramG_;
    std::array<uint32_t, 256> histogramB_;
    std::array<uint32_t, 256> histogramLuma_;
    
    uint32_t maxValue_;
    bool dataReady_;
    
    // Display options
    bool overlayMode_;
    bool showRGB_;
    bool showLuma_;
    bool logScale_;
    
    // OpenGL resources
    std::unique_ptr<QOpenGLShaderProgram> shaderProgram_;
    std::unique_ptr<QOpenGLBuffer> vbo_;
    std::unique_ptr<QOpenGLVertexArrayObject> vao_;
    
    // Overlay dragging
    bool isDragging_;
    QPoint dragStartPos_;
    QPoint widgetPos_;
};
