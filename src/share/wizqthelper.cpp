#include "wizqthelper.h"

#include <QtCore>
#include <QtGui>
#include <QApplication>
#include <QWidget>


bool PathFileExists(const CString& strPath)
{
    return QFile::exists(strPath);
}

bool DeleteFile(const CString& strFileName)
{
    QDir dir(strFileName);
    dir.remove(strFileName);
    return true;
}

QString COleDateTime::toHumanFriendlyString() const
{
    QDateTime t(QDateTime::currentDateTime());
    int nElapseSecs = secsTo(t);
    int nElapseDays = daysTo(t);

    if (nElapseDays == 1) {
        return QObject::tr("Yesterday");
    } else if (nElapseDays == 2) {
        return QObject::tr("The day before yesterday");
    } else if (nElapseDays > 2) {
        return toString("yyyy/M/d");
    }

    if (nElapseSecs < 60) {
        // less than 1 minutes: "just now"
        return QObject::tr("Just now");

    } else if (nElapseSecs >= 60 && nElapseSecs < 60 * 2) {
        // 1 minute: "1 minute ago"
        return QObject::tr("1 minute ago");

    } else if (nElapseSecs >= 120 && nElapseSecs < 60 * 60) {
        // 2-60 minutes: "x minutes ago"
        QString str = QObject::tr("%1 minutes ago");
        return str.arg(nElapseSecs/60);

    } else if (nElapseSecs >= 60 * 60 && nElapseSecs < 60 * 60 * 2) {
        // 1 hour: "1 hour ago"
        return QObject::tr("1 hour ago");

    } else if (nElapseSecs >= 60 * 60 * 2 && nElapseSecs < 60 * 60 * 24) {
        // 2-24 hours: "x hours ago"
        QString str = QObject::tr("%1 hours ago");
        return str.arg(nElapseSecs/60/60);
    }

    return QString("Error");
}

QString COleDateTime::toLocalLongDate() const
{
    QLocale local;
    if (local.language() == QLocale::Chinese)
    {
        QDateTime dt = QDateTime::currentDateTime();
        return local.toString(dt, "yyyy") + QObject::tr("year") + local.toString(dt, "MMMd") + QObject::tr("day");
    }

    return QDateTime::currentDateTime().toString("MMM d,yyyy");
}

COleDateTime &COleDateTime::operator=(const QDateTime &other)
{
    setDate(other.date());
    setTime(other.time());
    return *this;
}

COleDateTime &COleDateTime::operator=(const COleDateTime &other)
{
    setDate(other.date());
    setTime(other.time());
    return *this;
}


#ifndef Q_OS_WIN
int GetTickCount()
{
    QTime time = QTime::currentTime();
    return time.msecsSinceStartOfDay();
}
#endif  // Q_OS_WIN


void CString::Trim(char ch)
{
    while (startsWith(ch))
    {
        remove(0, 1);
    }
    //
    while (endsWith(ch))
    {
        remove(length() - 1, 1);
    }
}
void CString::TrimLeft()
{
    while (!isEmpty())
    {
        if (begin()->isSpace())
        {
            remove(0, 1);
        }
        else
        {
            break;
        }
    }
}

void CString::TrimRight()
{
    while (!isEmpty())
    {
        if (at(length() - 1).isSpace())
        {
            remove(length() - 1, 1);
        }
        else
        {
            break;
        }
    }
}

void CString::Insert(int index, const CString& str)
{
    insert(index, str);
}

void CString::Insert(int index, QChar ch)
{
    insert(index, ch);
}

void CString::SetAt(int index, QChar ch)
{
    replace(index, 1, ch);
}


void CString::Format(QString strFormat, ...)
{
    CString strFormat2 = strFormat;
    strFormat2.replace("%s", "%ls");
    //
    va_list argList;
    va_start( argList, strFormat );
    vsprintf(strFormat2.toUtf8(), argList);
    va_end( argList );
}

int CString::FindOneOf(const CString& strFind) const
{
    std::set<QChar> chars;
    for (int i = 0; i < strFind.length(); i++)
    {
        chars.insert(strFind[i]);
    }
    //
    for (int i = 0; i < length(); i++)
    {
        if (chars.find(at(i)) != chars.end())
            return i;
    }
    //
    return -1;
}

int _tcsicmp(const CString& str1, const CString& str2)
{
    return str1.CompareNoCase(str2);
}

int _tcsnicmp(const CString& str1, const CString& str2, int count)
{
    CString s1 = (str1.length() > count) ? str1.Left(count) : str1;
    CString s2 = (str2.length() > count) ? str2.Left(count) : str2;
    return s1.CompareNoCase(s2);
}

int _ttoi(const CString& str)
{
    return str.toInt();
}

__int64 _ttoi64(const CString& str)
{
    return str.toLongLong();
}


