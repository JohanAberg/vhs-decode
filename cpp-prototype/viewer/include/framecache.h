#pragma once

#include <QImage>
#include <QMutex>
#include <QSet>
#include <unordered_map>
#include <list>
#include <memory>

struct CachedFrame {
    QImage image;
    int frameNumber;
    size_t memorySize;
    
    CachedFrame() : frameNumber(-1), memorySize(0) {}
    CachedFrame(const QImage &img, int num)
        : image(img)
        , frameNumber(num)
        , memorySize(img.sizeInBytes())
    {}
};

class FrameCache {
public:
    explicit FrameCache(size_t maxSizeBytes = 512 * 1024 * 1024); // Default 512MB
    ~FrameCache() = default;
    
    // Add or update a frame in the cache
    void put(int frameNumber, const QImage &image);
    
    // Get a frame from cache (returns null QImage if not found)
    QImage get(int frameNumber);
    
    // Check if frame is in cache
    bool contains(int frameNumber) const;
    
    // Get all cached frame numbers
    QSet<int> getCachedFrameNumbers() const;
    
    // Clear all cached frames
    void clear();
    
    // Clear frames outside a range (e.g., when in/out points change)
    void clearOutsideRange(int minFrame, int maxFrame);
    
    // Get cache statistics
    size_t getCurrentSize() const { return currentSize_; }
    size_t getMaxSize() const { return maxSize_; }
    size_t getFrameCount() const { return cache_.size(); }
    float getHitRate() const;

private:
    void evictLRU();
    
    struct CacheEntry {
        std::shared_ptr<CachedFrame> frame;
        std::list<int>::iterator lruIterator;
    };
    
    std::unordered_map<int, CacheEntry> cache_;
    std::list<int> lruList_;  // Most recently used at front
    
    size_t maxSize_;
    size_t currentSize_;
    
    mutable QMutex mutex_;
    
    // Statistics
    mutable size_t hits_;
    mutable size_t misses_;
};
