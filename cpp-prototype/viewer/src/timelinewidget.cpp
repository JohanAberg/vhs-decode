#include "timelinewidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <algorithm>

TimelineWidget::TimelineWidget(QWidget *parent)
    : QWidget(parent)
    , totalFrames_(0)
    , currentFrame_(0)
    , inPoint_(-1)
    , outPoint_(-1)
    , isDragging_(false)
{
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(80);
}

void TimelineWidget::setTotalFrames(int total) {
    totalFrames_ = total;
    update();
}

void TimelineWidget::setCurrentFrame(int frame) {
    currentFrame_ = std::max(0, std::min(frame, totalFrames_ - 1));
    update();
}

void TimelineWidget::setInPoint(int frame) {
    inPoint_ = std::max(0, std::min(frame, totalFrames_ - 1));
    
    // Ensure in point is before out point
    if (outPoint_ >= 0 && inPoint_ > outPoint_) {
        std::swap(inPoint_, outPoint_);
    }
    
    emit inPointSet(inPoint_);
    update();
}

void TimelineWidget::setOutPoint(int frame) {
    outPoint_ = std::max(0, std::min(frame, totalFrames_ - 1));
    
    // Ensure out point is after in point
    if (inPoint_ >= 0 && outPoint_ < inPoint_) {
        std::swap(inPoint_, outPoint_);
    }
    
    emit outPointSet(outPoint_);
    update();
}

void TimelineWidget::clearInOutPoints() {
    inPoint_ = -1;
    outPoint_ = -1;
    update();
}

void TimelineWidget::setCachedFrames(const QSet<int> &frames) {
    cachedFrames_ = frames;
    update();
}

void TimelineWidget::addCachedFrame(int frame) {
    cachedFrames_.insert(frame);
    update();
}

void TimelineWidget::removeCachedFrame(int frame) {
    cachedFrames_.remove(frame);
    update();
}

void TimelineWidget::clearCachedFrames() {
    cachedFrames_.clear();
    update();
}

void TimelineWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    drawTimeline(painter);
    drawCachedFrames(painter);
    drawInOutPoints(painter);
    drawCurrentPosition(painter);
}

void TimelineWidget::drawTimeline(QPainter &painter) {
    if (totalFrames_ <= 0) {
        return;
    }
    
    int timelineWidth = width() - 2 * MARGIN;
    int timelineTop = (height() - TIMELINE_HEIGHT) / 2;
    
    // Draw background
    painter.fillRect(MARGIN, timelineTop, timelineWidth, TIMELINE_HEIGHT,
                     QColor(60, 60, 60));
    
    // Draw border
    painter.setPen(QColor(100, 100, 100));
    painter.drawRect(MARGIN, timelineTop, timelineWidth, TIMELINE_HEIGHT);
    
    // Draw frame markers every 10 frames
    painter.setPen(QColor(150, 150, 150));
    int markerInterval = std::max(1, totalFrames_ / 50);
    for (int i = 0; i <= totalFrames_; i += markerInterval) {
        int x = xFromFrame(i);
        int markerHeight = (i % (markerInterval * 5) == 0) ? 10 : 5;
        painter.drawLine(x, timelineTop, x, timelineTop + markerHeight);
    }
}

void TimelineWidget::drawCachedFrames(QPainter &painter) {
    if (totalFrames_ <= 0 || cachedFrames_.isEmpty()) {
        return;
    }
    
    int timelineTop = (height() - TIMELINE_HEIGHT) / 2;
    int timelineBottom = timelineTop + TIMELINE_HEIGHT;
    
    // Draw small markers at bottom of timeline for cached frames
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(100, 200, 255)); // Cyan/blue color for cache markers
    
    for (int frame : cachedFrames_) {
        if (frame >= 0 && frame < totalFrames_) {
            int x = xFromFrame(frame);
            // Draw small rectangle at bottom of timeline
            painter.drawRect(x - 1, timelineBottom - CACHE_MARKER_HEIGHT, 
                           2, CACHE_MARKER_HEIGHT);
        }
    }
}

