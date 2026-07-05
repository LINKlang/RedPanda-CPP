#ifndef TOOLSWAKATIMEWIDGET_H
#define TOOLSWAKATIMEWIDGET_H

#include "settingswidget.h"

#include "../settings/wakatimesettings.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QToolButton;

class ToolsWakaTimeWidget : public SettingsWidget
{
    Q_OBJECT

public:
    explicit ToolsWakaTimeWidget(const QString& name, const QString& group, IconsManager *iconsManager, QWidget *parent = nullptr);

protected:
    void doLoad() override;
    void doSave() override;
    void updateIcons(const QSize &size) override;

private:
    QList<WakaTimeApiUrlRule> rules() const;
    void setRules(const QList<WakaTimeApiUrlRule> &rules);
    void addRuleRow(const WakaTimeApiUrlRule &rule);
    void swapRows(int first, int second);
    void testCli();

private:
    QCheckBox *mEnabledCheckBox;
    QLineEdit *mCliPathEdit;
    QToolButton *mBrowseCliButton;
    QPushButton *mTestButton;
    QLineEdit *mApiUrlEdit;
    QLineEdit *mApiKeyEdit;
    QCheckBox *mDebugCheckBox;
    QLabel *mTestResultLabel;
    QTableWidget *mRulesTable;
    QPushButton *mAddRuleButton;
    QPushButton *mRemoveRuleButton;
    QPushButton *mMoveRuleUpButton;
    QPushButton *mMoveRuleDownButton;
};

#endif // TOOLSWAKATIMEWIDGET_H
