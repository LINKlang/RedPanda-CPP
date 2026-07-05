#ifndef WAKATIMEMANAGER_H
#define WAKATIMEMANAGER_H

#include <QHash>
#include <QObject>
#include <QSet>

class Editor;
class MainWindow;

class WakaTimeManager: public QObject {
    Q_OBJECT
public:
    explicit WakaTimeManager(MainWindow *mainWindow);

    void attachEditor(Editor *editor);
    void reloadSettings();

private:
    void sendHeartbeat(Editor *editor, bool isWrite);
    bool shouldSkipFile(const QString &filename) const;
    void logDebug(const QString &message) const;
    QString stderrSummary(const QByteArray &stderrOutput) const;

private:
    MainWindow *mMainWindow;
    bool mEnabled;
    bool mDebugEnabled;
    QString mCliPath;
    QString mConfigFileName;
    QString mPlugin;
    QHash<QString, qint64> mLastHeartbeatTimes;
    QSet<Editor*> mAttachedEditors;
};

#endif // WAKATIMEMANAGER_H
