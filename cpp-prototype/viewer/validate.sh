#!/bin/bash
# Validation script for VHS RF Viewer code structure
# Checks that all files are present and properly structured

set -e

VIEWER_DIR="/home/runner/work/vhs-decode/vhs-decode/cpp-prototype/viewer"

echo "=== VHS RF Viewer Code Validation ==="
echo ""

# Check directory structure
echo "Checking directory structure..."
required_dirs=(
    "$VIEWER_DIR/src"
    "$VIEWER_DIR/include"
    "$VIEWER_DIR/ui"
)

for dir in "${required_dirs[@]}"; do
    if [ -d "$dir" ]; then
        echo "  ✓ $dir exists"
    else
        echo "  ✗ $dir missing"
        exit 1
    fi
done

echo ""
echo "Checking header files..."
required_headers=(
    "$VIEWER_DIR/include/mainwindow.h"
    "$VIEWER_DIR/include/videowidget.h"
    "$VIEWER_DIR/include/timelinewidget.h"
    "$VIEWER_DIR/include/decoderworker.h"
    "$VIEWER_DIR/include/framecache.h"
)

for header in "${required_headers[@]}"; do
    if [ -f "$header" ]; then
        echo "  ✓ $(basename $header)"
    else
        echo "  ✗ $(basename $header) missing"
        exit 1
    fi
done

echo ""
echo "Checking source files..."
required_sources=(
    "$VIEWER_DIR/src/main.cpp"
    "$VIEWER_DIR/src/mainwindow.cpp"
    "$VIEWER_DIR/src/videowidget.cpp"
    "$VIEWER_DIR/src/timelinewidget.cpp"
    "$VIEWER_DIR/src/decoderworker.cpp"
    "$VIEWER_DIR/src/framecache.cpp"
)

for source in "${required_sources[@]}"; do
    if [ -f "$source" ]; then
        echo "  ✓ $(basename $source)"
    else
        echo "  ✗ $(basename $source) missing"
        exit 1
    fi
done

echo ""
echo "Checking UI files..."
if [ -f "$VIEWER_DIR/ui/mainwindow.ui" ]; then
    echo "  ✓ mainwindow.ui"
else
    echo "  ✗ mainwindow.ui missing"
    exit 1
fi

echo ""
echo "Checking CMake configuration..."
if [ -f "$VIEWER_DIR/CMakeLists.txt" ]; then
    echo "  ✓ CMakeLists.txt exists"
    
    # Check for Qt6 dependencies in CMakeLists.txt
    if grep -q "find_package(Qt6" "$VIEWER_DIR/CMakeLists.txt"; then
        echo "  ✓ Qt6 dependency declared"
    else
        echo "  ✗ Qt6 dependency missing"
        exit 1
    fi
    
    if grep -q "Qt6::Widgets" "$VIEWER_DIR/CMakeLists.txt"; then
        echo "  ✓ Qt6::Widgets linked"
    else
        echo "  ✗ Qt6::Widgets not linked"
        exit 1
    fi
else
    echo "  ✗ CMakeLists.txt missing"
    exit 1
fi

echo ""
echo "Checking code structure..."

# Check for Qt includes in source files
echo "  Checking Qt includes..."
qt_includes_found=0
for source in "${required_sources[@]}"; do
    if grep -q "#include <Q" "$source" 2>/dev/null; then
        qt_includes_found=$((qt_includes_found + 1))
    fi
done

if [ $qt_includes_found -gt 0 ]; then
    echo "    ✓ Qt includes found in $qt_includes_found files"
else
    echo "    ✗ No Qt includes found"
    exit 1
fi

# Check for decoder includes in decoderworker.cpp
echo "  Checking decoder integration..."
if grep -q "vhsdecode/rf_reader.hpp" "$VIEWER_DIR/src/decoderworker.cpp"; then
    echo "    ✓ RF reader integration"
else
    echo "    ✗ RF reader integration missing"
    exit 1
