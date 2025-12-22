#pragma once

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QImage>
#include <queue>
#include <memory>
#include <string>
#include <vector>

// Forward declarations for decoder types
namespace vhsdecode {
    class RFReader;
    namespace rf {
        class RFProcessor;
    }
    namespace demod {
        class FMDemodulator;
    }
    class TBCScaler;
    class SyncDetector;
    class FilterBank;
}

struct DecodeJob {
    int frameNumber;
    size_t fileOffset;
    int priority;
    
    bool operator<(const DecodeJob &other) const {
        return priority < other.priority; // Higher priority first
    }
};

struct DecoderConfig {
    std::string filename;
    double rfBandpassLow;
    double rfBandpassHigh;
    double ire0;
    double hzPerIre;
    size_t fileSize;
    size_t samplesPerFrame;
    std::string tapeFormat;
    std::string tvSystem;
    bool alignToFirstField;
    
    DecoderConfig()
        : rfBandpassLow(1.3)
        , rfBandpassHigh(5.78)
        , ire0(4.1)
        , hzPerIre(7000.0)
        , fileSize(0)
        , samplesPerFrame(1600000)
        , tapeFormat("VHS")
        , tvSystem("PAL")
        , alignToFirstField(true)
    {}
};

class DecoderThread : public QThread {
    Q_OBJECT

public:
    explicit DecoderThread(const DecoderConfig &config, QObject *parent = nullptr);
    ~DecoderThread() override;
    
    void addJob(const DecodeJob &job);
    void stop();
    
signals:
    void frameDecoded(int frameNum, const QImage image);
    void decodingError(int frameNum, const QString &error);

protected:
    void run() override;

private:
    QImage decodeFrame(int frameNumber, size_t fileOffset);
    
    DecoderConfig config_;
    std::priority_queue<DecodeJob> jobQueue_;
    QMutex mutex_;
    QWaitCondition condition_;
    bool shouldStop_;
    
    // MOCK: Decoder components commented out for UI preview mode
    // TODO: Integrate actual decoder when APIs are finalized
    // std::unique_ptr<vhsdecode::RFReader> rfReader_;
    // std::unique_ptr<vhsdecode::rf::RFProcessor> rfProcessor_;
    // std::unique_ptr<vhsdecode::demod::FMDemodulator> fmDemod_;
    // std::unique_ptr<vhsdecode::TBCScaler> tbcScaler_;
    // std::unique_ptr<vhsdecode::SyncDetector> syncDetector_;
};

class DecoderWorker : public QObject {
    Q_OBJECT

public:
    explicit DecoderWorker(const DecoderConfig &config, int numThreads = 4, QObject *parent = nullptr);
    ~DecoderWorker();
    
    void requestFrame(int frameNumber, int priority = 0);
    void updateConfig(const DecoderConfig &config);
    void clear();

signals:
    void frameDecoded(int frameNum, const QImage image);
    void decodingProgress(int current, int total);

private slots:
    void onFrameDecoded(int frameNum, const QImage image);

private:
    DecoderConfig config_;
    std::vector<std::unique_ptr<DecoderThread>> threads_;
    int nextThread_;
    int threadCount_;

    size_t computeFileOffset(int frameNumber) const;
};
