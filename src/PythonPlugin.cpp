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

#include "PythonPlugin.h"
#include "AboutDialog.h"
#include "CodeEditor/PythonEditor.h"
#include "FileRunner.h"
#include "PackageManager.h"
#include "PythonActionLauncher.h"
#include "PythonPluginSettings.h"
#include "PythonRepl.h"
#include "PythonStdErrOutRedirect.h"
#include "Resources.h"
#include "Runtime/Runtime.h"
#include "Utilities.h"

#include <QDesktopServices>
#include <QUrl>

#define slots Q_SLOTS
#define signals Q_SIGNALS
#include <ccCommandLineInterface.h>
#include <QFileDialog>
#include <QDialog>
#include <QSettings>
#include <functional>
#include <QFile>
#include <algorithm>

// Useful link:
// https://docs.python.org/3/c-api/init.html#initialization-finalization-and-threads
PythonPlugin::PythonPlugin(QObject *parent)
    : QObject(parent),
      ccStdPluginInterface(":/CC/plugin/PythonPlugin/info.json"),
      m_settings(new PythonPluginSettings),
      m_interp(nullptr),
      m_editor(new PythonEditor(&m_interp)),
      m_fileRunner(new FileRunner(&m_interp)),
      m_actionLauncher(new PythonActionLauncher(&m_pluginManager, &m_interp))
{
    PythonConfig config;
    bool isDefaultPythonEnv;

    if (PythonConfig::IsInsideEnvironment())
    {
        config = PythonConfig::fromContainingEnvironment();
        isDefaultPythonEnv = false;
        plgPrint() << "CloudCompare was loaded from within a " << config.type() << "env. "
                   << "Will try to use it";
    }
    else
    {
        config = m_settings->pythonEnvConfig();
        isDefaultPythonEnv = m_settings->isDefaultPythonEnv();
    }

    if (!isDefaultPythonEnv)
    {
        bool seemsValid = config.validateAndDisplayErrors();
        if (!seemsValid)
        {
            plgError() << "Falling back to default Python configuration due to previous errors";
            config.initDefault();
        }
    }

    try
    {
        m_interp.initialize(config);
    }
    catch (const std::exception &e)
    {
        plgError() << "Failed to initialize Python: " << e.what();
        return;
    }

    m_config = config;

    connect(QCoreApplication::instance(),
            &QCoreApplication::aboutToQuit,
            this,
            &PythonPlugin::finalizeInterpreter);
}

static std::unique_ptr<QSettings> LoadSettings()
{
    return std::make_unique<QSettings>(
        QCoreApplication::organizationName(),
        QCoreApplication::applicationName().append(":PythonPlugin.Settings"));
}

void PythonPlugin::stop() {

    //On software exit, the script list needs to be saved in a txt file
    std::unique_ptr<QSettings> settings = LoadSettings();
    settings->setValue(QStringLiteral("RegisterListPath"), m_savedPath);
}

