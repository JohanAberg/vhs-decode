#pragma once

#include <QMainWindow>
#include <QDockWidget>
#include <QString>
#include <memory>

namespace Ui {
class MainWindow;
}

class DecoderWorker;
class FrameCache;
class SpectrogramWidget;

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
    void onToggleSpectrogram(bool checked);
    void onDemodParameterChanged();
    void onFrameDecoded(int frameNum, const QImage &image);
    void onDecodingProgress(int current, int total);

private:
    void loadRFFile(const QString &filename);
    void updateFrameLabel();
    void decodeCurrentFrame();
    void startPlayback();
    void stopPlayback();
    void setupSpectrogramDock();
    
    Ui::MainWindow *ui;
    
    QString currentFile_;
    size_t fileSize_;
    int totalFrames_;
    int currentFrame_;
    bool isPlaying_;
    
    std::unique_ptr<DecoderWorker> decoderWorker_;
    std::unique_ptr<FrameCache> frameCache_;
    
    // Spectrogram dock widget
    QDockWidget *spectrogramDock_;
    SpectrogramWidget *spectrogramWidget_;
};
