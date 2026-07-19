#include "test_editor_autosave.h"

#include "src/editor.h"

#include <QDir>
#include <QTemporaryFile>
#include <QTest>

void TestEditorAutoSave::testSuccessfulSaveClearsAutoSaveFlag()
{
    QTemporaryFile file(QDir::current().filePath("test-editor-autosave-XXXXXX.cpp"));
    QVERIFY2(file.open(), qPrintable(file.errorString()));
    QCOMPARE(file.write("int main() {}\n"), qint64(14));
    const QString filename = file.fileName();
    file.close();

    mEditor->setFilename(filename);
    mEditor->loadFile(filename, false);
    mEditor->setCaretXY(mEditor->fileEnd());
    mEditor->setSelText(QStringLiteral("// first change\n"));
    QVERIFY(mEditor->canAutoSave());
    QVERIFY(mEditor->save());
    QVERIFY(!mEditor->canAutoSave());

    mEditor->setCaretXY(mEditor->fileEnd());
    mEditor->setSelText(QStringLiteral("// changed again\n"));
    QVERIFY(mEditor->canAutoSave());
}
