#ifndef WAKATIMEMANAGER_H
#define WAKATIMEMANAGER_H

#include "wakatimeheartbeat.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QTimer>

class Editor;
class MainWindow;
class QProcess;

class WakaTimeManager: public QObject {
    Q_OBJECT
public:
    explicit WakaTimeManager(MainWindow *mainWindow);

    void attachEditor(Editor *editor);
    void reloadSettings();
    void onEditorActivated(Editor *editor);
    void setBuilding(bool building);
    void setDebugging(bool debugging);
    void shutdown(int timeoutMs = 2000);

private:
    void debounceEditorActivity(Editor *editor);
    void appendHeartbeat(Editor *editor, bool isWrite);
    bool shouldSkipFile(const QString &filename) const;
    WakaTimeHeartbeat makeHeartbeat(Editor *editor, bool isWrite, qint64 timeMs) const;

    void beginCapabilityProbe();
    void finishCapabilityProbe(QProcess *process, const QByteArray &output);
    void stopCapabilityProbe();

    void enqueueHeartbeat(const WakaTimeHeartbeat &heartbeat);
    void scheduleFlush();
    void flushPending();
    void finishSendProcess(QProcess *process, bool requeueInFlight);
    bool canSendPending() const;

    void logDebug(const QString &message) const;
    QString stderrSummary(const QByteArray &stderrOutput) const;

private:
    MainWindow *mMainWindow;
    bool mEnabled;
    bool mDebugEnabled;
    bool mBuilding {false};
    bool mDebugging {false};
    bool mDraining {false};
    bool mBatchDrainActive {false};
    bool mShuttingDown {false};
    bool mProbeComplete {false};
    QString mCliPath;
    QString mConfigFileName;
    QString mPlugin;

    WakaTimeHeartbeatPolicy mPolicy;
    WakaTimeHeartbeatBuffer mBuffer;
    QList<WakaTimeHeartbeat> mInFlightHeartbeats;
    WakaTimeCliCapabilities mCliCapabilities;
    QHash<QString, WakaTimeCliCapabilities> mCapabilityCache;

    QTimer mActivityDebounceTimer;
    QTimer mFlushTimer;
    QTimer mRetryTimer;
    QPointer<Editor> mPendingActivityEditor;
    QProcess *mSendProcess {nullptr};
    QProcess *mProbeProcess {nullptr};
    qint64 mLastSentAt {0};
    int mRetryDelayMs {1000};

    QSet<Editor*> mAttachedEditors;
};

#endif // WAKATIMEMANAGER_H
