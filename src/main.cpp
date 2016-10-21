#include <QtGlobal>
#include <QApplication>
#include <QTreeWidget>
#include <QMessageBox>
#include <QIcon>
#include <QDir>
#include <QPixmapCache>
#include <QTranslator>
#include <QProcess>
#include <QSettings>
#include <QDesktopServices>
#include <QSslConfiguration>
#include <QNetworkProxy>
#include <QtWebEngine>

#include <sys/stat.h>

#include "utils/pathresolve.h"
#include "utils/logger.h"
#include "utils/stylehelper.h"
#include "share/wizsettings.h"
#include "share/wizwin32helper.h"
#include "share/wizDatabaseManager.h"
#include "share/wizSingleApplication.h"
#include "share/wizthreads.h"
#include "share/wizGlobal.h"

#include "core/wizNoteManager.h"

#ifdef Q_OS_MAC
#include "mac/wizmachelper.h"
#include "mac/wizIAPHelper.h"
#endif

#include "sync/token.h"
#include "sync/apientry.h"
#include "sync/avatar.h"
#include "thumbcache.h"
#include "wizmainwindow.h"
#include "wizDocumentWebEngine.h"
#include "wizLoginDialog.h"


#ifdef Q_OS_MAC
#  define SHARE_PATH "/../Resources"
#else
#  define SHARE_PATH "/../share/wiznote"
#endif

const char* g_lpszDesktopFileName = "\
[Desktop Entry]\n\
Exec=%1WizNote\n\
Icon=wiznote\n\
Type=Application\n\
Terminal=false\n\
Name=%2\n\
GenericName=%3\n\
Categories=WizNote;\n\
Name[en_US]=WizNote\n\
GenericName[en_US.UTF-8]=WizNote\n\
";

#ifdef Q_OS_LINUX

void installOnLinux()
{
    QString appPath = Utils::PathResolve::appPath();
    QString strText = WizFormatString3(g_lpszDesktopFileName,
                                       appPath,
                                       QObject::tr("WizNote"),
                                       QObject::tr("WizNote"));
    //
    QString applicationsPath = QDir::homePath() + "/.local/share/applications/";
    ::WizEnsurePathExists(applicationsPath);
    //
    QString iconsBasePath = QDir::homePath() + "/.local/share/icons/hicolor/";
    ::WizEnsurePathExists(applicationsPath);
    //
    CWizStdStringArray arrayIconSize;
    arrayIconSize.push_back("16");
    arrayIconSize.push_back("32");
    arrayIconSize.push_back("48");
    arrayIconSize.push_back("64");
    arrayIconSize.push_back("128");
    arrayIconSize.push_back("256");
    for (CWizStdStringArray::const_iterator it = arrayIconSize.begin();
        it != arrayIconSize.end();
        it++)
    {
        QString iconSize = *it;
        QString iconPathName = iconSize + "x" + iconSize;
        QString iconFullPath = iconsBasePath + iconPathName + "/apps/";
        WizEnsurePathExists(iconFullPath);
        //
        QString resourceName = ":/logo_" + iconSize + ".png";
        QPixmap pixmap(resourceName);
        if (pixmap.isNull())
            continue;
        //
        pixmap.save(iconFullPath + "wiznote.png");
    }

    QString desktopFileName = applicationsPath + "wiznote.desktop";
    ::WizSaveUnicodeTextToUtf8File(desktopFileName, strText, false);
    //
    chmod(desktopFileName.toUtf8(), ACCESSPERMS);
}

#endif // Q_OS_LINUX

