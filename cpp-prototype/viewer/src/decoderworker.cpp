#include "decoderworker.h"
#include <QMutexLocker>
#include <QImage>
#include <QDebug>
#include <QPainter>
#include <QFont>
#include <stdexcept>
#include <cmath>

// MOCK IMPLEMENTATION - Decoder integration TODO
// This allows the UI to launch without full decoder integration

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
    // MOCK: No real decoder initialization
    qDebug() << "DecoderThread started (MOCK MODE - decoder integration pending)";
    
    if (config_.filename.empty()) {
        emit decodingError(-1, "No filename specified");
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
    // MOCK: Generate test pattern instead of real decoding
    const int width = 720;
    const int height = 576; // PAL resolution
    
    QImage image(width, height, QImage::Format_Grayscale8);
    image.fill(128); // Gray background
    
    QPainter painter(&image);
    
    // Draw color bars at top
    for (int i = 0; i < 8; i++) {
        int gray = (i * 255) / 7;
        painter.fillRect(i * width / 8, 0, width / 8, height / 3, QColor(gray, gray, gray));
    }
    
    // Draw frame info text
    painter.setPen(Qt::white);
    QFont font("Arial", 32, QFont::Bold);
    painter.setFont(font);
    
    QString text = QString("MOCK DECODER\n\nFrame %1\n\nDecoder Integration Pending\n\nUI Preview Mode").arg(frameNumber);
    painter.drawText(QRect(0, height / 3, width, height * 2 / 3), Qt::AlignCenter, text);
    
    painter.end();
    
    // Simulate processing delay
    QThread::msleep(50);
    
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
