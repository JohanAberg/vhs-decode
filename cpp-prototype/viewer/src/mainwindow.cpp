#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "decoderworker.h"
#include "framecache.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QKeyEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , fileSize_(0)
    , totalFrames_(0)
    , currentFrame_(0)
    , isPlaying_(false)
{
    ui->setupUi(this);
    
    // Create frame cache (512 MB)
    frameCache_ = std::make_unique<FrameCache>(512 * 1024 * 1024);
    
    // Connect menu actions
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::onOpenFile);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::onExit);
    connect(ui->actionZoomIn, &QAction::triggered, this, &MainWindow::onZoomIn);
    connect(ui->actionZoomOut, &QAction::triggered, this, &MainWindow::onZoomOut);
    connect(ui->actionZoomReset, &QAction::triggered, this, &MainWindow::onZoomReset);
    
    // Connect transport controls
    connect(ui->playButton, &QPushButton::clicked, this, &MainWindow::onPlay);
    connect(ui->pauseButton, &QPushButton::clicked, this, &MainWindow::onPause);
    connect(ui->prevFrameButton, &QPushButton::clicked, this, &MainWindow::onPrevFrame);
    connect(ui->nextFrameButton, &QPushButton::clicked, this, &MainWindow::onNextFrame);
    
    // Connect parameter controls
    connect(ui->rfBandpassLowSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onDemodParameterChanged);
    connect(ui->rfBandpassHighSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onDemodParameterChanged);
    connect(ui->ire0SpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onDemodParameterChanged);
    connect(ui->hzPerIreSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onDemodParameterChanged);
    
    // Connect timeline
    connect(ui->timelineWidget, &TimelineWidget::frameSeek,
            [this](int frame) {
                currentFrame_ = frame;
                updateFrameLabel();
                decodeCurrentFrame();
            });
    
    connect(ui->timelineWidget, &TimelineWidget::inPointSet,
            [this](int frame) {
                statusBar()->showMessage(QString("In point set at frame %1").arg(frame), 2000);
            });
    
    connect(ui->timelineWidget, &TimelineWidget::outPointSet,
            [this](int frame) {
                statusBar()->showMessage(QString("Out point set at frame %1").arg(frame), 2000);
            });
    
    // Set focus policy for timeline to receive key events
    ui->timelineWidget->setFocusPolicy(Qt::StrongFocus);
    
    updateFrameLabel();
}

MainWindow::~MainWindow() {
    if (decoderWorker_) {
        decoderWorker_->clear();
    }
    delete ui;
}

void MainWindow::onOpenFile() {
    QString filename = QFileDialog::getOpenFileName(
        this,
        tr("Open RF File"),
        QString(),
        tr("RF Files (*.u8 *.r40 *.lds);;All Files (*)")
    );
    
    if (!filename.isEmpty()) {
        loadRFFile(filename);
    }
}

void MainWindow::loadRFFile(const QString &filename) {
    // Stop playback if running
    if (isPlaying_) {
        stopPlayback();
    }
    
    // Clear cache
    if (frameCache_) {
        frameCache_->clear();
    }
    
    // Get file size
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot open file: %1").arg(filename));
        return;
    }
    fileSize_ = file.size();
    file.close();
    
    currentFile_ = filename;
    
    // Estimate total frames (PAL: ~2.5 MB per frame at 40 MSPS)
    // PAL field: ~800,000 samples, 2 fields per frame = 1,600,000 samples = 1.6 MB
    const size_t samplesPerFrame = 1600000;
    totalFrames_ = static_cast<int>(fileSize_ / samplesPerFrame);
    
    // Update timeline
    ui->timelineWidget->setTotalFrames(totalFrames_);
    ui->timelineWidget->clearInOutPoints();
    ui->timelineWidget->clearCachedFrames();
    
    // Reset to first frame
    currentFrame_ = 0;
    ui->timelineWidget->setCurrentFrame(currentFrame_);
    
    // Create decoder worker
    DecoderConfig config;
    config.filename = filename.toStdString();
    config.rfBandpassLow = ui->rfBandpassLowSpinBox->value();
    config.rfBandpassHigh = ui->rfBandpassHighSpinBox->value();
    config.ire0 = ui->ire0SpinBox->value();
    config.hzPerIre = ui->hzPerIreSpinBox->value();
    config.fileSize = fileSize_;
    config.samplesPerFrame = samplesPerFrame;
    config.tapeFormat = "VHS";
    config.tvSystem = "PAL";
    config.alignToFirstField = true;
    
    decoderWorker_ = std::make_unique<DecoderWorker>(config, 4);
    
    connect(decoderWorker_.get(), &DecoderWorker::frameDecoded,
            this, &MainWindow::onFrameDecoded);
    connect(decoderWorker_.get(), &DecoderWorker::decodingProgress,
            this, &MainWindow::onDecodingProgress);
    
    // Update UI
    updateFrameLabel();
    statusBar()->showMessage(tr("Loaded %1 (%2 frames estimated)")
        .arg(filename)
        .arg(totalFrames_));
    
    // Decode first frame
    decodeCurrentFrame();
}

void MainWindow::onExit() {
    close();
}

void MainWindow::onPlay() {
    if (!decoderWorker_ || totalFrames_ == 0) {
        return;
    }
    
    startPlayback();
}

