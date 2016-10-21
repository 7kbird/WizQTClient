#include "wizDocumentWebView.h"

#include <QRunnable>
#include <QList>
#include <QMimeData>
#include <QUrl>
#include <QDir>
#include <QString>
#include <QRegExp>
#include <QAction>
#include <QPrinter>
#include <QFileDialog>
#include <QTextEdit>
#include <QMultiMap>
#include <QPrintDialog>
#include <QPrinterInfo>
#include <QMessageBox>

#include <QApplication>
#include <QUndoStack>
#include <QDesktopServices>
#include <QNetworkDiskCache>

#ifdef Q_OS_MAC
#include <QMacPasteboardMime>
#endif

#include "share/wizGlobal.h"

#include "wizdef.h"
#include "utils/pathresolve.h"
#include "utils/logger.h"
#include "utils/misc.h"
#include "utils/stylehelper.h"

#include "share/wizmisc.h"
#include "share/wizAnalyzer.h"
#include "share/wizMessageBox.h"
#include "share/wizObjectDataDownloader.h"
#include "share/wizDatabaseManager.h"
#include "share/wizthreads.h"
#include "sync/avatar.h"
#include "sync/token.h"
#include "sync/apientry.h"
#include "core/wizAccountManager.h"
#include "core/wizNoteManager.h"
#include "widgets/WizCodeEditorDialog.h"
#include "widgets/wizScreenShotWidget.h"
#include "widgets/wizEmailShareDialog.h"
#include "widgets/wizShareLinkDialog.h"
#include "widgets/wizScrollBar.h"
#include "widgets/wizexecutingactiondialog.h"
#include "mac/wizmachelper.h"

#include "wizmainwindow.h"
#include "wizEditorInsertLinkForm.h"
#include "wizEditorInsertTableForm.h"
#include "wizDocumentTransitionView.h"
#include "wizDocumentView.h"
#include "wizSearchReplaceWidget.h"

#include "html/wizhtmlreader.h"

#include "titlebar.h"


enum WizLinkType {
    WizLink_Doucment,
    WizLink_Attachment
};

CWizDocumentWebViewPage::CWizDocumentWebViewPage(CWizDocumentWebView* parent)
    : WizWebEnginePage(parent)
    , m_engineView(parent)
{
    Q_ASSERT(m_engineView);
    //
    action(QWebEnginePage::Undo)->setShortcut(QKeySequence());
    action(QWebEnginePage::Redo)->setShortcut(QKeySequence());
    action(QWebEnginePage::Copy)->setShortcut(QKeySequence());
    action(QWebEnginePage::Cut)->setShortcut(QKeySequence());
    action(QWebEnginePage::Paste)->setShortcut(QKeySequence());
    action(QWebEnginePage::SelectAll)->setShortcut(QKeySequence());
}

bool CWizDocumentWebViewPage::acceptNavigationRequest(const QUrl &url, QWebEnginePage::NavigationType type, bool isMainFrame)
{
    if (NavigationTypeBackForward == type || NavigationTypeReload == type)
        return false;
    //
    return WizWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}

void CWizDocumentWebViewPage::triggerAction(QWebEnginePage::WebAction typeAction, bool checked)
{
    if (typeAction == QWebEnginePage::Back || typeAction == QWebEnginePage::Forward) {
        return;
    }

    if (typeAction == QWebEnginePage::Paste) {
        if (m_engineView->onPasteCommand())
            return;
    } else if (typeAction == QWebEnginePage::Undo || typeAction == QWebEnginePage::Redo) {
        //FIXME: 在QT5.4.2之后无法禁止webpage的快捷键，webpage的快捷键会覆盖menubar上的
        Q_EMIT actionTriggered(typeAction);
        return;
    }

    QWebEnginePage::triggerAction(typeAction, checked);

    Q_EMIT actionTriggered(typeAction);
}

void CWizDocumentWebViewPage::javaScriptConsoleMessage(QWebEnginePage::JavaScriptConsoleMessageLevel level, const QString& message, int lineNumber, const QString& sourceID)
{
    Q_UNUSED(sourceID);

    qDebug() << "[Console]line: " << lineNumber << ", " << message;
}


static int nWindowIDCounter = 0;

CWizDocumentWebView::CWizDocumentWebView(CWizExplorerApp& app, QWidget* parent)
    : WizWebEngineView(parent)
    , m_app(app)
    , m_dbMgr(app.databaseManager())
    , m_bNewNote(false)
    , m_bNewNoteTitleInited(false)
    , m_bContentsChanged(false)
    , m_bInSeperateWindow(false)
    , m_nWindowID(nWindowIDCounter ++)
    , m_searchReplaceWidget(nullptr)
    , m_ignoreActiveWindowEvent(false)
{
    CWizDocumentWebViewPage* page = new CWizDocumentWebViewPage(this);
    setPage(page);

    connect(page, SIGNAL(actionTriggered(QWebEnginePage::WebAction)), SLOT(onActionTriggered(QWebEnginePage::WebAction)));
    connect(page, SIGNAL(linkClicked(QUrl,QWebEnginePage::NavigationType,bool,WizWebEnginePage*)), this, SLOT(onEditorLinkClicked(QUrl,QWebEnginePage::NavigationType,bool,WizWebEnginePage*)));

    // minimum page size hint
    setMinimumSize(400, 250);

    // only accept focus by mouse click as the best way to trigger toolbar reset
    setFocusPolicy(Qt::ClickFocus);
    setAttribute(Qt::WA_AcceptTouchEvents, false);

    // FIXME: should accept drop picture, attachment, link etc.
    setAcceptDrops(true);

    // refers
    qRegisterMetaType<WizEditorMode>("WizEditorMode");
    m_docLoadThread = new CWizDocumentWebViewLoaderThread(m_dbMgr, this);
    connect(m_docLoadThread, SIGNAL(loaded(const QString&, const QString, const QString, WizEditorMode)),
            SLOT(onDocumentReady(const QString&, const QString, const QString, WizEditorMode)), Qt::QueuedConnection);
    //
    m_docSaverThread = new CWizDocumentWebViewSaverThread(m_dbMgr, this);
    connect(m_docSaverThread, SIGNAL(saved(const QString, const QString,bool)),
            SLOT(onDocumentSaved(const QString, const QString,bool)), Qt::QueuedConnection);

    // loading and saving thread
    m_timerAutoSave.setInterval(5*60*1000); // 5 minutes
    connect(&m_timerAutoSave, SIGNAL(timeout()), SLOT(onTimerAutoSaveTimout()));
    //
    //
    addToJavaScriptWindowObject("WizExplorerApp", m_app.object());
    addToJavaScriptWindowObject("WizQtEditor", this);


    connect(this, SIGNAL(loadFinishedEx(bool)), SLOT(onEditorLoadFinished(bool)));
}

CWizDocumentWebView::~CWizDocumentWebView()
{
    if (m_searchReplaceWidget)
        delete m_searchReplaceWidget;
}
void CWizDocumentWebView::waitForDone()
{
    if (m_docLoadThread) {
        m_docLoadThread->waitForDone();
        m_docLoadThread = NULL;
    }
    if (m_docSaverThread) {
        m_docSaverThread->waitForDone();
        m_docSaverThread = NULL;
    }
}


bool CWizDocumentWebView::onPasteCommand()
{
    QClipboard* clip = QApplication::clipboard();
    Q_ASSERT(clip);

    if (!clip->image().isNull()) {
        // save clipboard image to
        QString strImagePath = noteResourcesPath();
        CString strFileName = strImagePath + WizIntToStr(GetTickCount()) + ".png";
        if (!clip->image().save(strFileName)) {
            TOLOG("ERROR: Can't save clipboard image to file");
            return false;
        }

        QString strHtml;
        if (!WizImage2Html(strFileName, strHtml, strImagePath))
            return false;
        //
        editorCommandExecuteInsertHtml(strHtml, true);
        //
        return true;
    }
    //
    return false;
}

void CWizDocumentWebView::inputMethodEvent(QInputMethodEvent* event)
{
    // On X windows, fcitx flick while preediting, only update while webview end process.
    // maybe it's a QT-BUG?
#ifdef Q_OS_LINUX
    setUpdatesEnabled(false);
    //QWebView::inputMethodEvent(event);
    setUpdatesEnabled(true);
#else
    QWebEngineView::inputMethodEvent(event);
#endif

#ifdef Q_OS_MAC
    /*
    ///暂时注释代码，移动输入光标会导致极高的CPU占用率，导致输入卡顿。

    //int nLength = 0;
    int nOffset = 0;
    for (int i = 0; i < event->attributes().size(); i++) {
        const QInputMethodEvent::Attribute& a = event->attributes().at(i);
        if (a.type == QInputMethodEvent::Cursor) {
            //nLength = a.length;
            nOffset = a.start;
            break;
        }
    }

    // Move cursor
    // because every time input method event triggered, old preedit text will be
    // deleted and the new one inserted, this action made cursor restore to the
    // beginning of the input context, move it as far as offset indicated after
    // default implementation should correct this issue!!!
    for (int i = 0; i < nOffset; i++) {
        page()->triggerAction(QWebPage::MoveToNextChar);
    }
    */
#endif // Q_OS_MAC
}

