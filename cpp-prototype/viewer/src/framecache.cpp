#include "framecache.h"
#include <QMutexLocker>

FrameCache::FrameCache(size_t maxSizeBytes)
    : maxSize_(maxSizeBytes)
    , currentSize_(0)
    , hits_(0)
    , misses_(0)
{
}

void FrameCache::put(int frameNumber, const QImage &image) {
    QMutexLocker locker(&mutex_);
    
    // Check if frame already exists
    auto it = cache_.find(frameNumber);
    if (it != cache_.end()) {
        // Update existing entry
        currentSize_ -= it->second.frame->memorySize;
        it->second.frame->image = image;
        it->second.frame->memorySize = image.sizeInBytes();
        currentSize_ += it->second.frame->memorySize;
        
        // Move to front of LRU list
        lruList_.erase(it->second.lruIterator);
        lruList_.push_front(frameNumber);
        it->second.lruIterator = lruList_.begin();
        
        return;
    }
    
    // Create new entry
    auto frame = std::make_shared<CachedFrame>(image, frameNumber);
    
    // Evict old frames if necessary
    while (currentSize_ + frame->memorySize > maxSize_ && !cache_.empty()) {
        evictLRU();
    }
    
    // Add new frame
    lruList_.push_front(frameNumber);
    
    CacheEntry entry;
    entry.frame = frame;
    entry.lruIterator = lruList_.begin();
    
    cache_[frameNumber] = entry;
    currentSize_ += frame->memorySize;
}

QImage FrameCache::get(int frameNumber) {
    QMutexLocker locker(&mutex_);
    
    auto it = cache_.find(frameNumber);
    if (it == cache_.end()) {
        misses_++;
        return QImage();
    }
    
    hits_++;
    
    // Move to front of LRU list
    lruList_.erase(it->second.lruIterator);
    lruList_.push_front(frameNumber);
    it->second.lruIterator = lruList_.begin();
    
    return it->second.frame->image;
}

bool FrameCache::contains(int frameNumber) const {
    QMutexLocker locker(&mutex_);
    return cache_.find(frameNumber) != cache_.end();
}

QSet<int> FrameCache::getCachedFrameNumbers() const {
    QMutexLocker locker(&mutex_);
    
    QSet<int> frames;
    for (const auto &entry : cache_) {
        frames.insert(entry.first);
    }
    return frames;
}

void FrameCache::clear() {
    QMutexLocker locker(&mutex_);
    
    cache_.clear();
    lruList_.clear();
    currentSize_ = 0;
}

void FrameCache::clearOutsideRange(int minFrame, int maxFrame) {
    QMutexLocker locker(&mutex_);
    
    auto it = cache_.begin();
    while (it != cache_.end()) {
        if (it->first < minFrame || it->first > maxFrame) {
            currentSize_ -= it->second.frame->memorySize;
            lruList_.erase(it->second.lruIterator);
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

float FrameCache::getHitRate() const {
    QMutexLocker locker(&mutex_);
    
    size_t total = hits_ + misses_;
    if (total == 0) {
        return 0.0f;
    }
    
    return static_cast<float>(hits_) / total;
}

void FrameCache::evictLRU() {
    if (lruList_.empty()) {
        return;
    }
    
    // Get least recently used frame
    int frameToEvict = lruList_.back();
    lruList_.pop_back();
    
    auto it = cache_.find(frameToEvict);
    if (it != cache_.end()) {
        currentSize_ -= it->second.frame->memorySize;
        cache_.erase(it);
    }
}
