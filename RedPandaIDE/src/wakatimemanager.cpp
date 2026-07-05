#include "wakatimemanager.h"

#include "editor.h"
#include "mainwindow.h"
#include "settings.h"

#include <QDateTime>
#include <QFileInfo>
#include <QProcess>

static constexpr qint64 WAKATIME_HEARTBEAT_THROTTLE_MS = 2 * 60 * 1000;

WakaTimeManager::WakaTimeManager(MainWindow *mainWindow):
    QObject(mainWindow),
    mMainWindow(mainWindow),
    mEnabled(false),
    mDebugEnabled(false),
    mPlugin(QStringLiteral("redpanda-cpp-wakatime/%1").arg(QString::fromUtf8(REDPANDA_CPP_VERSION)))
{
    reloadSettings();
}

void WakaTimeManager::attachEditor(Editor *editor)
{
    if (!editor || mAttachedEditors.contains(editor)) {
        return;
    }

    mAttachedEditors.insert(editor);
    connect(editor, &QObject::destroyed, this, [this, editor] {
        mAttachedEditors.remove(editor);
    });
    connect(editor, &Editor::showOccured, this, [this](Editor *e) {
        sendHeartbeat(e, false);
    });
    connect(editor, &Editor::focusInOccured, this, [this](Editor *e) {
        sendHeartbeat(e, false);
    });
    connect(editor, &Editor::statusChanged, this, [this, editor] {
        sendHeartbeat(editor, false);
    });
    connect(editor, &Editor::fileSaved, this, [this](Editor *e, const QString &) {
        sendHeartbeat(e, true);
    });
}

void WakaTimeManager::reloadSettings()
{
    const WakaTimeSettings &settings = pSettings->wakatime();
    mEnabled = settings.enabled();
    mDebugEnabled = settings.debugEnabled();
    mCliPath = settings.cliPath();
    mConfigFileName = settings.managedConfigFileName();
    if (mEnabled) {
        try {
            settings.saveManagedConfig();
        } catch (FileError &e) {
            logDebug(e.reason());
        }
    }
}

void WakaTimeManager::sendHeartbeat(Editor *editor, bool isWrite)
{
    if (!mEnabled || !editor || mCliPath.trimmed().isEmpty()) {
        return;
    }

    if (editor->isNew()) {
        return;
    }

    const QString filename = editor->filename();
    if (shouldSkipFile(filename)) {
        return;
    }

    if (!isWrite) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qint64 lastTime = mLastHeartbeatTimes.value(filename, 0);
        if (lastTime > 0 && now - lastTime < WAKATIME_HEARTBEAT_THROTTLE_MS) {
            return;
        }
        mLastHeartbeatTimes.insert(filename, now);
    }

    QString program = mCliPath;
    QString workingDir = QFileInfo(filename).absolutePath();
    QStringList args;
    args << QStringLiteral("--config") << mConfigFileName
         << QStringLiteral("--entity") << filename
         << QStringLiteral("--plugin") << mPlugin
         << QStringLiteral("--time") << QString::number(QDateTime::currentMSecsSinceEpoch() / 1000.0, 'f', 3)
         << QStringLiteral("--lineno") << QString::number(editor->caretY() + 1)
         << QStringLiteral("--cursorpos") << QString::number(editor->caretX() + 1)
         << QStringLiteral("--category") << QStringLiteral("coding");
    if (isWrite) {
        args << QStringLiteral("--write");
    }

    QProcess *process = new QProcess(this);
    process->setProgram(program);
    process->setArguments(args);
    process->setWorkingDirectory(workingDir);
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        Q_UNUSED(error);
        logDebug(tr("WakaTime CLI failed to start: %1").arg(process->errorString()));
        process->deleteLater();
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        const QByteArray stderrOutput = process->readAllStandardError();
        if (exitStatus != QProcess::NormalExit || exitCode != 0 || !stderrOutput.trimmed().isEmpty()) {
            QString message = tr("WakaTime CLI exited with code %1.").arg(exitCode);
            const QString stderrText = stderrSummary(stderrOutput);
            if (!stderrText.isEmpty()) {
                message += QStringLiteral(" ") + tr("stderr: %1").arg(stderrText);
            }
            logDebug(message);
        }
        process->deleteLater();
    });
    process->start();
}

bool WakaTimeManager::shouldSkipFile(const QString &filename) const
{
    if (filename.trimmed().isEmpty()) {
        return true;
    }

    const QFileInfo fileInfo(filename);
    return !fileInfo.exists() || !fileInfo.isFile();
}

void WakaTimeManager::logDebug(const QString &message) const
{
    if (mDebugEnabled && mMainWindow) {
        mMainWindow->logToolsOutput(QStringLiteral("[WakaTime] %1").arg(message));
    }
}

QString WakaTimeManager::stderrSummary(const QByteArray &stderrOutput) const
{
    QString result = QString::fromUtf8(stderrOutput).trimmed();
    result.replace('\r', ' ');
    result.replace('\n', ' ');
    if (result.length() > 800) {
        result = result.left(800) + QStringLiteral("...");
    }
    return result;
}
