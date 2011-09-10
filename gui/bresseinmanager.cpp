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
#include "sipc/aux.h"
#include "chatview.h"
#include "sidepanelview.h"
#include "contactsscene.h"

#include <QApplication>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QMenu>
#include <QTimer>
#include <QDir>
#include <QLabel>
/**
struct iiibiiay
{
    int width;
    int height;
    int rowstride;
    bool hasAlpha;
    int bitsPerSample;
    int channels;
    QByteArray image;
};
Q_DECLARE_METATYPE (iiibiiay)
QDBusArgument &operator<< (QDBusArgument &a, const iiibiiay &i)
{
    a.beginStructure();
    a << i.width << i.height << i.rowstride << i.hasAlpha
      << i.bitsPerSample << i.channels << i.image;
    a.endStructure();
    return a;
}

const QDBusArgument &operator >> (const QDBusArgument &a,  iiibiiay &i)
{
    a.beginStructure();
    a >> i.width >> i.height >> i.rowstride >> i.hasAlpha >>
      i.bitsPerSample >> i.channels >> i.image;
    a.endStructure();
    return a;
}
**/
namespace Bressein
{
BresseinManager::BresseinManager (QObject *parent)
    : QObject (parent),
      account (new Account),
      // loginDialog (new LoginWidget),
      sidePanel (new SidepanelView),
      contactsScene (new ContactsScene (this)),
      tray (new QSystemTrayIcon (this)),
      trayIconMenu (new QMenu (0))
{
    connectSignalSlots();
    initialize();
    initializeTray();
    sidePanel->show();
}

BresseinManager::~BresseinManager()
{
}

//TODO initialize database
void BresseinManager::initialize()
{
    bresseinIcon = QDir::homePath().append ("/.bressein/icons/").
                   append ("bressein.png").toLocal8Bit();
    if (not QDir::root().exists (bresseinIcon))
    {
        QImage image (":/images/envelop_48.png");
        image.save (bresseinIcon, "png");
    }
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
    QDBusInterface notify ("org.freedesktop.Notifications",
                           "/org/freedesktop/Notifications",
                           "org.freedesktop.Notifications");
    QVariantMap hints;
    hints.insert ("category", QString ("presence.offline"));
    QVariantList args;
    args << qAppName(); //app_name
    args << uint (0);
    args << QString (bresseinIcon); //app_icon
    args << QString ("Bressein"); //summary
    args << QString (tr ("Login as ") + number); // body
    args << QStringList(); // actions
    args << hints;
    args << int (5000);// timeout
    QDBusMessage call =
        notify.callWithArgumentList (QDBus::NoBlock, "Notify", args);
}

void BresseinManager::onContactChanged (const QByteArray &contact)
{
    if (not contacts.contains (contact))
    {
        ContactInfo *contactInfo = new ContactInfo;
        contacts.insert (contact, contactInfo);
    }
    account->getContactInfo (contact, *contacts.value (contact));
    contactsScene->updateContact (contact, contacts.value (contact));
    if (not portraitMap.contains (contact))
    {
        onPortraitDownloaded (contact);
    }
}

void BresseinManager::onPortraitDownloaded (const QByteArray &contact)
{
    QByteArray portrait = QDir::homePath().append ("/.bressein/icons/").
                          append (sipToFetion (contact)).
                          append (".jpeg").toLocal8Bit();
    if (QDir::root().exists (portrait))
    {
        portraitMap.insert (contact, portrait);
        contactsScene->updateContactPortrait (contact, portrait);
    }
    else
    {
        contactsScene->updateContactPortrait (contact, bresseinIcon);
    }
}

void BresseinManager::dbusNotify (const QString &summary,
                                  const QString &text)
{
    QDBusInterface notify ("org.freedesktop.Notifications",
                           "/org/freedesktop/Notifications",
                           "org.freedesktop.Notifications");
    QVariantMap hints;
    hints.insert ("category", QString ("presence"));
    QVariantList args;
    args << qAppName(); //app_name
    args << uint (0);
    args << QString (bresseinIcon); //app_icon
    args << summary; //summary
    args << text;  // body
    args << QStringList(); // actions
    args << hints;
    args << int (1000);// timeout
    QDBusMessage call =
        notify.callWithArgumentList (QDBus::NoBlock, "Notify", args);
}

void BresseinManager::onGroupChanged (const QByteArray &id,
                                      const QByteArray &name)
{
    contactsScene->addGroup (id, name);
}

void BresseinManager::onWrongPassword()
{
    // account->close();
    sidePanel->activateLogin (true);
}


void BresseinManager::readyShow()
{
    account->getFetion (mySipc);
    initializePortrait();
    QByteArray nickname;
    account->getNickname (nickname);
    sidePanel->setScene (contactsScene);
    sidePanel->resize (contactsScene->width(),600);
    sidePanel->showNormal();
}

// TODO move to loginScene
void BresseinManager::onVerificationPic (const QByteArray &data)
{
    sidePanel->requestVerify (data);
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
            break;
        case QSystemTrayIcon::DoubleClick:
            if (sidePanel->isHidden())
            {
                sidePanel->show();
            }
            else
            {
                sidePanel->setWindowState (Qt::WindowNoState);
                sidePanel->showNormal();
                sidePanel->raise();
            }
            break;
        case QSystemTrayIcon::MiddleClick:
            //TODO
            break;
        default:
            ;
    }
}

