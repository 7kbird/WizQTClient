#ifndef WIZKMXMLRPC_H
#define WIZKMXMLRPC_H

#include "wizXmlRpcServer.h"
#include "wizJSONServerBase.h"

#define WIZKM_XMLRPC_ERROR_TRAFFIC_LIMIT		304
#define WIZKM_XMLRPC_ERROR_STORAGE_LIMIT		305
#define WIZKM_XMLRPC_ERROR_NOTE_COUNT_LIMIT		3032
#define WIZKM_XMLRPC_ERROR_BIZ_SERVICE_EXPR		380

class CWizKMXmlRpcServerBase : public CWizXmlRpcServerBase
{
public:
    CWizKMXmlRpcServerBase(const QString& strUrl, QObject* parent);
    bool GetValueVersion(const QString& strMethodPrefix, const QString& strToken, const QString& strKbGUID, const QString& strKey, __int64& nVersion);
    bool GetValue(const QString& strMethodPrefix, const QString& strToken, const QString& strKbGUID, const QString& strKey, QString& strValue, __int64& nVersion);
    bool SetValue(const QString& strMethodPrefix, const QString& strToken, const QString& strKbGUID, const QString& strKey, const QString& strValue, __int64& nRetVersion);
};


class CWizKMAccountsServer : public CWizKMXmlRpcServerBase, public CWizJSONServerBase
{
public:
    CWizKMAccountsServer(const QString& strUrl, QObject* parent = 0);
    virtual ~CWizKMAccountsServer(void);

    virtual void OnXmlRpcError();

protected:
    bool m_bLogin;
    bool m_bAutoLogout;

public:
    WIZUSERINFO m_userInfo;

public:
    bool Login(const QString& strUserName, const QString& strPassword, const QString& strType = "normal");
    bool Logout();
    bool ChangePassword(const QString& strUserName, const QString& strOldPassword, const QString& strNewPassword);
    bool ChangeUserId(const QString& strUserName, const QString& strPassword, const QString& strNewUserId);
    bool GetToken(const QString& strUserName, const QString& strPassword, QString& strToken);
    bool GetCert(const QString& strUserName, const QString& strPassword, QString& strN, QString& stre, QString& strd, QString& strHint);
    bool SetCert(const QString& strUserName, const QString& strPassword, const QString& strN, const QString& stre, const QString& strd, const QString& strHint);
    bool CreateAccount(const QString& strUserName, const QString& strPassword, const QString& InviteCode, const QString& strCaptchaID, const QString& strCaptcha);
    void SetAutoLogout(bool b) { m_bAutoLogout = b; }
    bool ShareSNS(const QString& strToken, const QString& strSNS, const QString& strComment, const QString& strURL, const QString& strDocumentGUID);
    bool GetGroupList(CWizGroupDataArray& arrayGroup);
    bool GetBizList(CWizBizDataArray& arrayBiz);
    bool CreateTempGroup(const QString& strEmails, const QString& strAccessControl, const QString& strSubject, const QString& strEmailText, WIZGROUPDATA& group);
    bool KeepAlive(const QString& strToken);
    bool GetMessages(__int64 nVersion, CWizUserMessageDataArray& arrayMessage);
    bool SetMessageReadStatus(const QString& strMessageIDs, int nStatus);

    //
    bool SetMessageDeleteStatus(const QString &strMessageIDs, int nStatus);

    bool GetValueVersion(const QString& strKey, __int64& nVersion);
    bool GetValue(const QString& strKey, QString& strValue, __int64& nVersion);
    bool SetValue(const QString& strKey, const QString& strValue, __int64& nRetVersion);

public:
    bool GetWizKMDatabaseServer(QString& strServer, int& nPort, QString& strXmlRpcFile);
    QString GetToken();
    QString GetKbGUID();
    //void setKbGUID(const QString& strkbGUID) { m_retLogin.strKbGUID = strkbGUID; }
    const WIZUSERINFO& GetUserInfo() const { return m_userInfo; }
    WIZUSERINFO& GetUserInfo() { return m_userInfo; }
    void SetUserInfo(const WIZUSERINFO& userInfo);

private:
    QString MakeXmlRpcPassword(const QString& strPassword);

