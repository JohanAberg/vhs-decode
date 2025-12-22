#include "spectrogramwidget.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDebug>
#include <cmath>
#include <chrono>
#include <algorithm>

// Constructor
SpectrogramWidget::SpectrogramWidget(QWidget *parent)
    : QWidget(parent)
    , realtimeMode_(false)
    , droppedFrames_(0)
    , lastComputeTime_(0)
{
    setupUI();
    
    // Setup update timer for spectrogram computation
    updateTimer_ = new QTimer(this);
    connect(updateTimer_, &QTimer::timeout, this, &SpectrogramWidget::computeSpectrogram);
    updateTimer_->start(50); // Update every 50ms (20 Hz)
    
    // Setup mock data timer (for testing without real decoder)
    mockDataTimer_ = new QTimer(this);
    connect(mockDataTimer_, &QTimer::timeout, this, &SpectrogramWidget::generateMockData);
    mockDataTimer_->start(100); // Generate mock data every 100ms
}

SpectrogramWidget::~SpectrogramWidget() {
}

void SpectrogramWidget::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Create renderer
    renderer_ = new SpectrogramRenderer(this);
    renderer_->setMinimumHeight(300);
    mainLayout->addWidget(renderer_, 1); // Stretch factor 1
    
    // Create controls group
    QGroupBox *controlsGroup = new QGroupBox("Spectrogram Controls", this);
    QVBoxLayout *controlsLayout = new QVBoxLayout(controlsGroup);
    
    // Color map selector
    QHBoxLayout *colorMapLayout = new QHBoxLayout();
    colorMapLayout->addWidget(new QLabel("Color Map:", this));
    colorMapCombo_ = new QComboBox(this);
    colorMapCombo_->addItem("Viridis");
    colorMapCombo_->addItem("Jet");
    colorMapCombo_->addItem("Hot");
    colorMapCombo_->addItem("Grayscale");
    colorMapCombo_->setCurrentIndex(0);
    connect(colorMapCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpectrogramWidget::onColorMapChanged);
    colorMapLayout->addWidget(colorMapCombo_);
    colorMapLayout->addStretch();
    controlsLayout->addLayout(colorMapLayout);
    
    // Frequency range sliders
    QHBoxLayout *minFreqLayout = new QHBoxLayout();
    minFreqLayout->addWidget(new QLabel("Min Freq (MHz):", this));
    minFreqSlider_ = new QSlider(Qt::Horizontal, this);
    minFreqSlider_->setRange(0, 100); // 0-10 MHz in 0.1 MHz steps
    minFreqSlider_->setValue(13); // 1.3 MHz default
    connect(minFreqSlider_, &QSlider::valueChanged,
            this, &SpectrogramWidget::onMinFreqChanged);
    minFreqLayout->addWidget(minFreqSlider_);
    minFreqLabel_ = new QLabel("1.3", this);
    minFreqLabel_->setMinimumWidth(40);
    minFreqLayout->addWidget(minFreqLabel_);
    controlsLayout->addLayout(minFreqLayout);
    
    QHBoxLayout *maxFreqLayout = new QHBoxLayout();
    maxFreqLayout->addWidget(new QLabel("Max Freq (MHz):", this));
    maxFreqSlider_ = new QSlider(Qt::Horizontal, this);
    maxFreqSlider_->setRange(0, 200); // 0-20 MHz in 0.1 MHz steps
    maxFreqSlider_->setValue(58); // 5.8 MHz default
    connect(maxFreqSlider_, &QSlider::valueChanged,
            this, &SpectrogramWidget::onMaxFreqChanged);
    maxFreqLayout->addWidget(maxFreqSlider_);
    maxFreqLabel_ = new QLabel("5.8", this);
    maxFreqLabel_->setMinimumWidth(40);
    maxFreqLayout->addWidget(maxFreqLabel_);
    controlsLayout->addLayout(maxFreqLayout);
    
    // Contrast slider
    QHBoxLayout *contrastLayout = new QHBoxLayout();
    contrastLayout->addWidget(new QLabel("Contrast:", this));
    contrastSlider_ = new QSlider(Qt::Horizontal, this);
    contrastSlider_->setRange(50, 200);
    contrastSlider_->setValue(100);
    connect(contrastSlider_, &QSlider::valueChanged,
            this, &SpectrogramWidget::onContrastChanged);
    contrastLayout->addWidget(contrastSlider_);
    controlsLayout->addLayout(contrastLayout);
    
    // Brightness slider
    QHBoxLayout *brightnessLayout = new QHBoxLayout();
    brightnessLayout->addWidget(new QLabel("Brightness:", this));
    brightnessSlider_ = new QSlider(Qt::Horizontal, this);
    brightnessSlider_->setRange(-100, 100);
    brightnessSlider_->setValue(0);
    connect(brightnessSlider_, &QSlider::valueChanged,
            this, &SpectrogramWidget::onBrightnessChanged);
    brightnessLayout->addWidget(brightnessSlider_);
    controlsLayout->addLayout(brightnessLayout);
    
    // Status label
    statusLabel_ = new QLabel("Waiting for data...", this);
    statusLabel_->setStyleSheet("QLabel { color: gray; font-size: 10px; }");
    controlsLayout->addWidget(statusLabel_);
    
    mainLayout->addWidget(controlsGroup);
    
    setLayout(mainLayout);
}