QList<QAction *> PythonPlugin::getActions()
{
    const bool isPythonProperlyInitialized = PythonInterpreter::IsInitialized();

    if (!m_showEditor)
    {
        m_showEditor = new QAction("Show Editor", this);
        m_showEditor->setToolTip("Show the code editor window");
        m_showEditor->setIcon(QIcon(EDITOR_ICON_PATH));
        connect(m_showEditor, &QAction::triggered, this, &PythonPlugin::showEditor);
        m_showEditor->setEnabled(isPythonProperlyInitialized);
    }

    if (!m_showRepl)
    {
        m_showRepl = new QAction("Show REPL", this);
        m_showRepl->setToolTip("Show the Python REPL");
        m_showRepl->setIcon(QIcon(REPL_ICON_PATH));
        connect(m_showRepl, &QAction::triggered, this, &PythonPlugin::showRepl);
        m_showRepl->setEnabled(isPythonProperlyInitialized);
    }

    if (!m_showDoc)
    {
        m_showDoc = new QAction("Show Documentation", this);
        m_showDoc->setToolTip("Open online documentation in your web browser");
        m_showDoc->setIcon(QIcon(DOCUMENTATION_ICON_PATH));
        connect(m_showDoc, &QAction::triggered, &PythonPlugin::showDocumentation);
        m_showDoc->setEnabled(isPythonProperlyInitialized);
    }

    if (!m_showAboutDialog)
    {
        m_showAboutDialog = new QAction("About", this);
        m_showAboutDialog->setToolTip("About this plugin");
        m_showAboutDialog->setIcon(QIcon(ABOUT_ICON_PATH));
        connect(m_showAboutDialog, &QAction::triggered, this, &PythonPlugin::showAboutDialog);
        m_showAboutDialog->setEnabled(isPythonProperlyInitialized);
    }

    if (!m_showFileRunner)
    {
        m_showFileRunner = new QAction("File Runner", this);
        m_showFileRunner->setToolTip("Small widget to select and run a script");
        m_showFileRunner->setIcon(QIcon(RUNNER_ICON_PATH));
        connect(m_showFileRunner, &QAction::triggered, this, &PythonPlugin::showFileRunner);
        m_showFileRunner->setEnabled(isPythonProperlyInitialized);
    }

    if (!m_showPackageManager)
    {
        m_showPackageManager = new QAction("Package Manager", this);
        m_showPackageManager->setToolTip("Manage packages with pip");
        m_showPackageManager->setIcon(QIcon(PACKAGE_MANAGER_ICON_PATH));
        connect(m_showPackageManager, &QAction::triggered, this, &PythonPlugin::showPackageManager);
        m_showPackageManager->setEnabled(isPythonProperlyInitialized);
    }

    if (!m_showActionLauncher)
    {
        m_showActionLauncher = new QAction("Show Action Launcher", this);
        m_showActionLauncher->setIcon(QIcon(ACTION_LAUNCHER_ICON_PATH));
        m_showActionLauncher->setToolTip("Launch actions of custom Python plugins");
        connect(m_showActionLauncher,
                &QAction::triggered,
                this,
                &PythonPlugin::showPythonActionLauncher);
        m_showActionLauncher->setEnabled(isPythonProperlyInitialized);
    }

    if (!m_showSettings)
    {
        m_showSettings = new QAction("Show Settings", this);
        m_showSettings->setIcon(QIcon(SETTINGS_ICON_PATH));
        m_showSettings->setToolTip("Show some settings");
        connect(m_showSettings, &QAction::triggered, this, &PythonPlugin::showSettings);
        // Settings do not need Python to be initialized in a valid state
        m_showSettings->setEnabled(true);
    }
    if (!m_drawScriptRegister) 
    {
        m_drawScriptRegister = new QMenu("Script Register");
        m_drawScriptRegister->setToolTip("Show all registered script");
        m_drawScriptRegister->setEnabled(true);

        m_addScript = new QAction("Add Script");
        m_addScript->setToolTip("Add Script");
        connect(m_addScript, &QAction::triggered, this, &PythonPlugin::addScriptAction);
        m_addScript->setEnabled(true);

        m_removeScript = new QMenu("Remove Script");
        m_removeScript->setToolTip("Remove Script");
        m_removeScript->setEnabled(false);

        m_drawScriptRegister->addAction(m_addScript);
        m_drawScriptRegister->addMenu(m_removeScript);
        m_drawScriptRegister->addSeparator();

        std::unique_ptr<QSettings> settings = LoadSettings();
        QStringList loaded_paths = settings->value(QStringLiteral("RegisterListPath")).value<QStringList>();

        for (QString path : loaded_paths)
        {
            QFileInfo fi(path);
            if (!fi.exists())
                plgPrint() << "Script registered \"" << path << "\" doesn't exist.";
            else
                addScript(path);
        }
    }

    return {m_showEditor,
            m_showFileRunner,
            m_showAboutDialog,
            m_showDoc,
            m_showRepl,
            m_showPackageManager,
            m_showActionLauncher,
            m_showSettings,
            m_drawScriptRegister->menuAction()};
}

void PythonPlugin::showRepl()
{
    if (m_repl)
    {
        m_repl->show();
        m_repl->raise();
        m_repl->activateWindow();
    }
    else
    {
        m_repl = new PythonRepl(&m_interp);
        m_repl->show();
    }
}

void PythonPlugin::showEditor() const
{
    if (m_editor)
    {
        m_editor->show();
        m_editor->raise();
        m_editor->activateWindow();
    }
}

void PythonPlugin::addScriptAction() 
{
    if(m_scriptList.empty())
        m_removeScript->setEnabled(true);

    QString filePath = QFileDialog::getOpenFileName(m_drawScriptRegister,
                                              QStringLiteral("Select Python Script"),
                                              QString(),
                                              QStringLiteral("Python Script (*.py)"));
    addScript(filePath);
}

void PythonPlugin::addScript(QString path) {
    if(m_scriptList.empty())
        m_removeScript->setEnabled(true);

    QFileInfo fi(path);

    //Doesn't add if file doesn't exist or if it's already present.
    if (!fi.exists() || m_savedPath.contains(path))
        return;

    QString fileName = fi.baseName();  

    auto* newScript= new QAction(fileName);
    newScript->setToolTip(fileName);
    auto* removeNewScript = new QAction(fileName);
    removeNewScript->setToolTip(fileName);

    connect(newScript, &QAction::triggered, [this,path]()
    {
        executeScript(path);
    });
    newScript->setEnabled(true);
    connect(removeNewScript, &QAction::triggered, [this,fileName,removeNewScript,path]()
    {
        removeScript(fileName,removeNewScript);

        //prepare to save script list
        auto pos = std::find(m_savedPath.begin(), m_savedPath.end(), path);
        if (pos != m_savedPath.end()){
            m_savedPath.erase(pos);
        }
    });
    removeNewScript->setEnabled(true);

    m_scriptList.insert({fileName, newScript});
    m_drawScriptRegister->addAction(newScript);
    m_removeScript->addAction(removeNewScript);

    //prepare to save script list
    m_savedPath.push_back(path);
}

