#include "decoderworker.h"
#include <QMutexLocker>
#include <QImage>
#include <QDebug>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <utility>

#include "formats/format_base.hpp"
#include "vhsdecode/decoder_pipeline.hpp"
#include "vhsdecode/types.hpp"

using vhsdecode::VideoField;
using vhsdecode::FieldMetadata;
using vhsdecode::core::DecoderPipelineOptions;
using vhsdecode::core::DecoderPipelineResult;
using vhsdecode::core::DecoderObserver;

namespace {

class FrameCaptureObserver : public DecoderObserver {
public:
    void onFieldDecoded(VideoField&& composite,
                        VideoField&& /*chroma*/,
                        const FieldMetadata& metadata) override {
        fields_.push_back({std::move(composite), metadata});
        maybeComposeImage();
    }

    bool hasImage() const { return imageReady_; }

    void finalize() {
        if (!imageReady_) {
            maybeComposeImage();
        }
    }

    QImage takeImage() {
        if (!imageReady_) {
            finalize();
        }
        QImage result = image_;
        image_ = QImage();
        imageReady_ = false;
        fields_.clear();
        return result;
    }

private:
    struct FieldData {
        VideoField data;
        FieldMetadata metadata;
    };

    void maybeComposeImage() {
        if (fields_.empty()) {
            return;
        }
        image_ = composeImage();
        imageReady_ = !image_.isNull();
    }

    QImage composeImage() const {
        if (fields_.empty()) {
            return QImage();
        }

        size_t maxWidth = 0;
        size_t maxLines = 0;
        for (const auto& field : fields_) {
            if (!field.data.empty()) {
                maxWidth = std::max(maxWidth, field.data.front().size());
            }
            maxLines = std::max(maxLines, field.data.size());
        }

        if (maxWidth == 0 || maxLines == 0) {
            return QImage();
        }

        const int imageWidth = static_cast<int>(maxWidth);
        const int imageHeight = static_cast<int>(maxLines * 2);
        QImage image(imageWidth, imageHeight, QImage::Format_Grayscale8);
        image.fill(0);

        auto writeField = [&](const FieldData& fieldData) {
            const auto& lines = fieldData.data;
            const bool firstField = fieldData.metadata.isFirstField;
            for (size_t lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
                size_t targetRow = lineIdx * 2 + (firstField ? 0 : 1);
                if (targetRow >= static_cast<size_t>(imageHeight)) {
                    break;
                }
                uchar* scanLine = image.scanLine(static_cast<int>(targetRow));
                const auto& samples = lines[lineIdx];
                const size_t lineWidth = std::min(samples.size(), static_cast<size_t>(imageWidth));
                for (size_t col = 0; col < lineWidth; ++col) {
                    scanLine[col] = static_cast<uchar>(samples[col] >> 8);
                }
            }
        };

        for (const auto& field : fields_) {
            writeField(field);
        }

        return image;
    }

    std::vector<FieldData> fields_;
    QImage image_;
    bool imageReady_ = false;
};

} // namespace

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
    if (config_.filename.empty()) {
        emit decodingError(-1, "No filename specified");
        return;
    }

    while (!shouldStop_) {
        DecodeJob job;
        
        {
            QMutexLocker locker(&mutex_);
            if (jobQueue_.empty()) {
                condition_.wait(&mutex_, 100);
                if (shouldStop_) {
                    break;
                }
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
    (void)frameNumber;
    FrameCaptureObserver observer;

    DecoderPipelineOptions options;
    options.inputPath = config_.filename;
    options.outputBasename.clear();
    options.lengthFrames = 1;
    options.threadCount = 1;
    options.seekOffset = fileOffset;
    options.alignToFirstField = config_.alignToFirstField && (fileOffset == 0);
    options.enableFileOutput = false;

    try {
        options.format = vhsdecode::formats::formatFromString(config_.tapeFormat);
        options.system = vhsdecode::formats::systemFromString(config_.tvSystem);
    } catch (const std::exception &ex) {
        throw std::runtime_error(std::string("Invalid decoder configuration: ") + ex.what());
    }

    (void)runDecoderPipeline(options, &observer);
    observer.finalize();

    QImage image = observer.takeImage();
    if (image.isNull()) {
        throw std::runtime_error("Decoder produced no image data");
    }
    return image;
}

// DecoderWorker implementation
DecoderWorker::DecoderWorker(const DecoderConfig &config, int numThreads, QObject *parent)
    : QObject(parent)
    , config_(config)
    , nextThread_(0)
    , threadCount_(std::max(1, numThreads))
{
    // Create worker threads
    for (int i = 0; i < threadCount_; i++) {
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
    job.fileOffset = computeFileOffset(frameNumber);
    job.priority = priority;
    
    threads_[nextThread_]->addJob(job);
    nextThread_ = (nextThread_ + 1) % threads_.size();
}

void DecoderWorker::updateConfig(const DecoderConfig &config) {
    clear();
    config_ = config;

    // Recreate threads with new config
    for (int i = 0; i < threadCount_; i++) {
        auto thread = std::make_unique<DecoderThread>(config_, this);
        
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

void DecoderWorker::clear() {
    for (auto &thread : threads_) {
        if (thread) {
            thread->stop();
            thread->wait();
        }
    }
    threads_.clear();
    nextThread_ = 0;
}

void DecoderWorker::onFrameDecoded(int frameNum, const QImage image) {
    emit frameDecoded(frameNum, image);
}

size_t DecoderWorker::computeFileOffset(int frameNumber) const {
    if (frameNumber <= 0) {
        return 0;
    }

    const size_t samplesPerFrame = std::max<size_t>(1, config_.samplesPerFrame);
    size_t offset = static_cast<size_t>(frameNumber) * samplesPerFrame;

    if (config_.fileSize > 0) {
        if (offset >= config_.fileSize) {
            if (config_.fileSize > samplesPerFrame) {
                offset = config_.fileSize - samplesPerFrame;
            } else {
                offset = 0;
            }
        }
    }

    return offset;
}
