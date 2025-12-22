#pragma once

#include <QMainWindow>
#include <QString>
#include <memory>

namespace Ui {
class MainWindow;
}

class DecoderWorker;
class FrameCache;
class HistogramWidget;
class VectorscopeWidget;
class ColorSamplerWidget;
class ScanlinePlotWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onOpenFile();
    void onExit();
    void onPlay();
    void onPause();
    void onPrevFrame();
    void onNextFrame();
    void onZoomIn();
    void onZoomOut();
    void onZoomReset();
    void onDemodParameterChanged();
    void onFrameDecoded(int frameNum, const QImage &image);
    void onDecodingProgress(int current, int total);
    
    // New slots for analysis widgets
    void onToggleHistogram();
    void onToggleVectorscope();
    void onToggleColorSampler();
    void onToggleScanlinePlot();
    void onHistogramOverlay();
    void onVectorscopeOverlay();
    void onScanlinePlotOverlay();
    
    // Color management slots
    void onGammaChanged(int value);
    void onExposureChanged(int value);
    void onColorProfileChanged(int index);
    
    // Hair cross / sampling
    void onHairCrossPositionChanged(const QPoint &imagePos, const QPoint &screenPos);
    void onScanlineChanged(int scanlineY);

private:
    void loadRFFile(const QString &filename);
    void updateFrameLabel();
    void decodeCurrentFrame();
    void startPlayback();
    void stopPlayback();
    void setupAnalysisWidgets();
    void updateAnalysisWidgets(const QImage &frame);
    
    Ui::MainWindow *ui;
    
    QString currentFile_;
    size_t fileSize_;
    int totalFrames_;
    int currentFrame_;
    bool isPlaying_;
    
    std::unique_ptr<DecoderWorker> decoderWorker_;
    std::unique_ptr<FrameCache> frameCache_;
    
    // Analysis widgets
    std::unique_ptr<HistogramWidget> histogramWidget_;
    std::unique_ptr<VectorscopeWidget> vectorscopeWidget_;
    std::unique_ptr<ColorSamplerWidget> colorSamplerWidget_;
    std::unique_ptr<ScanlinePlotWidget> scanlinePlotWidget_;
    
    // Overlay widgets (separate instances for overlay mode)
    HistogramWidget *histogramOverlay_;
    VectorscopeWidget *vectorscopeOverlay_;
    ScanlinePlotWidget *scanlinePlotOverlay_;
    
    QImage currentDisplayFrame_;
};
