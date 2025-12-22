#pragma once

#include <QOpenGLWidget>
#include <QImage>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFunctions>
#include <vector>
#include <memory>

class VectorscopeWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit VectorscopeWidget(QWidget *parent = nullptr);
    ~VectorscopeWidget() override;

    void updateVectorscope(const QImage &frame);
    void clear();
    
    void setOverlayMode(bool overlay);
    bool isOverlayMode() const { return overlayMode_; }
    
    void setIntensity(float intensity) { intensity_ = intensity; update(); }
    void setShowTargets(bool show) { showTargets_ = show; update(); }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void computeVectorscope(const QImage &frame);
    void uploadVectorscopeData();
    void renderVectorscope();
    void renderOverlay();
    void renderTargets();
    
    // Vectorscope data (U, V pairs)
    std::vector<float> vectorData_; // Pairs of U, V values (-1 to +1)
    bool dataReady_;
    
    // Display options
    bool overlayMode_;
    float intensity_;
    bool showTargets_;
    
    // OpenGL resources
    std::unique_ptr<QOpenGLShaderProgram> pointShader_;
    std::unique_ptr<QOpenGLShaderProgram> targetShader_;
    std::unique_ptr<QOpenGLBuffer> vbo_;
    std::unique_ptr<QOpenGLVertexArrayObject> vao_;
    
    // Target markers (75% color bars)
    struct Target {
        float u, v;
        float r, g, b;
    };
    std::vector<Target> targets_;
    
    // Overlay dragging
    bool isDragging_;
    QPoint dragStartPos_;
    QPoint widgetPos_;
};