    bool accounts_clientLogin(const QString& strUserName, const QString& strPassword, const QString& strType, WIZUSERINFO& ret);
    bool accounts_clientLogout(const QString& strToken);
    bool accounts_keepAlive(const QString& strToken);
    bool accounts_createAccount(const QString& strUserName, const QString& strPassword, const QString& strInviteCode, const QString& strCaptchaID, const QString& strCaptcha);
    bool accounts_changePassword(const QString& strUserName, const QString& strOldPassword, const QString& strNewPassword);
    bool accounts_changeUserId(const QString& strUserName, const QString& strPassword, const QString& strNewUserId);
    bool accounts_getToken(const QString& strUserName, const QString& strPassword, QString& strToken);
    bool accounts_getCert(const QString& strUserName, const QString& strPassword, QString& strN, QString& stre, QString& strd, QString& strHint);
    bool accounts_setCert(const QString& strUserName, const QString& strPassword, const QString& strN, const QString& stre, const QString& strd, const QString& strHint);
    bool document_shareSNS(const QString& strToken, const QString& strSNS, const QString& strComment, const QString& strURL, const QString& strDocumentGUID);

    bool accounts_getGroupList(CWizGroupDataArray& arrayGroup);
    bool accounts_getBizList(CWizBizDataArray& arrayBiz);
    bool accounts_createTempGroupKb(const QString& strEmails, const QString& strAccessControl, const QString& strSubject, const QString& strEmailText, WIZGROUPDATA& group);
    bool accounts_getMessagesByXmlrpc(int nCountPerPage, __int64 nVersion, CWizUserMessageDataArray& arrayMessage);

    //
    bool accounts_getMessagesByJson(int nCountPerPage, __int64 nVersion, CWizUserMessageDataArray& arrayMessage);
};



#define WIZKM_WEBAPI_VERSION		5

struct CWizKMBaseParam: public CWizXmlRpcStructValue
{
    CWizKMBaseParam(int apiVersion = WIZKM_WEBAPI_VERSION)
    {
        ChangeApiVersion(apiVersion);
        //
#ifdef Q_OS_MAC
        AddString(_T("client_type"), _T("mac"));
#else
        AddString(_T("client_type"), _T("linux"));
#endif
        AddString(_T("client_version"), _T("2.0"));
    }
    //
    void ChangeApiVersion(int nApiVersion)
    {
        AddString(_T("api_version"), WizIntToStr(nApiVersion));
    }
    int GetApiVersion()
    {
        QString str;
        GetString(_T("api_version"), str);
        return _ttoi(str);
    }
};

struct CWizKMTokenOnlyParam : public CWizKMBaseParam
{
    CWizKMTokenOnlyParam(const QString& strToken, const QString& strKbGUID)
    {
        AddString(_T("token"), strToken);
        AddString(_T("kb_guid"), strKbGUID);
    }
};


struct WIZOBJECTVERSION
{
    __int64 nDocumentVersion;
    __int64 nTagVersion;
    __int64 nStyleVersion;
    __int64 nAttachmentVersion;
    __int64 nDeletedGUIDVersion;
    //
    WIZOBJECTVERSION()
    {
        nDocumentVersion = -1;
        nTagVersion = -1;
        nStyleVersion = -1;
        nAttachmentVersion = -1;
        nDeletedGUIDVersion = -1;
    }
};

class CWizKMDatabaseServer: public CWizKMXmlRpcServerBase
{
    Q_OBJECT
public:
    CWizKMDatabaseServer(const WIZUSERINFOBASE& kbInfo, QObject* parent = 0);
    virtual ~CWizKMDatabaseServer();
    virtual void OnXmlRpcError();

    const WIZKBINFO& kbInfo();
    void setKBInfo(const WIZKBINFO& info);

protected:
    WIZUSERINFOBASE m_userInfo;
    WIZKBINFO m_kbInfo;

public:
    QString GetToken() const { return m_userInfo.strToken; }
    QString GetKbGUID() const { return m_userInfo.strKbGUID; }
    int GetMaxFileSize() const { return m_kbInfo.GetMaxFileSize(); }

    bool wiz_getInfo();
    bool wiz_getVersion(WIZOBJECTVERSION& version, bool bAuto = FALSE);

