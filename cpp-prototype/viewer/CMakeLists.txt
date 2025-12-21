cmake_minimum_required(VERSION 3.16)

project(vhs-rf-viewer
    VERSION 0.1.0
    DESCRIPTION "Qt-based RF viewer for VHS-Decode"
    LANGUAGES CXX
)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt settings
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    OpenGL
    OpenGLWidgets
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/videowidget.cpp
    src/timelinewidget.cpp
    src/decoderworker.cpp
    src/framecache.cpp
)

# Header files
set(HEADERS
    include/mainwindow.h
    include/videowidget.h
    include/timelinewidget.h
    include/decoderworker.h
    include/framecache.h
)

# UI files
set(UI_FILES
    ui/mainwindow.ui
)

# Create executable
add_executable(vhs-rf-viewer
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
)

# Link Qt libraries
target_link_libraries(vhs-rf-viewer PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Threads::Threads
)

# Link decoder libraries (from parent cpp-prototype)
target_link_libraries(vhs-rf-viewer PRIVATE
    vhsdecode-lib
)

# Platform-specific settings
if(WIN32)
    set_target_properties(vhs-rf-viewer PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Installation
install(TARGETS vhs-rf-viewer
    RUNTIME DESTINATION bin
)
