#include "wakatimemanager.h"

#include "editor.h"
#include "editormanager.h"
#include "mainwindow.h"
#include "project.h"
#include "settings.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QProcess>

#include <algorithm>

static constexpr int WAKATIME_ACTIVITY_DEBOUNCE_MS = 50;
static constexpr int WAKATIME_CAPABILITY_PROBE_TIMEOUT_MS = 5000;
static constexpr int WAKATIME_MAX_RETRY_DELAY_MS = 30000;

WakaTimeManager::WakaTimeManager(MainWindow *mainWindow):
    QObject(mainWindow),
    mMainWindow(mainWindow),
    mEnabled(false),
    mDebugEnabled(false),
    mPlugin(QStringLiteral("redpanda-cpp/%1 redpanda-cpp-wakatime/%2")
                .arg(QString::fromUtf8(REDPANDA_CPP_VERSION),
                     QString::fromUtf8(WAKATIME_PLUGIN_VERSION)))
{
    mActivityDebounceTimer.setSingleShot(true);
    mFlushTimer.setSingleShot(true);
    mRetryTimer.setSingleShot(true);

    connect(&mActivityDebounceTimer, &QTimer::timeout, this, [this] {
        Editor *editor = mPendingActivityEditor.data();
        mPendingActivityEditor.clear();
        if (editor) {
            appendHeartbeat(editor, false);
        }
    });
    connect(&mFlushTimer, &QTimer::timeout, this, &WakaTimeManager::flushPending);
    connect(&mRetryTimer, &QTimer::timeout, this, &WakaTimeManager::flushPending);

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
    connect(editor, &Editor::statusChanged, this,
            [this, editor](QSynedit::StatusChanges changes) {
        if (!isWakaTimeEditorActivity(changes)
                || QApplication::applicationState() != Qt::ApplicationActive
                || !editor->hasFocus()) {
            return;
        }
        debounceEditorActivity(editor);
    });
    connect(editor, &Editor::fileSaved, this, [this](Editor *e, const QString &) {
        appendHeartbeat(e, true);
    });
}

void WakaTimeManager::reloadSettings()
{
    const WakaTimeSettings &settings = pSettings->wakatime();
    const bool newEnabled = settings.enabled();
    const QString newCliPath = settings.cliPath().trimmed();
    const bool resetPolicy = (!mEnabled && newEnabled) || mCliPath != newCliPath;

    if (mEnabled && !newEnabled) {
        mActivityDebounceTimer.stop();
        mPendingActivityEditor.clear();
        mFlushTimer.stop();
        mDraining = !mBuffer.isEmpty() || mSendProcess;
        if (!mProbeComplete) {
            stopCapabilityProbe();
            mProbeComplete = true;
            mCliCapabilities = WakaTimeCliCapabilities{};
        }
        flushPending();
    }

    mEnabled = newEnabled;
    mDebugEnabled = settings.debugEnabled();
    mCliPath = newCliPath;
    mConfigFileName = settings.managedConfigFileName();

    if (resetPolicy) {
        mPolicy.reset();
    }

    if (!mEnabled) {
        return;
    }

    try {
        settings.saveManagedConfig();
    } catch (FileError &e) {
        logDebug(e.reason());
    }
    beginCapabilityProbe();
}

void WakaTimeManager::onEditorActivated(Editor *editor)
{
    if (QApplication::applicationState() != Qt::ApplicationActive
            || (mMainWindow && (mMainWindow->openingFiles() || mMainWindow->openingProject()))) {
        return;
    }
    appendHeartbeat(editor, false);
}

void WakaTimeManager::setBuilding(bool building)
{
    if (mBuilding == building) {
        return;
    }
    mBuilding = building;
    if (mMainWindow && mMainWindow->editorManager()) {
        appendHeartbeat(mMainWindow->editorManager()->getEditor(), false);
    }
}

void WakaTimeManager::setDebugging(bool debugging)
{
    if (mDebugging == debugging) {
        return;
    }
    mDebugging = debugging;
    if (mMainWindow && mMainWindow->editorManager()) {
        appendHeartbeat(mMainWindow->editorManager()->getEditor(), false);
    }
}

