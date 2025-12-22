#include "mainwindow.h"
#include <QApplication>
#include <QSurfaceFormat>
#include <QIcon>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    // Set application icon (affects taskbar and some window managers)
    app.setWindowIcon(QIcon(":/images/viewer/signal_color_v01.png"));
    
    // Set up OpenGL format
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    
    // Create and show main window
    MainWindow window;
    // Ensure window icon is set explicitly
    window.setWindowIcon(QIcon(":/images/viewer/signal_color_v01.png"));
    window.show();
    
    return app.exec();
}
