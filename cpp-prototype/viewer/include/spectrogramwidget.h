#pragma once

#include <QOpenGLWidget>
#include <QTimer>
#include <QSlider>
#include <QComboBox>
#include <QVBoxLayout>
#include <QLabel>
#include <vector>
#include <memory>
#include <deque>
#include <complex>

/**
 * @brief Real-time spectrogram widget for VHS FM signal visualization
 * 
 * Displays a 1-second moving window of frequency content over time.
 * Uses STFT (Short-Time Fourier Transform) to compute frequency bins.
 * Non-blocking design: drops frames if computation can't keep up with realtime.
 */
class SpectrogramWidget : public QWidget {
    Q_OBJECT

public:
    explicit SpectrogramWidget(QWidget *parent = nullptr);
    ~SpectrogramWidget() override;

    // Data input interface
    void pushFMData(const float *data, size_t length);
    void clearData();
    
    // Enable/disable realtime mode
    void setRealtimeMode(bool enabled);
    bool isRealtimeMode() const { return realtimeMode_; }

public slots:
    void onColorMapChanged(int index);
    void onMinFreqChanged(int value);
    void onMaxFreqChanged(int value);
    void onContrastChanged(int value);
    void onBrightnessChanged(int value);

signals:
    void parametersChanged();

private:
    // Internal rendering widget
    class SpectrogramRenderer : public QOpenGLWidget {
    public:
        explicit SpectrogramRenderer(QWidget *parent = nullptr);
        ~SpectrogramRenderer() override;

        void setSpectrogramData(const std::vector<std::vector<float>> &data);
        void setColorMap(int colorMapIndex);
        void setFrequencyRange(float minFreq, float maxFreq);
        void setContrast(float contrast);
        void setBrightness(float brightness);

    protected:
        void initializeGL() override;
        void resizeGL(int w, int h) override;
        void paintGL() override;

    private:
        QRgb applyColorMap(float value, int colorMapIndex);
        
        std::vector<std::vector<float>> spectrogramData_;
        int colorMapIndex_;
        float minFreqHz_;
        float maxFreqHz_;
        float contrast_;
        float brightness_;
        int viewportWidth_;
        int viewportHeight_;
    };

    // Spectrogram computation
    void computeSpectrogram();
    void generateMockData();
    std::vector<float> applyWindow(const std::vector<float> &data, size_t windowSize);
    std::vector<std::complex<float>> fft(const std::vector<float> &input);
    
    // UI setup
    void setupUI();
    
    // Data management
    std::deque<float> dataBuffer_;        // Rolling buffer of FM samples
    std::vector<std::vector<float>> spectrogramData_;  // Computed spectrogram [time][freq]
    
    // Configuration
    static constexpr size_t SAMPLE_RATE = 40000000;  // 40 MSPS (VHS RF sample rate)
    static constexpr size_t FFT_SIZE = 2048;         // FFT window size
    static constexpr size_t HOP_SIZE = 512;          // Hop between FFTs
    static constexpr float TIME_WINDOW_SEC = 1.0f;   // Display 1 second of data
    static constexpr size_t MAX_BUFFER_SIZE = SAMPLE_RATE * 2;  // Keep 2 seconds max
    
    // UI components
    SpectrogramRenderer *renderer_;
    QSlider *minFreqSlider_;
    QSlider *maxFreqSlider_;
    QSlider *contrastSlider_;
    QSlider *brightnessSlider_;
    QComboBox *colorMapCombo_;
    QLabel *minFreqLabel_;
    QLabel *maxFreqLabel_;
    QLabel *statusLabel_;
    
    // Timers
    QTimer *updateTimer_;
    QTimer *mockDataTimer_;
    
    // State
    bool realtimeMode_;
    int droppedFrames_;
    size_t lastComputeTime_;
};
