#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "decoderworker.h"
#include "framecache.h"
#include "histogramwidget.h"
#include "vectorscopewidget.h"
#include "colorsamplerwidget.h"
#include "scanlineplotwidget.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QKeyEvent>
#include <QDockWidget>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , fileSize_(0)
    , totalFrames_(0)
    , currentFrame_(0)
    , isPlaying_(false)
    , histogramOverlay_(nullptr)
    , vectorscopeOverlay_(nullptr)
    , scanlinePlotOverlay_(nullptr)
{
    ui->setupUi(this);
    
    // Create frame cache (512 MB)
    frameCache_ = std::make_unique<FrameCache>(512 * 1024 * 1024);
    
    // Setup analysis widgets
    setupAnalysisWidgets();
    
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
    
    // Connect color management controls
    // Create color management widgets programmatically
    QGroupBox *colorMgmtBox = new QGroupBox("Color Management", ui->controlPanel);
    QVBoxLayout *colorMgmtLayout = new QVBoxLayout(colorMgmtBox);
    
    // Gamma slider
    QHBoxLayout *gammaLayout = new QHBoxLayout();
    gammaLayout->addWidget(new QLabel("Gamma:"));
    QSlider *gammaSlider = new QSlider(Qt::Horizontal);
    gammaSlider->setMinimum(10);
    gammaSlider->setMaximum(300);
    gammaSlider->setValue(220);
    gammaLayout->addWidget(gammaSlider);
    QLabel *gammaValueLabel = new QLabel("2.2");
    gammaValueLabel->setMinimumWidth(40);
    gammaLayout->addWidget(gammaValueLabel);
    colorMgmtLayout->addLayout(gammaLayout);
    
    // Exposure slider
    QHBoxLayout *exposureLayout = new QHBoxLayout();
    exposureLayout->addWidget(new QLabel("Exposure:"));
    QSlider *exposureSlider = new QSlider(Qt::Horizontal);
    exposureSlider->setMinimum(-50);
    exposureSlider->setMaximum(50);
    exposureSlider->setValue(0);
    exposureLayout->addWidget(exposureSlider);
    QLabel *exposureValueLabel = new QLabel("0.0");
    exposureValueLabel->setMinimumWidth(40);
    exposureLayout->addWidget(exposureValueLabel);
    colorMgmtLayout->addLayout(exposureLayout);
    
    // Color profile combo
    QHBoxLayout *profileLayout = new QHBoxLayout();
    profileLayout->addWidget(new QLabel("Color Profile:"));
    QComboBox *colorProfileCombo = new QComboBox();
    colorProfileCombo->addItem("Rec709");
    colorProfileCombo->addItem("sRGB");
    colorProfileCombo->addItem("Rec2020");
    colorProfileCombo->addItem("DCI-P3");
    colorProfileCombo->addItem("Adobe RGB");
    profileLayout->addWidget(colorProfileCombo);
    colorMgmtLayout->addLayout(profileLayout);
    
    // Add to control panel
    qobject_cast<QVBoxLayout*>(ui->controlPanel->layout())->insertWidget(2, colorMgmtBox);
    
    // Create analysis tools group box
    QGroupBox *analysisBox = new QGroupBox("Analysis Tools", ui->controlPanel);
    QVBoxLayout *analysisLayout = new QVBoxLayout(analysisBox);
    
    QPushButton *histogramBtn = new QPushButton("Toggle Histogram");
    histogramBtn->setCheckable(true);
    analysisLayout->addWidget(histogramBtn);
    
    QPushButton *vectorscopeBtn = new QPushButton("Toggle Vectorscope");
    vectorscopeBtn->setCheckable(true);
    analysisLayout->addWidget(vectorscopeBtn);
    
    QPushButton *colorSamplerBtn = new QPushButton("Toggle Color Sampler");
    colorSamplerBtn->setCheckable(true);
    analysisLayout->addWidget(colorSamplerBtn);
    
    QPushButton *scanlineBtn = new QPushButton("Toggle Scanline Plot");
    scanlineBtn->setCheckable(true);
    analysisLayout->addWidget(scanlineBtn);
    
    analysisLayout->addWidget(new QLabel("Overlay Options:"));
    
    QPushButton *histogramOverlayBtn = new QPushButton("Histogram Overlay");
    histogramOverlayBtn->setCheckable(true);
    analysisLayout->addWidget(histogramOverlayBtn);
    
    QPushButton *vectorscopeOverlayBtn = new QPushButton("Vectorscope Overlay");
    vectorscopeOverlayBtn->setCheckable(true);
    analysisLayout->addWidget(vectorscopeOverlayBtn);
    
    QPushButton *scanlineOverlayBtn = new QPushButton("Scanline Plot Overlay");
    scanlineOverlayBtn->setCheckable(true);
    analysisLayout->addWidget(scanlineOverlayBtn);
    
    // Add to control panel
    qobject_cast<QVBoxLayout*>(ui->controlPanel->layout())->insertWidget(3, analysisBox);
    
    // Connect color management signals
    connect(gammaSlider, &QSlider::valueChanged, [this, gammaValueLabel](int value) {
        float gamma = value / 100.0f;
        gammaValueLabel->setText(QString::number(gamma, 'f', 2));
        onGammaChanged(value);
    });
    
    connect(exposureSlider, &QSlider::valueChanged, [this, exposureValueLabel](int value) {
        float exposure = value / 10.0f;
        exposureValueLabel->setText(QString::number(exposure, 'f', 1));
        onExposureChanged(value);
    });
    
    connect(colorProfileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onColorProfileChanged);
    
    // Connect analysis tool buttons
    connect(histogramBtn, &QPushButton::clicked, this, &MainWindow::onToggleHistogram);
    connect(vectorscopeBtn, &QPushButton::clicked, this, &MainWindow::onToggleVectorscope);
    connect(colorSamplerBtn, &QPushButton::clicked, this, &MainWindow::onToggleColorSampler);
    connect(scanlineBtn, &QPushButton::clicked, this, &MainWindow::onToggleScanlinePlot);
    connect(histogramOverlayBtn, &QPushButton::clicked, this, &MainWindow::onHistogramOverlay);
    connect(vectorscopeOverlayBtn, &QPushButton::clicked, this, &MainWindow::onVectorscopeOverlay);
    connect(scanlineOverlayBtn, &QPushButton::clicked, this, &MainWindow::onScanlinePlotOverlay);
    
    // Connect color management controls (kept for compatibility if UI has these elements)
    
    // Connect video widget signals
    connect(ui->videoWidget, &VideoWidget::hairCrossPositionChanged,
            this, &MainWindow::onHairCrossPositionChanged);
    connect(ui->videoWidget, &VideoWidget::scanlineChanged,
            this, &MainWindow::onScanlineChanged);
    
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
        updateAnalysisWidgets(image);
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

void MainWindow::setupAnalysisWidgets() {
    // Create dockable histogram widget
    histogramWidget_ = std::make_unique<HistogramWidget>();
    QDockWidget *histogramDock = new QDockWidget("Histogram", this);
    histogramDock->setWidget(histogramWidget_.get());
    histogramDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, histogramDock);
    histogramDock->hide();
    
    // Create dockable vectorscope widget
    vectorscopeWidget_ = std::make_unique<VectorscopeWidget>();
    QDockWidget *vectorscopeDock = new QDockWidget("Vectorscope", this);
    vectorscopeDock->setWidget(vectorscopeWidget_.get());
    vectorscopeDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, vectorscopeDock);
    vectorscopeDock->hide();
    
    // Create color sampler widget (add to layout below video widget)
    colorSamplerWidget_ = std::make_unique<ColorSamplerWidget>();
    colorSamplerWidget_->hide();
    
    // Create dockable scanline plot widget
    scanlinePlotWidget_ = std::make_unique<ScanlinePlotWidget>();
    QDockWidget *scanlineDock = new QDockWidget("Scanline Plot", this);
    scanlineDock->setWidget(scanlinePlotWidget_.get());
    scanlineDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::BottomDockWidgetArea, scanlineDock);
    scanlineDock->hide();
}