void WakaTimeManager::shutdown(int timeoutMs)
{
    if (mShuttingDown) {
        return;
    }

    if (mActivityDebounceTimer.isActive()) {
        mActivityDebounceTimer.stop();
        Editor *editor = mPendingActivityEditor.data();
        mPendingActivityEditor.clear();
        if (editor) {
            appendHeartbeat(editor, false);
        }
    }
    mShuttingDown = true;
    mDraining = true;
    mFlushTimer.stop();
    if (!mProbeComplete) {
        stopCapabilityProbe();
        mProbeComplete = true;
        mCliCapabilities = WakaTimeCliCapabilities{};
    }
    mRetryTimer.stop();
    flushPending();

    QElapsedTimer elapsed;
    elapsed.start();
    while ((!mBuffer.isEmpty() || mSendProcess) && elapsed.elapsed() < timeoutMs) {
        if (!mSendProcess && !mRetryTimer.isActive()) {
            flushPending();
        }
        if (mSendProcess) {
            const int remaining = std::max(1, timeoutMs - static_cast<int>(elapsed.elapsed()));
            mSendProcess->waitForFinished(std::min(remaining, 50));
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    if (!mBuffer.isEmpty() || mSendProcess) {
        logDebug(tr("Timed out while flushing %1 pending WakaTime heartbeat(s).")
                     .arg(mBuffer.size() + mInFlightHeartbeats.size()));
    }
}

void WakaTimeManager::debounceEditorActivity(Editor *editor)
{
    if (!mEnabled || mShuttingDown) {
        return;
    }
    mPendingActivityEditor = editor;
    mActivityDebounceTimer.start(WAKATIME_ACTIVITY_DEBOUNCE_MS);
}

void WakaTimeManager::appendHeartbeat(Editor *editor, bool isWrite)
{
    if (!mEnabled || mShuttingDown || !editor || mCliPath.isEmpty()
            || !QFileInfo(mCliPath).isFile() || editor->isNew()) {
        return;
    }

    const QString filename = editor->filename();
    if (shouldSkipFile(filename)) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const WakaTimeCategory category = wakaTimeCategory(mBuilding, mDebugging);
    const int lineNumber = editor->caretY() + 1;
    const int cursorPosition = editor->caretX() + 1;
    if (!mPolicy.shouldAppend(filename, now, lineNumber, cursorPosition, isWrite, category)) {
        return;
    }

    enqueueHeartbeat(makeHeartbeat(editor, isWrite, now));
}

bool WakaTimeManager::shouldSkipFile(const QString &filename) const
{
    if (filename.trimmed().isEmpty()) {
        return true;
    }

    const QFileInfo fileInfo(filename);
    return !fileInfo.exists() || !fileInfo.isFile();
}

WakaTimeHeartbeat WakaTimeManager::makeHeartbeat(Editor *editor, bool isWrite, qint64 timeMs) const
{
    WakaTimeHeartbeat heartbeat;
    heartbeat.entity = editor->filename();
    heartbeat.timeMs = timeMs;
    heartbeat.lineNumber = editor->caretY() + 1;
    heartbeat.cursorPosition = editor->caretX() + 1;
    heartbeat.linesInFile = editor->lineCount();
    heartbeat.isWrite = isWrite;
    heartbeat.category = wakaTimeCategory(mBuilding, mDebugging);

    if (mMainWindow && editor->inProject()) {
        const std::shared_ptr<Project> project = mMainWindow->project();
        if (project && project->inProject(heartbeat.entity)) {
            heartbeat.alternateProject = project->name();
            heartbeat.projectFolder = project->directory();
        }
    }
    return heartbeat;
}

void WakaTimeManager::beginCapabilityProbe()
{
    stopCapabilityProbe();
    mProbeComplete = false;
    mCliCapabilities = WakaTimeCliCapabilities{};

    if (mCliPath.isEmpty()) {
        mProbeComplete = true;
        scheduleFlush();
        return;
    }

    const QString cacheKey = QFileInfo(mCliPath).absoluteFilePath();
    const auto cached = mCapabilityCache.constFind(cacheKey);
    if (cached != mCapabilityCache.constEnd()) {
        mCliCapabilities = cached.value();
        mProbeComplete = true;
        scheduleFlush();
        return;
    }

    QProcess *process = new QProcess(this);
    mProbeProcess = process;
    process->setProgram(mCliPath);
    process->setArguments({QStringLiteral("--help")});
    process->setWorkingDirectory(QFileInfo(mCliPath).absolutePath());
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            finishCapabilityProbe(process, QByteArray{});
        }
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, process](int, QProcess::ExitStatus) {
        finishCapabilityProbe(process,
                              process->readAllStandardOutput() + process->readAllStandardError());
    });
    process->start();

    QTimer::singleShot(WAKATIME_CAPABILITY_PROBE_TIMEOUT_MS, this, [this, process] {
        if (mProbeProcess != process) {
            return;
        }
        process->kill();
        finishCapabilityProbe(process,
                              process->readAllStandardOutput() + process->readAllStandardError());
    });
}

void WakaTimeManager::finishCapabilityProbe(QProcess *process, const QByteArray &output)
{
    if (!process || mProbeProcess != process) {
        return;
    }

    mCliCapabilities = WakaTimeCliCapabilities::fromHelpOutput(output);
    mCapabilityCache.insert(QFileInfo(mCliPath).absoluteFilePath(), mCliCapabilities);
    mProbeComplete = true;
    mProbeProcess = nullptr;
    process->disconnect(this);
    process->deleteLater();

    if (!mCliCapabilities.extraHeartbeats) {
        logDebug(tr("WakaTime CLI does not support batch heartbeats; using compatibility mode."));
    }
    scheduleFlush();
}

