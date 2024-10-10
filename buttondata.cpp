#include "buttondata.h"

ButtonData::ButtonData(const QString& paramStr /* = QString()*/)
{
    if (!paramStr.isEmpty())
    {
        fromString(paramStr);
    }
}

void ButtonData::setParam(const QString &paramName, const QString &paramValue)
{
    Q_ASSERT(!paramName.isEmpty());

    _params.insert(paramName, paramValue);
}

Q_GLOBAL_STATIC(QString, defaultValue, QString());

const QString &ButtonData::getParam(const QString &paramName) const
{
    const auto params_it = _params.find(paramName);
    if (params_it == _params.end())
    {
        return *defaultValue;
    }

    return params_it.value();
}

void ButtonData::fromString(const QString &paramStr)
{
    _params.clear();

    QStringList paramList = paramStr.split(u';');

    for (const auto& param: paramList)
    {
        QStringList paramNameValue = param.split('=');

        if (paramNameValue.empty())
        {
            continue;
        }

        const auto name = paramNameValue[0];
        if (name.isEmpty())
        {
            continue;
        }

        QString value;
        if (paramNameValue.size() > 1)
        {
            value = paramNameValue[1];
        }

        _params.insert(name, value);
    }
}

QString ButtonData::toString() const
{
    QString result;
    for (auto params_it = _params.begin(); params_it != _params.end(); ++params_it)
    {
        result += QString("%1=%2;").arg(params_it.key()).arg(params_it.value());
    }

    if (!result.isEmpty())
    {
        result.removeLast();
    }

    return result;
}

bool ButtonData::isExist(const QString &paramName) const
{
    return _params.contains(paramName);
}

bool ButtonData::isEmpty() const
{
    return _params.empty();
}

qsizetype ButtonData::count() const
{
    return _params.size();
}
