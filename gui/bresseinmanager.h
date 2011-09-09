/*
This file is part of Bressein.
Copyright (C) 2011  颜烈彬 <slbyan@gmail.com>

Bressein is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

Bressein is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

OpenSSL linking exception
--------------------------
If you modify this Program, or any covered work, by linking or
combining it with the OpenSSL project's "OpenSSL" library (or a
modified version of that library), containing parts covered by
the terms of OpenSSL/SSLeay license, the licensors of this
Program grant you additional permission to convey the resulting
work. Corresponding Source for a non-source form of such a
combination shall include the source code for the parts of the
OpenSSL library used as well as that of the covered work.
*/

#ifndef BRESSEINMANAGER_H
#define BRESSEINMANAGER_H

#include "sipc/types.h"
#include <QObject>
#include <QMutex>
#include <QMap>
#include <QTextImageFormat>
#include <QSystemTrayIcon>

class QMenu;
namespace Bressein
{
class Account;
class ChatView;
class SidepanelView;
class ContactsScene;
/**
 * @brief BresseinManager a message deliver
 *
 **/

class BresseinManager : public QObject
{
    Q_OBJECT
public:
    BresseinManager (QObject *parent = 0);
    virtual ~BresseinManager();
    void initialize();

signals:

private slots:
    void loginAs (const QByteArray &, const QByteArray &);
    void onContactChanged (const QByteArray &);
    void onPortraitDownloaded (const QByteArray &);
    void onGroupChanged (const QByteArray &, const QByteArray &);
    void readyShow();
    void onVerificationPic (const QByteArray &);
    void onWrongPassword();
    void onStateAuthorized();
    void onTrayActivated (QSystemTrayIcon::ActivationReason);
    void onIncomeMessage (const QByteArray &,
                          const QByteArray &,
                          const QByteArray &);
    void onNotSentMessage (const QByteArray &,
                           const QDateTime &,
                           const QByteArray &);
    void showOnlineState (const QByteArray &, int);
    void bye();
private:
    void connectSignalSlots();
    void initializeTray();
    void initializePortrait();
    void dbusNotify (const QString &summary,
                     const QString &text);
private:
    Account *account;
    SidepanelView *sidePanel;
    ContactsScene *contactsScene;
    QSystemTrayIcon *tray;
    QMenu *trayIconMenu;
    QByteArray mySipc;
    QByteArray myPortrait;
    QByteArray bresseinIcon;
    QMap<QByteArray, QByteArray> portraitMap;
    Contacts contacts;
    QMutex mutex;
    //TODO it should be better to store QTextImageFormats here, but ...
//     QMap<QByteArray, QTextImageFormat> images;
};

}
#endif // BRESSEINMANAGER_H
