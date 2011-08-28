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

#include <QtCore/QObject>
#include <QMap>
#include <QTextImageFormat>
#include <QSystemTrayIcon>

class QMenu;
namespace Bressein
{
class Account;
//class LoginWidget;
class ChatView;
class SidepanelView;
/**
 * @brief BresseinManager takes control of Account and Views and as a postal
 * who deliver message back and forth for them.
 **/
// initialize Account and connect signals to slots

// initialize login dialog, connect to login slot,
// which may call setAccount from Account and delete this dialog

// when login successfully, initialize sidePanelView with data from Account.
// connect signals to slots that may request BresseinManager to spawn a chatView


class BresseinManager : public QObject
{
    Q_OBJECT
public:
    BresseinManager (QObject *parent = 0);
    virtual ~BresseinManager();
    void initialize();

signals:

public slots:
    void loginAs (const QByteArray &, const QByteArray &);
    void onContactChanged (const QByteArray &);
    void onGroupChanged (const QByteArray &, const QByteArray &);

private slots:

    void readyShow();
    void onVerificationPic (const QByteArray &);
    void onWrongPassword();
    void onStateAuthorized();
    void onTrayActivated (QSystemTrayIcon::ActivationReason);
private:
    void initializeTray();
    void connectSignalSlots();
private:
    Account *account;
//   LoginWidget *loginDialog;
    SidepanelView *sidePanel;
    QSystemTrayIcon *tray;
    QMenu *trayIconMenu;
    QImage myPortrait;
    QByteArray myPortraitName;

    //TODO it should be better to store QTextImageFormats here, but ...
//     QMap<QByteArray, QTextImageFormat> images;
};

}
#endif // BRESSEINMANAGER_H
