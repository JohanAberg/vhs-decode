#pragma once

#include <QString>
#include <QImage>
#include <vector>
#include <memory>

namespace vhsdecode {
namespace export_format {

/**
 * @brief Base class for frame export formats
 * 
 * This architecture supports exporting decoded frames to various formats
 * including image sequences (PNG, TIFF, EXR) and video codecs (ProRes, FFV1, etc.)
 */
class FrameExporter {
public:
    virtual ~FrameExporter() = default;
    
    /**
     * @brief Initialize exporter with output path and parameters
     * @param outputPath Output file or directory path
     * @param width Frame width
     * @param height Frame height
     * @param fps Frames per second
     * @return true if initialization successful
     */
    virtual bool initialize(const QString &outputPath, int width, int height, double fps) = 0;
    
    /**
     * @brief Write a single frame
     * @param frameNumber Frame number in sequence
     * @param image Frame image data
     * @return true if write successful
     */
    virtual bool writeFrame(int frameNumber, const QImage &image) = 0;
    
    /**
     * @brief Finalize export (flush buffers, write trailer, etc.)
     * @return true if finalization successful
     */
    virtual bool finalize() = 0;
    
    /**
     * @brief Get format name
     */
    virtual QString formatName() const = 0;
    
    /**
     * @brief Get file extension
     */
    virtual QString fileExtension() const = 0;
};

/**
 * @brief PNG image sequence exporter
 */
class PNGSequenceExporter : public FrameExporter {
public:
    bool initialize(const QString &outputPath, int width, int height, double fps) override;
    bool writeFrame(int frameNumber, const QImage &image) override;
    bool finalize() override;
    QString formatName() const override { return "PNG Sequence"; }
    QString fileExtension() const override { return "png"; }
    
private:
    QString outputDir_;
    int width_;
    int height_;
};

/**
 * @brief OpenEXR image sequence exporter (16-bit float per channel)
 * 
 * EXR format provides:
 * - High dynamic range
 * - Lossless or lossy compression
 * - Multiple layers/channels
 * - Industry-standard for VFX workflows
 * 
 * Note: Requires OpenEXR library (libIlmImf)
 */
class EXRSequenceExporter : public FrameExporter {
public:
    enum class Compression {
        None,
        RLE,
        ZIP,
        PIZ,
        PXR24,
        B44,
        B44A,
        DWAA,
        DWAB
    };
    
    bool initialize(const QString &outputPath, int width, int height, double fps) override;
    bool writeFrame(int frameNumber, const QImage &image) override;
    bool finalize() override;
    QString formatName() const override { return "OpenEXR Sequence"; }
    QString fileExtension() const override { return "exr"; }
    
    void setCompression(Compression comp) { compression_ = comp; }
    void setBitDepth(int bits) { bitDepth_ = bits; } // 16 or 32
    
private:
    QString outputDir_;
    int width_;
    int height_;
    Compression compression_;
    int bitDepth_;
};

/**
 * @brief Apple ProRes video exporter
 * 
 * ProRes provides:
 * - Professional-grade quality
 * - Efficient encoding/decoding
 * - Wide application support
 * - Multiple quality levels
 * 
 * Note: Requires FFmpeg with ProRes support
 */
class ProResExporter : public FrameExporter {
public:
    enum class Profile {
        Proxy,        // 422 Proxy (low bandwidth)
        LT,           // 422 LT (lightweight)
        Standard,     // 422 (standard)
        HQ,           // 422 HQ (high quality)
        _4444,        // 4444 (alpha channel support)
        _4444XQ       // 4444 XQ (highest quality)
    };
    
    bool initialize(const QString &outputPath, int width, int height, double fps) override;
    bool writeFrame(int frameNumber, const QImage &image) override;
    bool finalize() override;
    QString formatName() const override { return "Apple ProRes"; }
    QString fileExtension() const override { return "mov"; }
    
    void setProfile(Profile profile) { profile_ = profile; }
    
private:
    QString outputPath_;
    int width_;
    int height_;
    double fps_;
    Profile profile_;
    void *ffmpegContext_; // FFmpeg encoding context (opaque pointer)
};

/**
 * @brief Factory for creating frame exporters
 */
class FrameExporterFactory {
public:
    enum class Format {
        PNG_Sequence,
        EXR_Sequence,
        ProRes,
        FFV1,           // Lossless codec
        H264,           // H.264/AVC
        H265            // H.265/HEVC
    };
    
    /**
     * @brief Create exporter for specified format
     */
    static std::unique_ptr<FrameExporter> createExporter(Format format);
    
    /**
     * @brief Get list of available formats
     */
    static std::vector<Format> availableFormats();
    
    /**
     * @brief Check if format is available (dependencies satisfied)
     */
    static bool isFormatAvailable(Format format);
};

} // namespace export_format
} // namespace vhsdecode