    bool document_downloadData(const QString& strDocumentGUID, WIZDOCUMENTDATAEX& ret);
    bool attachment_downloadData(const QString& strAttachmentGUID, WIZDOCUMENTATTACHMENTDATAEX& ret);
    //
    bool document_postData(const WIZDOCUMENTDATAEX& data, bool bWithDocumentData, __int64& nServerVersion);
    bool attachment_postData(WIZDOCUMENTATTACHMENTDATAEX& data, __int64& nServerVersion);
    //
    bool document_getList(int nCountPerPage, __int64 nVersion, std::deque<WIZDOCUMENTDATAEX>& arrayRet);
    bool attachment_getList(int nCountPerPage, __int64 nVersion, std::deque<WIZDOCUMENTATTACHMENTDATAEX>& arrayRet);
    bool tag_getList(int nCountPerPage, __int64 nVersion, std::deque<WIZTAGDATA>& arrayRet);
    bool style_getList(int nCountPerPage, __int64 nVersion, std::deque<WIZSTYLEDATA>& arrayRet);
    bool deleted_getList(int nCountPerPage, __int64 nVersion, std::deque<WIZDELETEDGUIDDATA>& arrayRet);

    bool tag_postList(std::deque<WIZTAGDATA>& arrayTag);
    bool style_postList(std::deque<WIZSTYLEDATA>& arrayStyle);
    bool deleted_postList(std::deque<WIZDELETEDGUIDDATA>& arrayDeletedGUID);
    QByteArray DownloadDocumentData(const QString& strDocumentGUID);
    QByteArray DownloadAttachmentData(const QString& strAttachmentGUID);
    //
    bool document_getListByGuids(const CWizStdStringArray& arrayDocumentGUID, std::deque<WIZDOCUMENTDATAEX>& arrayRet);
    bool document_getInfo(const QString& strDocumentGuid, WIZDOCUMENTDATAEX& doc);

    bool category_getAll(QString& str);

    bool data_download(const QString& strObjectGUID, const QString& strObjectType, QByteArray& stream, const QString& strDisplayName);
    bool data_upload(const QString& strObjectGUID, const QString& strObjectType, const QByteArray& stream, const QString& strObjMD5, const QString& strDisplayName);
    //
    bool GetValueVersion(const QString& strKey, __int64& nVersion);
    bool GetValue(const QString& strKey, QString& strValue, __int64& nVersion);
    bool SetValue(const QString& strKey, const QString& strValue, __int64& nRetVersion);

public:
    virtual int GetCountPerPage();

signals:
    void downloadProgress(int totalSize, int loadedSize);

protected:
    bool data_download(const QString& strObjectGUID, const QString& strObjectType, int pos, int size, QByteArray& stream, int& nAllSize, bool& bEOF);
    bool data_upload(const QString& strObjectGUID, const QString& strObjectType, const QString& strObjectMD5, int allSize, int partCount, int partIndex, int partSize, const QByteArray& stream);
    //
    ////////////////////////////////////////////////////////////
    //downloadList
    ////通过GUID列表，下载完整的对象信息列表，和getList不同，getList对于文档，仅下载有限的信息，例如md5信息等等////
    //
    //
    template <class TData, class TWrapData>
    bool downloadListCore(const QString& strMethodName, const QString& strGUIDArrayValueName, const CWizStdStringArray& arrayGUID, std::deque<TData>& arrayRet)
    {
        if (arrayGUID.empty())
            return TRUE;
        //
        CWizKMTokenOnlyParam param(m_userInfo.strToken, m_userInfo.strKbGUID);
        param.AddStringArray(strGUIDArrayValueName, arrayGUID);
        //
        std::deque<TWrapData> arrayWrap;
        if (!Call(strMethodName, arrayWrap, &param))
        {
            TOLOG1(_T("%1 failure!"), strMethodName);
            return FALSE;
        }
        //
        arrayRet.assign(arrayWrap.begin(), arrayWrap.end());
        //
        return TRUE;
    }