void BresseinManager::onIncomeMessage (const QByteArray &contact,
                                       const QByteArray &date,
                                       const QByteArray &content)
{
    contactsScene->onIncomeMessage (contact, date, content);

    QDBusInterface notify ("org.freedesktop.Notifications",
                           "/org/freedesktop/Notifications",
                           "org.freedesktop.Notifications");
    QVariantMap hints;
    hints.insert ("category", QString ("im.received"));
    QVariantList args;
    args << qAppName(); //app_name
    args << uint (0);
    args << QString (portraitMap.value (contact)); //app_icon
    args << QString (tr ("You have message.")); //summary
    // TODO make a prefer name for contact
    args << QString::fromUtf8 (contacts.value (contact)->localName +
                               contacts.value (contact)->nickName + "\n" +
                               date + "\n" + content); // body
    args << QStringList(); // actions
    args << hints;
    args << int (3000);// timeout
    QDBusMessage call =
        notify.callWithArgumentList (QDBus::NoBlock, "Notify", args);
}

void BresseinManager::onNotSentMessage (const QByteArray &sipuri,
                                        const QDateTime &datetime,
                                        const QByteArray &content)
{
    QString textbody = datetime.toLocalTime().toString();
    textbody.append ("<br/>");
    textbody.append (QString::fromUtf8 (content));
    dbusNotify (tr ("Message failed to send"),textbody);
}

void BresseinManager::bye()
{
    account->setOffline();
    QDBusInterface notify ("org.freedesktop.Notifications",
                           "/org/freedesktop/Notifications",
                           "org.freedesktop.Notifications");
    QVariantMap hints;
    hints.insert ("category", QString ("presence.offline"));
    QVariantList args;
    args << qAppName(); //app_name
    args << uint (0);
    args << QString (bresseinIcon); //app_icon
    args << QString ("Bressein"); //summary
    args << QString (tr ("Bye"));  // body
    args << QStringList(); // actions
    args << hints;
    args << int (3000);// timeout
    QDBusMessage call =
        notify.callWithArgumentList (QDBus::NoBlock, "Notify", args);
    QApplication::processEvents (QEventLoop::AllEvents);
    QTimer::singleShot (1000, qApp, SLOT (quit()));
}