void CWizDocumentWebView::keyPressEvent(QKeyEvent* event)
{
    // special cases process
    if (event->key() == Qt::Key_Escape)
    {
        // FIXME: press esc will insert space at cursor if not clear focus
        clearFocus();
        return;
    }
    else if (event->key() == Qt::Key_S
             && event->modifiers() == Qt::ControlModifier)
    {
        trySaveDocument(view()->note(), false, [=](const QVariant&){});
        return;
    }
#if QT_VERSION >= 0x050402
//    else if (event->modifiers() == Qt::ControlModifier)
//    {
//        return;
//    }
    //FIXME: QT5.4.2之后无法触发全局的保存按钮
    else if (event->key() == Qt::Key_V && event->modifiers() == Qt::ControlModifier)
    {
        WizGetAnalyzer().LogAction("paste");

        triggerPageAction(QWebEnginePage::Paste);
        return;
    }
#endif
#if QT_VERSION < 0x050000
    #ifdef Q_OS_MAC
    else if (event->key() == Qt::Key_Z)
    {
        //Ctrl+Shift+Z,  shortcut for redo can't catch by actions in QT4
        Qt::KeyboardModifiers keyMod = QApplication::keyboardModifiers();
        bool isSHIFT = keyMod.testFlag(Qt::ShiftModifier);
        bool isCTRL = keyMod.testFlag(Qt::ControlModifier);
        if (isCTRL && isSHIFT) {
            redo();
            return;
        } else if (isCTRL && !isSHIFT) {
            undo();
            return;
        }
    }
    #endif

#endif


#ifdef Q_OS_LINUX
    setUpdatesEnabled(false);
    //QWebView::keyPressEvent(event);
    setUpdatesEnabled(true);
#else

    QWebEngineView::keyPressEvent(event);
#endif

    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        tryResetTitle();
    }

    emit updateEditorToolBarRequest();
}

void CWizDocumentWebView::mousePressEvent(QMouseEvent* event)
{
    QWebEngineView::mousePressEvent(event);
    emit updateEditorToolBarRequest();
}

void CWizDocumentWebView::focusInEvent(QFocusEvent *event)
{
    if (m_currentEditorMode == modeEditor) {
        Q_EMIT focusIn();
    }

    QWebEngineView::focusInEvent(event);

    applySearchKeywordHighlight();
}

void CWizDocumentWebView::focusOutEvent(QFocusEvent *event)
{
    // because qt will clear focus when context menu popup, we need keep focus there.
    if (event->reason() == Qt::PopupFocusReason)
    {
        return;
    }
    else if (m_ignoreActiveWindowEvent && event->reason() == Qt::ActiveWindowFocusReason)
    {
        //NOTE:显示CWizTipsWidget的时候会造成编辑器失去焦点，进而导致toolbar关联的tips消失。此处通过
        //忽略tips显示时产生的ActiveWindowFocusReason来进行tips的显示
        return;
    }

    Q_EMIT focusOut();
    QWebEngineView::focusOutEvent(event);
}

void CWizDocumentWebView::contextMenuEvent(QContextMenuEvent *event)
{
    Q_EMIT showContextMenuRequest(mapToGlobal(event->pos()));
}

void CWizDocumentWebView::dragEnterEvent(QDragEnterEvent *event)
{
    if (!isEditing())
        return;

    int nAccepted = 0;
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> li = event->mimeData()->urls();
        QList<QUrl>::const_iterator it;
        for (it = li.begin(); it != li.end(); it++) {
            QUrl url = *it;
            if (url.toString().startsWith("file:///")) {
                nAccepted++;
            }
        }

        if (nAccepted == li.size()) {
            event->acceptProposedAction();
        }
    } else if (event->mimeData()->hasFormat(WIZNOTE_MIMEFORMAT_DOCUMENTS)) {
        if (!event->mimeData()->data(WIZNOTE_MIMEFORMAT_DOCUMENTS).isEmpty()) {
            setFocus();
            event->acceptProposedAction();
        }
    }
}

void CWizDocumentWebView::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasFormat(WIZNOTE_MIMEFORMAT_DOCUMENTS)) {
            if (!event->mimeData()->data(WIZNOTE_MIMEFORMAT_DOCUMENTS).isEmpty()) {
                setFocus();
                event->acceptProposedAction();
            }
    }
}

void CWizDocumentWebView::onActionTriggered(QWebEnginePage::WebAction act)
{
    //在QT5.4.2之后webpage会覆盖menubar的快捷键，且无法禁止。某些操作需要由编辑器进行操作(undo, redo)，
   //需要webpage将操作反馈给webview来执行编辑器操作
    if (act == QWebEnginePage::Paste)
    {
        tryResetTitle();
    }
    else if (QWebEnginePage::Undo == act)
    {
        //WizGetAnalyzer().LogAction("Undo");
        undo();
    }
    else if (QWebEnginePage::Redo == act)
    {
        //WizGetAnalyzer().LogAction("Redo");
        redo();
    }
}

void CWizDocumentWebView::tryResetTitle()
{
    if (m_bNewNoteTitleInited)
        return;

    // if note already modified, maybe title changed by use manuallly
    if (view()->note().tCreated.secsTo(view()->note().tModified) != 0)
        return;

    //
    page()->toPlainText([=](const QString& text){
        QString strTitle = WizStr2Title(text.left(128));
        if (strTitle.isEmpty())
            return;

        view()->resetTitle(strTitle);
        //
        m_bNewNoteTitleInited = true;
    });
}

void CWizDocumentWebView::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();

    int nAccepted = 0;
    if (mimeData->hasUrls())
    {
        QList<QUrl> li = mimeData->urls();
        QList<QUrl>::const_iterator it;
        for (it = li.begin(); it != li.end(); it++) {
            QUrl url = *it;
            url.setScheme(0);

            //paste image file as images, add attachment for other file
            QString strFileName = url.toString();
#ifdef Q_OS_MAC
            if (wizIsYosemiteFilePath(strFileName))
            {
                strFileName = wizConvertYosemiteFilePathToNormalPath(strFileName);
            }
#endif
//            QImageReader reader(strFileName);
            QFileInfo info(strFileName);

            //FIXME: //TODO:  应该和附件列表中的添加附件合并
            if (info.size() == 0)
            {
                CWizMessageBox::warning(nullptr , tr("Info"), tr("Can not add a 0 bit size file as attachment! File name : ' %1 '").arg(strFileName));
                continue;
            }
            else if (info.isBundle())
            {
                CWizMessageBox::warning(nullptr, tr("Info"), tr("Can not add a bundle file as attachment! File name : ' %1 '").arg(strFileName));
                continue;
            }

            QList<QByteArray> imageFormats = QImageReader::supportedImageFormats();
            if (imageFormats.contains(info.suffix().toUtf8()))
            {
                QString strHtml;
                if (WizImage2Html(strFileName, strHtml, noteResourcesPath())) {
                    editorCommandExecuteInsertHtml(strHtml, true);
                    nAccepted++;
                }
            }
            else
            {
                WIZDOCUMENTDATA doc = view()->note();
                CWizDatabase& db = m_dbMgr.db(doc.strKbGUID);
                WIZDOCUMENTATTACHMENTDATA data;
                data.strKbGUID = doc.strKbGUID; // needed by under layer
                if (!db.AddAttachment(doc, strFileName, data))
                {
                    TOLOG1("[drop] add attachment failed %1", strFileName);
                    continue;
                }
                addAttachmentThumbnail(strFileName, data.strGUID);
                nAccepted ++;
            }
        }
    }
    else if (mimeData->hasFormat(WIZNOTE_MIMEFORMAT_DOCUMENTS))
    {
        QString strData(mimeData->data(WIZNOTE_MIMEFORMAT_DOCUMENTS));
        if (!strData.isEmpty())
        {
            QString strLinkHtml;
            QStringList doclist = strData.split(';');
            foreach (QString doc, doclist) {
                QStringList docIds = doc.split(':');
                if (docIds.count() == 2)
                {
                    WIZDOCUMENTDATA document;
                    CWizDatabase& db = m_dbMgr.db(docIds.first());
                    if (db.DocumentFromGUID(docIds.last(), document))
                    {
                        QString strHtml, strLink;
                        WizNoteToHtmlLink(document, strHtml, strLink);
                        strLinkHtml += "<span>&nbsp;" + strHtml + "&nbsp;</span>";
                    }
                }
            }

            editorCommandExecuteInsertHtml(strLinkHtml, false);

            nAccepted ++;
        }
    }

    if (nAccepted > 0) {
        event->accept();
        trySaveDocument(view()->note(), false, [=](const QVariant&){});
    }
}

CWizDocumentView* CWizDocumentWebView::view()
{
    QWidget* pParent = parentWidget();
    while(pParent) {
        if (CWizDocumentView* view = dynamic_cast<CWizDocumentView*>(pParent)) {
            return view;
        }

        pParent = pParent->parentWidget();
    }

    return 0;
}

