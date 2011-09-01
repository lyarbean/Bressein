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

#include "bresseinmanager.h"
#include "sipc/account.h"
#include "chatview.h"
#include "sidepanelview.h"
//#include "loginwidget.h"
#include <QApplication>
#include <QMenu>
#include <QDir>
#include <QLabel>

namespace Bressein
{
BresseinManager::BresseinManager (QObject *parent) : QObject (parent),
    account (new Account),
    // loginDialog (new LoginWidget),
    sidePanel (new SidepanelView),
    tray (new QSystemTrayIcon (sidePanel)),
    trayIconMenu (new QMenu (sidePanel))
{
    connectSignalSlots();
    initializeTray();
    sidePanel->show();
}

BresseinManager::~BresseinManager()
{
    sidePanel->disconnect();
    sidePanel->close();
    account->disconnect();
    account->close();
}

//TODO initialize database
void BresseinManager::initialize()
{
}

// public slots:
void BresseinManager::loginAs (const QByteArray &number,
                               const QByteArray &password)
{
    if (number.isEmpty() or password.isEmpty())
    {
        return;
    }
    account->setAccount (number, password);
    account->login();
    // loginDialog->setMessage (tr ("Connecting... "));
}

void BresseinManager::onContactChanged (const QByteArray &contact)
{
    // notify sidePanel to update its data
    // as sidePanel has no knowledge about Account, we pass that contact too
    const ContactInfo contactInfo = account->getContactInfo (contact);
    sidePanel->updateContact (contact, contactInfo);
}

void BresseinManager::onGroupChanged (const QByteArray &id,
                                      const QByteArray &name)
{
    sidePanel->addGroup (id, name);
}

void BresseinManager::onWrongPassword()
{
    // account->close();
    sidePanel->activateLogin (true);
}


void BresseinManager::readyShow()
{
    QByteArray sipuri;
    account->getFetion (sipuri);
    QByteArray nickname;
    account->getNickname (nickname);
    sidePanel->setupContactsScene();
    sidePanel->setNickname (nickname);
    sidePanel->setHostSipuri (sipuri);
}

// TODO move to loginScene
void BresseinManager::onVerificationPic (const QByteArray &data)
{
    qDebug() << "onVerificationPic" << data;
    QImage img = QImage::fromData (QByteArray::fromBase64 (data));
    QLabel *label = new QLabel (0);
    label->setPixmap (QPixmap::fromImage (img));
    label->show();
}

void BresseinManager::onStateAuthorized()
{
    // loginDialog->setMessage (tr ("Downloading Portraits"));
}

void BresseinManager::onTrayActivated (QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
        case QSystemTrayIcon::Trigger:
        case QSystemTrayIcon::DoubleClick:
            sidePanel->show();
            break;
        case QSystemTrayIcon::MiddleClick:
            //TODO
            break;
        default:
            ;
    }
}


void BresseinManager::initializeTray()
{
    // trayIconMenu.addAction ...
    QAction *action;
    action = new QAction (tr ("Online"),tray);
    connect (action, SIGNAL (triggered ()), account, SLOT (setOnline()));
    trayIconMenu->addAction (action);
    action = new QAction (tr ("Offline"),tray);
    connect (action, SIGNAL (triggered ()), account, SLOT (setOffline()));
    trayIconMenu->addAction (action);
    action = new QAction (tr ("Busy"),tray);
    connect (action, SIGNAL (triggered ()), account, SLOT (setBusy()));
    trayIconMenu->addAction (action);
    action = new QAction (tr ("Hidden"),tray);
    connect (action, SIGNAL (triggered ()), account, SLOT (setHidden()));
    trayIconMenu->addAction (action);
    action = new QAction (tr ("Metting"),tray);
    connect (action, SIGNAL (triggered ()), account, SLOT (setMeeting()));
    trayIconMenu->addAction (action);
    action = new QAction (tr ("quit"), tray);
    connect (action, SIGNAL (triggered (bool)), qApp, SLOT (quit()));
    trayIconMenu->addAction (action);
    tray->setContextMenu (trayIconMenu);
    tray->setIcon (QIcon (":/images/envelop_64.png"));
    tray->setToolTip ("Bressein");
    connect (tray, SIGNAL (activated (QSystemTrayIcon::ActivationReason)),
             this, SLOT (onTrayActivated (QSystemTrayIcon::ActivationReason)));
    tray->show();
}

void BresseinManager::connectSignalSlots()
{
    connect (account, SIGNAL (groupChanged (const QByteArray &,
                                            const QByteArray &)),
             this, SLOT (onGroupChanged (const QByteArray &,
                                         const QByteArray &)),
             Qt::QueuedConnection);
    connect (account, SIGNAL (contactChanged (const QByteArray &)),
             this, SLOT (onContactChanged (const QByteArray &)),
             Qt::QueuedConnection);
//    connect (account, SIGNAL (portraitDownloaded (const QByteArray &)),
//              this, SLOT (onPortraitDownloaded (const QByteArray &)),
//              Qt::QueuedConnection);
    connect (account, SIGNAL (logined()), this, SLOT (readyShow()),
             Qt::QueuedConnection);
    connect (account, SIGNAL (verificationPic (const QByteArray &)),
             this, SLOT (onVerificationPic (const QByteArray &)));
    connect (account, SIGNAL (incomeMessage (const QByteArray &,
                                             const QByteArray &,
                                             const QByteArray &)),
             sidePanel, SLOT (onIncomeMessage (const QByteArray &,
                                               const QByteArray &,
                                               const QByteArray &)),
             Qt::QueuedConnection);
    connect (account, SIGNAL (wrongPassword()),
             this, SLOT (onWrongPassword()),
             Qt::QueuedConnection);
    connect (account, SIGNAL (sipcAuthorizeParsed()),
             this, SLOT (onStateAuthorized()), Qt::QueuedConnection);
    connect (sidePanel,
             SIGNAL (toLogin (const QByteArray &, const QByteArray &)),
             this,
             SLOT (loginAs (const QByteArray &, const QByteArray &)));
    connect (sidePanel,
             SIGNAL (sendMessage (const QByteArray &, const QByteArray &)),
             account,
             SLOT (sendMessage (const QByteArray &, const QByteArray &)),
             Qt::QueuedConnection);
}

}
#include "bresseinmanager.moc"
