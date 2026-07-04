#include <QApplication>
#include <QMainWindow>
#include <QDir>

#include "MainControllerStandalone.h"
#include "gui/MainWindowStandalone.h"
#include "persistence/Settings.h"
#include "log/Logging.h"
#include "Configurator.h"

#ifdef Q_OS_WIN
    #include <windows.h>
#endif

#ifdef Q_OS_WIN
namespace {

void appendBootstrapLog(const char *stage)
{
    char tempPath[MAX_PATH] = {0};
    DWORD tempPathLength = GetTempPathA(MAX_PATH, tempPath);
    if (tempPathLength == 0 || tempPathLength >= MAX_PATH)
        return;

    char logPath[MAX_PATH] = {0};
    int written = snprintf(logPath, MAX_PATH, "%sjamtaba-bootstrap.log", tempPath);
    if (written <= 0 || written >= MAX_PATH)
        return;

    HANDLE logFile = CreateFileA(logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (logFile == INVALID_HANDLE_VALUE)
        return;

    SYSTEMTIME systemTime;
    GetLocalTime(&systemTime);

    char buffer[256] = {0};
    written = snprintf(buffer, sizeof(buffer),
                       "%04u-%02u-%02u %02u:%02u:%02u.%03u pid=%lu %s\r\n",
                       systemTime.wYear, systemTime.wMonth, systemTime.wDay,
                       systemTime.wHour, systemTime.wMinute, systemTime.wSecond,
                       systemTime.wMilliseconds, GetCurrentProcessId(), stage);
    if (written > 0) {
        DWORD bytesWritten = 0;
        WriteFile(logFile, buffer, (DWORD)written, &bytesWritten, nullptr);
    }

    CloseHandle(logFile);
}

}
#endif

int main(int argc, char *args[])
{
#ifdef Q_OS_WIN
    appendBootstrapLog("main: entered");
#endif
    QApplication::setApplicationName("JamTaba 2");
    QApplication::setApplicationVersion(APP_VERSION);
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling); // fixing issue https://github.com/elieserdejesus/JamTaba/issues/1216

#ifdef Q_OS_WIN
    appendBootstrapLog("main: before QApplication");
#endif
    QApplication application(argc, args);
#ifdef Q_OS_WIN
    appendBootstrapLog("main: after QApplication");
#endif

    auto configurator = Configurator::getInstance();
#ifdef Q_OS_WIN
    appendBootstrapLog("main: before Configurator::setUp");
#endif
    if (!configurator->setUp())
        qCritical() << "JTBConfig->setUp() FAILED !";
#ifdef Q_OS_WIN
    appendBootstrapLog("main: after Configurator::setUp");
#endif

    application.setStyle("fusion"); // same visual in all platforms
#ifdef Q_OS_WIN
    appendBootstrapLog("main: after setStyle");
#endif

    persistence::Settings settings;
    settings.load();
#ifdef Q_OS_WIN
    appendBootstrapLog("main: after settings.load");
#endif

    controller::MainControllerStandalone mainController(settings, &application);
#ifdef Q_OS_WIN
    appendBootstrapLog("main: before mainController.start");
#endif
    mainController.start();
#ifdef Q_OS_WIN
    appendBootstrapLog("main: after mainController.start");
#endif

    if (mainController.isUsingNullAudioDriver())
        QMessageBox::about(nullptr, "Fatal error!", "Jamtaba can't detect any audio device in your machine!");

    MainWindowStandalone mainWindow(&mainController);
    mainController.setMainWindow(&mainWindow);
#ifdef Q_OS_WIN
    appendBootstrapLog("main: before mainWindow.initialize");
#endif

    mainWindow.initialize();
    mainWindow.show();
#ifdef Q_OS_WIN
    appendBootstrapLog("main: after mainWindow.show");
#endif

    int execResult = application.exec();
#ifdef Q_OS_WIN
    appendBootstrapLog("main: after application.exec");
#endif

    mainController.saveLastUserSettings(mainWindow.getInputsSettings());
#ifdef Q_OS_WIN
    appendBootstrapLog("main: after saveLastUserSettings");
#endif

    return execResult;
}