void CWizDocumentWebView::clear()
{
    load(QUrl("about:blank"));
}

void CWizDocumentWebView::onTimerAutoSaveTimout()
{
    trySaveDocument(view()->note(), false, [=](const QVariant&){});
}

void CWizDocumentWebView::onTitleEdited(QString strTitle)
{
    WIZDOCUMENTDATA document = view()->note();
    document.strTitle = strTitle;
    // Only sync when contents unchanged. If contents changed would sync after document saved.
    isModified([=](bool modified){

        if (modified)
        {
            MainWindow* mainWindow = qobject_cast<MainWindow*>(m_app.mainWindow());
            mainWindow->quickSyncKb(document.strKbGUID);
        }
    });
}

void CWizDocumentWebView::onDocumentReady(const QString kbGUID, const QString strGUID, const QString strFileName, WizEditorMode editorMode)
{
    m_mapFile.insert(strGUID, strFileName);

    WIZDOCUMENTDATA doc;
    if (!m_dbMgr.db(kbGUID).DocumentFromGUID(strGUID, doc))
        return;

    //
    loadDocumentInWeb(editorMode);
}

void CWizDocumentWebView::onDocumentSaved(const QString kbGUID, const QString strGUID, bool ok)
{
    if (!ok)
    {
        TOLOG("Save document failed");
    }
    //
    view()->sendDocumentSavedSignal(strGUID, kbGUID);
    //
    MainWindow* mainWindow = qobject_cast<MainWindow*>(m_app.mainWindow());
    mainWindow->quickSyncKb(kbGUID);
}

void CWizDocumentWebView::viewDocument(const WIZDOCUMENTDATA& doc, WizEditorMode editorMode)
{
    if (!m_docLoadThread)
        return;
    // set data
    int seconds = doc.tCreated.secsTo(QDateTime::currentDateTime());
    m_bNewNote = (seconds >= 0 && seconds <= 1) ? true : false;
    m_bNewNoteTitleInited = m_bNewNote ? false : true;
    //
    setModified(false);

    if (m_bNewNote)
    {
        editorMode = modeEditor;
    }

    // ask extract and load
    m_docLoadThread->load(doc, editorMode);
}

void CWizDocumentWebView::reloadNoteData(const WIZDOCUMENTDATA& data)
{
    if (!m_docLoadThread)
        return;
    //
    Q_ASSERT(!data.strGUID.isEmpty());

    // reset only if user not in editing mode
    if (isEditing() && hasFocus())
        return;

    // reload may triggered when update from server or locally reflected by modify
    m_docLoadThread->load(data, m_currentEditorMode);
}


void CWizDocumentWebView::setNoteTitleInited(bool inited)
{
    m_bNewNoteTitleInited = inited;
}

void CWizDocumentWebView::setInSeperateWindow(bool inSeperateWindow)
{
    m_bInSeperateWindow = inSeperateWindow;
}

bool CWizDocumentWebView::isInSeperateWindow() const
{
    return m_bInSeperateWindow;
}

void CWizDocumentWebView::replaceDefaultCss(QString& strHtml)
{
    QString strFileName = Utils::PathResolve::resourcesPath() + "files/wizeditor/default.css";
    QFile f(strFileName);
    if (!f.open(QIODevice::ReadOnly)) {
        qDebug() << "[Editor]Failed to get default css code";
        return;
    }
    //
    QString strCss;
    if (!WizLoadUnicodeTextFromFile(strFileName, strCss))
        return;
    //
    QString strFont = m_app.userSettings().defaultFontFamily();
    int nSize = m_app.userSettings().defaultFontSize();

    strCss.replace("/*default-font-family*/", QString("font-family:%1;").arg(strFont));
    strCss.replace("/*default-font-size*/", QString("font-size:%1px;").arg(nSize));
    QString backgroundColor = m_app.userSettings().editorBackgroundColor();
    if (backgroundColor.isEmpty())
    {
        backgroundColor = m_bInSeperateWindow ? "#F5F5F5" : "#FFFFFF";
    }
    strCss.replace("/*default-background-color*/", QString("background-color:%1;").arg(backgroundColor));
    //
    const QString customCssId("wiz_custom_css");

    WizHtmlRemoveStyle(strHtml, customCssId);
    WizHtmlInsertStyle(strHtml, customCssId, strCss);
}

void CWizDocumentWebView::editorResetFont()
{
    WIZDOCUMENTDATA data = view()->note();
    trySaveDocument(data, false, [=](const QVariant& vRet){
        //
        reloadNoteData(data);
    });
}

void CWizDocumentWebView::editorFocus()
{
    page()->runJavaScript("WizEditor.focus();");
    emit focusIn();
}

void CWizDocumentWebView::enableEditor(bool enalbe)
{
    if (enalbe)
    {
        page()->runJavaScript("WizEditor.on();");
        setFocus();
    }
    else
    {
        QString code = QString("WizEditor.off({noteType:'%1'});").arg(getNoteType());
        //
        page()->runJavaScript(code);
        //
        clearFocus();
    }
}

void CWizDocumentWebView::setIgnoreActiveWindowEvent(bool igoreEvent)
{
    m_ignoreActiveWindowEvent = igoreEvent;
}

bool CWizDocumentWebView::evaluateJavaScript(const QString& js)
{
    page()->runJavaScript(js);
    return true;
}


void CWizDocumentWebView::on_insertCommentToNote_request(const QString& docGUID, const QString& comment)
{
    if (docGUID != view()->note().strGUID)
        return;

    if (!isEditing() && WizIsMarkdownNote(view()->note()))
    {
        CWizMessageBox::information(this, tr("Info"), tr("Do not support insert comment into markdown note."));
        return;
    }
    //
    //应该优化，对于模版类型的笔记，插入位置不应该是最前面
    if (isEditing())
    {
        QString htmlBody = "<div>" + comment + "</div><hr>";
        htmlBody.replace("\n", "<br>");
        htmlBody.replace("'", "&#39;");
        //
        QString code = QString("var objDiv = document.createElement('div');objDiv.innerHTML= unescape('%1');"
                               "var first=document.body.firstChild;document.body.insertBefore(objDiv,first);").arg(htmlBody);
        //
        page()->runJavaScript(code);
        //
        saveEditingViewDocument(view()->note(), true, [=](const QVariant &){});

    }
    else
    {
        QString commentsText = QUrl::fromPercentEncoding(comment.toUtf8());
        QString commentsHtml = ::WizText2Html(commentsText);
        //
        QString documentGuid = docGUID;
        QString kbGuid = view()->note().strKbGUID;
        //
        ::WizExecutingActionDialog::executeAction(QObject::tr("Saving comments..."), WIZ_THREAD_DEFAULT, [=]{
            //
            CWizDatabase& db = m_dbMgr.db(kbGuid);
            //
            WIZDOCUMENTDATA doc;
            if (!db.DocumentFromGUID(documentGuid, doc))
                return;
            //
            QString strTempPath = ::Utils::PathResolve::tempPath() + WizGenGUIDLowerCaseLetterOnly() + "/";
            ::WizEnsurePathExists(strTempPath);
            //
            if (!db.DocumentToHtmlFile(doc, strTempPath))
                return;
            //
            QString htmlFileName = strTempPath + "index.html";
            if (!QFileInfo::exists(htmlFileName))
                return;
            //
            QString html;
            if (!WizLoadUnicodeTextFromFile(htmlFileName, html))
                return;
            //
            WizHtmlInsertHtmlBeforeAllBodyChildren(html, commentsHtml);
            //
            if (!::WizSaveUnicodeTextToUtf8File(htmlFileName, html, true))
                return;
            //
            db.UpdateDocumentDataWithFolder(doc, strTempPath, true);
        });
    }
}

void CWizDocumentWebView::setWindowVisibleOnScreenShot(bool bVisible)
{
    MainWindow* mainWindow = qobject_cast<MainWindow *>(m_app.mainWindow());
    if (mainWindow)
    {
        bVisible ? mainWindow->show() : mainWindow->hide();
    }
}

QString CWizDocumentWebView::noteResourcesPath()
{
    Q_ASSERT(!m_strNoteHtmlFileName.isEmpty());
    if (m_strNoteHtmlFileName.isEmpty())
        return QString();
    //
    QString path = Utils::Misc::extractFilePath(m_strNoteHtmlFileName);
    path += "index_files/";
    Utils::Misc::ensurePathExists(path);
    //
    return path;
}

void CWizDocumentWebView::insertImage(const QString& strFileName)
{
    QString strHtml;
    if (WizImage2Html(strFileName, strHtml, noteResourcesPath())) {
        editorCommandExecuteInsertHtml(strHtml, true);
    }
}