int mainCore(int argc, char *argv[])
{

#ifdef Q_OS_LINUX
    // create single application for linux
    CWizSingleApplication a(argc, argv, "Special-Message-for-WizNote-SingleApplication");
    if (a.isRunning())
    {
        a.sendMessage(WIZ_SINGLE_APPLICATION);
        return 0;
    }
    //
    QtWebEngine::initialize();
#else
    QApplication a(argc, argv);
    //
#ifdef BUILD4APPSTORE
    QDir dir(QApplication::applicationDirPath());
    dir.cdUp();
    dir.cd("PlugIns");
    QApplication::setLibraryPaths(QStringList(dir.absolutePath()));
#endif
    //
    QtWebEngine::initialize();

#ifdef BUILD4APPSTORE
    CWizIAPHelper helper;
    helper.validteReceiptOnLauch();
#endif
#endif

   qInstallMessageHandler(Utils::Logger::messageHandler);
   QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication::setApplicationName(QObject::tr("WizNote"));
    QApplication::setOrganizationName(QObject::tr("cn.wiz.wiznoteformac"));

    QIcon icon;
    icon.addPixmap(QPixmap(":/logo_16.png"));
    icon.addPixmap(QPixmap(":/logo_32.png"));
    icon.addPixmap(QPixmap(":/logo_48.png"));
    icon.addPixmap(QPixmap(":/logo_64.png"));
    icon.addPixmap(QPixmap(":/logo_128.png"));
    icon.addPixmap(QPixmap(":/logo_256.png"));
    QApplication::setWindowIcon(icon);

#ifdef Q_OS_MAC
    wizMacInitUncaughtExceptionHandler();
    wizMacRegisterSystemService();

    // init sys local for crash report
    QString sysLocal = QLocale::system().name();
    QTranslator translatorSys;
    QString sysLocalFile = Utils::PathResolve::localeFileName(sysLocal);
    translatorSys.load(sysLocalFile);
    a.installTranslator(&translatorSys);

    initCrashReporter();

    a.removeTranslator(&translatorSys);

    WizQueuedThreadsInit();

    //FIXME: 在Mac osx安全更新之后存在ssl握手问题，此处进行特殊处理
    QSslConfiguration conf = QSslConfiguration::defaultConfiguration();
    conf.setPeerVerifyMode(QSslSocket::VerifyNone);
    QSslConfiguration::setDefaultConfiguration(conf);
#endif

    a.setStyleSheet("QToolTip { \
                    font: 12px; \
                    color:#000000; \
                    padding:0px 1px; \
                    background-color: #F8F8F8; \
                    border:0px;}");

    // setup settings
    QSettings::setDefaultFormat(QSettings::IniFormat);
    //
    QSettings* globalSettings = new QSettings(Utils::PathResolve::globalSettingsFile(), QSettings::IniFormat);
    WizGlobal::setGlobalSettings(globalSettings);
    //

    // use 3 times(30M) of Qt default usage
    int nCacheSize = globalSettings->value("Common/Cache", 10240*3).toInt();
    QPixmapCache::setCacheLimit(nCacheSize);

    QString strUserGuid = globalSettings->value("Users/DefaultUserGuid").toString();
    QList<WizLocalUser> localUsers;
    WizGetLocalUsers(localUsers);    
    QString strAccountFolderName = WizGetLocalFolderName(localUsers, strUserGuid);

    QString strPassword;
    CWizUserSettings userSettings(strAccountFolderName);

    QSettings* settings = new QSettings(Utils::PathResolve::userSettingsFile(strAccountFolderName), QSettings::IniFormat);
    WizGlobal::setSettings(settings);
    //
    // setup locale for welcome dialog
    QString strLocale = userSettings.locale();
    QLocale::setDefault(strLocale);

    QTranslator translatorWizNote;
    QString strLocaleFile = Utils::PathResolve::localeFileName(strLocale);
    translatorWizNote.load(strLocaleFile);
    a.installTranslator(&translatorWizNote);

    QTranslator translatorQt;
    strLocaleFile = Utils::PathResolve::qtLocaleFileName(strLocale);
    translatorQt.load(strLocaleFile);
    a.installTranslator(&translatorQt);

#ifdef Q_OS_LINUX
    if (globalSettings->value("Common/Installed", 0).toInt() == 0)
    {
        globalSettings->setValue("Common/Installed", 1);
        installOnLinux();
    }
#endif

    // figure out auto login or manually login
    bool bFallback = true;

    // FIXME: move to WizService initialize
    Token token;


    bool bAutoLogin = userSettings.autoLogin();
    strPassword = userSettings.password();

    if (bAutoLogin && !strPassword.isEmpty()) {
        bFallback = false;
    }    

    //
    //set network proxy
    CWizSettings wizSettings(Utils::PathResolve::globalSettingsFile());
    if (wizSettings.GetProxyStatus())
    {
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::HttpProxy);
        proxy.setHostName(wizSettings.GetProxyHost());
        proxy.setPort(wizSettings.GetProxyPort());
        proxy.setUser(wizSettings.GetProxyUserName());
        proxy.setPassword(wizSettings.GetProxyPassword());
        QNetworkProxy::setApplicationProxy(proxy);
    }

    //
    QString strUserId = WizGetLocalUserId(localUsers, strUserGuid);
    bool isNewRegisterAccount = false;
    // manually login
    if (bFallback)
    {
        CWizLoginDialog loginDialog(strLocale, localUsers);
        if (QDialog::Accepted != loginDialog.exec())
            return 0;

//        qDebug() << "deafult user id : " << strUserGuid << " login dailog user id : " << loginDialog.loginUserGuid();
        if (strUserId.isEmpty() || loginDialog.loginUserGuid() != strUserGuid)
        {
            strAccountFolderName = WizGetLocalFolderName(localUsers, loginDialog.loginUserGuid());
            if (strAccountFolderName.isEmpty())
            {
                strAccountFolderName = loginDialog.userId();
            }
            qDebug() << "login user id : " << loginDialog.userId();
            settings = new QSettings(Utils::PathResolve::userSettingsFile(strAccountFolderName), QSettings::IniFormat);
            WizGlobal::setSettings(settings);
        }
        strPassword = loginDialog.password();
        strUserId = loginDialog.userId();
        isNewRegisterAccount = loginDialog.isNewRegisterAccount();
    }
    else
    {
        if (userSettings.serverType() == EnterpriseServer)
        {
            CommonApiEntry::setEnterpriseServerIP(userSettings.enterpriseServerIP());
        }
        else if (userSettings.serverType() == WizServer ||
                 (userSettings.serverType() == NoServer && !userSettings.myWizMail().isEmpty()))
        {
            CommonApiEntry::setEnterpriseServerIP(WIZNOTE_API_SERVER);
        }
    }

    //
    //
    // reset locale for current user.
    userSettings.setAccountFolderName(strAccountFolderName);
    userSettings.setUserId(strUserId);
    strLocale = userSettings.locale();

    a.removeTranslator(&translatorWizNote);
    strLocaleFile = Utils::PathResolve::localeFileName(strLocale);
    translatorWizNote.load(strLocaleFile);
    a.installTranslator(&translatorWizNote);

    a.removeTranslator(&translatorQt);
    strLocaleFile = Utils::PathResolve::qtLocaleFileName(strLocale);
    translatorQt.load(strLocaleFile);
    a.installTranslator(&translatorQt);

    CommonApiEntry::setLanguage(strLocale);

    CWizDatabaseManager dbMgr(strAccountFolderName);
    if (!dbMgr.openAll()) {
        QMessageBox::critical(NULL, "", QObject::tr("Can not open database"));
        return 0;
    }


    qDebug() << "set user id for token ; " << strUserId;
    Token::setUserId(strUserId);
    Token::setPasswd(strPassword);

    dbMgr.db().SetPassword(::WizEncryptPassword(strPassword));
    dbMgr.db().UpdateInvalidData();

    // FIXME: move to plugins
    AvatarHost avatarHost;

    // FIXME: move to core plugin initialize
    ThumbCache cache;

    MainWindow w(dbMgr);
