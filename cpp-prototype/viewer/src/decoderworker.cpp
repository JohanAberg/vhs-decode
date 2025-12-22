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

QImage DecoderThread::decodeFrame(int frameNumber, size_t /*fileOffset*/) {
    // Generate a synthetic PAL EBU 100/75 color bars test chart
    const int width = 720;
    const int height = 576; // PAL

    QImage image(width, height, QImage::Format_RGB32);
    QPainter p(&image);
    p.fillRect(0, 0, width, height, Qt::black);

    // Top: 7 vertical bars (White, Yellow, Cyan, Green, Magenta, Red, Blue)
    const QColor bars[7] = {
        QColor(235,235,235), // White
        QColor(219,219,16),  // Yellow (approx 100/75)
        QColor(16,235,235),  // Cyan
        QColor(16,219,16),   // Green
        QColor(235,16,235),  // Magenta
        QColor(235,16,16),   // Red
        QColor(16,16,235)    // Blue
    };
    const int topH = height * 5 / 9; // ~60%
    for (int i = 0; i < 7; ++i) {
        int x = i * width / 7;
        int w = ((i+1) * width / 7) - x;
        p.fillRect(x, 0, w, topH, bars[i]);
    }

    // Middle: grey ramp (8 steps)
    const int midY = topH;
    const int midH = height * 2 / 9; // ~22%
    for (int i = 0; i < 8; ++i) {
        int x = i * width / 8;
        int w = ((i+1) * width / 8) - x;
        int g = i * 255 / 7;
        p.fillRect(x, midY, w, midH, QColor(g,g,g));
    }

    // Bottom: PLUGE on black background
    const int botY = midY + midH;
    const int botH = height - botY;
    p.fillRect(0, botY, width, botH, QColor(0,0,0));
    // Three PLUGE bars near left
    int plugeW = width / 32;
    p.fillRect(width/16,          botY, plugeW, botH, QColor(0,0,0));     // below black (clipped)
    p.fillRect(width/16+plugeW+4, botY, plugeW, botH, QColor(16,16,16));  // black
    p.fillRect(width/16+2*(plugeW+4), botY, plugeW, botH, QColor(32,32,32)); // above black

    // Label
    p.setPen(Qt::white);
    p.setFont(QFont("Arial", 16, QFont::Bold));
    p.drawText(QRect(0, botY, width, botH), Qt::AlignCenter,
               QString("PAL EBU 100/75 Color Bars - Frame %1").arg(frameNumber));
    p.end();

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

    // Queue an initial frame by default so the test chart is shown on startup
    requestFrame(0, /*priority*/1);
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