void CWizDocumentWebView::addAttachmentThumbnail(const QString strFile, const QString& strGuid)
{
    QImage img;
    ::WizCreateThumbnailForAttachment(img, strFile, QSize(32, 32));
    QString strDestFile =Utils::PathResolve::tempPath() + WizGenGUIDLowerCaseLetterOnly() + ".png";
    img.save(strDestFile, "PNG");
    QString strLink = QString("wiz://open_attachment?guid=%1").arg(strGuid);
    QSize szImg = img.size();
    if (WizIsHighPixel())
    {
        szImg.scale(szImg.width() / 2, szImg.height() / 2, Qt::IgnoreAspectRatio);
    }
    QString strHtml = WizGetImageHtmlLabelWithLink(strDestFile, szImg, strLink);
    editorCommandExecuteInsertHtml(strHtml, true);
}

void CWizDocumentWebView::getMailSender(std::function<void(QString)> callback)
{
    QString scriptFileName = Utils::PathResolve::resourcesPath() + "files/scripts/GetMailSender.js";
    QString code;
    ::WizLoadUnicodeTextFromFile(scriptFileName, code);
    //
    page()->runJavaScript(code, [=](const QVariant& vRet){
        //
        QString mailSender = vRet.toString();

        if (mailSender.isEmpty())
        {
            QRegExp rxlen("\\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,4}\\b");
            rxlen.setCaseSensitivity(Qt::CaseInsensitive);
            rxlen.setPatternSyntax(QRegExp::RegExp);
            QString strTitle = view()->note().strTitle;
            int pos = rxlen.indexIn(strTitle);
            if (pos > -1) {
                mailSender = rxlen.cap(0);
            }
        }
        //
        callback(mailSender);
    });
}

///*
// * 是否添加用户自定义的样式，目前有字体类型、字体大小、背景颜色等信息等信息
// */
//bool CWizDocumentWebView::shouldAddUserDefaultCSS()
//{
//    if (!shouldAddCustomCSS())
//        return false;

//    bool isTemplate = page()->runJavaScript("wizIsTemplate()").toBool();

//    return !isTemplate;
//}

void CWizDocumentWebView::shareNoteByEmail()
{
    getMailSender([=](QString sendTo){
        //

        CWizEmailShareDialog dlg(m_app);
        //
        dlg.setNote(view()->note(), sendTo);
        //
        connect(&dlg, SIGNAL(insertCommentToNoteRequest(QString,QString)),
                SLOT(on_insertCommentToNote_request(QString,QString)));

        dlg.exec();

    });
}

void CWizDocumentWebView::shareNoteByLink()
{
    const WIZDOCUMENTDATA& doc = view()->note();
    emit shareDocumentByLinkRequest(doc.strKbGUID, doc.strGUID);
}

QString CWizDocumentWebView::getNoteType()
{
    const WIZDOCUMENTDATA& doc = view()->note();
    //
    QString title = doc.strTitle;
    if (title.endsWith(".md"))
        return "markdown";
    if (title.endsWith(".mj"))
        return "mathjax";
    //
    if (title.indexOf(".md@") != -1)
        return "markdown";
    if (title.indexOf(".mj@") != -1)
        return "mathjax";
    //
    if (title.indexOf(".md ") != -1)
        return "markdown";
    if (title.indexOf(".mj ") != -1)
        return "mathjax";
    //
    return "common";
}

void CWizDocumentWebView::onEditorLoadFinished(bool ok)
{
    if (!ok)
        return;
    //
    QString resPath = Utils::PathResolve::resourcesPath() + "files/";
    QString editorPath = resPath + "wizeditor/";
    QString lang = "zh-cn";
    QString userGUID = m_dbMgr.db().GetUserGUID();
    QString userAlias = m_dbMgr.db().GetUserAlias();
    //
    const WIZDOCUMENTDATA& doc = view()->note();
    bool ignoreTable = doc.strURL.startsWith("http");
    //
    QString noteType = getNoteType();
    //
    QString strCode = WizFormatString6("WizEditorInit(\"%1\", \"%2\", \"%3\", \"%4\", %5, \"%6\");",
                                       editorPath, lang, userGUID, userAlias,
                                       ignoreTable ? "true" : "false",
                                       noteType);
    qDebug() << strCode;
    if (m_currentEditorMode == modeEditor)
    {
        strCode += "WizEditor.on();";
    }
    else
    {
        strCode += "WizEditor.off();";
    }
    //
    page()->runJavaScript(strCode);
}

void CWizDocumentWebView::onEditorLinkClicked(QUrl url, QWebEnginePage::NavigationType navigationType, bool isMainFrame, WizWebEnginePage* page)
{
    if (!isMainFrame)
        return;
    //
    page->stopCurrentNavigation();
    //
    if (isInternalUrl(url))
    {
        QString strUrl = url.toString();
        switch (GetWizUrlType(strUrl)) {
        case WizUrl_Document:
            viewDocumentByUrl(strUrl);
            return;
        case WizUrl_Attachment:
            if (!isEditing())
            {
                viewAttachmentByUrl(view()->note().strKbGUID, strUrl);
            }
            return;
        default:
            qDebug() << QString("%1 is a wiz internal url , but we can not identify it");
            return;
        }
    }
    else
    {
        QString strUrl = url.toString();
        if (strUrl.left(12) == "http://file/")
        {
            strUrl.replace(0, 12, "file:/");
        }

        qDebug() << "Open url " << strUrl;
        QDesktopServices::openUrl(strUrl);
        return;
    }
}

bool CWizDocumentWebView::isInternalUrl(const QUrl& url)
{
    return url.scheme().toLower() == "wiz";
}

bool WizStringList2Map(const QStringList& list, QMap<QString, QString>& map)
{
    for (int i = 0; i < list.size(); i++) {
        int indx = list[i].indexOf("=");
        if (indx == -1) {
            return false;
        }

        qDebug() << "key: " << list[i].left(indx).toLower();
        qDebug() << "value: " << list[i].mid(indx + 1);

        map.insert(list[i].left(indx).toLower(), list[i].mid(indx + 1));
    }

    return true;
}

void CWizDocumentWebView::viewDocumentByUrl(const QString& strUrl)
{
    if (strUrl.isEmpty())
        return;

    MainWindow* mainWindow = qobject_cast<MainWindow *>(m_app.mainWindow());
    mainWindow->viewDocumentByWizKMURL(strUrl);
}

void CWizDocumentWebView::viewAttachmentByUrl(const QString& strKbGUID, const QString& strUrl)
{
    if (strUrl.isEmpty())
        return;

    MainWindow* mainWindow = qobject_cast<MainWindow *>(m_app.mainWindow());
    mainWindow->viewAttachmentByWizKMURL(strKbGUID, strUrl);
}

void getHtmlBodyStyle(const QString& strHtml, QString& strBodyStyle)
{
    QRegExp regh("<body ([^>]*)>", Qt::CaseInsensitive);
    if (regh.indexIn(strHtml) != -1)
    {
        strBodyStyle = regh.cap(1);
    }
}

void CWizDocumentWebView::saveEditingViewDocument(const WIZDOCUMENTDATA &data, bool force, std::function<void(const QVariant &)> callback)
{
    //FIXME: remove me, just for find a image losses bug.
    Q_ASSERT(!data.strGUID.isEmpty());

    // check note permission
    if (!m_dbMgr.db(data.strKbGUID).CanEditDocument(data)) {
        callback(QVariant(false));
        return;
    }
    //
    WIZDOCUMENTDATA doc = data;
    //
    isModified([=](bool modified) {

        if (!force)
        {
            if (!modified)
            {
                callback(QVariant(true));
                return;
            }
        }
        //
        setModified(false);
        //
        QString strFileName = m_mapFile.value(data.strGUID);
        //
        /*
        CString strResourcePath = m_browser.GetResourceFilePath();
        CString strResourcePathParam = strResourcePath;
        strResourcePathParam.Replace(_T("\\"), _T("\\\\"));
        //
        //
        CString strScript = WizFormatString1(CString(
            "WizEditor.clearWizDom();\n"
            "WizEditor.img.saveRemote('%1');\n"
            "WizEditor.getContentHtml()"), strResourcePathParam);
            */
        //
        QString strScript =
            "WizEditor.clearWizDom();\n"
            "WizEditor.getContentHtml()";
        //
        page()->runJavaScript(strScript, [=](const QVariant& ret){
            //
            bool succeeded = false;
            if (ret.type() == QVariant::String)
            {
                QString html = ret.toString();
                if (!html.isEmpty())
                {
                    succeeded = true;
                    m_currentNoteHtml = html;
                    emit currentHtmlChanged();
                    //
                    qDebug() << m_currentNoteHtml;
                    //
                    m_docSaverThread->save(doc, html, strFileName, 0);
                }
            }
            callback(QVariant(succeeded));
        });
        //
    });

}