void MainWindow::onPause() {
    stopPlayback();
}

void MainWindow::onPrevFrame() {
    if (currentFrame_ > 0) {
        currentFrame_--;
        ui->timelineWidget->setCurrentFrame(currentFrame_);
        updateFrameLabel();
        decodeCurrentFrame();
    }
}

void MainWindow::onNextFrame() {
    if (currentFrame_ < totalFrames_ - 1) {
        currentFrame_++;
        ui->timelineWidget->setCurrentFrame(currentFrame_);
        updateFrameLabel();
        decodeCurrentFrame();
    }
}

void MainWindow::onZoomIn() {
    float zoom = ui->videoWidget->getZoom();
    ui->videoWidget->setZoom(zoom * 1.2f);
}

void MainWindow::onZoomOut() {
    float zoom = ui->videoWidget->getZoom();
    ui->videoWidget->setZoom(zoom / 1.2f);
}

void MainWindow::onZoomReset() {
    ui->videoWidget->resetView();
}

void MainWindow::onDemodParameterChanged() {
    if (!decoderWorker_) {
        return;
    }
    
    // Clear cache when parameters change
    frameCache_->clear();
    ui->timelineWidget->clearCachedFrames();
    
    // Update decoder config
    DecoderConfig config;
    config.filename = currentFile_.toStdString();
    config.rfBandpassLow = ui->rfBandpassLowSpinBox->value();
    config.rfBandpassHigh = ui->rfBandpassHighSpinBox->value();
    config.ire0 = ui->ire0SpinBox->value();
    config.hzPerIre = ui->hzPerIreSpinBox->value();
    config.fileSize = fileSize_;
    config.samplesPerFrame = 1600000;
    config.tapeFormat = "VHS";
    config.tvSystem = "PAL";
    config.alignToFirstField = true;
    
    decoderWorker_->updateConfig(config);
    
    // Re-decode current frame
    decodeCurrentFrame();
}

void MainWindow::onFrameDecoded(int frameNum, const QImage &image) {
    // Add to cache
    frameCache_->put(frameNum, image);
    
    // Update timeline with cached frames
    ui->timelineWidget->addCachedFrame(frameNum);
    
    // Display if it's the current frame
    if (frameNum == currentFrame_) {
        ui->videoWidget->setFrame(image);
    }
    
    // Update cache stats in status bar
    float hitRate = frameCache_->getHitRate();
    statusBar()->showMessage(
        tr("Cache: %1 frames, %2 MB, Hit rate: %3%")
            .arg(frameCache_->getFrameCount())
            .arg(frameCache_->getCurrentSize() / (1024 * 1024))
            .arg(hitRate * 100.0f, 0, 'f', 1),
        5000
    );
}

void MainWindow::onDecodingProgress(int current, int total) {
    statusBar()->showMessage(tr("Decoding: %1 / %2").arg(current).arg(total));
}

void MainWindow::updateFrameLabel() {
    ui->frameLabel->setText(tr("Frame: %1 / %2").arg(currentFrame_).arg(totalFrames_));
}

void MainWindow::decodeCurrentFrame() {
    if (!decoderWorker_) {
        return;
    }
    
    // Check cache first
    QImage cached = frameCache_->get(currentFrame_);
    if (!cached.isNull()) {
        ui->videoWidget->setFrame(cached);
        return;
    }
    
    // Request decode
    decoderWorker_->requestFrame(currentFrame_, 10); // High priority for current frame
    
    // Pre-fetch next few frames
    for (int i = 1; i <= 5; i++) {
        int nextFrame = currentFrame_ + i;
        if (nextFrame < totalFrames_ && !frameCache_->contains(nextFrame)) {
            decoderWorker_->requestFrame(nextFrame, 5 - i);
        }
    }
}

void MainWindow::startPlayback() {
    if (isPlaying_) {
        return;
    }
    
    isPlaying_ = true;
    
    // Determine playback range
    int startFrame = currentFrame_;
    int endFrame = totalFrames_ - 1;
    
    if (ui->timelineWidget->hasInOutPoints()) {
        startFrame = ui->timelineWidget->getInPoint();
        endFrame = ui->timelineWidget->getOutPoint();
        if (currentFrame_ < startFrame || currentFrame_ > endFrame) {
            currentFrame_ = startFrame;
        }
    }
    
    // Create timer for playback (25 fps for PAL)
    QTimer *playbackTimer = new QTimer(this);
    connect(playbackTimer, &QTimer::timeout, [this, startFrame, endFrame, playbackTimer]() {
        if (!isPlaying_) {
            playbackTimer->stop();
            playbackTimer->deleteLater();
            return;
        }
        
        currentFrame_++;
        if (currentFrame_ > endFrame) {
            if (ui->loopCheckBox->isChecked()) {
                currentFrame_ = startFrame;
            } else {
                stopPlayback();
                playbackTimer->stop();
                playbackTimer->deleteLater();
                return;
            }
        }
        
        ui->timelineWidget->setCurrentFrame(currentFrame_);
        updateFrameLabel();
        decodeCurrentFrame();
    });
    
    playbackTimer->start(40); // 25 fps = 40ms per frame
}

void MainWindow::stopPlayback() {
    isPlaying_ = false;
}
