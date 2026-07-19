#ifndef TEST_EDITOR_AUTOSAVE_H
#define TEST_EDITOR_AUTOSAVE_H

#include "test_editor_base.h"

class TestEditorAutoSave: public TestEditorBase
{
    Q_OBJECT
private slots:
    void testSuccessfulSaveClearsAutoSaveFlag();
};

#endif // TEST_EDITOR_AUTOSAVE_H