void CWizDocumentWebView::saveReadingViewDocument(const WIZDOCUMENTDATA &data, bool force, std::function<void(const QVariant &)> callback)
{
    Q_UNUSED(force);
    //
    const WIZDOCUMENTDATA doc = data;

    QString strScript = QString("WizReader.todo.closeDocument();");
    page()->runJavaScript(strScript, [=](const QVariant& vRet) {
        //
        QString strHtml = vRet.toString();
        //
        if (!strHtml.isEmpty())
        {
            if (!doc.strGUID.isEmpty())
            {
                QString strFileName = m_mapFile.value(doc.strGUID);
                if (!strFileName.isEmpty())
                {
                    m_docSaverThread->save(doc, strHtml, strFileName, 0);
                }
            }
        }
        //
        callback(true);
    });
}


void CWizDocumentWebView::applySearchKeywordHighlight()
{
    MainWindow* window = qobject_cast<MainWindow *>(m_app.mainWindow());
    QString strKeyWords = window->searchKeywords();
    if (!strKeyWords.isEmpty() && !hasFocus())
    {
        QStringList keyList = strKeyWords.split(getWizSearchSplitChar());
        foreach (QString strText, keyList)
        {
            findText(strText);
        }
    }
    else
    {
        findText("");
    }
}

void CWizDocumentWebView::clearSearchKeywordHighlight()
{
    findText("");
}

void CWizDocumentWebView::on_insertCodeHtml_requset(QString strOldHtml)
{
    QString strHtml = strOldHtml;
    if (WizGetBodyContentFromHtml(strHtml, false))
    {
        QString name("wiz_code_highlight.css");
        QString strSrcCssFileName = Utils::PathResolve::resourcesPath() + "files/code/" + name;
        QString strDestCssFileName = noteResourcesPath() + name;

        if (QFile::exists(strDestCssFileName))
        {
            QFile::remove(strDestCssFileName);
        }
        if (!QFile::copy(strSrcCssFileName, strDestCssFileName))
        {
            QMessageBox::critical(this, tr("Error"), tr("Can't copy style files"));
            return;
        }
        //
        QString link = "index_files/" + name;
        //
        page()->runJavaScript(QString("WizAddCssForCode('%1');").arg(link));
        //
        editorCommandExecuteInsertHtml(strHtml, true);
    }
}


void CWizDocumentWebView::getAllEditorScriptAndStypeFileName(QStringList& arrayFile)
{
    QString strResourcePath = Utils::PathResolve::resourcesPath();
    QString strHtmlEditorPath = strResourcePath + "files/wizeditor/";
    //
    QString strEditorJS = strHtmlEditorPath + _T("wizEditorForMac.js");
    QString strInit = strHtmlEditorPath + _T("editorHelper.js");
    //
    arrayFile.empty();
    arrayFile.push_back(strEditorJS);
    arrayFile.push_back(strInit);
}



void CWizDocumentWebView::insertScriptAndStyleCore(QString& strHtml, const QStringList& arrayFiles)
{
    Q_ASSERT(!arrayFiles.empty());
    for (auto it : arrayFiles)
    {
        Q_ASSERT(PathFileExists(it));
        //
        QString strFileName(it);
        QString strExt = Utils::Misc::extractFileExt(strFileName);
        if (0 == strExt.compare(".css", Qt::CaseInsensitive))
        {
            QString strTag = WizFormatString1("<link rel=\"stylesheet\" type=\"text/css\" \
href=\"file:///%1\" wiz_style=\"unsave\" charset=\"utf-8\">", strFileName);
            //
            if (strHtml.indexOf(strTag) == -1)
            {
                WizHTMLAppendTextInHead(strTag, strHtml);
            }
        }
        else if (0 == strExt.compare(".js", Qt::CaseInsensitive))
        {
            QString	strTag = WizFormatString1(
                    "<script type=\"text/javascript\" src=\"file:///%1\" wiz_style=\"unsave\" charset=\"utf-8\"></script>",
                    strFileName);
            //
            if (strHtml.indexOf(strTag) == -1)
            {
                WizHTMLAppendTextInHead(strTag, strHtml);
            }
        }
        else
        {
            Q_ASSERT(false);
        }
    }
    //
    QString strTemplateJsFileName = ::Utils::PathResolve::wizTemplateJsFilePath();
    if (QFileInfo(strTemplateJsFileName).exists())
    {
        QString strTag = QString("<script type=\"text/javascript\" src=\"file:///%1\" wiz_style=\"unsave\" charset=\"utf-8\"></script>").arg(strTemplateJsFileName);
        //
        WizHTMLAppendTextInHead(strTag, strHtml);
    }
}

void CWizDocumentWebView::loadDocumentInWeb(WizEditorMode editorMode)
{
    QString strGUID = view()->note().strGUID;
    QString strFileName = m_mapFile.value(strGUID);
    if (strFileName.isEmpty()) {
        return;
    }
    //
    QString strHtml;
    bool ret = WizLoadUnicodeTextFromFile(strFileName, strHtml);
    if (!ret) {
        // hide client and show error
        return;
    }
    //
    m_currentNoteHtml = strHtml;
    emit currentHtmlChanged();

    WizEditorMode oldMode = m_currentEditorMode;
    m_currentEditorMode = editorMode;
    if (oldMode != m_currentEditorMode)
    {
        if (m_currentEditorMode == modeEditor) {
            Q_EMIT focusIn();
        } else {
            Q_EMIT focusOut();
        }
        //
        view()->titleBar()->setEditorMode(m_currentEditorMode);
    }
    //
    QStringList arrayFiles;
    getAllEditorScriptAndStypeFileName(arrayFiles);
    insertScriptAndStyleCore(strHtml, arrayFiles);
    //
    replaceDefaultCss(strHtml);
    //
    ::WizSaveUnicodeTextToUtf8File(strFileName, strHtml, true);
    //
    m_strNoteHtmlFileName = strFileName;
    load(QUrl::fromLocalFile(strFileName));

    //Waiting for the editor initialization complete if it's the first time to load a document.
    QTimer::singleShot(100, this, SLOT(applySearchKeywordHighlight()));

    onNoteLoadFinished();
}

void CWizDocumentWebView::onNoteLoadFinished()
{
    WizGlobal::instance()->emitViewNoteLoaded(view(), view()->note(), true);
}

void CWizDocumentWebView::setEditorMode(WizEditorMode editorMode)
{
    if (m_currentEditorMode == editorMode)
        return;
    //
    bool editing = editorMode == modeEditor;
    // show editor toolbar properly
    if (!editing && hasFocus()) {
        Q_EMIT focusOut();
    }

    if (editing && hasFocus()) {
        Q_EMIT focusIn();
    }

    WIZDOCUMENTDATA docData;
    CWizDatabase& db = m_dbMgr.db(view()->note().strKbGUID);
    if (!db.DocumentFromGUID(view()->note().strGUID, docData))
        return;

    trySaveDocument(docData, false, [=](const QVariant&){});

    m_currentEditorMode = editorMode;
    //
    enableEditor(editing);
    //
    if (editing) {
        setFocus(Qt::MouseFocusReason);
        editorFocus();
    }
}

void CWizDocumentWebView::trySaveDocument(const WIZDOCUMENTDATA& data, bool force, std::function<void(const QVariant &)> callback)
{
    if (!view()->noteLoaded())  //encrypting note & has been loaded
    {
        callback(QVariant(false));
        return;
    }

    if (m_currentEditorMode == modeEditor)
    {
        saveEditingViewDocument(data, force, callback);
    }
    else
    {
        saveReadingViewDocument(data, force, callback);
    }
}

void CWizDocumentWebView::editorCommandQueryCommandValue(const QString& strCommand, std::function<void(const QString& value)> callback)
{
    QString script = "document.queryCommandValue('" + strCommand +"');";
    page()->runJavaScript(script, [=](const QVariant& ret){
        //
        callback(ret.toString());
        //
    });
}

void CWizDocumentWebView::editorCommandQueryCommandState(const QString& strCommand, std::function<void(int state)> callback)
{
    QString script = "document.queryCommandState('" + strCommand +"');";
    page()->runJavaScript(script, [=](const QVariant& ret){
        //
        callback(ret.toInt());
        //
    });
}

/*
 * Execute command and also save status to undostack.
 * All commands execute from client which may modify document MUST invoke this
 * instead of use frame's evaluateJavascript.
 */
void CWizDocumentWebView::editorCommandExecuteCommand(const QString& strCommand,
                                                      const QString& arg1 /* = QString() */,
                                                      const QString& arg2 /* = QString() */,
                                                      const QString& arg3 /* = QString() */)
{
    QString strExec = QString("document.execCommand('%1'").arg(strCommand);
    if (!arg1.isEmpty()) {
        strExec += ", " + arg1;
    }

    if (!arg2.isEmpty()) {
        strExec += ", " + arg2;
    }

    if (!arg3.isEmpty()) {
        strExec += ", " + arg3;
    }

    strExec += ");";

    qDebug() << strExec;

    page()->runJavaScript(strExec);
    //
    setModified(true);
}


bool CWizDocumentWebView::editorCommandQueryMobileFileReceiverState()
{
    return m_app.userSettings().receiveMobileFile();
}

void CWizDocumentWebView::editorCommandExecuteParagraph(const QString& strType)
{
    WizGetAnalyzer().LogAction("editorParagraph");
    editorCommandExecuteCommand("formatBlock", "false", "'" + strType + "'");
}

