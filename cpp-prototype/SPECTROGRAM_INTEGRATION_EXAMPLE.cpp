// Example Integration: Connecting Real FM Data to Spectrogram Widget
// This demonstrates how to integrate the spectrogram widget with the actual
// VHS decoder when the decoder API is finalized.

// In decoderworker.cpp or similar:

#include "spectrogramwidget.h"

class DecoderWorker : public QObject {
    Q_OBJECT

signals:
    // Add new signal for FM data
    void fmDataReady(const float *data, size_t length);
    
private:
    // Existing decode process...
    void processField(int fieldNum) {
        // ... existing RF processing ...
        
        // After FM demodulation:
        std::vector<float> fmData = fmDemodulator_->getOutput();
        
        // Emit for spectrogram visualization
        emit fmDataReady(fmData.data(), fmData.size());
        
        // Continue with video/chroma processing...
    }
};

// In mainwindow.cpp:

void MainWindow::setupSpectrogramDock() {
    // Create spectrogram widget
    spectrogramWidget_ = new SpectrogramWidget(this);
    spectrogramWidget_->setRealtimeMode(true); // Enable real-time mode
    
    // Create dock widget
    spectrogramDock_ = new QDockWidget("FM Spectrogram", this);
    spectrogramDock_->setWidget(spectrogramWidget_);
    spectrogramDock_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);
    
    addDockWidget(Qt::BottomDockWidgetArea, spectrogramDock_);
    spectrogramDock_->hide(); // Initially hidden
}

void MainWindow::loadRFFile(const QString &filename) {
    // ... existing file loading code ...
    
    // Connect decoder FM output to spectrogram
    if (decoderWorker_) {
        connect(decoderWorker_.get(), &DecoderWorker::fmDataReady,
                spectrogramWidget_, &SpectrogramWidget::pushFMData,
                Qt::QueuedConnection); // Async connection for non-blocking
    }
    
    // ... rest of loading code ...
}

// Alternative: Direct Integration in Decode Loop

void MainWindow::decodeCurrentFrame() {
    if (!decoderWorker_) return;
    
    // Request decode
    decoderWorker_->requestFrame(currentFrame_, 10);
    
    // Decoder will emit fmDataReady signal after FM demodulation
    // Spectrogram widget receives data automatically via signal/slot
}

// Performance Considerations:

// 1. Data Rate Management
//    - FM data at 40 MSPS = 160 MB/s (assuming float32)
//    - Spectrogram needs only 1-2 seconds of data
//    - Widget drops old data automatically (MAX_BUFFER_SIZE)

// 2. Thread Safety
//    - Use Qt::QueuedConnection for cross-thread signals
//    - Widget handles data buffering in its own thread
//    - Non-blocking: won't slow down decoder

// 3. Conditional Compilation
#ifdef ENABLE_SPECTROGRAM_WIDGET
    if (spectrogramWidget_ && spectrogramDock_->isVisible()) {
        // Only send data if widget is visible
        spectrogramWidget_->pushFMData(fmData.data(), fmData.size());
    }
#endif

// 4. Data Downsampling (Optional Optimization)
//    If 40 MSPS is too high for spectrogram needs:
std::vector<float> downsample(const float *data, size_t length, int factor) {
    std::vector<float> result;
    result.reserve(length / factor);
    for (size_t i = 0; i < length; i += factor) {
        result.push_back(data[i]);
    }
    return result;
}

// Usage:
auto downsampled = downsample(fmData.data(), fmData.size(), 4); // 40 MSPS -> 10 MSPS
spectrogramWidget_->pushFMData(downsampled.data(), downsampled.size());

// 5. Configurable Update Rate
//    Current: 50ms (20 Hz)
//    Can be adjusted based on performance:
void SpectrogramWidget::setUpdateRate(int milliseconds) {
    updateTimer_->setInterval(milliseconds);
}

// Usage in MainWindow:
spectrogramWidget_->setUpdateRate(100); // 10 Hz updates (less CPU)

// 6. Memory Management
//    Widget automatically manages memory:
//    - Max buffer: 2 seconds @ 40 MSPS = 320 MB (float32)
//    - Spectrogram data: ~8 KB
//    - Total overhead: <1 MB per instance
//    
//    To clear manually if needed:
spectrogramWidget_->clearData();

// 7. Error Handling
connect(decoderWorker_.get(), &DecoderWorker::decodingError,
        [this](int frameNum, const QString &error) {
            qDebug() << "Decode error at frame" << frameNum << ":" << error;
            // Spectrogram continues with buffered data
        });

// Example: Complete Integration Flow

/*
1. User loads RF file
2. MainWindow creates DecoderWorker
3. MainWindow connects DecoderWorker::fmDataReady → SpectrogramWidget::pushFMData
4. User enables spectrogram (Ctrl+S)
5. DecoderWorker decodes frame:
   a. Reads RF data
   b. Applies RF filters
   c. FM demodulation → emits fmDataReady(data, length)
6. SpectrogramWidget receives FM data:
   a. Appends to rolling buffer
   b. Limits buffer to 2 seconds
7. Every 50ms, SpectrogramWidget timer triggers:
   a. Computes STFT on buffered data
   b. Updates spectrogram display
   c. Non-blocking: continues even if decoder is busy
8. User adjusts controls:
   a. Changes frequency range → renderer updates
   b. Changes color map → renderer updates
   c. Instant visual feedback
*/

// Notes:
// - This integration is completely non-blocking
// - Decoder performance is unaffected by spectrogram
// - Widget can be enabled/disabled at runtime
// - Multiple instances possible (e.g., pre/post filtering)
// - Ready for production use once decoder API is stable