void SpectrogramWidget::pushFMData(const float *data, size_t length) {
    // Add new data to buffer
    for (size_t i = 0; i < length; i++) {
        dataBuffer_.push_back(data[i]);
    }
    
    // Limit buffer size
    while (dataBuffer_.size() > MAX_BUFFER_SIZE) {
        dataBuffer_.pop_front();
    }
}

void SpectrogramWidget::clearData() {
    dataBuffer_.clear();
    spectrogramData_.clear();
    renderer_->setSpectrogramData(spectrogramData_);
}

void SpectrogramWidget::setRealtimeMode(bool enabled) {
    realtimeMode_ = enabled;
    if (enabled) {
        droppedFrames_ = 0;
    }
}

void SpectrogramWidget::generateMockData() {
    // Generate mock VHS FM signal
    // VHS FM: carrier around 4.1 MHz with ±1 MHz deviation for video
    // Add some noise and harmonics for realism
    
    const size_t chunkSize = SAMPLE_RATE / 10; // 0.1 second of data
    static float phase = 0.0f;
    static float videoPhase = 0.0f;
    
    for (size_t i = 0; i < chunkSize; i++) {
        // Base FM carrier at 4.1 MHz
        float baseFreq = 4.1e6f;
        
        // Video modulation (25 Hz for PAL frame rate)
        float videoSignal = 0.5f * std::sin(videoPhase);
        float modulatedFreq = baseFreq + videoSignal * 1.0e6f; // ±1 MHz deviation
        
        // Generate FM signal
        float sample = std::sin(phase);
        
        // Add harmonics
        sample += 0.3f * std::sin(2.0f * phase);
        sample += 0.1f * std::sin(3.0f * phase);
        
        // Add noise
        float noise = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.05f;
        sample += noise;
        
        dataBuffer_.push_back(sample);
        
        // Update phases
        phase += 2.0f * M_PI * modulatedFreq / SAMPLE_RATE;
        videoPhase += 2.0f * M_PI * 25.0f / SAMPLE_RATE;
        
        // Wrap phase
        while (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
        while (videoPhase > 2.0f * M_PI) videoPhase -= 2.0f * M_PI;
    }
    
    // Limit buffer size
    while (dataBuffer_.size() > MAX_BUFFER_SIZE) {
        dataBuffer_.pop_front();
    }
}

void SpectrogramWidget::computeSpectrogram() {
    if (dataBuffer_.size() < FFT_SIZE) {
        statusLabel_->setText(QString("Buffering... %1/%2 samples")
                             .arg(dataBuffer_.size())
                             .arg(FFT_SIZE));
        return;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Calculate number of FFT windows for 1 second
    size_t samplesPerWindow = TIME_WINDOW_SEC * SAMPLE_RATE;
    size_t numWindows = (dataBuffer_.size() - FFT_SIZE) / HOP_SIZE;
    
    // Limit to 1 second of data
    size_t maxWindows = samplesPerWindow / HOP_SIZE;
    if (numWindows > maxWindows) {
        numWindows = maxWindows;
    }
    
    spectrogramData_.clear();
    spectrogramData_.reserve(numWindows);
    
    // Compute STFT
    for (size_t w = 0; w < numWindows; w++) {
        size_t startIdx = dataBuffer_.size() - (numWindows - w) * HOP_SIZE - FFT_SIZE;
        
        // Extract window
        std::vector<float> window;
        window.reserve(FFT_SIZE);
        for (size_t i = 0; i < FFT_SIZE; i++) {
            window.push_back(dataBuffer_[startIdx + i]);
        }
        
        // Apply Hann window
        window = applyWindow(window, FFT_SIZE);
        
        // Compute FFT
        std::vector<std::complex<float>> fftResult = fft(window);
        
        // Compute power spectrum (only positive frequencies)
        std::vector<float> powerSpectrum;
        powerSpectrum.reserve(FFT_SIZE / 2);
        for (size_t i = 0; i < FFT_SIZE / 2; i++) {
            float real = fftResult[i].real();
            float imag = fftResult[i].imag();
            float power = std::sqrt(real * real + imag * imag);
            
            // Convert to dB
            power = 20.0f * std::log10(power + 1e-10f);
            powerSpectrum.push_back(power);
        }
        
        spectrogramData_.push_back(powerSpectrum);
    }
    
    // Update renderer
    renderer_->setSpectrogramData(spectrogramData_);
    renderer_->update();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    lastComputeTime_ = duration.count();
    
    // Update status
    statusLabel_->setText(QString("Compute time: %1 ms | Windows: %2 | Buffer: %3 samples")
                         .arg(lastComputeTime_)
                         .arg(numWindows)
                         .arg(dataBuffer_.size()));
    
    // Check if we're keeping up with realtime
    if (realtimeMode_ && lastComputeTime_ > 50) {
        droppedFrames_++;
        qDebug() << "Warning: Spectrogram compute took" << lastComputeTime_ 
                 << "ms (dropped frames:" << droppedFrames_ << ")";
    }
}

std::vector<float> SpectrogramWidget::applyWindow(const std::vector<float> &data, size_t windowSize) {
    std::vector<float> windowed;
    windowed.reserve(windowSize);
    
    // Hann window
    for (size_t i = 0; i < windowSize; i++) {
        float window = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (windowSize - 1)));
        windowed.push_back(data[i] * window);
    }
    
    return windowed;
}