    template <class TData, class TWrapData>
    bool downloadList(const QString& strMethodName, const QString& strGUIDArrayValueName, const CWizStdStringArray& arrayGUID, std::deque<TData>& arrayRet)
    {
        int nCountPerPage = 30;
        //
        CWizStdStringArray::const_iterator it = arrayGUID.begin();
        //
        while (1)
        {
            CWizStdStringArray subArray;
            //
            for (;
                 it != arrayGUID.end(); )
            {
                subArray.push_back(*it);
                it++;
                //
                if (subArray.size() == nCountPerPage)
                    break;
            }
            //
            std::deque<TData> subRet;
            if (!downloadListCore<TData, TWrapData>(strMethodName, strGUIDArrayValueName, subArray, subRet))
                return FALSE;
            //
            arrayRet.insert(arrayRet.end(), subRet.begin(), subRet.end());
            //
            if (it == arrayGUID.end())
                break;
        }
        //
        return TRUE;
    }

    //
    //
    ////////////////////////////////////////////
    //postList
    ////上传对象列表，适用于简单对象：标签，样式，已删除对象////
    //
    template <class TData, class TWrapData>
    bool postList(const QString& strMethosName, const QString& strArrayName, std::deque<TData>& arrayData)
    {
        if (arrayData.empty())
            return TRUE;
        //
        int nCountPerPage = GetCountPerPage();
        //
        typename std::deque<TData>::const_iterator it = arrayData.begin();
        //
        while (1)
        {
            //
            std::deque<TWrapData> subArray;
            //
            for (;
                it != arrayData.end(); )
            {
                subArray.push_back(*it);
                it++;
                //
                if (subArray.size() == nCountPerPage)
                    break;
            }
            //

            CWizKMTokenOnlyParam param(m_userInfo.strToken, m_userInfo.strKbGUID);
            //
            param.AddArray<TWrapData>(strArrayName, subArray);
            //
            QString strCount;
            if (!Call(strMethosName, _T("success_count"), strCount, &param))
            {
        #ifdef _DEBUG
                //WizMessageBox1(_T("Failed to upload list: %1"), strMethosName);
        #endif
                TOLOG1(_T("%1 failure!"), strMethosName);
                return FALSE;
            }
            //
            if (_ttoi(strCount) != (int)subArray.size())
            {
                QString strError = WizFormatString3(_T("Failed to upload list: %1, upload count=%2, success_count=%3"), strMethosName,
                    WizIntToStr(int(subArray.size())),
                    strCount);
        #ifdef _DEBUG
                //WizMessageBox(strError);
        #endif
                TOLOG1(_T("%1 failure!"), strMethosName);
                //
                ATLASSERT(FALSE);
                return FALSE;
            }
            //
            //
            if (it == arrayData.end())
                break;
        }
        //
        return TRUE;
    }
    //
    /////////////////////////////////////////////////////////
    ////下载对象数据/////////////////

    template <class TData>
    bool downloadObjectData(TData& data)
    {
        return TRUE;
    }
    template <class TData>
    bool downloadObjectData(WIZDOCUMENTDATAEX& data)
    {
        return document_downloadData(data.strGUID, data);
    }
    template <class TData>
    bool downloadObjectData(WIZDOCUMENTATTACHMENTDATAEX& data)
    {
        return attachment_downloadData(data.strGUID, data);
    }


    /////////////////////////////////////////////
    //getList
    ////通过版本号获得对象列表////
    //

    template <class TData, class TWrapData>
    bool getList(const QString& strMethodName, int nCountPerPage, __int64 nVersion, std::deque<TData>& arrayRet)
    {
        CWizKMTokenOnlyParam param(m_userInfo.strToken, m_userInfo.strKbGUID);
        param.AddInt(_T("count"), nCountPerPage);
        param.AddString(_T("version"), WizInt64ToStr(nVersion));
        //
        std::deque<TWrapData> arrayWrap;
        if (!Call(strMethodName, arrayWrap, &param))
        {
            TOLOG(_T("object.getList failure!"));
            return FALSE;
        }
        //
        arrayRet.assign(arrayWrap.begin(), arrayWrap.end());
        //
        return TRUE;
    }

public:
    //