void TimelineWidget::drawInOutPoints(QPainter &painter) {
    if (!hasInOutPoints()) {
        return;
    }
    
    int timelineTop = (height() - TIMELINE_HEIGHT) / 2;
    
    // Draw shaded region between in and out points
    int inX = xFromFrame(inPoint_);
    int outX = xFromFrame(outPoint_);
    painter.fillRect(inX, timelineTop, outX - inX, TIMELINE_HEIGHT,
                     QColor(100, 150, 200, 80));
    
    // Draw in point marker (green)
    painter.setPen(QPen(QColor(0, 200, 0), 2));
    painter.drawLine(inX, timelineTop, inX, timelineTop + TIMELINE_HEIGHT);
    painter.setBrush(QColor(0, 200, 0));
    QPolygon inTriangle;
    inTriangle << QPoint(inX - 5, timelineTop - 5)
               << QPoint(inX + 5, timelineTop - 5)
               << QPoint(inX, timelineTop);
    painter.drawPolygon(inTriangle);
    
    // Draw out point marker (red)
    painter.setPen(QPen(QColor(200, 0, 0), 2));
    painter.drawLine(outX, timelineTop, outX, timelineTop + TIMELINE_HEIGHT);
    painter.setBrush(QColor(200, 0, 0));
    QPolygon outTriangle;
    outTriangle << QPoint(outX - 5, timelineTop - 5)
                << QPoint(outX + 5, timelineTop - 5)
                << QPoint(outX, timelineTop);
    painter.drawPolygon(outTriangle);
}

void TimelineWidget::drawCurrentPosition(QPainter &painter) {
    if (totalFrames_ <= 0) {
        return;
    }
    
    int timelineTop = (height() - TIMELINE_HEIGHT) / 2;
    int x = xFromFrame(currentFrame_);
    
    // Draw position line (yellow)
    painter.setPen(QPen(QColor(255, 255, 0), 2));
    painter.drawLine(x, timelineTop, x, timelineTop + TIMELINE_HEIGHT);
    
    // Draw position marker
    painter.setBrush(QColor(255, 255, 0));
    QPolygon marker;
    marker << QPoint(x - 6, timelineTop + TIMELINE_HEIGHT)
           << QPoint(x + 6, timelineTop + TIMELINE_HEIGHT)
           << QPoint(x, timelineTop + TIMELINE_HEIGHT + 8);
    painter.drawPolygon(marker);
    
    // Draw frame number
    painter.setPen(QColor(255, 255, 255));
    QString text = QString::number(currentFrame_);
    QRect textRect = painter.fontMetrics().boundingRect(text);
    textRect.moveCenter(QPoint(x, timelineTop + TIMELINE_HEIGHT + 20));
    painter.drawText(textRect, Qt::AlignCenter, text);
}

void TimelineWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        isDragging_ = true;
        int frame = frameFromX(event->pos().x());
        setCurrentFrame(frame);
        emit frameSeek(currentFrame_);
    }
}

void TimelineWidget::mouseMoveEvent(QMouseEvent *event) {
    if (isDragging_) {
        int frame = frameFromX(event->pos().x());
        setCurrentFrame(frame);
        emit frameSeek(currentFrame_);
    }
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        isDragging_ = false;
    }
}

void TimelineWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_I) {
        setInPoint(currentFrame_);
        event->accept();
    } else if (event->key() == Qt::Key_O) {
        setOutPoint(currentFrame_);
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

int TimelineWidget::frameFromX(int x) const {
    if (totalFrames_ <= 0) {
        return 0;
    }
    
    int timelineWidth = width() - 2 * MARGIN;
    int relX = x - MARGIN;
    
    float ratio = static_cast<float>(relX) / timelineWidth;
    int frame = static_cast<int>(ratio * totalFrames_);
    
    return std::max(0, std::min(frame, totalFrames_ - 1));
}

int TimelineWidget::xFromFrame(int frame) const {
    if (totalFrames_ <= 0) {
        return MARGIN;
    }
    
    int timelineWidth = width() - 2 * MARGIN;
    float ratio = static_cast<float>(frame) / totalFrames_;
    
    return MARGIN + static_cast<int>(ratio * timelineWidth);
}