void CWizDocumentWebView::editorCommandExecuteInsertHtml(const QString& strHtml, bool bNotSerialize)
{
    QString s = bNotSerialize ? "true" : "false";
    //
    QString base64Html = WizStringToBase64(strHtml);
    base64Html.replace("\r", "");
    base64Html.replace("\n", "");
    QString code = QString("WizEditor.insertB64Html('%1')").arg(base64Html);
    //
    page()->runJavaScript(code);
    //editorCommandExecuteCommand("insertHtml", s, "'" + strHtml + "'");
}


void CWizDocumentWebView::editorCommandExecutePastePlainText()
{
    QClipboard* clip = QApplication::clipboard();
    if (!clip)
        return;
    const QMimeData* data = clip->mimeData();
    if (!data)
        return;
    QString text = data->text();
    //
    QString html = WizText2Html(text);

    editorCommandExecuteInsertHtml(html, false);
}

void CWizDocumentWebView::editorCommandExecuteIndent()
{
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("indent");
    editorCommandExecuteCommand("indent");
}

void CWizDocumentWebView::editorCommandExecuteOutdent()
{
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("outdent");
    editorCommandExecuteCommand("outdent");
}

void CWizDocumentWebView::editorCommandExecuteLinkInsert()
{
    if (!m_editorInsertLinkForm) {
        m_editorInsertLinkForm = new CWizEditorInsertLinkForm(window());
        connect(m_editorInsertLinkForm, SIGNAL(accepted()), SLOT(on_editorCommandExecuteLinkInsert_accepted()));
    }
    //
    page()->runJavaScript("WizEditor.link.queryCurrentLink();", [=](const QVariant& vLink){
        //
        QString strUrl = vLink.toString();
        //
        m_editorInsertLinkForm->setUrl(strUrl);

        m_editorInsertLinkForm->exec();

        CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
        analyzer.LogAction("linkInsert");
        //
    });
}

void CWizDocumentWebView::on_editorCommandExecuteLinkInsert_accepted()
{
    // append http if not exist
    QString strUrl = m_editorInsertLinkForm->getUrl();
    QUrl url(strUrl);
    if (url.scheme().isEmpty())
    {
        strUrl = "http://" + strUrl;
    }
    else
    {
        strUrl = url.toString();
    }
    //
    QString code = QString("WizEditor.link.setCurrentLink('%1');").arg(strUrl);
    //
    page()->runJavaScript(code);
}

void CWizDocumentWebView::editorCommandExecuteLinkRemove()
{
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("unlink");
    editorCommandExecuteCommand("unlink");
}

void CWizDocumentWebView::editorCommandExecuteFindReplace()
{
    if (!m_searchReplaceWidget)
    {
        m_searchReplaceWidget = new CWizSearchReplaceWidget(this);
        //
        connect(m_searchReplaceWidget, SIGNAL(findPre(QString,bool)), SLOT(findPre(QString,bool)));
        connect(m_searchReplaceWidget, SIGNAL(findNext(QString,bool)), SLOT(findNext(QString,bool)));
        connect(m_searchReplaceWidget, SIGNAL(replaceCurrent(QString,QString)), SLOT(replaceCurrent(QString,QString)));
        connect(m_searchReplaceWidget, SIGNAL(replaceAndFindNext(QString,QString,bool)), SLOT(replaceAndFindNext(QString,QString,bool)));
        connect(m_searchReplaceWidget, SIGNAL(replaceAll(QString,QString,bool)), SLOT(replaceAll(QString,QString,bool)));
    }

    QRect rect = geometry();
    rect.moveTo(mapToGlobal(pos()));
    m_searchReplaceWidget->showInEditor(rect);

    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("findReplace");

}

void CWizDocumentWebView::innerFindText(QString text, bool next, bool matchCase)
{
    bool back = !next;
    QString code = QString("find('%1', %2, %3, true)").arg(text)
            .arg(back ? "true" : "false")
            .arg(matchCase ? "true" : "false");
    page()->runJavaScript(code);

}

static QString strOldSearchText = "";
static bool strOldCase = false;
void CWizDocumentWebView::findPre(QString strTxt, bool bCasesensitive)
{
    //FIXME:  there is a problem here, HighlightAllOccurrences can not be used togethor with find one.
    if (strOldSearchText != strTxt || strOldCase != bCasesensitive)
    {
        // clear highlight
        //findText("");
        strOldSearchText = strTxt;
        strOldCase = bCasesensitive;
    }
    QWebEnginePage::FindFlags options;
    options |= QWebEnginePage::FindBackward;
    if (bCasesensitive)
    {
        options |= QWebEnginePage::FindCaseSensitively;
    }
    //
    //findText(strTxt, options);
    innerFindText(strTxt, false, bCasesensitive);
}

void CWizDocumentWebView::findNext(QString strTxt, bool bCasesensitive)
{
    if (strOldSearchText != strTxt || strOldCase != bCasesensitive)
    {
        //findText("", 0);
        strOldSearchText = strTxt;
        strOldCase = bCasesensitive;
    }
    QWebEnginePage::FindFlags options;
    if (bCasesensitive)
    {
        options |= QWebEnginePage::FindCaseSensitively;
    }
    //
    //findText(strTxt, options);
    innerFindText(strTxt, true, bCasesensitive);
}

void CWizDocumentWebView::replaceCurrent(QString strSource, QString strTarget)
{
    QString strExec = QString("WizEditor.replace('%1', '%2', true)").arg(strSource).arg(strTarget);
    page()->runJavaScript(strExec);
}

void CWizDocumentWebView::replaceAndFindNext(QString strSource, QString strTarget, bool bCasesensitive)
{
    QString strExec = QString("WizEditor.replace('%1', '%2', %3)").arg(strSource).arg(strTarget).arg(bCasesensitive ? "true" : "false");
    page()->runJavaScript(strExec);
}

void CWizDocumentWebView::replaceAll(QString strSource, QString strTarget, bool bCasesensitive)
{
    QString strExec = QString("WizEditor.replaceAll('%1', '%2', %3)").arg(strSource).arg(strTarget).arg(bCasesensitive ? "true" : "false");
    page()->runJavaScript(strExec);
    setModified(true);
}

void CWizDocumentWebView::editorCommandExecuteFontFamily(const QString& strFamily)
{
    WizGetAnalyzer().LogAction(QString("editorSetFontFamily : %1").arg(strFamily));
    editorCommandExecuteCommand("fontName", "false", "'" + strFamily + "'");
}

void CWizDocumentWebView::editorCommandExecuteFontSize(const QString& strSize)
{
    WizGetAnalyzer().LogAction(QString("editorSetFontSize : %1pt").arg(strSize));
    //
    CString strStyle = WizFormatString1(_T("{\\\"font-size\\\" : \\\"%1pt\\\"}"), strSize);
    CString strScript = WizFormatString1(_T("WizEditor.modifySelectionDom(JSON.parse(\"%1\"));"), strStyle);

    page()->runJavaScript(strScript);
    setModified(true);
}

void CWizDocumentWebView::editorCommandExecuteBackColor(const QColor& color)
{
    if (color == QColor(Qt::transparent)) {
        editorCommandExecuteCommand("backColor", "false", "'default'");
    }
    else {
        editorCommandExecuteCommand("backColor", "false", "'" + color.name() + "'");
    }
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("backColor");
}

void CWizDocumentWebView::editorCommandExecuteForeColor(const QColor& color)
{
    if (color == QColor(Qt::transparent)) {
        editorCommandExecuteCommand("foreColor", "false", "'default'");
    }
    else {
        editorCommandExecuteCommand("foreColor", "false", "'" + color.name() + "'");
    }
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("foreColor");
}

void CWizDocumentWebView::editorCommandExecuteBold()
{
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("bold");
    editorCommandExecuteCommand("bold");
}

void CWizDocumentWebView::editorCommandExecuteItalic()
{
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("italic");
    editorCommandExecuteCommand("italic");
}

void CWizDocumentWebView::editorCommandExecuteUnderLine()
{
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("underline");
    editorCommandExecuteCommand("underline");
}

void CWizDocumentWebView::editorCommandExecuteStrikeThrough()
{
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("strikethrough");
    editorCommandExecuteCommand("strikethrough");
}

void CWizDocumentWebView::editorCommandExecuteJustifyLeft()
{
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("justifyLeft");
    editorCommandExecuteCommand("JustifyLeft");
}

void CWizDocumentWebView::editorCommandExecuteJustifyRight()
{
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("justifyRight");
    editorCommandExecuteCommand("JustifyRight");
}

void CWizDocumentWebView::editorCommandExecuteJustifyCenter()
{
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("justifyCenter");
    editorCommandExecuteCommand("JustifyCenter");
}

void CWizDocumentWebView::editorCommandExecuteJustifyJustify()
{
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("justifyJustify");
    editorCommandExecuteCommand("justify", "'justify'");
}

