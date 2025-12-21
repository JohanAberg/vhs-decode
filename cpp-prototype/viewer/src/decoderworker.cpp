#include "decoderworker.h"
#include "vhsdecode/rf_reader.hpp"
#include "vhsdecode/rf_processor.hpp"
#include "vhsdecode/fm_demodulator.hpp"
#include "vhsdecode/tbc_scaler.hpp"
#include "vhsdecode/sync_detector.hpp"
#include "vhsdecode/filter_bank.hpp"
#include "vhsdecode/hilbert.hpp"
#include <QMutexLocker>
#include <QImage>
#include <stdexcept>

using namespace vhsdecode;

// DecoderThread implementation
DecoderThread::DecoderThread(const DecoderConfig &config, QObject *parent)
    : QThread(parent)
    , config_(config)
    , shouldStop_(false)
{
}

DecoderThread::~DecoderThread() {
    stop();
    wait();
}

void DecoderThread::addJob(const DecodeJob &job) {
    QMutexLocker locker(&mutex_);
    jobQueue_.push(job);
    condition_.wakeOne();
}

void DecoderThread::stop() {
    QMutexLocker locker(&mutex_);
    shouldStop_ = true;
    condition_.wakeAll();
}

void DecoderThread::run() {
    try {
        // Initialize decoder components
        rfReader_ = std::make_unique<RFReader>(config_.filename);
        
        // Configure RF processor
        rf::RFProcessor::Config rfConfig;
        rfConfig.sampleRateMHz = 40.0;
        rfConfig.blockSize = 131072; // 128K samples
        rfConfig.rfBandpassLowMHz = config_.rfBandpassLow;
        rfConfig.rfBandpassHighMHz = config_.rfBandpassHigh;
        rfConfig.useGPU = false;
        
        rfProcessor_ = std::make_unique<rf::RFProcessor>(rfConfig);
        
        // Configure FM demodulator
        FMDemodulator::Config fmConfig;
        fmConfig.sampleRateMHz = 40.0;
        fmConfig.centerFreqMHz = config_.ire0;
        fmConfig.deviationMHz = config_.hzPerIre * 100.0 / 1000000.0; // 100 IRE in MHz
        
        fmDemod_ = std::make_unique<FMDemodulator>(fmConfig);
        
        // Configure sync detector
        SyncDetector::Config syncConfig;
        syncConfig.sampleRateMHz = 40.0;
        syncConfig.linePeriodUs = 64.0; // PAL line period
        syncConfig.hsyncPeriodUs = 4.7;
        syncConfig.minLineLength = 2400;
        syncConfig.maxLineLength = 2700;
        
        syncDetector_ = std::make_unique<SyncDetector>(syncConfig);
        
        // Configure TBC scaler
        TBCScaler::Config tbcConfig;
        tbcConfig.outputLineLen = 1135; // 4 × fsc for PAL
        tbcConfig.outputSampleRate = 17734475.0;
        tbcConfig.inputSampleRate = 40000000.0;
        
        tbcScaler_ = std::make_unique<TBCScaler>(tbcConfig);
        
    } catch (const std::exception &e) {
        emit decodingError(-1, QString("Failed to initialize decoder: %1").arg(e.what()));
        return;
    }
    
    // Process jobs
    while (!shouldStop_) {
        DecodeJob job;
        
        {
            QMutexLocker locker(&mutex_);
            if (jobQueue_.empty()) {
                condition_.wait(&mutex_, 100);
                continue;
            }
            
            job = jobQueue_.top();
            jobQueue_.pop();
        }
        
        try {
            QImage image = decodeFrame(job.frameNumber, job.fileOffset);
            emit frameDecoded(job.frameNumber, image);
        } catch (const std::exception &e) {
            emit decodingError(job.frameNumber, QString("Decode error: %1").arg(e.what()));
        }
    }
}

