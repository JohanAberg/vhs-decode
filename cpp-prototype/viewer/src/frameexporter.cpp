#include "frameexporter.h"
#include <QDir>
#include <QFileInfo>

namespace vhsdecode {
namespace export_format {

// PNG Sequence Exporter Implementation
bool PNGSequenceExporter::initialize(const QString &outputPath, int width, int height, double fps) {
    Q_UNUSED(fps); // Not needed for image sequences
    
    outputDir_ = outputPath;
    width_ = width;
    height_ = height;
    
    // Create output directory if it doesn't exist
    QDir dir;
    if (!dir.mkpath(outputDir_)) {
        return false;
    }
    
    return true;
}

bool PNGSequenceExporter::writeFrame(int frameNumber, const QImage &image) {
    // Generate filename with zero-padded frame number (e.g., frame_0001.png)
    QString filename = QString("%1/frame_%2.png")
        .arg(outputDir_)
        .arg(frameNumber, 6, 10, QChar('0'));
    
    // Convert to appropriate format if needed
    QImage exportImage = image;
    if (image.format() != QImage::Format_RGB888 && 
        image.format() != QImage::Format_RGBA8888) {
        exportImage = image.convertToFormat(QImage::Format_RGB888);
    }
    
    // Save as PNG
    return exportImage.save(filename, "PNG");
}

bool PNGSequenceExporter::finalize() {
    // Nothing to finalize for PNG sequence
    return true;
}

// EXR Sequence Exporter Stub
bool EXRSequenceExporter::initialize(const QString &outputPath, int width, int height, double fps) {
    Q_UNUSED(fps);
    
    outputDir_ = outputPath;
    width_ = width;
    height_ = height;
    compression_ = Compression::PIZ; // Default compression
    bitDepth_ = 16; // Default 16-bit float
    
    // Create output directory
    QDir dir;
    if (!dir.mkpath(outputDir_)) {
        return false;
    }
    
    // TODO: Initialize OpenEXR library
    // This requires linking against libIlmImf
    return true;
}

bool EXRSequenceExporter::writeFrame(int frameNumber, const QImage &image) {
    Q_UNUSED(frameNumber);
    Q_UNUSED(image);
    
    // TODO: Implement EXR writing using OpenEXR library
    // This is a stub implementation showing the architecture
    // 
    // Pseudo-code:
    // 1. Convert QImage to float/half data
    // 2. Create EXR header with compression settings
    // 3. Write pixel data to .exr file
    // 4. Handle bit depth conversion (8-bit → 16/32-bit float)
    
    return false; // Not yet implemented
}

bool EXRSequenceExporter::finalize() {
    return true;
}

// ProRes Exporter Stub
bool ProResExporter::initialize(const QString &outputPath, int width, int height, double fps) {
    outputPath_ = outputPath;
    width_ = width;
    height_ = height;
    fps_ = fps;
    profile_ = Profile::Standard; // Default profile
    ffmpegContext_ = nullptr;
    
    // TODO: Initialize FFmpeg for ProRes encoding
    // This requires:
    // 1. av_register_all()
    // 2. avformat_alloc_output_context2()
    // 3. avcodec_find_encoder(AV_CODEC_ID_PRORES)
    // 4. avcodec_open2() with codec and options
    // 5. Create output stream and write header
    
    return false; // Not yet implemented
}

bool ProResExporter::writeFrame(int frameNumber, const QImage &image) {
    Q_UNUSED(frameNumber);
    Q_UNUSED(image);
    
    // TODO: Implement ProRes frame encoding
    // This is a stub showing the architecture
    //
    // Pseudo-code:
    // 1. Convert QImage to AVFrame
    // 2. Set frame pts and encoding parameters
    // 3. avcodec_send_frame()
    // 4. avcodec_receive_packet()
    // 5. av_interleaved_write_frame()
    
    return false; // Not yet implemented
}

bool ProResExporter::finalize() {
    // TODO: Write trailer and cleanup
    // av_write_trailer()
    // avcodec_free_context()
    // avformat_free_context()
    
    return true;
}

// Factory Implementation
std::unique_ptr<FrameExporter> FrameExporterFactory::createExporter(Format format) {
    switch (format) {
        case Format::PNG_Sequence:
            return std::make_unique<PNGSequenceExporter>();
        case Format::EXR_Sequence:
            return std::make_unique<EXRSequenceExporter>();
        case Format::ProRes:
            return std::make_unique<ProResExporter>();
        default:
            return nullptr;
    }
}

std::vector<FrameExporterFactory::Format> FrameExporterFactory::availableFormats() {
    std::vector<Format> formats;
    
    // PNG is always available (Qt built-in)
    formats.push_back(Format::PNG_Sequence);
    
    // Check for optional format support
    if (isFormatAvailable(Format::EXR_Sequence)) {
        formats.push_back(Format::EXR_Sequence);
    }
    if (isFormatAvailable(Format::ProRes)) {
        formats.push_back(Format::ProRes);
    }
    
    return formats;
}

bool FrameExporterFactory::isFormatAvailable(Format format) {
    switch (format) {
        case Format::PNG_Sequence:
            return true; // Always available via Qt
            
        case Format::EXR_Sequence:
            // TODO: Check if OpenEXR library is available
            // Could check for libIlmImf.so at runtime
            return false; // Not implemented yet
            
        case Format::ProRes:
        case Format::FFV1:
        case Format::H264:
        case Format::H265:
            // TODO: Check if FFmpeg is available
            // Could use dlopen/dlsym to check for libavcodec
            return false; // Not implemented yet
            
        default:
            return false;
    }
}

} // namespace export_format
} // namespace vhsdecode
