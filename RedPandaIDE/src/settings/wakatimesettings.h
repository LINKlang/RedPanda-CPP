#ifndef WAKATIME_SETTINGS_H
#define WAKATIME_SETTINGS_H

#include "basesettings.h"

#include <QList>

#define SETTING_WAKATIME "WakaTime"

struct WakaTimeApiUrlRule {
    QString pattern;
    QString apiUrl;
    QString apiKey;
};

class WakaTimeSettings: public BaseSettings {
public:
    static const QString DEFAULT_API_URL;

    explicit WakaTimeSettings(SettingsPersistor *persistor);

    bool enabled() const;
    void setEnabled(bool newEnabled);
    const QString &cliPath() const;
    void setCliPath(const QString &newCliPath);
    const QString &apiKey() const;
    void setApiKey(const QString &newApiKey);
    const QString &apiUrl() const;
    void setApiUrl(const QString &newApiUrl);
    const QList<WakaTimeApiUrlRule> &apiUrlRules() const;
    void setApiUrlRules(const QList<WakaTimeApiUrlRule> &newApiUrlRules);
    bool debugEnabled() const;
    void setDebugEnabled(bool newDebugEnabled);

    QString managedConfigFileName() const;
    void saveManagedConfig() const;

protected:
    void doSave() override;
    void doLoad() override;

private:
    QString managedConfigText() const;

private:
    bool mEnabled;
    QString mCliPath;
    QString mApiKey;
    QString mApiUrl;
    QList<WakaTimeApiUrlRule> mApiUrlRules;
    bool mDebugEnabled;
};

#endif // WAKATIME_SETTINGS_H
