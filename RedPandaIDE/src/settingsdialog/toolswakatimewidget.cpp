#include "toolswakatimewidget.h"

#include "../iconsmanager.h"
#include "../mainwindow.h"
#include "../settings.h"
#include "../systemconsts.h"
#include "../utils.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

class PasswordItemDelegate: public QStyledItemDelegate {
public:
    explicit PasswordItemDelegate(QObject *parent = nullptr): QStyledItemDelegate(parent) {}

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QLineEdit *editor = qobject_cast<QLineEdit*>(QStyledItemDelegate::createEditor(parent, option, index));
        if (editor) {
            editor->setEchoMode(QLineEdit::Password);
        }
        return editor;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem optionCopy(option);
        initStyleOption(&optionCopy, index);
        if (!optionCopy.text.isEmpty()) {
            optionCopy.text = QString(optionCopy.text.length(), QLatin1Char('*'));
        }
        QStyledItemDelegate::paint(painter, optionCopy, index);
    }
};

ToolsWakaTimeWidget::ToolsWakaTimeWidget(const QString &name, const QString &group, IconsManager *iconsManager, QWidget *parent):
    SettingsWidget(name, group, iconsManager, parent),
    mEnabledCheckBox(new QCheckBox(tr("Enable WakaTime"), this)),
    mCliPathEdit(new QLineEdit(this)),
    mBrowseCliButton(new QToolButton(this)),
    mTestButton(new QPushButton(tr("Test"), this)),
    mApiUrlEdit(new QLineEdit(this)),
    mApiKeyEdit(new QLineEdit(this)),
    mDebugCheckBox(new QCheckBox(tr("Debug output to Tools Output"), this)),
    mTestResultLabel(new QLabel(this)),
    mRulesTable(new QTableWidget(this)),
    mAddRuleButton(new QPushButton(tr("Add"), this)),
    mRemoveRuleButton(new QPushButton(tr("Remove"), this)),
    mMoveRuleUpButton(new QPushButton(tr("Up"), this)),
    mMoveRuleDownButton(new QPushButton(tr("Down"), this))
{
    mBrowseCliButton->setText(QStringLiteral("..."));
    mBrowseCliButton->setToolTip(tr("Browse"));
    mApiKeyEdit->setEchoMode(QLineEdit::Password);
    mTestResultLabel->setVisible(false);
    mTestResultLabel->setWordWrap(true);

    QGridLayout *basicLayout = new QGridLayout();
    basicLayout->addWidget(mEnabledCheckBox, 0, 0, 1, 4);
    basicLayout->addWidget(new QLabel(tr("WakaTime CLI:"), this), 1, 0);
    basicLayout->addWidget(mCliPathEdit, 1, 1);
    basicLayout->addWidget(mBrowseCliButton, 1, 2);
    basicLayout->addWidget(mTestButton, 1, 3);
    basicLayout->addWidget(new QLabel(tr("Default API URL:"), this), 2, 0);
    basicLayout->addWidget(mApiUrlEdit, 2, 1, 1, 3);
    basicLayout->addWidget(new QLabel(tr("Default API Key:"), this), 3, 0);
    basicLayout->addWidget(mApiKeyEdit, 3, 1, 1, 3);
    basicLayout->addWidget(mDebugCheckBox, 4, 0, 1, 4);
    basicLayout->addWidget(mTestResultLabel, 5, 0, 1, 4);
    basicLayout->setColumnStretch(1, 1);

    QGroupBox *basicGroup = new QGroupBox(tr("Settings"), this);
    basicGroup->setLayout(basicLayout);

    mRulesTable->setColumnCount(3);
    mRulesTable->setHorizontalHeaderLabels({tr("Regex Pattern"), tr("API URL"), tr("API Key")});
    mRulesTable->horizontalHeader()->setStretchLastSection(true);
    mRulesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    mRulesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    mRulesTable->verticalHeader()->setVisible(false);
    mRulesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mRulesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mRulesTable->setItemDelegateForColumn(2, new PasswordItemDelegate(mRulesTable));

    QHBoxLayout *rulesButtonsLayout = new QHBoxLayout();
    rulesButtonsLayout->addWidget(mAddRuleButton);
    rulesButtonsLayout->addWidget(mRemoveRuleButton);
    rulesButtonsLayout->addWidget(mMoveRuleUpButton);
    rulesButtonsLayout->addWidget(mMoveRuleDownButton);
    rulesButtonsLayout->addStretch();

    QVBoxLayout *rulesLayout = new QVBoxLayout();
    rulesLayout->addWidget(mRulesTable);
    rulesLayout->addLayout(rulesButtonsLayout);

    QGroupBox *rulesGroup = new QGroupBox(tr("API URLs"), this);
    rulesGroup->setLayout(rulesLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(basicGroup);
    mainLayout->addWidget(rulesGroup);

    connect(mBrowseCliButton, &QToolButton::clicked, this, [this] {
        QString filename = QFileDialog::getOpenFileName(
                    this,
                    tr("WakaTime CLI Executable"),
                    QString(),
                    tr("All files (%1)").arg(ALL_FILE_WILDCARD));
        if (!filename.isEmpty() && fileExists(filename)) {
            mCliPathEdit->setText(filename);
        }
    });
    connect(mTestButton, &QPushButton::clicked, this, &ToolsWakaTimeWidget::testCli);
    connect(mAddRuleButton, &QPushButton::clicked, this, [this] {
        addRuleRow({});
        mRulesTable->setCurrentCell(mRulesTable->rowCount() - 1, 0);
    });
    connect(mRemoveRuleButton, &QPushButton::clicked, this, [this] {
        const int row = mRulesTable->currentRow();
        if (row >= 0) {
            mRulesTable->removeRow(row);
        }
    });
    connect(mMoveRuleUpButton, &QPushButton::clicked, this, [this] {
        const int row = mRulesTable->currentRow();
        if (row > 0) {
            swapRows(row, row - 1);
            mRulesTable->setCurrentCell(row - 1, 0);
        }
    });
    connect(mMoveRuleDownButton, &QPushButton::clicked, this, [this] {
        const int row = mRulesTable->currentRow();
        if (row >= 0 && row + 1 < mRulesTable->rowCount()) {
            swapRows(row, row + 1);
            mRulesTable->setCurrentCell(row + 1, 0);
        }
    });
}

void ToolsWakaTimeWidget::doLoad()
{
    mEnabledCheckBox->setChecked(pSettings->wakatime().enabled());
    mCliPathEdit->setText(pSettings->wakatime().cliPath());
    mApiUrlEdit->setText(pSettings->wakatime().apiUrl());
    mApiKeyEdit->setText(pSettings->wakatime().apiKey());
    mDebugCheckBox->setChecked(pSettings->wakatime().debugEnabled());
    setRules(pSettings->wakatime().apiUrlRules());
    mTestResultLabel->setVisible(false);
}

void ToolsWakaTimeWidget::doSave()
{
    WakaTimeSettings &settings = pSettings->wakatime();
    settings.setEnabled(mEnabledCheckBox->isChecked());
    settings.setCliPath(mCliPathEdit->text());
    settings.setApiUrl(mApiUrlEdit->text());
    settings.setApiKey(mApiKeyEdit->text());
    settings.setDebugEnabled(mDebugCheckBox->isChecked());
    settings.setApiUrlRules(rules());
    settings.save();
    pMainWindow->applySettings();
}

void ToolsWakaTimeWidget::updateIcons(const QSize &/*size*/)
{
    iconsManager()->setIcon(mBrowseCliButton, IconsManager::ACTION_FILE_OPEN_FOLDER);
}

QList<WakaTimeApiUrlRule> ToolsWakaTimeWidget::rules() const
{
    QList<WakaTimeApiUrlRule> result;
    for (int row = 0; row < mRulesTable->rowCount(); ++row) {
        WakaTimeApiUrlRule rule;
        rule.pattern = mRulesTable->item(row, 0) ? mRulesTable->item(row, 0)->text() : QString();
        rule.apiUrl = mRulesTable->item(row, 1) ? mRulesTable->item(row, 1)->text() : QString();
        rule.apiKey = mRulesTable->item(row, 2) ? mRulesTable->item(row, 2)->text() : QString();
        result.append(rule);
    }
    return result;
}

void ToolsWakaTimeWidget::setRules(const QList<WakaTimeApiUrlRule> &rules)
{
    mRulesTable->setRowCount(0);
    for (const WakaTimeApiUrlRule &rule: rules) {
        addRuleRow(rule);
    }
}

void ToolsWakaTimeWidget::addRuleRow(const WakaTimeApiUrlRule &rule)
{
    const int row = mRulesTable->rowCount();
    mRulesTable->insertRow(row);
    mRulesTable->setItem(row, 0, new QTableWidgetItem(rule.pattern));
    mRulesTable->setItem(row, 1, new QTableWidgetItem(rule.apiUrl));
    mRulesTable->setItem(row, 2, new QTableWidgetItem(rule.apiKey));
}

void ToolsWakaTimeWidget::swapRows(int first, int second)
{
    for (int column = 0; column < mRulesTable->columnCount(); ++column) {
        QTableWidgetItem *firstItem = mRulesTable->takeItem(first, column);
        QTableWidgetItem *secondItem = mRulesTable->takeItem(second, column);
        mRulesTable->setItem(first, column, secondItem);
        mRulesTable->setItem(second, column, firstItem);
    }
}

void ToolsWakaTimeWidget::testCli()
{
    QFileInfo fileInfo(mCliPathEdit->text());
    mTestResultLabel->setVisible(true);
    if (!fileInfo.exists()) {
        mTestResultLabel->setText(tr("WakaTime CLI does not exist."));
        return;
    }

    mTestButton->setEnabled(false);
    mTestResultLabel->setText(tr("Testing..."));
    QProcess *process = new QProcess(this);
    process->setProgram(fileInfo.absoluteFilePath());
    process->setArguments({QStringLiteral("--version")});
    process->setWorkingDirectory(fileInfo.absolutePath());
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
        mTestResultLabel->setText(process->errorString());
        mTestButton->setEnabled(true);
        process->deleteLater();
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        QString output = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
        const QString errorOutput = QString::fromUtf8(process->readAllStandardError()).trimmed();
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            output = tr("WakaTime CLI exited with code %1.").arg(exitCode);
            if (!errorOutput.isEmpty()) {
                output += QStringLiteral(" ") + errorOutput;
            }
        } else if (output.isEmpty()) {
            output = tr("WakaTime CLI is valid.");
        }
        mTestResultLabel->setText(output);
        mTestButton->setEnabled(true);
        process->deleteLater();
    });
    process->start();
}
