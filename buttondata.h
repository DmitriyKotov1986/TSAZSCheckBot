#pragma once

//Qt
#include <QHash>
#include <QString>

class ButtonData
{
public:
    ButtonData(const QString& paramStr = QString());

    void setParam(const QString& paramName, const QString& paramValue);
    const QString& getParam(const QString& paramName) const;

    void fromString(const QString& paramStr);
    QString toString() const;

private:
    QHash<QString, QString> _params;

};