void PythonPlugin::executeScript(QString path)
{
    const std::string path_str = path.toStdString();
    m_interp.executeFile(path_str);
}

void PythonPlugin::removeScript(QString name, QAction* self)
{
    QAction* script = m_scriptList[name];
    m_drawScriptRegister->removeAction(script);
    m_removeScript->removeAction(self);
    m_scriptList.erase(name);
    delete script;
    delete self;
    if(m_scriptList.empty())
        m_removeScript->setEnabled(false);
}

void PythonPlugin::showFileRunner() const
{
    m_fileRunner->show();
}

void PythonPlugin::showDocumentation()
{
    const QUrl url(QString("https://tmontaigu.github.io/CloudCompare-PythonPlugin/index.html"));
    QDesktopServices::openUrl(url);
}

void PythonPlugin::showAboutDialog() const
{
    AboutDialog dlg(m_app->getMainWindow());
    dlg.exec();
}

void PythonPlugin::showPackageManager()
{
    if (m_packageManager == nullptr)
    {
        m_packageManager = new PackageManager(m_config);
    }
    m_packageManager->show();
    m_editor->raise();
    m_editor->activateWindow();
}

void PythonPlugin::showPythonActionLauncher() const
{
    m_actionLauncher->show();
}

void PythonPlugin::showSettings() const
{
    m_settings->show();
}

PythonPlugin::~PythonPlugin() noexcept
{
    Runtime::unsetMainAppInterfaceInstance();
    Runtime::unsetCmdLineInterfaceInstance();
}

struct PythonPluginCommand final : public ccCommandLineInterface::Command
{
    explicit PythonPluginCommand(PythonInterpreter *interpreter_)
        : Command("PYTHON", "PYTHON_SCRIPT"), interpreter(interpreter_)
    {
    }

    bool process(ccCommandLineInterface &cmd) override
    {
        Q_ASSERT(interpreter);
        cmd.print("[PythonPlugin] Starting");
        Args args;
        if (!args.parseFrom(cmd))
        {
            return false;
        }

        if (!Py_IsInitialized())
        {
            return cmd.error("[PythonPlugin] Python is not properly initialized");
        }

        PySys_SetArgvEx(static_cast<int>(args.pythonArgv.size()), args.pythonArgv.data(), 1);
        const bool success = interpreter->executeFile(qPrintable(args.filepath));

        cmd.print(QString("[PythonPlugin] Script %1 executed")
                      .arg(success ? "successfully" : "unsuccessfully"));
        return success;
    }

    struct Args
    {
        QString filepath;
        std::vector<wchar_t *> pythonArgv{};

        Args() = default;

        bool parseFrom(ccCommandLineInterface &cmd)
        {
            if (cmd.arguments().empty())
            {
                return cmd.error(QString("Missing parameter: parameters filename after \"-%1\"")
                                     .arg("PYTHON_SCRIPT"));
            }
            filepath = cmd.arguments().takeFirst();

            pythonArgv.reserve(cmd.arguments().size() + 1);
            pythonArgv.push_back(QStringToWcharArray(filepath));
            while (!cmd.arguments().isEmpty())
            {
                pythonArgv.push_back(QStringToWcharArray(cmd.arguments().takeFirst()));
            }
            return true;
        }

        virtual ~Args()
        {
            for (wchar_t *arg : pythonArgv)
            {
                delete[] arg;
            }
        }
    };

    PythonInterpreter *interpreter{nullptr};
};

void PythonPlugin::registerCommands(ccCommandLineInterface *cmd)
{
    cmd->registerCommand(
        ccCommandLineInterface::Command::Shared(new PythonPluginCommand(&m_interp)));
    Runtime::setCmdLineInterfaceInstance(cmd);
}

// We use this as our last step in the initialization process
// since many of our stuff relies on the app interface to be valid
void PythonPlugin::setMainAppInterface(ccMainAppInterface *app)
{
    LogPythonHome();
    LogPythonPath();

    ccStdPluginInterface::setMainAppInterface(app);
    Runtime::setMainAppInterfaceInstance(m_app);

    // Now that the mainAppInterface is set, we can load custom
    // python plugins, if we did this earlier, `pycc.GetInstance()`
    // in python would return `None` and that's bad.
    try
    {
        m_pluginManager.loadPluginsFrom(m_settings->pluginsPaths());
    }
    catch (const std::exception &e)
    {
        ccLog::Warning("[PythonPlugin] Failed to load custom python plugins: %e", e.what());
    }

    m_fileRunner->setParent(m_app->getMainWindow(), Qt::Window);
    m_actionLauncher->setParent(m_app->getMainWindow(), Qt::Window);
    m_settings->setParent(m_app->getMainWindow(), Qt::Window);
}

void PythonPlugin::finalizeInterpreter()
{
    // We have to clear registered functions before
    // we finalize the interpreter otherwise our references to
    // python object would outlive the interpreter
    m_pluginManager.unloadPlugins();
    m_interp.finalize();
}