#ifdef Q_OS_LINUX
    QObject::connect(&a, SIGNAL(messageAvailable(QString)), &w,
                     SLOT(on_application_messageAvailable(QString)));
#endif

    //settings->setValue("Users/DefaultUser", strUserId);

    w.show();
    w.init();

#ifdef Q_OS_MAC
    //start and set safari extension
    WIZUSERINFO userInfo;
    dbMgr.db().GetUserInfo(userInfo);
    WizExecuteOnThread(WIZ_THREAD_DEFAULT, [strUserId, userInfo](){
        updateShareExtensionAccount(strUserId, userInfo.strUserGUID, userInfo.strMywizEmail ,userInfo.strDisplayName);
        //readShareExtensionAccount();
    });
#endif

    //create introduction note for new register users
    CWizNoteManager noteManager(dbMgr);
    noteManager.updateTemplateJS(userSettings.locale());
    noteManager.downloadTemplatePurchaseRecord();
    if (isNewRegisterAccount)
    {
        noteManager.createIntroductionNoteForNewRegisterAccount();
    }

    int ret = a.exec();
    if (w.isLogout()) {
        userSettings.setPassword("");
#ifndef BUILD4APPSTORE
        QProcess::startDetached(argv[0], QStringList());
#else
        QString strAppFile = QApplication::applicationDirPath().remove("/Contents/MacOS");
        if (!QProcess::startDetached("/usr/bin/open -W "+strAppFile))
        {
            QMessageBox::information(0, "Info", "open " + strAppFile + " failed");
        }
#endif
    }

    return ret;
}

int main(int argc, char *argv[])
{
    int ret = mainCore(argc, argv);

    WizQueuedThreadsShutdown();
    // clean up
    QString strTempPath = Utils::PathResolve::tempPath();
    ::WizDeleteAllFilesInFolder(strTempPath);

    return ret;
}
