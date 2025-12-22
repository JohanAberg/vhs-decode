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
    
    // Color management
    void setGamma(float gamma) { gamma_ = gamma; update(); }
    float getGamma() const { return gamma_; }
    
    void setExposure(float exposure) { exposure_ = exposure; update(); }
    float getExposure() const { return exposure_; }
    
    void setColorProfile(int profile) { colorProfile_ = profile; update(); }
    int getColorProfile() const { return colorProfile_; }
    
    // Color sampler
    void setShowHairCross(bool show) { showHairCross_ = show; update(); }
    bool getShowHairCross() const { return showHairCross_; }
    
    QPoint getHairCrossPos() const { return hairCrossPos_; }
    
signals:
    void hairCrossPositionChanged(const QPoint &imagePos, const QPoint &screenPos);
    void scanlineChanged(int scanlineY);

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
    void drawHairCross();
    void applyColorTransform(QImage &image);
    
    QImage currentFrame_;
    std::unique_ptr<QOpenGLTexture> texture_;
    
    float zoom_;
    QPointF pan_;
    
    bool isPanning_;
    QPoint lastMousePos_;
    
    int viewportWidth_;
    int viewportHeight_;
    
    // Color management
    float gamma_;
    float exposure_;
    int colorProfile_;  // 0=Rec709, 1=sRGB, 2=Rec2020, 3=DCI-P3, 4=Adobe RGB
    
    // Hair cross sampler
    bool showHairCross_;
    QPoint hairCrossPos_;
};