std::vector<std::complex<float>> SpectrogramWidget::fft(const std::vector<float> &input) {
    // Simple DFT implementation (not optimized, but works for prototype)
    // TODO: Replace with FFTW or similar for production
    
    size_t N = input.size();
    std::vector<std::complex<float>> output(N);
    
    for (size_t k = 0; k < N; k++) {
        std::complex<float> sum(0.0f, 0.0f);
        for (size_t n = 0; n < N; n++) {
            float angle = -2.0f * M_PI * k * n / N;
            sum += input[n] * std::complex<float>(std::cos(angle), std::sin(angle));
        }
        output[k] = sum;
    }
    
    return output;
}

void SpectrogramWidget::onColorMapChanged(int index) {
    renderer_->setColorMap(index);
    renderer_->update();
}

void SpectrogramWidget::onMinFreqChanged(int value) {
    float freqMHz = value / 10.0f;
    minFreqLabel_->setText(QString::number(freqMHz, 'f', 1));
    renderer_->setFrequencyRange(freqMHz * 1e6f, maxFreqSlider_->value() / 10.0f * 1e6f);
    renderer_->update();
}

void SpectrogramWidget::onMaxFreqChanged(int value) {
    float freqMHz = value / 10.0f;
    maxFreqLabel_->setText(QString::number(freqMHz, 'f', 1));
    renderer_->setFrequencyRange(minFreqSlider_->value() / 10.0f * 1e6f, freqMHz * 1e6f);
    renderer_->update();
}

void SpectrogramWidget::onContrastChanged(int value) {
    renderer_->setContrast(value / 100.0f);
    renderer_->update();
}

void SpectrogramWidget::onBrightnessChanged(int value) {
    renderer_->setBrightness(value / 100.0f);
    renderer_->update();
}

// SpectrogramRenderer implementation
SpectrogramWidget::SpectrogramRenderer::SpectrogramRenderer(QWidget *parent)
    : QOpenGLWidget(parent)
    , colorMapIndex_(0)
    , minFreqHz_(1.3e6f)
    , maxFreqHz_(5.8e6f)
    , contrast_(1.0f)
    , brightness_(0.0f)
    , viewportWidth_(0)
    , viewportHeight_(0)
{
}

SpectrogramWidget::SpectrogramRenderer::~SpectrogramRenderer() {
}

void SpectrogramWidget::SpectrogramRenderer::setSpectrogramData(
    const std::vector<std::vector<float>> &data) {
    spectrogramData_ = data;
}

void SpectrogramWidget::SpectrogramRenderer::setColorMap(int colorMapIndex) {
    colorMapIndex_ = colorMapIndex;
}

void SpectrogramWidget::SpectrogramRenderer::setFrequencyRange(float minFreq, float maxFreq) {
    minFreqHz_ = minFreq;
    maxFreqHz_ = maxFreq;
}

void SpectrogramWidget::SpectrogramRenderer::setContrast(float contrast) {
    contrast_ = contrast;
}

void SpectrogramWidget::SpectrogramRenderer::setBrightness(float brightness) {
    brightness_ = brightness;
}

void SpectrogramWidget::SpectrogramRenderer::initializeGL() {
    // Nothing special needed for 2D rendering
}

void SpectrogramWidget::SpectrogramRenderer::resizeGL(int w, int h) {
    viewportWidth_ = w;
    viewportHeight_ = h;
}