void WakaTimeManager::stopCapabilityProbe()
{
    if (!mProbeProcess) {
        return;
    }
    QProcess *process = mProbeProcess;
    mProbeProcess = nullptr;
    process->disconnect(this);
    if (process->state() != QProcess::NotRunning) {
        process->kill();
    }
    process->deleteLater();
}

void WakaTimeManager::enqueueHeartbeat(const WakaTimeHeartbeat &heartbeat)
{
    mBuffer.append(heartbeat);
    scheduleFlush();
}

void WakaTimeManager::scheduleFlush()
{
    if (!canSendPending() || !mProbeComplete || mSendProcess
            || mRetryTimer.isActive() || mBuffer.isEmpty()) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (mDraining || mShuttingDown || mBuffer.shouldFlushNow(now, mLastSentAt)) {
        mFlushTimer.start(0);
        return;
    }

    const int delay = mBuffer.nextFlushDelay(now, mLastSentAt);
    if (delay >= 0 && (!mFlushTimer.isActive() || mFlushTimer.remainingTime() > delay)) {
        mFlushTimer.start(delay);
    }
}

void WakaTimeManager::flushPending()
{
    if (!canSendPending() || !mProbeComplete || mSendProcess
            || mRetryTimer.isActive() || mBuffer.isEmpty()) {
        return;
    }

    mFlushTimer.stop();
    if (mDraining || mShuttingDown || mLastSentAt > 0
            || mBuffer.size() >= WakaTimeHeartbeatBuffer::MAX_BATCH_SIZE) {
        mBatchDrainActive = true;
    }
    mInFlightHeartbeats = mBuffer.takeBatch(mCliCapabilities.extraHeartbeats);
    if (mInFlightHeartbeats.isEmpty()) {
        return;
    }

    const WakaTimeCliInvocation invocation = makeWakaTimeCliInvocation(
        mInFlightHeartbeats, mConfigFileName, mPlugin, mCliCapabilities);
    QProcess *process = new QProcess(this);
    mSendProcess = process;
    process->setProgram(mCliPath);
    process->setArguments(invocation.arguments);
    process->setWorkingDirectory(QFileInfo(mInFlightHeartbeats.first().entity).absolutePath());

    connect(process, &QProcess::started, this, [this, process, invocation] {
        if (mSendProcess != process) {
            return;
        }
        mLastSentAt = QDateTime::currentMSecsSinceEpoch();
        mRetryDelayMs = 1000;
        if (!invocation.standardInput.isEmpty()) {
            process->write(invocation.standardInput);
        }
        process->closeWriteChannel();
        mInFlightHeartbeats.clear();
    });
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
        if (mSendProcess != process) {
            return;
        }
        logDebug(tr("WakaTime CLI process error: %1").arg(process->errorString()));
        if (error == QProcess::FailedToStart) {
            finishSendProcess(process, true);
        }
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        if (mSendProcess != process) {
            return;
        }
        const QByteArray stderrOutput = process->readAllStandardError();
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            QString message = tr("WakaTime CLI exited with code %1.").arg(exitCode);
            const QString stderrText = stderrSummary(stderrOutput);
            if (!stderrText.isEmpty()) {
                message += QStringLiteral(" ") + tr("stderr: %1").arg(stderrText);
            }
            logDebug(message);
        } else if (!stderrOutput.trimmed().isEmpty()) {
            logDebug(tr("WakaTime CLI stderr: %1").arg(stderrSummary(stderrOutput)));
        }
        finishSendProcess(process, !mInFlightHeartbeats.isEmpty());
    });
    process->start();
}

void WakaTimeManager::finishSendProcess(QProcess *process, bool requeueInFlight)
{
    if (!process || mSendProcess != process) {
        return;
    }

    if (requeueInFlight && !mInFlightHeartbeats.isEmpty()) {
        mBuffer.prepend(mInFlightHeartbeats);
    }
    mInFlightHeartbeats.clear();
    mSendProcess = nullptr;
    process->disconnect(this);
    process->deleteLater();

    if (requeueInFlight && !mBuffer.isEmpty()) {
        const int retryDelay = mShuttingDown ? 100 : mRetryDelayMs;
        mRetryTimer.start(retryDelay);
        mRetryDelayMs = std::min(mRetryDelayMs * 2, WAKATIME_MAX_RETRY_DELAY_MS);
        return;
    }

    if (mBuffer.isEmpty()) {
        mBatchDrainActive = false;
        mDraining = false;
    } else if (mBatchDrainActive) {
        mFlushTimer.start(0);
    } else {
        scheduleFlush();
    }
}

bool WakaTimeManager::canSendPending() const
{
    return (mEnabled || mDraining || mShuttingDown)
        && !mCliPath.isEmpty()
        && QFileInfo(mCliPath).isFile();
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