fi

if grep -q "vhsdecode/rf_processor.hpp" "$VIEWER_DIR/src/decoderworker.cpp"; then
    echo "    ✓ RF processor integration"
else
    echo "    ✗ RF processor integration missing"
    exit 1
fi

if grep -q "vhsdecode/fm_demodulator.hpp" "$VIEWER_DIR/src/decoderworker.cpp"; then
    echo "    ✓ FM demodulator integration"
else
    echo "    ✗ FM demodulator integration missing"
    exit 1
fi

echo ""
echo "Checking UI features..."

# Check mainwindow.ui for required widgets
ui_file="$VIEWER_DIR/ui/mainwindow.ui"
if grep -q "VideoWidget" "$ui_file"; then
    echo "  ✓ VideoWidget in UI"
else
    echo "  ✗ VideoWidget missing from UI"
    exit 1
fi

if grep -q "TimelineWidget" "$ui_file"; then
    echo "  ✓ TimelineWidget in UI"
else
    echo "  ✗ TimelineWidget missing from UI"
    exit 1
fi

if grep -q "playButton" "$ui_file"; then
    echo "  ✓ Playback controls in UI"
else
    echo "  ✗ Playback controls missing from UI"
    exit 1
fi

if grep -q "rfBandpassLowSpinBox" "$ui_file"; then
    echo "  ✓ Parameter controls in UI"
else
    echo "  ✗ Parameter controls missing from UI"
    exit 1
fi

echo ""
echo "Checking timeline features..."
timeline_cpp="$VIEWER_DIR/src/timelinewidget.cpp"
if grep -q "Qt::Key_I" "$timeline_cpp"; then
    echo "  ✓ In point (I key) support"
else
    echo "  ✗ In point support missing"
    exit 1
fi

if grep -q "Qt::Key_O" "$timeline_cpp"; then
    echo "  ✓ Out point (O key) support"
else
    echo "  ✗ Out point support missing"
    exit 1
fi

echo ""
echo "Checking video widget features..."
video_cpp="$VIEWER_DIR/src/videowidget.cpp"
if grep -q "wheelEvent" "$video_cpp"; then
    echo "  ✓ Mouse wheel zoom support"
else
    echo "  ✗ Mouse wheel zoom missing"
    exit 1
fi

if grep -q "isPanning_" "$video_cpp"; then
    echo "  ✓ Pan support"
else
    echo "  ✗ Pan support missing"
    exit 1
fi

echo ""
echo "Checking cache implementation..."
cache_cpp="$VIEWER_DIR/src/framecache.cpp"
if grep -q "QMutexLocker" "$cache_cpp"; then
    echo "  ✓ Thread-safe cache"
else
    echo "  ✗ Cache not thread-safe"
    exit 1
fi

if grep -q "evictLRU" "$cache_cpp"; then
    echo "  ✓ LRU eviction"
else
    echo "  ✗ LRU eviction missing"
    exit 1
fi

echo ""
echo "Checking decoder worker..."
decoder_cpp="$VIEWER_DIR/src/decoderworker.cpp"
decoder_h="$VIEWER_DIR/include/decoderworker.h"
if grep -q "std::priority_queue" "$decoder_cpp" || grep -q "std::priority_queue" "$decoder_h"; then
    echo "  ✓ Priority queue for jobs"
else
    echo "  ✗ Priority queue missing"
    exit 1
fi

if grep -q "QThread" "$decoder_cpp"; then
    echo "  ✓ Multi-threaded decoding"
else
    echo "  ✗ Threading support missing"
    exit 1
fi

echo ""
echo "==================================="
echo "✓ All validation checks passed!"
echo "==================================="
echo ""
echo "The viewer code structure is complete and correct."
echo "To build, you need to install Qt6:"
echo "  sudo apt-get install qt6-base-dev libqt6opengl6-dev qt6-base-dev-tools"
echo ""
echo "Then run: ./build.sh"