void MainWindow::updateAnalysisWidgets(const QImage &frame) {
    if (frame.isNull()) {
        return;
    }
    
    currentDisplayFrame_ = frame;
    
    // Update histogram
    if (histogramWidget_ && histogramWidget_->isVisible()) {
        histogramWidget_->updateHistogram(frame);
    }
    if (histogramOverlay_) {
        histogramOverlay_->updateHistogram(frame);
    }
    
    // Update vectorscope
    if (vectorscopeWidget_ && vectorscopeWidget_->isVisible()) {
        vectorscopeWidget_->updateVectorscope(frame);
    }
    if (vectorscopeOverlay_) {
        vectorscopeOverlay_->updateVectorscope(frame);
    }
    
    // Update color sampler
    if (colorSamplerWidget_ && colorSamplerWidget_->isVisible()) {
        colorSamplerWidget_->updateFrame(frame);
    }
}

void MainWindow::onToggleHistogram() {
    // Toggle visibility of histogram dock
    QDockWidget *dock = qobject_cast<QDockWidget*>(histogramWidget_->parentWidget());
    if (dock) {
        dock->setVisible(!dock->isVisible());
        if (dock->isVisible() && !currentDisplayFrame_.isNull()) {
            histogramWidget_->updateHistogram(currentDisplayFrame_);
        }
    }
}