void CWizDocumentWebView::editorCommandExecuteInsertOrderedList()
{
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("insertOrderedList");
    editorCommandExecuteCommand("insertOrderedList");
}

void CWizDocumentWebView::editorCommandExecuteInsertUnorderedList()
{
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("insertUnorderedList");
    editorCommandExecuteCommand("insertUnorderedList");
}

void CWizDocumentWebView::editorCommandExecuteTableInsert(int row, int col)
{
    QString code = QString("WizEditor.table.insertTable(%1, %2);").arg(col).arg(row);
    //
    page()->runJavaScript(code);
}


void CWizDocumentWebView::on_editorCommandExecuteScreenShot_imageAccepted(QPixmap pix)
{
    QObject *ssSender = qobject_cast<QObject*>(sender());
    if (ssSender)
        delete ssSender;

    setWindowVisibleOnScreenShot(true);

    if (pix.isNull())
        return;

    CString strFileName = noteResourcesPath() + WizIntToStr(GetTickCount()) + ".png";
    if (!pix.save(strFileName)) {
        TOLOG("ERROR: Can't save clipboard image to file");
        return;
    }

    insertImage(strFileName);
}

void CWizDocumentWebView::on_editorCommandExecuteScreenShot_finished()
{
    QObject *ssSender = qobject_cast<QObject*>(sender());
    if (ssSender)
        delete ssSender;

    setWindowVisibleOnScreenShot(true);
}

void CWizDocumentWebView::editorCommandExecuteInsertHorizontal()
{
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("insertHorizontal");
    editorCommandExecuteCommand("InsertHorizontalRule");
}

void CWizDocumentWebView::editorCommandExecuteInsertCheckList()
{
    QString strExec = "WizEditor.todo.setTodo();";
    page()->runJavaScript(strExec);

    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("insertCheckList");
}

void CWizDocumentWebView::editorCommandExecuteInsertImage()
{
    static QString initPath = QDir::homePath();
    QStringList strImgFileList = QFileDialog::getOpenFileNames(0, tr("Image File"), initPath, tr("Images (*.png *.bmp *.gif *.jpg)"));
    if (strImgFileList.isEmpty())
        return;

    foreach (QString strImgFile, strImgFileList)
    {
        insertImage(strImgFile);
        //
        initPath = Utils::Misc::extractFilePath(strImgFile);
    }

    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("insertImage");
}

void CWizDocumentWebView::editorCommandExecuteInsertDate()
{
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("insertDate");
    //
    QString date = QDate::currentDate().toString(Qt::DefaultLocaleLongDate);
    editorCommandExecuteInsertHtml(date, false);
}

void CWizDocumentWebView::editorCommandExecuteInsertTime()
{
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("insertTime");
    //
    QString time = QTime::currentTime().toString(Qt::DefaultLocaleLongDate);
    editorCommandExecuteInsertHtml(time, false);
}

void CWizDocumentWebView::editorCommandExecuteRemoveFormat()
{
    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("removeFormat");
    editorCommandExecuteCommand("removeFormat");
}


void CWizDocumentWebView::editorCommandExecuteFormatMatch()
{
}

void CWizDocumentWebView::editorCommandExecuteInsertCode()
{
    QString strSelectHtml = page()->selectedText();
    WizCodeEditorDialog *dialog = new WizCodeEditorDialog(m_app, this);
    connect(dialog, SIGNAL(insertHtmlRequest(QString)), SLOT(on_insertCodeHtml_requset(QString)));
    dialog->setCode(strSelectHtml);
    //dialog->exec();
    dialog->show();
    //dialog->setWindowState(dialog->windowState() & ~Qt::WindowFullScreen | Qt::WindowActive);

    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("insertCode");
}

void CWizDocumentWebView::editorCommandExecuteMobileImage(bool bReceiveImage)
{
    MainWindow* mainWindow = qobject_cast<MainWindow *>(m_app.mainWindow());
    if (bReceiveImage && m_app.userSettings().needShowMobileFileReceiverUserGuide())
    {
        mainWindow->showMobileFileReceiverUserGuide();
    }

    m_app.userSettings().setReceiveMobileFile(bReceiveImage);
    mainWindow->setMobileFileReceiverEnable(bReceiveImage);

    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("mobileImage");
}

void CWizDocumentWebView::editorCommandExecuteScreenShot()
{
    CWizScreenShotHelper* helper = new CWizScreenShotHelper();

    connect(helper, SIGNAL(screenShotCaptured(QPixmap)),
            SLOT(on_editorCommandExecuteScreenShot_imageAccepted(QPixmap)));
    connect(helper, SIGNAL(shotScreenQuit()), SLOT(on_editorCommandExecuteScreenShot_finished()));

    setWindowVisibleOnScreenShot(false);
    QTimer::singleShot(200, helper, SLOT(startScreenShot()));

    CWizAnalyzer& analyzer = CWizAnalyzer::GetAnalyzer();
    analyzer.LogAction("screenShot");
}


void CWizDocumentWebView::saveAsPDF()
{
    QString strFileName = QFileDialog::getSaveFileName(this, QString(),
                                                       QDir::homePath() + "/untited.pdf", tr("PDF Files (*.pdf)"));
    if (::PathFileExists(strFileName))
    {
        ::DeleteFile(strFileName);
    }
    QPrinter::Unit marginUnit =  (QPrinter::Unit)m_app.userSettings().printMarginUnit();
    double marginTop = m_app.userSettings().printMarginValue(wizPositionTop);
    double marginBottom = m_app.userSettings().printMarginValue(wizPositionBottom);
    double marginLeft = m_app.userSettings().printMarginValue(wizPositionLeft);
    double marginRight = m_app.userSettings().printMarginValue(wizPositionRight);
    QMarginsF margins(marginLeft, marginTop, marginRight, marginBottom);
    //
    const QPageLayout layout = QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Portrait, margins);

#if QT_VERSION >= 0x050700
    page()->printToPdf(strFileName, layout);    // WARNING: this require Qt 5.7
#endif
}

void CWizDocumentWebView::saveAsHtml(const QString& strDirPath)
{
    const WIZDOCUMENTDATA& doc = view()->note();
    CWizDatabase& db = m_dbMgr.db(doc.strKbGUID);
    db.ExportToHtmlFile(doc, strDirPath);    
}


void CWizDocumentWebView::isModified(std::function<void(bool modified)> callback)
{
    if (m_bContentsChanged)
    {
        callback(true);
        return;
    }
    page()->runJavaScript(QString("WizEditor.isModified();"), [=](const QVariant& vModified){
        //
        callback(vModified.toBool());

    });
}

void CWizDocumentWebView::setModified(bool b)
{
    m_bContentsChanged = b;
}

void CWizDocumentWebView::undo()
{
    page()->runJavaScript("WizEditor.undo()");
}

void CWizDocumentWebView::redo()
{
    page()->runJavaScript("WizEditor.redo()");
}

QString CWizDocumentWebView::getUserGuid()
{
    return m_dbMgr.db().GetUserGUID();
}


QString CWizDocumentWebView::getUserAvatarFilePath()
{
    int size = 24;
    QString strFileName;
    QString strUserID = m_dbMgr.db().GetUserId();
    if (AvatarHost::customSizeAvatar(strUserID, size, size, strFileName))
        return strFileName;


    return QString();
}

QString CWizDocumentWebView::getUserAlias()
{
    QString strKbGUID = view()->note().strKbGUID;
    return m_dbMgr.db(strKbGUID).GetUserAlias();
}


bool CWizDocumentWebView::isPersonalDocument()
{
    QString strKbGUID = view()->note().strKbGUID;
    QString dbKbGUID = m_dbMgr.db().kbGUID();
    return strKbGUID.isEmpty() || (strKbGUID == dbKbGUID);
}

QString CWizDocumentWebView::getCurrentNoteHtml()
{
    return m_currentNoteHtml;
}


void copyFileToFolder(const QString& strFileFoler, const QString& strIndexFile, \
                         const QStringList& strResourceList)
{
    //copy index file
    QString strFolderIndex = strFileFoler + "index.html";
    if (strIndexFile != strFolderIndex)
    {
        QFile::remove(strFolderIndex);
        QFile::copy(strIndexFile, strFolderIndex);
    }

    //copy resources to temp folder
    QString strResourcePath = strFileFoler + "index_files/";
    for (int i = 0; i < strResourceList.count(); i++)
    {
        if (QFile::exists(strResourceList.at(i)))
        {
            QFile::copy(strResourceList.at(i), strResourcePath + Utils::Misc::extractFileName(strResourceList.at(i)));
        }
    }
}

bool CWizDocumentWebView::hasEditPermissionOnCurrentNote()
{
    WIZDOCUMENTDATA docData = view()->note();
    CWizDatabase& db = m_dbMgr.db(docData.strKbGUID);
    return db.CanEditDocument(docData) && !CWizDatabase::IsInDeletedItems(docData.strLocation);
}