unsigned short* wiz_strinc(const unsigned short* current)
{
    return (unsigned short *)(current + 1);
}

int wiz_isdigit(int c)
{
    return isdigit(c);
    return QChar(c).isDigit();
}

int wiz_isupper(int c)
{
    return QChar(c).isUpper();
}

int wiz_isupper(QChar c)
{
    return c.isUpper();
}


int wiz_isalpha(int c)
{
    return isalpha(c);
    return (c >= 'a' && c <= 'z')
            || (c >= 'A' && c <= 'Z');
    //return QChar(c).isLetter();
}

int wiz_isalpha(QChar c)
{
    return isalpha(c.unicode());
    return (c >= 'a' && c <= 'z')
            || (c >= 'A' && c <= 'Z');
    //return c.isLetter();
}

int wiz_isxdigit(int c)
{
    return isxdigit(c);
    if (c >= '0' && c<= '9')
        return TRUE;
    if (c >= 'a' && c <= 'f')
        return TRUE;
    if (c >= 'A' && c <= 'F')
        return TRUE;
    return FALSE;
}

int wiz_isxdigit(QChar c)
{
    return isxdigit(c.unicode());
    //
    return wiz_isxdigit(c.unicode());
}

size_t wiz_strlen(const unsigned short* str)
{
    int len = 0;
    while (*str)
    {
        len++;
        str++;
    }
    //
    return len;
}

long wiz_strtoul( const unsigned short *nptr, unsigned short **endptr, int base )
{
    ATLASSERT(endptr == NULL);
    //
    return CString(nptr).toLong(NULL, base);
}

int wiz_atoi(const unsigned short *nptr)
{
    return CString(nptr).toInt();
}

int wiz_isspace(int c)
{
    return QChar((unsigned short)c).isSpace();
}

int wiz_isalnum(int c)
{
    return isalnum(c);
    return (c >= 'a' && c <= 'z')
            || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9');
    //return wiz_isalpha(c) || wiz_isdigit(c);
}

unsigned short * wiz_strstr( const unsigned short *wcs1, const unsigned short* wcs2)
{
    unsigned short *cp = (unsigned short *) wcs1;
    unsigned short *s1, *s2;

    if ( !*wcs2)
        return (unsigned short *)wcs1;

    while (*cp)
    {
            s1 = cp;
            s2 = (unsigned short *) wcs2;

            while ( *s1 && *s2 && !(*s1-*s2) )
                    s1++, s2++;

            if (!*s2)
                    return(cp);

            cp++;
    }

    return(NULL);

}

unsigned short * wiz_strstr( const unsigned short *wcs1, const CString& str)
{
    return wiz_strstr(wcs1, str.utf16());
}

unsigned short * wiz_strchr( const unsigned short *string, int ch)
{
    while (*string && *string != (unsigned short)ch)
            string++;

    if (*string == (unsigned short)ch)
            return((unsigned short *)string);
    return(NULL);
}


unsigned short * wiz_strchr( const unsigned short *string, QChar ch)
{
    return wiz_strchr(string, ch.unicode());
}

int wiz_strncmp( const unsigned short* first, const char* last, size_t count )
{
    if (!count)
            return(0);

    while (--count && *first && *first == *last)
    {
            first++;
            last++;
    }

    return((int)(*first - *last));
}

int wiz_strncmp(const unsigned short* first, const unsigned short* last, size_t count )
{
    if (!count)
            return(0);

    while (--count && *first && *first == *last)
    {
            first++;
            last++;
    }

    return((int)(*first - *last));
}



int wiz_strnicmp(const unsigned short* first, const unsigned short* last, size_t count )
{
    if (!count)
            return(0);

    while (--count && *first && QChar(*first).toLower() == QChar(*last).toLower())
    {
            first++;
            last++;
    }

    return((int)(QChar(*first).toLower().unicode() - QChar(*last).toLower().unicode()));
}

int wiz_strnicmp(const unsigned short* first, const char* last, size_t count )
{
    if (!count)
            return(0);

    while (--count && *first && QChar(*first).toLower() == QChar(*last).toLower())
    {
            first++;
            last++;
    }

    return((int)(QChar(*first).toLower().unicode() - QChar(*last).toLower().unicode()));
}


long wiz_strtoul(const unsigned short* nptr, QChar endchar, int base)
{
    //
    CString ctr(nptr);
    ctr = ctr.left(ctr.indexOf(QChar(endchar)));
    return ctr.toLong(NULL, base);
}

