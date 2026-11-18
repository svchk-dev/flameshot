// SPDX-License-Identifier: GPL-3.0-or-later
// Integration test: only sends events inside its own application process.
#include "core/capturerequest.h"
#include "core/flameshot.h"
#include "core/flameshotdaemon.h"
#include "widgets/capture/capturetoolbutton.h"
#include "widgets/capture/capturewidget.h"
#include "widgets/capture/selectionwidget.h"
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QMouseEvent>
#include <QPointer>
#include <QScreen>
#include <QThread>
#include <iostream>
#include <cstdlib>
#include <stdexcept>

static void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

static bool samePixels(const QImage& first, const QImage& second)
{
    return first.size() == second.size() &&
           first.convertToFormat(QImage::Format_ARGB32) ==
             second.convertToFormat(QImage::Format_ARGB32);
}

static void settle(int milliseconds = 100)
{
    QElapsedTimer timer;
    timer.start();
    do {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QThread::msleep(5);
    } while (timer.elapsed() < milliseconds);
}

static void mouse(QWidget* widget, QEvent::Type type, QPoint position,
                  Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent event(type, QPointF(position),
                      QPointF(widget->mapToGlobal(position)), button, buttons,
                      Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}

static void drag(QWidget* widget, QPoint from, QPoint to)
{
    mouse(widget, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
    mouse(widget, QEvent::MouseMove, (from + to) / 2, Qt::NoButton, Qt::LeftButton);
    mouse(widget, QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton);
    mouse(widget, QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
    settle();
}

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "xcb");
    QApplication app(argc, argv);
    app.setApplicationName("flameshot-multimon-smoke");
    app.setOrganizationName("flameshot");
    app.setQuitOnLastWindowClosed(false);
    std::cout << "Starting MultiMonitor integration test" << std::endl;
    int result = 0;
    try {
        require(argc == 2, "Pass a local directory for test captures");
        QDir directory(argv[1]);
        require(directory.mkpath("."), "Cannot create test directory");
        const auto screens = app.screens();
        require(screens.size() >= 2, "Two monitors are required");
        QRect desktop;
        for (auto* screen : screens) desktop = desktop.united(screen->geometry());
        require(desktop.width() >= 2800, "Unexpected desktop width");
        std::cout << "Starting test daemon" << std::endl;
        FlameshotDaemon::start();
        const QList<QRect> regions = {
            QRect(80, 160, 320, 180),
            QRect(screens[1]->geometry().topLeft() - desktop.topLeft() + QPoint(80, 160), QSize(320, 180)),
            QRect(screens[1]->geometry().x() - desktop.x() - 150, 350, 400, 200)
        };
        int number = 0;
        for (const QRect& region : regions) {
            CaptureRequest request(CaptureRequest::GRAPHICAL_MODE);
            const QString filename = directory.filePath(QString("region-%1.png").arg(++number));
            request.addSaveTask(filename);
            request.addTask(CaptureRequest::COPY);
            std::cout << "Opening region " << number << std::endl;
            QPointer<CaptureWidget> overlay = Flameshot::instance()->gui(request);
            require(overlay, "Overlay was not created");
            settle();
            require(overlay->spansDesktop(), "Monitor picker was not bypassed");
            require(overlay->geometry() == desktop, "Overlay does not cover both monitors");
            require(!(overlay->windowState() & Qt::WindowFullScreen), "Native fullscreen confines the overlay");
            const QImage baseline = overlay->pixmap().toImage();
            require(baseline.size() == desktop.size(), "Screenshot coordinates differ from desktop coordinates");
            drag(overlay, region.topLeft(), region.bottomRight());
            auto* selection = overlay->findChild<SelectionWidget*>();
            require(selection, "Selection widget is missing");
            const QRect selected = selection->geometry();
            // Upstream excludes a one-pixel border for one drag direction.
            require(std::abs(selected.left() - region.left()) <= 1 &&
                    std::abs(selected.top() - region.top()) <= 1 &&
                    std::abs(selected.right() - region.right()) <= 1 &&
                    std::abs(selected.bottom() - region.bottom()) <= 1,
                    "Mouse selection has incorrect coordinates");
            require(samePixels(overlay->pixmap().toImage(), baseline.copy(selected)), "Selected pixels do not match the visible region");
            CaptureToolButton* pencil = nullptr;
            for (auto* button : overlay->findChildren<CaptureToolButton*>())
                if (button->tool()->type() == CaptureTool::TYPE_PENCIL) pencil = button;
            require(pencil, "Pencil tool is missing");
            require(QMetaObject::invokeMethod(overlay, "setState", Qt::DirectConnection,
                        Q_ARG(CaptureToolButton*, pencil)), "Cannot activate pencil");
            drag(overlay, region.topLeft() + QPoint(40, 50), region.bottomRight() - QPoint(40, 50));
            overlay->commitCurrentTool();
            require(QMetaObject::invokeMethod(overlay, "handleToolSignal", Qt::DirectConnection,
                        Q_ARG(CaptureTool::Request, CaptureTool::REQ_CLEAR_SELECTION)), "Cannot finalize drawing");
            const QImage annotated = overlay->pixmap().toImage();
            require(!samePixels(annotated, baseline.copy(selected)), "Drawing did not change the image");
            require(QMetaObject::invokeMethod(overlay, "handleToolSignal", Qt::DirectConnection,
                        Q_ARG(CaptureTool::Request, CaptureTool::REQ_CAPTURE_DONE_OK)), "Cannot finish capture");
            overlay->close();
            settle(800);
            require(overlay.isNull(), "Capture overlay did not close");
            require(samePixels(QImage(filename), annotated), "Saved PNG differs from the selected image");
            require(samePixels(app.clipboard()->image(), annotated), "Clipboard differs from the selected image");
            std::cout << "PASS region " << number << ": " << region.x() << ',' << region.y()
                      << ' ' << region.width() << 'x' << region.height()
                      << " selection/pixels/drawing/save/clipboard" << std::endl;
        }
        std::cout << "PASS overlay " << desktop.width() << 'x' << desktop.height()
                  << ": both monitors, no monitor picker" << std::endl;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << std::endl;
        for (auto* window : app.topLevelWidgets()) window->hide();
        result = 1;
    }
    settle();
    return result;
}

