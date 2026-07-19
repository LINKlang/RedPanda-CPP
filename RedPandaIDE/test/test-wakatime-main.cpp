#include "test_wakatimeheartbeat.h"

#include <QCoreApplication>
#include <QTest>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    TestWakaTimeHeartbeat test;
    return QTest::qExec(&test, argc, argv);
}
