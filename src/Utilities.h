//##########################################################################
//#                                                                        #
//#                CLOUDCOMPARE PLUGIN: PythonPlugin                       #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 of the License.               #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#                   COPYRIGHT: Thomas Montaigu                           #
//#                                                                        #
//##########################################################################

#ifndef PYTHON_PLUGIN_UTILITIES_H
#define PYTHON_PLUGIN_UTILITIES_H

#include <QString>

#include <Python.h>

#include <ccLog.h>

template <enum ccLog::MessageLevelFlags level> class ccLogger
{
  public:
    ccLogger() : m_message()
    {
        m_message.reserve(255);
    }

    virtual ~ccLogger()
    {
        flush();
    }

    inline ccLogger &operator<<(const QString &message)
    {
        m_message += message;
        return *this;
    }

    inline ccLogger &operator<<(const char *message)
    {
        m_message += message;
        return *this;
    }

    inline ccLogger &operator<<(int value)
    {
        m_message += QString::number(value);
        return *this;
    }

    void flush()
    {
        ccLog::LogMessage(m_message, level);
        m_message.clear();
    }

  protected:
    QString m_message;
};

using ccPrint = ccLogger<ccLog::LOG_STANDARD>;
using ccDebug = ccLogger<ccLog::LOG_DEBUG>;
using ccWarning = ccLogger<ccLog::LOG_WARNING>;
using ccError = ccLogger<ccLog::LOG_ERROR>;

template <enum ccLog::MessageLevelFlags level> class PluginLogger : public ccLogger<level>
{
  public:
    PluginLogger() : ccLogger<level>()
    {
        this->m_message += "[PythonPlugin] ";
    }

    //    friend PluginLogger& endl(PluginLogger<level>& logger) {
    //        logger.flush();
    //        return logger;
    //    }
};

using plgPrint = PluginLogger<ccLog::LOG_STANDARD>;
using plgDebug = PluginLogger<ccLog::LOG_DEBUG>;
using plgWarning = PluginLogger<ccLog::LOG_WARNING>;
using plgError = PluginLogger<ccLog::LOG_ERROR>;

/// Returns a newly allocated array (null terminated) from a QString
inline wchar_t *QStringToWcharArray(const QString &string)
{
    auto *wcharArray = new wchar_t[string.size() + 1];
    const int len = string.toWCharArray(wcharArray);
    Q_ASSERT(len <= string.size());
    wcharArray[len] = '\0';
    return wcharArray;
}

inline void LogPythonPath()
{
    const wchar_t *pythonPath = Py_GetPath();
    if (pythonPath != nullptr)
    {
        size_t errPos{0};
        char *cPythonPath = Py_EncodeLocale(pythonPath, &errPos);
        if (cPythonPath)
        {
            ccLog::Print("[PythonPlugin] PythonPath is set to: %s", cPythonPath);
            PyMem_Free(cPythonPath);
        }
        else
        {
            ccLog::Print("[PythonPlugin] Failed to convert the PythonPath");
        }
    }
    else
    {
        ccLog::Print("[PythonPlugin] PythonPath is not set");
    }
}

inline void LogPythonHome()
{
    const wchar_t *pythonHome = Py_GetPythonHome();
    if (pythonHome != nullptr)
    {
        size_t errPos{0};
        char *cPythonHome = Py_EncodeLocale(pythonHome, &errPos);
        if (cPythonHome)
        {
            ccLog::Print("[PythonPlugin] PythonHome is set to: %s", cPythonHome);
            PyMem_Free(cPythonHome);
        }
        else
        {
            ccLog::Print("[PythonPlugin]Failed to convert the PythonHome path");
        }
    }
    else
    {
        ccLog::Print("[PythonPlugin] PythonHome is not set");
    }
}

#endif // PYTHON_PLUGIN_UTILITIES_H