int WizSmartScaleUI(int spec)
{
#ifdef Q_OS_MAC
    return spec;
#else

    static double rate = 0;
    if (0 == (int)rate)
    {
        rate = 1.0;
        //
        QList<QScreen*> screens = QApplication::screens();
        if (screens.size() > 0)
        {
            QScreen* screen = screens[0];
            double dpi = screen->logicalDotsPerInch();
            //
            rate = dpi / 96.0;
            //

            if (rate < 1.1)
            {
                rate = 1.0;
            }
            else if (rate < 1.4)
            {
                rate = 1.25;
            }
            else if (rate < 1.6)
            {
                rate = 1.5;
            }
            else if (rate < 1.8)
            {
                rate = 1.75;
            }
            else
            {
                rate = 2.0;
            }
        }
    }
    //
    return int(spec * rate);
    //
#endif
}

#ifdef Q_OS_LINUX
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>

class linux_x11
{
public:
    linux_x11();
    QList<WizWindowInfo> getActiveWindows();

private:
    Window* active(Display *disp, unsigned long *len);
    char *name (Display *disp, Window win);
    int *pid(Display *disp, Window win);
    QString processName(long pid);
};

/*
  Linux/X11 specific code for obtaining information about the frontmost window
*/



linux_x11::linux_x11()
{
}

/**
  * Returns the window name for a specific window on a display
***/
char *linux_x11::name (Display *disp, Window win) {
    Atom prop = XInternAtom(disp,"WM_NAME",False), type;
    int form;
    unsigned long remain, len;
    unsigned char *list;

    if (XGetWindowProperty(disp,win,prop,0,1024,False,AnyPropertyType, &type,&form,&len,&remain,&list) != Success)
        return NULL;

    return (char*)list;
}

/**
  * Returns the pid for a specific window on a display
***/
int* linux_x11::pid(Display *disp, Window win) {
    Atom prop = XInternAtom(disp,"_NET_WM_PID",False), type;
    int form;
    unsigned long remain, len;
    unsigned char *list;

    if (XGetWindowProperty(disp,win,prop,0,1024,False,AnyPropertyType, &type,&form,&len,&remain,&list) != Success)
        return NULL;

    return (int*)list;
}

/**
  * Returns the active window on a specific display
***/
Window * linux_x11::active (Display *disp, unsigned long *len) {
    Atom prop = XInternAtom(disp,"_NET_ACTIVE_WINDOW",False), type;
    int form;
    unsigned long remain;
    unsigned char *list;

    if (XGetWindowProperty(disp,XDefaultRootWindow(disp),prop,0,1024,False,XA_WINDOW, &type,&form,len,&remain,&list) != Success)
        return NULL;

    return (Window*)list;
}

/**
  * Returns process name from pid (processes output from /proc/<pid>/status)
***/
QString linux_x11::processName(long pid)
{
    // construct command string
    QString command = "cat /proc/" + QString("%1").arg(pid) + "/status";
    // capture output in a FILE pointer returned from popen
    FILE * output = popen(command.toStdString().c_str(), "r");
    // initialize a buffer for storing the first line of the output
    char buffer[1024];
    // put the contents of the buffer into a QString
    QString line = QString::fromUtf8(fgets(buffer, sizeof(buffer), output));
    // close the process pipe
    pclose(output);
    // take right substring of line returned to get process name
    return line.right(line.length() - 6).replace("\n", "");
}

QList<WizWindowInfo> linux_x11::getActiveWindows()
{
    QList<WizWindowInfo> windowTitles;
    unsigned long len;
    Display *disp = XOpenDisplay(NULL);
    Window *list;
    char *n;
    int* p;

    list = (Window*)active(disp,&len);
    if((int)len > 0)
    {
        for (int i=0;i<(int)len;i++) {
            n = name(disp,list[i]);
            p = pid(disp, list[i]);
            long p_id = 0;
            QString pName;
            QString windowTitle;

            if(p!=NULL)
            {
                p_id = *p; // dereference pointer for obtaining pid
                pName = processName(p_id);
            }

            if(n!=NULL)
                windowTitle = QString::fromUtf8(n);

            WizWindowInfo wi;
            wi.windowTitle = windowTitle;
            wi.processName = pName;
            wi.pid = p_id;
            windowTitles.append(wi);
            delete n;
            delete p;
        }
    }
    delete list;
    XCloseDisplay (disp);
    return windowTitles;
}

QList<WizWindowInfo> WizGetActiveWindows()
{
    linux_x11 x11;
    return x11.getActiveWindows();
}
#elif defined(Q_OS_WIN)

QList<WizWindowInfo> WizGetActiveWindows()
{
    QList<WizWindowInfo> res;
    foreach(QWidget *widget, QApplication::topLevelWidgets()) {
        if (widget->isActiveWindow()) {
            WizWindowInfo info;
            info.pid = QCoreApplication::applicationPid();
            info.processName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
            info.windowTitle = widget->windowTitle();
            res.append(info);
            break;
        }
    }

    return res;
}

#endif