    //
    /////////////////////////////////////////////////////
    ////获得所有的对象列表//
    //
    template <class TData>
    bool getList(int nCountPerPage, __int64 nVersion, std::deque<TData>& arrayRet)
    {
        ATLASSERT(FALSE);
        return FALSE;
    }
    template <class TData>
    bool getList(int nCountPerPage, __int64 nVersion, std::deque<WIZTAGDATA>& arrayRet)
    {
        return tag_getList(nCountPerPage, nVersion, arrayRet);
    }
    template <class TData>
    bool getList(int nCountPerPage, __int64 nVersion, std::deque<WIZSTYLEDATA>& arrayRet)
    {
        return style_getList(nCountPerPage, nVersion, arrayRet);
    }
    template <class TData>
    bool getList(int nCountPerPage, __int64 nVersion, std::deque<WIZDELETEDGUIDDATA>& arrayRet)
    {
        return deleted_getList(nCountPerPage, nVersion, arrayRet);
    }
    template <class TData>
    bool getList(int nCountPerPage, __int64 nVersion, std::deque<WIZDOCUMENTDATAEX>& arrayRet)
    {
        return document_getList(nCountPerPage, nVersion, arrayRet);
    }
    template <class TData>
    bool getList(int nCountPerPage, __int64 nVersion, std::deque<WIZDOCUMENTATTACHMENTDATAEX>& arrayRet)
    {
        return attachment_getList(nCountPerPage, nVersion, arrayRet);
    }
    ///////////////////////////////////////////////////////
    ////下载列表//////////////
    //
    template <class TData>
    bool downloadSimpleList(const CWizStdStringArray& arrayGUID, std::deque<TData>& arrayData)
    {
        return TRUE;
    }
    //
    template <class TData>
    bool postList(std::deque<TData>& arrayData)
    {
        ATLASSERT(FALSE);
        return FALSE;
    }
    //
    template <class TData>
    bool postList(std::deque<WIZTAGDATA>& arrayData)
    {
        return tag_postList(arrayData);
    }
    template <class TData>
    bool postList(std::deque<WIZSTYLEDATA>& arrayData)
    {
        return style_postList(arrayData);
    }
    template <class TData>
    bool postList(std::deque<WIZDELETEDGUIDDATA>& arrayData)
    {
        return deleted_postList(arrayData);
    }
    //
    template <class TData>
    bool postData(TData& data, bool bWithData, __int64& nServerVersion)
    {
        ATLASSERT(FALSE);
        return FALSE;
    }
    //
    template <class TData>
    bool postData(WIZDOCUMENTDATAEX& data, bool bWithData, __int64& nServerVersion)
    {
        return document_postData(data, bWithData, nServerVersion);
    }
    template <class TData>
    bool postData(WIZDOCUMENTATTACHMENTDATAEX& data, bool bWithData, __int64& nServerVersion)
    {
        return attachment_postData(data, nServerVersion);
    }
public:
    //
    bool getDocumentInfoOnServer(const QString& strDocumentGUID, WIZDOCUMENTDATAEX& dataServer)
    {
        CWizStdStringArray arrayDocument;
        arrayDocument.push_back(strDocumentGUID);
        //
        CWizDocumentDataArray arrayData;
        if (!downloadSimpleList<WIZDOCUMENTDATAEX>(arrayDocument, arrayData))
        {
            TOLOG(_T("Can't download document info list from server!"));
            return FALSE;
        }
        //
        if (arrayData.empty())
            return TRUE;
        //
        if (arrayData.size() != 1)
        {
            TOLOG1(_T("Too more documents info downloaded: %1!"), WizInt64ToStr(arrayData.size()));
            return FALSE;
        }
        //
        dataServer = arrayData[0];
        //
        return TRUE;
    }
};


enum WizKMSyncProgress
{
    syncAccountLogin = 0,
    syncDatabaseLogin,
    syncDownloadDeletedList,
    syncUploadDeletedList,
    syncUploadTagList,
    syncUploadStyleList,
    syncUploadDocumentList,
    syncUploadAttachmentList,
    syncDownloadTagList,
    syncDownloadStyleList,
    syncDownloadSimpleDocumentList,
    syncDownloadFullDocumentList,
    syncDownloadAttachmentList,
    syncDownloadObjectData
};

struct WIZKEYVALUEDATA
{
    QString strValue;
    __int64 nVersion;

    //
    WIZKEYVALUEDATA(const QString& value, __int64 ver)
    {
        strValue = value;
        nVersion = ver;
    }
    //
    WIZKEYVALUEDATA()
    {
        nVersion = 0;
    }
};

void GetSyncProgressRange(WizKMSyncProgress progress, int& start, int& count);
int GetSyncStartProgress(WizKMSyncProgress progress);


#define _TR(x) x




#endif // WIZKMXMLRPC_H
