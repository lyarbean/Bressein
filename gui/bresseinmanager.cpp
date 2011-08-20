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
#include <QSystemTrayIcon>
#include <QMenu>
namespace Bressein
{
BresseinManager::BresseinManager (QObject *parent) : QObject (parent),
    account (new Account),
    sidePanel (new SidepanelView),
    tray (new QSystemTrayIcon),
    trayIconMenu (new QMenu)
{
    connect (account, SIGNAL (groupChanged (const QByteArray &, const QByteArray &)),
             this, SLOT (onGroupChanged (const QByteArray &, const QByteArray &)),
             Qt::QueuedConnection);
    connect (account, SIGNAL (contactChanged (const QByteArray &)),
             this, SLOT (onContactChanged (const QByteArray &)),
             Qt::QueuedConnection);
    connect (account, SIGNAL (logined()), this, SLOT (readyShow()),
             Qt::QueuedConnection);
    connect (account, SIGNAL (incomeMessage (const QByteArray &,
                                             const QByteArray &,
                                             const QByteArray &)),
             this, SLOT (onIncomeMessage (const QByteArray &,
                                          const QByteArray &,
                                          const QByteArray &)),
             Qt::QueuedConnection);
    connect (sidePanel, SIGNAL (contactActivated (const QByteArray &)),
             this, SLOT (onChatSpawn (const QByteArray &)));
    // trayIconMenu.addAction ...
    tray->setContextMenu (trayIconMenu);
    tray->setIcon (QIcon ("/usr/share/icons/oxygen/32x32/emotes/face-smile.png"));
    tray->setToolTip ("Bressein");
    tray->show();
    //TODO show login dialogand connect signals and slots
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
    //demo
    loginAs (qgetenv ("FETIONNUMBER"), qgetenv ("FETIONPASSWORD"));
}

// public slots:
void BresseinManager::loginAs (const QByteArray &number,
                               const QByteArray &password)
{
    if (number.isEmpty() or password.isEmpty()) return;
    account->setAccount (number, password);
    account->login();
}

void BresseinManager::onContactChanged (const QByteArray &contact)
{
    // notify sidePanel to update its data
    // as sidePanel has no knowledge about Account, we pass that contact too
    const ContactInfo &contactInfo = account->getContactInfo (contact);
    sidePanel->updateContact (contact, contactInfo);
}

void BresseinManager::onGroupChanged (const QByteArray &id,
                                      const QByteArray &name)
{
    sidePanel->addGroup (id, name);
}

void BresseinManager::onChatSpawn (const QByteArray &contact)
{
    qDebug() << "onChatSpawn" << contact;
    if (chatViews.find (contact) == chatViews.end())
    {
        ChatView *chatview = new ChatView (0);
        chatview->setContact (contact);
        chatViews.insert (contact, chatview);
        connect (chatview, SIGNAL (toClose (const QByteArray &)),
                 this, SLOT (onChatClose (const QByteArray &)));
        connect (chatview,
                 SIGNAL (sendMessage (const QByteArray &, const QByteArray &)),
                 account,
                 SLOT (sendMessage (const QByteArray &, const QByteArray &)));
        chatview->resize (500, 500);
        chatview->show();
    }
    else
    {
        chatViews.value (contact)->showNormal();
    }
}

void BresseinManager::onChatClose (const QByteArray &contact)
{
    qDebug() << "BresseinManager::onChatClose (const QByteArray &contact)";
    if (chatViews.find (contact) == chatViews.end())
    {
        qDebug() << "onChatClose not found" << contact;
        return;
    }
// TODO static cast
    qDebug() << "onChatClose found";
    ChatView *chatview  = chatViews.value (contact);
    chatview->close();
    chatview->deleteLater();
    chatViews.remove (contact);
}

void BresseinManager::onIncomeMessage (const QByteArray &contact,
                                       const QByteArray &datetime,
                                       const QByteArray &content)
{
    //TODO notification
    if (chatViews.find (contact) == chatViews.end())
    {
        // TODO should make one chatview for it
        onChatSpawn (contact);
    }
    chatViews.value (contact)->incomeMessage (datetime, content);
}

void BresseinManager::readyShow()
{
    sidePanel->show();
}

}
#include "bresseinmanager.moc"
