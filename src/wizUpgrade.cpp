#include "wizUpgrade.h"

#include "share/wizmisc.h"
#include "utils/logger.h"
#include "sync/apientry.h"
#include "share/wizEventLoop.h"

#if defined(Q_OS_MAC)
#define strUpgradeUrlParam "/download?product=wiznote&client=macos"
#elif defined(Q_OS_LINUX)

#if defined(_M_X64) || defined(__amd64)
#define strUpgradeUrlParam "/download?product=wiznote&client=linux-x64"
#else
#define strUpgradeUrlParam "/download?product=wiznote&client=linux-x86"
#endif // __amd64

#elif defined(Q_OS_WIN)

#ifdef Q_OS_WIN64
#define strUpgradeUrlParam "/download?product=wiznote&client=win-x64"
#else
#define strUpgradeUrlParam "/download?product=wiznote&client=win-32"
#endif // Q_OS_WIN64

#endif // Q_OS_MAC

CWizUpgradeChecker::CWizUpgradeChecker(QObject *parent) :
    QObject(parent)
{
}

CWizUpgradeChecker::~CWizUpgradeChecker()
{
}

QString CWizUpgradeChecker::getWhatsNewUrl()
{
    return WizApiEntry::standardCommandUrl("changelog");
}

void CWizUpgradeChecker::checkUpgrade()
{
    QString strApiUrl = WizApiEntry::standardCommandUrl("download_server");

    if (!m_net.get()) {
        m_net = std::make_shared<QNetworkAccessManager>();
    }

    QNetworkReply* reply = m_net->get(QNetworkRequest(strApiUrl));
    CWizAutoTimeOutEventLoop loop(reply);
    loop.exec();

    if (loop.error() != QNetworkReply::NoError)
    {
        Q_EMIT checkFinished(false);
        return;
    }

    QString strCheckUrl = QString::fromUtf8(loop.result().constData());
    strCheckUrl += strUpgradeUrlParam;

    _check(strCheckUrl);
}

void CWizUpgradeChecker::_check(const QString& strUrl)
{
    QNetworkReply* reply = m_net->get(QNetworkRequest(strUrl));
    CWizAutoTimeOutEventLoop loop(reply);
    loop.exec();

    QUrl possibleRedirectedUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    m_redirectedUrl = redirectUrl(possibleRedirectedUrl, m_redirectedUrl);

    if (!m_redirectedUrl.isEmpty()) {
        // redirect to download server.
        QString strVersion;
        QRegExp regexp("(\\d{4}-\\d{2}-\\d{2})");
        if (regexp.indexIn(m_redirectedUrl.toString()) == -1) {
            Q_EMIT checkFinished(false);
            return;
        }

        strVersion = regexp.cap(0);

        int y = strVersion.split("-").at(0).toInt();
        int m = strVersion.split("-").at(1).toInt();
        int d = strVersion.split("-").at(2).toInt();

        QDate dateUpgrade(y, m, d);

        QFileInfo fi(::WizGetAppFileName());
        QDate dateLocal = fi.created().date();

        if (dateUpgrade > dateLocal) {
            TOLOG(QObject::tr("INFO: Upgrade is avaliable, version time: %1").arg(dateUpgrade.toString()));
            Q_EMIT checkFinished(true);
        } else {
            TOLOG(QObject::tr("INFO: Local version is up to date"));
            Q_EMIT checkFinished(false);
        }
    } else {
        TOLOG(QObject::tr("ERROR: Check upgrade failed"));
        Q_EMIT checkFinished(false);
    }
}

QUrl CWizUpgradeChecker::redirectUrl(QUrl const &possible_redirect_url, \
                              QUrl const &old_redirect_url) const
{
    QUrl redirect_url;

    if(!possible_redirect_url.isEmpty() \
            && possible_redirect_url != old_redirect_url)
    {
            redirect_url = possible_redirect_url;
    }

    return redirect_url;
}
