#ifndef TEST_WAKATIMEHEARTBEAT_H
#define TEST_WAKATIMEHEARTBEAT_H

#include <QObject>

class TestWakaTimeHeartbeat: public QObject
{
    Q_OBJECT
private slots:
    void testActivityChanges();
    void testHeartbeatIntervalAndFileChanges();
    void testWriteDedupeAndSlidingWindow();
    void testCategoryPriorityAndChanges();
    void testCapabilities();
    void testCliSerialization();
    void testHeartbeatBuffer();
};

#endif // TEST_WAKATIMEHEARTBEAT_H
