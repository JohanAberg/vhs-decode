#pragma once

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>

class TimelineWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget *parent = nullptr);
    ~TimelineWidget() override = default;

    void setTotalFrames(int total);
    void setCurrentFrame(int frame);
    void setInPoint(int frame);
    void setOutPoint(int frame);
    void clearInOutPoints();
    
    int getTotalFrames() const { return totalFrames_; }
    int getCurrentFrame() const { return currentFrame_; }
    int getInPoint() const { return inPoint_; }
    int getOutPoint() const { return outPoint_; }
    bool hasInOutPoints() const { return inPoint_ >= 0 && outPoint_ >= 0; }

signals:
    void frameSeek(int frame);
    void inPointSet(int frame);
    void outPointSet(int frame);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    int frameFromX(int x) const;
    int xFromFrame(int frame) const;
    void drawTimeline(QPainter &painter);
    void drawCurrentPosition(QPainter &painter);
    void drawInOutPoints(QPainter &painter);
    
    int totalFrames_;
    int currentFrame_;
    int inPoint_;
    int outPoint_;
    
    bool isDragging_;
    
    static constexpr int MARGIN = 10;
    static constexpr int TIMELINE_HEIGHT = 40;
};