void MainWindow::onToggleVectorscope() {
    // Toggle visibility of vectorscope dock
    QDockWidget *dock = qobject_cast<QDockWidget*>(vectorscopeWidget_->parentWidget());
    if (dock) {
        dock->setVisible(!dock->isVisible());
        if (dock->isVisible() && !currentDisplayFrame_.isNull()) {
            vectorscopeWidget_->updateVectorscope(currentDisplayFrame_);
        }
    }
}

void MainWindow::onToggleColorSampler() {
    bool show = !colorSamplerWidget_->isVisible();
    colorSamplerWidget_->setVisible(show);
    ui->videoWidget->setShowHairCross(show);
    
    if (show && !currentDisplayFrame_.isNull()) {
        colorSamplerWidget_->updateFrame(currentDisplayFrame_);
    }
}

void MainWindow::onToggleScanlinePlot() {
    // Toggle visibility of scanline plot dock
    QDockWidget *dock = qobject_cast<QDockWidget*>(scanlinePlotWidget_->parentWidget());
    if (dock) {
        dock->setVisible(!dock->isVisible());
    }
}

void MainWindow::onHistogramOverlay() {
    if (!histogramOverlay_) {
        histogramOverlay_ = new HistogramWidget(this);
        histogramOverlay_->setOverlayMode(true);
        histogramOverlay_->setParent(ui->videoWidget);
        histogramOverlay_->setGeometry(10, 10, 256, 150);
        histogramOverlay_->show();
        
        if (!currentDisplayFrame_.isNull()) {
            histogramOverlay_->updateHistogram(currentDisplayFrame_);
        }
    } else {
        delete histogramOverlay_;
        histogramOverlay_ = nullptr;
    }
}

void MainWindow::onVectorscopeOverlay() {
    if (!vectorscopeOverlay_) {
        vectorscopeOverlay_ = new VectorscopeWidget(this);
        vectorscopeOverlay_->setOverlayMode(true);
        vectorscopeOverlay_->setParent(ui->videoWidget);
        vectorscopeOverlay_->setGeometry(10, 170, 256, 256);
        vectorscopeOverlay_->show();
        
        if (!currentDisplayFrame_.isNull()) {
            vectorscopeOverlay_->updateVectorscope(currentDisplayFrame_);
        }
    } else {
        delete vectorscopeOverlay_;
        vectorscopeOverlay_ = nullptr;
    }
}

void MainWindow::onScanlinePlotOverlay() {
    if (!scanlinePlotOverlay_) {
        scanlinePlotOverlay_ = new ScanlinePlotWidget(this);
        scanlinePlotOverlay_->setOverlayMode(true);
        scanlinePlotOverlay_->setParent(ui->videoWidget);
        scanlinePlotOverlay_->setGeometry(10, 440, 400, 150);
        scanlinePlotOverlay_->show();
        
        if (!currentDisplayFrame_.isNull() && ui->videoWidget->getShowHairCross()) {
            int scanlineY = ui->videoWidget->getHairCrossPos().y();
            scanlinePlotOverlay_->updateScanline(currentDisplayFrame_, scanlineY);
        }
    } else {
        delete scanlinePlotOverlay_;
        scanlinePlotOverlay_ = nullptr;
    }
}

void MainWindow::onGammaChanged(int value) {
    // Gamma slider: 10-300 (0.1 to 3.0)
    float gamma = value / 100.0f;
    ui->videoWidget->setGamma(gamma);
    statusBar()->showMessage(QString("Gamma: %1").arg(gamma, 0, 'f', 2), 2000);
}

void MainWindow::onExposureChanged(int value) {
    // Exposure slider: -50 to +50 (-5.0 to +5.0 stops)
    float exposure = value / 10.0f;
    ui->videoWidget->setExposure(exposure);
    statusBar()->showMessage(QString("Exposure: %1 stops").arg(exposure, 0, 'f', 1), 2000);
}

void MainWindow::onColorProfileChanged(int index) {
    ui->videoWidget->setColorProfile(index);
    const char* profiles[] = {"Rec709", "sRGB", "Rec2020", "DCI-P3", "Adobe RGB"};
    if (index >= 0 && index < 5) {
        statusBar()->showMessage(QString("Color Profile: %1").arg(profiles[index]), 2000);
    }
}

void MainWindow::onHairCrossPositionChanged(const QPoint &imagePos, const QPoint &screenPos) {
    Q_UNUSED(screenPos);
    
    if (colorSamplerWidget_ && colorSamplerWidget_->isVisible()) {
        colorSamplerWidget_->setSamplePosition(imagePos);
    }
}

void MainWindow::onScanlineChanged(int scanlineY) {
    if (!currentDisplayFrame_.isNull()) {
        if (scanlinePlotWidget_ && scanlinePlotWidget_->isVisible()) {
            scanlinePlotWidget_->updateScanline(currentDisplayFrame_, scanlineY);
        }
        if (scanlinePlotOverlay_) {
            scanlinePlotOverlay_->updateScanline(currentDisplayFrame_, scanlineY);
        }
    }
}