void SpectrogramWidget::SpectrogramRenderer::paintGL() {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false); // Faster for spectrogram
    
    // Clear background
    painter.fillRect(0, 0, viewportWidth_, viewportHeight_, Qt::black);
    
    if (spectrogramData_.empty()) {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "Waiting for data...");
        return;
    }
    
    // Draw spectrogram
    size_t numTimeSteps = spectrogramData_.size();
    size_t numFreqBins = spectrogramData_[0].size();
    
    float pixelWidth = static_cast<float>(viewportWidth_) / numTimeSteps;
    float pixelHeight = static_cast<float>(viewportHeight_) / numFreqBins;
    
    // Find min/max for normalization
    float minVal = 1e10f;
    float maxVal = -1e10f;
    for (const auto &timeSlice : spectrogramData_) {
        for (float val : timeSlice) {
            minVal = std::min(minVal, val);
            maxVal = std::max(maxVal, val);
        }
    }
    
    // Draw pixels
    for (size_t t = 0; t < numTimeSteps; t++) {
        for (size_t f = 0; f < numFreqBins; f++) {
            float value = spectrogramData_[t][f];
            
            // Normalize to 0-1
            value = (value - minVal) / (maxVal - minVal + 1e-10f);
            
            // Apply contrast and brightness
            value = (value - 0.5f) * contrast_ + 0.5f + brightness_;
            value = std::max(0.0f, std::min(1.0f, value));
            
            // Apply color map
            QRgb color = applyColorMap(value, colorMapIndex_);
            
            // Draw pixel
            float x = t * pixelWidth;
            float y = viewportHeight_ - (f + 1) * pixelHeight; // Flip Y axis
            painter.fillRect(QRectF(x, y, pixelWidth + 1, pixelHeight + 1), QColor(color));
        }
    }
    
    // Draw frequency axis labels
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);
    
    // Draw min/max frequency labels
    float minFreqMHz = minFreqHz_ / 1e6f;
    float maxFreqMHz = maxFreqHz_ / 1e6f;
    painter.drawText(5, viewportHeight_ - 5, QString("%1 MHz").arg(minFreqMHz, 0, 'f', 1));
    painter.drawText(5, 15, QString("%1 MHz").arg(maxFreqMHz, 0, 'f', 1));
    
    // Draw time label
    painter.drawText(viewportWidth_ - 80, viewportHeight_ - 5, "1 second");
}

QRgb SpectrogramWidget::SpectrogramRenderer::applyColorMap(float value, int colorMapIndex) {
    int r, g, b;
    
    switch (colorMapIndex) {
    case 0: // Viridis
        if (value < 0.25f) {
            float t = value / 0.25f;
            r = static_cast<int>(68 * (1 - t) + 59 * t);
            g = static_cast<int>(1 * (1 - t) + 82 * t);
            b = static_cast<int>(84 * (1 - t) + 139 * t);
        } else if (value < 0.5f) {
            float t = (value - 0.25f) / 0.25f;
            r = static_cast<int>(59 * (1 - t) + 33 * t);
            g = static_cast<int>(82 * (1 - t) + 145 * t);
            b = static_cast<int>(139 * (1 - t) + 140 * t);
        } else if (value < 0.75f) {
            float t = (value - 0.5f) / 0.25f;
            r = static_cast<int>(33 * (1 - t) + 94 * t);
            g = static_cast<int>(145 * (1 - t) + 201 * t);
            b = static_cast<int>(140 * (1 - t) + 97 * t);
        } else {
            float t = (value - 0.75f) / 0.25f;
            r = static_cast<int>(94 * (1 - t) + 253 * t);
            g = static_cast<int>(201 * (1 - t) + 231 * t);
            b = static_cast<int>(97 * (1 - t) + 37 * t);
        }
        break;
        
    case 1: // Jet
        if (value < 0.25f) {
            r = 0;
            g = 0;
            b = static_cast<int>(128 + 127 * (value / 0.25f));
        } else if (value < 0.5f) {
            r = 0;
            g = static_cast<int>(255 * ((value - 0.25f) / 0.25f));
            b = 255;
        } else if (value < 0.75f) {
            r = static_cast<int>(255 * ((value - 0.5f) / 0.25f));
            g = 255;
            b = static_cast<int>(255 * (1 - (value - 0.5f) / 0.25f));
        } else {
            r = 255;
            g = static_cast<int>(255 * (1 - (value - 0.75f) / 0.25f));
            b = 0;
        }
        break;
        
    case 2: // Hot
        if (value < 0.33f) {
            r = static_cast<int>(255 * (value / 0.33f));
            g = 0;
            b = 0;
        } else if (value < 0.67f) {
            r = 255;
            g = static_cast<int>(255 * ((value - 0.33f) / 0.34f));
            b = 0;
        } else {
            r = 255;
            g = 255;
            b = static_cast<int>(255 * ((value - 0.67f) / 0.33f));
        }
        break;
        
    case 3: // Grayscale
    default:
        r = g = b = static_cast<int>(255 * value);
        break;
    }
    
    return qRgb(r, g, b);
}