QImage DecoderThread::decodeFrame(int frameNumber, size_t fileOffset) {
    // Calculate file offset for this frame
    // PAL: ~800K samples per field, 2 fields per frame
    const size_t samplesPerField = 800000;
    const size_t samplesPerFrame = samplesPerField * 2;
    size_t offset = frameNumber * samplesPerFrame;
    
    // Read RF block (one field worth)
    const size_t blockSize = 1048576; // 1MB block
    auto rfBlock = rfReader_->readBlock(offset, blockSize);
    
    // Convert uint8 to float
    RealArray rfFloat(rfBlock.data.size());
    for (size_t i = 0; i < rfBlock.data.size(); i++) {
        rfFloat[i] = static_cast<float>(rfBlock.data[i]) - 128.0f;
    }
    
    // Process RF: bandpass filter + Hilbert transform
    auto rfResult = rfProcessor_->processBlock(rfFloat);
    
    // FM demodulate
    auto demodResult = fmDemod_->demodulate(rfResult.analyticSignal);
    
    // Detect sync
    auto syncResult = syncDetector_->detectLines(demodResult.video);
    
    if (syncResult.lineStarts.empty()) {
        throw std::runtime_error("No sync pulses detected");
    }
    
    // Scale to TBC format (one field)
    const int linesPerField = 312; // PAL
    const int outputLineLen = 1135;
    
    VideoField field;
    field.reserve(std::min(static_cast<size_t>(linesPerField), syncResult.lineStarts.size()));
    
    for (size_t i = 0; i < syncResult.lineStarts.size() && i < static_cast<size_t>(linesPerField); i++) {
        size_t lineStart = syncResult.lineStarts[i];
        size_t lineEnd = (i + 1 < syncResult.lineStarts.size()) 
            ? syncResult.lineStarts[i + 1] 
            : demodResult.video.size();
        
        size_t lineLen = lineEnd - lineStart;
        if (lineLen < 2000 || lineLen > 3000) {
            continue; // Skip invalid lines
        }
        
        // Extract line data
        RealArray lineData(demodResult.video.begin() + lineStart,
                          demodResult.video.begin() + lineEnd);
        
        // Scale to output length
        auto scaledLine = tbcScaler_->scaleLine(lineData, outputLineLen);
        field.push_back(scaledLine);
    }
    
    // Convert to QImage (grayscale)
    const int height = field.size();
    QImage image(outputLineLen, height, QImage::Format_Grayscale8);
    
    for (int y = 0; y < height; y++) {
        uint8_t *scanLine = image.scanLine(y);
        const auto &line = field[y];
        
        for (int x = 0; x < outputLineLen && x < static_cast<int>(line.size()); x++) {
            // Convert uint16 TBC value to uint8 for display
            // TBC range is typically 256 (sync) to 54016 (white)
            uint16_t val = line[x];
            
            // Simple mapping: clamp and scale
            if (val < 256) val = 256;
            if (val > 54016) val = 54016;
            
            // Map to 0-255 range
            uint8_t gray = static_cast<uint8_t>((val - 256) * 255 / (54016 - 256));
            scanLine[x] = gray;
        }
    }
    
    return image;
}

// DecoderWorker implementation
DecoderWorker::DecoderWorker(const DecoderConfig &config, int numThreads, QObject *parent)
    : QObject(parent)
    , config_(config)
    , nextThread_(0)
{
    // Create worker threads
    for (int i = 0; i < numThreads; i++) {
        auto thread = std::make_unique<DecoderThread>(config, this);
        
        connect(thread.get(), &DecoderThread::frameDecoded,
                this, &DecoderWorker::onFrameDecoded);
        connect(thread.get(), &DecoderThread::decodingError,
                [](int frame, const QString &error) {
                    qWarning() << "Frame" << frame << "decode error:" << error;
                });
        
        thread->start();
        threads_.push_back(std::move(thread));
    }
}

DecoderWorker::~DecoderWorker() {
    clear();
}

void DecoderWorker::requestFrame(int frameNumber, int priority) {
    if (threads_.empty()) {
        return;
    }
    
    // Round-robin job distribution
    DecodeJob job;
    job.frameNumber = frameNumber;
    job.fileOffset = 0; // Will be calculated in thread
    job.priority = priority;
    
    threads_[nextThread_]->addJob(job);
    nextThread_ = (nextThread_ + 1) % threads_.size();
}

void DecoderWorker::updateConfig(const DecoderConfig &config) {
    clear();
    config_ = config;
    
    // Recreate threads with new config
    threads_.clear();
    nextThread_ = 0;
    
    int numThreads = 4; // Default
    for (int i = 0; i < numThreads; i++) {
        auto thread = std::make_unique<DecoderThread>(config_, this);
        
        connect(thread.get(), &DecoderThread::frameDecoded,
                this, &DecoderWorker::onFrameDecoded);
        
        thread->start();
        threads_.push_back(std::move(thread));
    }
}

void DecoderWorker::clear() {
    for (auto &thread : threads_) {
        if (thread) {
            thread->stop();
            thread->wait();
        }
    }
}

void DecoderWorker::onFrameDecoded(int frameNum, const QImage image) {
    emit frameDecoded(frameNum, image);
}