void BresseinManager::initializeTray()
{
    // trayIconMenu.addAction ...
    QAction *action;
    action = new QAction (tr ("Online"), tray);
    connect (action, SIGNAL (triggered ()), account, SLOT (setOnline()));
    trayIconMenu->addAction (action);
    action = new QAction (tr ("Offline"), tray);
    connect (action, SIGNAL (triggered ()), account, SLOT (setOffline()));
    trayIconMenu->addAction (action);
    action = new QAction (tr ("Busy"), tray);
    connect (action, SIGNAL (triggered ()), account, SLOT (setBusy()));
    trayIconMenu->addAction (action);
    action = new QAction (tr ("Hidden"), tray);
    connect (action, SIGNAL (triggered ()), account, SLOT (setHidden()));
    trayIconMenu->addAction (action);
    action = new QAction (tr ("Metting"), tray);
    connect (action, SIGNAL (triggered ()), account, SLOT (setMeeting()));
    trayIconMenu->addAction (action);
    action = new QAction (tr ("quit"), tray);
    connect (action, SIGNAL (triggered (bool)), this, SLOT (bye()));
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
    connect (account,
             SIGNAL (groupChanged (const QByteArray &, const QByteArray &)),
             this,
             SLOT (onGroupChanged (const QByteArray &, const QByteArray &)),
             Qt::QueuedConnection);
    connect (account, SIGNAL (contactChanged (const QByteArray &)),
             this, SLOT (onContactChanged (const QByteArray &)),
             Qt::QueuedConnection);
    connect (account, SIGNAL (portraitDownloaded (const QByteArray &)),
             this, SLOT (onPortraitDownloaded (const QByteArray &)),
             Qt::QueuedConnection);
    connect (account, SIGNAL (logined()), this, SLOT (readyShow()),
             Qt::QueuedConnection);
    connect (account, SIGNAL (verificationPic (const QByteArray &)),
             this, SLOT (onVerificationPic (const QByteArray &)));
    connect (account, SIGNAL (incomeMessage (const QByteArray &,
                                             const QByteArray &,
                                             const QByteArray &)),
             this, SLOT (onIncomeMessage (const QByteArray &,
                                          const QByteArray &,
                                          const QByteArray &)),
             Qt::QueuedConnection);
    connect (account, SIGNAL (wrongPassword()),
             this, SLOT (onWrongPassword()),
             Qt::QueuedConnection);
    connect (account, SIGNAL (sipcAuthorizeParsed()),
             this, SLOT (onStateAuthorized()), Qt::QueuedConnection);
    connect (account, SIGNAL (contactStateChanged (const QByteArray &, int)),
             this, SLOT (showOnlineState (const QByteArray &, int)),
             Qt::QueuedConnection);
    connect (account,
             SIGNAL (notSentMessage (QByteArray, QDateTime, QByteArray)),
             this, SLOT (onNotSentMessage (QByteArray,QDateTime,QByteArray)),
             Qt::QueuedConnection);
    connect (sidePanel,
             SIGNAL (toLogin (const QByteArray &, const QByteArray &)),
             this,
             SLOT (loginAs (const QByteArray &, const QByteArray &)));
    connect (sidePanel,
             SIGNAL (toVerify (QByteArray)),
             account,
             SLOT (loginVerify (const QByteArray &)));
    connect (contactsScene,
             SIGNAL (sendMessage (const QByteArray &, const QByteArray &)),
             account,
             SLOT (sendMessage (const QByteArray &, const QByteArray &)),
             Qt::QueuedConnection);
}

void BresseinManager::initializePortrait()
{
    myPortrait = QDir::homePath().append ("/.bressein/icons/").
                 append (mySipc).append (".jpeg").toLocal8Bit();

    if (not QDir::root().exists (myPortrait))
    {
        myPortrait = bresseinIcon;
    }
}

void BresseinManager::showOnlineState (const QByteArray &contact, int state)
{
    QDBusInterface notify ("org.freedesktop.Notifications",
                           "/org/freedesktop/Notifications",
                           "org.freedesktop.Notifications");
    QVariantMap hints;
    hints.insert ("category", QString ("presence.offline"));
    mutex.lock();
    ContactInfo *contactInfo = contacts.value (contact);
    QVariantList args;
    args << qAppName(); //app_name
    args << uint (0);
    args << QString (portraitMap.value (contact)); //app_icon
    if (not contactInfo->nickName.isEmpty())
    {
        args << QString::fromUtf8 (contactInfo->nickName);
    }
    else if (not contactInfo->localName.isEmpty())
    {
        args << QString::fromUtf8 (contactInfo->localName);
    }
    else
    {
        args << QString::fromUtf8 (contactInfo->mobileno);
    }
    switch (state)
    {
        case StateType::ONLINE:
            args << tr ("online");
            break;
        case StateType::RIGHTBACK:
            args << tr ("right back");
            break;
        case StateType::AWAY:
            args << tr ("away");
            break;
        case StateType::BUSY:
            args << tr ("busy");
            break;
        case StateType::OUTFORLUNCH:
            args << tr ("out for lunch");
            break;
        case StateType::ONTHEPHONE:
            args << tr ("on the phone");
            break;
        case StateType::DONOTDISTURB:
            args << tr ("don't disturb");
            break;
        case StateType::MEETING:
            args << tr ("metting");
            break;
        case StateType::HIDDEN:
            args << tr ("hidden");
            break;
        case StateType::OFFLINE:
            args << tr ("offline");
            break;
        default:
            args << QString::number (contactInfo->state);
            break;
    }
    args << QStringList(); // actions
    args << hints;
    args << int (1500);// timeout
    mutex.unlock();
    QDBusMessage call =
        notify.callWithArgumentList (QDBus::NoBlock, "Notify", args);
}

}
#include "bresseinmanager.moc"