void CWizDocumentWebView::changeCurrentDocumentType(const QString &strType)
{
    WIZDOCUMENTDATA docData = view()->note();
    CWizDatabase& db = m_dbMgr.db(docData.strKbGUID);
    docData.strType = strType;
    db.ModifyDocumentInfoEx(docData);
}

bool CWizDocumentWebView::checkListClickable()
{
    CWizDocumentView* v = view();
    if (!m_dbMgr.db(v->note().strKbGUID).IsGroup())
    {
        emit clickingTodoCallBack(false, false);
        return true;
    }

    if (v->checkListClickable())
    {
        emit clickingTodoCallBack(false, false);
        v->setStatusToEditingByCheckList();
        return true;
    }
    emit clickingTodoCallBack(true, true);
    return false;
}

bool CWizDocumentWebView::shouldAddCustomCSS()
{
    const WIZDOCUMENTDATA& data = view()->note();
    // 通过网页剪辑的笔记不添加自定义的样式
    bool styledNote = data.strURL.startsWith("http");
    if (styledNote)
        return false;

    bool isMarkdown = WizIsMarkdownNote(data) && !view()->isEditing();

    return !isMarkdown;
}

bool CWizDocumentWebView::canRenderMarkdown()
{
    const WIZDOCUMENTDATA& doc = view()->note();

    if (view()->isEditing())
        return false;

    if (doc.strTitle.indexOf(".md") == -1 && doc.strTitle.indexOf(".mj") == -1)
        return false;

    int nPointPos = doc.strTitle.length() - 3;
    if (doc.strTitle.lastIndexOf(".md") == nPointPos || doc.strTitle.lastIndexOf(".mj") == nPointPos)
        return true;

    if (doc.strTitle.indexOf(".md ") != -1 || doc.strTitle.indexOf(".md@") != -1 ||
            doc.strTitle.indexOf(".mj ") != -1|| doc.strTitle.indexOf(".mj@") != -1)
        return true;

    return false;
}

bool CWizDocumentWebView::canEditNote()
{
    return view()->isEditing();
}

//编辑器初始化时使用
QString CWizDocumentWebView::getLocalLanguage()
{
    QLocale locale;
    if (locale.language() == QLocale::Chinese)
    {
        if (locale.country() == QLocale::China)
            return "zh-cn";
        else if (locale.country() == QLocale::Taiwan)
            return "zh-tw";
    }

    return "en";
}

void CWizDocumentWebView::OnSelectionChange(const QString& currentStyle)
{
    Q_EMIT statusChanged(currentStyle);
}


void CWizDocumentWebView::saveCurrentNote()
{
    WizExecuteOnThread(WIZ_THREAD_MAIN, [=]{
        //
        onTimerAutoSaveTimout();
        //
    });
}


/////////////////////////////////////////////////////////////////////////////////////////////////////

CWizDocumentWebViewLoaderThread::CWizDocumentWebViewLoaderThread(CWizDatabaseManager &dbMgr, QObject *parent)
    : QThread(parent)
    , m_dbMgr(dbMgr)
    , m_stop(false)
    , m_editorMode(modeReader)
{
}

void CWizDocumentWebViewLoaderThread::load(const WIZDOCUMENTDATA &doc, WizEditorMode editorMode)
{
    setCurrentDoc(doc.strKbGUID, doc.strGUID, editorMode);

    if (!isRunning())
    {
        start();
    }
}
void CWizDocumentWebViewLoaderThread::stop()
{
    QMutexLocker locker(&m_mutex);
    Q_UNUSED(locker);
    //
    m_stop = true;
    //
    m_waitEvent.wakeAll();
}
void CWizDocumentWebViewLoaderThread::waitForDone()
{
    stop();
    //
    WizWaitForThread(this);
}

void CWizDocumentWebViewLoaderThread::run()
{
    while (!m_stop)
    {
        if (m_stop)
            return;
        //
        QString kbGuid;
        QString docGuid;
        WizEditorMode editorMode = modeReader;
        PeekCurrentDocGUID(kbGuid, docGuid, editorMode);
        if (m_stop)
            return;
        //
        if (docGuid.isEmpty())
            continue;
        //
        CWizDatabase& db = m_dbMgr.db(kbGuid);
        WIZDOCUMENTDATA data;
        if (!db.DocumentFromGUID(docGuid, data))
        {
            continue;
        }
        //
        QString strHtmlFile;
        if (db.DocumentToTempHtmlFile(data, strHtmlFile))
        {
            emit loaded(kbGuid, docGuid, strHtmlFile, editorMode);
        }
        else
        {
            ::WizExecuteOnThread(WIZ_THREAD_MAIN, [=]{
                //
                QMessageBox::critical(MainWindow::instance(), tr("Error"), tr("Can't view note: (Can't unzip note data)"));
                //
            });
        }
    }
}

void CWizDocumentWebViewLoaderThread::setCurrentDoc(QString kbGUID, QString docGUID, WizEditorMode editorMode)
{
    //
    {
        QMutexLocker locker(&m_mutex);
        Q_UNUSED(locker);
        //
        m_strCurrentKbGUID = kbGUID;
        m_strCurrentDocGUID = docGUID;
        m_editorMode = editorMode;
    }
    //
    //
    m_waitEvent.wakeAll();
}

bool CWizDocumentWebViewLoaderThread::isEmpty()
{
    QMutexLocker locker(&m_mutex);
    Q_UNUSED(locker);
    //
    return m_strCurrentDocGUID.isEmpty();
}

void CWizDocumentWebViewLoaderThread::PeekCurrentDocGUID(QString& kbGUID, QString& docGUID, WizEditorMode& editorMode)
{
    if (isEmpty())
    {
        m_waitEvent.wait();
    }
    //
    //
    {
        QMutexLocker locker(&m_mutex);
        Q_UNUSED(locker);
        //
        kbGUID = m_strCurrentKbGUID;
        docGUID = m_strCurrentDocGUID;
        editorMode = m_editorMode;
        //
        m_strCurrentKbGUID.clear();
        m_strCurrentDocGUID.clear();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////



CWizDocumentWebViewSaverThread::CWizDocumentWebViewSaverThread(CWizDatabaseManager &dbMgr, QObject *parent)
    : QThread(parent)
    , m_dbMgr(dbMgr)
    , m_stop(false)
{
}

void CWizDocumentWebViewSaverThread::save(const WIZDOCUMENTDATA& doc, const QString& strHtml,
                                          const QString& strHtmlFile, int nFlags)
{
    SAVEDATA data;
    data.doc = doc;
    data.html = strHtml;
    data.htmlFile = strHtmlFile;
    data.flags = nFlags;
    //
    QMutexLocker locker(&m_mutex);
    Q_UNUSED(locker);
    //
    m_arrayData.push_back(data);
    //
    m_waitEvent.wakeAll();

    if (!isRunning())
    {
        start();
    }
}
void CWizDocumentWebViewSaverThread::waitForDone()
{
    stop();
    //
    WizWaitForThread(this);
}

void CWizDocumentWebViewSaverThread::stop()
{
    m_stop = true;
    m_waitEvent.wakeAll();
}

bool CWizDocumentWebViewSaverThread::isEmpty()
{
    QMutexLocker locker(&m_mutex);
    Q_UNUSED(locker);
    //
    return m_arrayData.empty();
}

CWizDocumentWebViewSaverThread::SAVEDATA CWizDocumentWebViewSaverThread::peekFirst()
{
    QMutexLocker locker(&m_mutex);
    Q_UNUSED(locker);
    //
    SAVEDATA data = m_arrayData[0];
    m_arrayData.erase(m_arrayData.begin());
    return data;
}

void CWizDocumentWebViewSaverThread::PeekData(SAVEDATA& data)
{
    while (1)
    {
        if (m_stop)
            return;
        //
        if (isEmpty())
        {
            m_waitEvent.wait();
        }
        //
        if (isEmpty())
        {
            if (m_stop)
                return;
            //
            continue;
        }
        //
        data = peekFirst();
        //
        break;
    }
}

void CWizDocumentWebViewSaverThread::run()
{
    while (true)
    {
        SAVEDATA data;
        PeekData(data);
        //
        if (data.doc.strGUID.isEmpty())
        {
            if (m_stop)
                return;
            //
            continue;
        }
        //
        CWizDatabase& db = m_dbMgr.db(data.doc.strKbGUID);
        //
        WIZDOCUMENTDATA doc;
        if (!db.DocumentFromGUID(data.doc.strGUID, doc))
        {
            qDebug() << "fault error: can't find doc in database: " << doc.strGUID;
            continue;
        }
        //
        qDebug() << "Saving note: " << doc.strTitle;

        bool notify = false;    //don't notify
        bool ok = db.UpdateDocumentData(doc, data.html, data.htmlFile, data.flags, notify);

        //
        if (ok)
        {
            qDebug() << "Save note done: " << doc.strTitle;
        }
        else
        {
            qDebug() << "Save note failed: " << doc.strTitle;
        }

        QString kbGuid = db.IsGroup() ? db.kbGUID() : "";
        emit saved(kbGuid, doc.strGUID, ok);
    }
}

