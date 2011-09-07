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

#include "sidepanelview.h"
#include "sipc/account.h"
#include "loginscene.h"
#include "contactsscene.h"
#include "contactitem.h"
#include "chatview.h"
#include <QGraphicsWidget>
namespace Bressein
{
SidepanelView::SidepanelView (QWidget *parent)
    : QGraphicsView (parent)
    , loginScene (new LoginScene (this))
    , contactsScene (new ContactsScene (this))

{
    setRenderingSystem();
    setScene (loginScene);
    connect (loginScene,
             SIGNAL (loginCommit (const QByteArray &, const QByteArray &)),
             this,
             SLOT (onLoginCommit (const QByteArray &, const QByteArray &)));
    connect (loginScene,
             SIGNAL (verifyCommit (const QByteArray &)),
             this,
             SLOT (onVerifycommit (QByteArray)));
    setMinimumSize (loginScene->itemsBoundingRect().size().toSize());
}

SidepanelView::~SidepanelView()
{
    loginScene->deleteLater();
    contactsScene->deleteLater();
    itemList.clear();
}

void SidepanelView::setHostSipuri (const QByteArray &sipuri)
{
    hostSipuri = sipuri;
}

void SidepanelView::setNickname (const QByteArray &nickname)
{
    myNickname = nickname;
}

void SidepanelView::updateContact (const QByteArray &contact,
                                   ContactInfo *contactInfo)
{
    qDebug() << "SidepanelView::updateContact" << contact;
    bool updated = false;
    if (not itemList.isEmpty())
    {
        int itemlists = itemList.size();
        for (int i = 0; i < itemlists; i++)
        {
            if (itemList.at (i)->getSipuri() == contact)
            {
                // update this
                itemList.at (i)->updateContact (contactInfo);
                // FIXME need resorting sometimes
                resizeScene();
                updated = true;
                break;
            }
        }
    }
    if (not updated)
    {
        ContactItem *item;
        QGraphicsSimpleTextItem *groupItem = 0;
        QByteArray groupId = contactInfo->groupId;
        // one may have multi-groups, like, 1;0;2
        // we take the first group id
        QList<QByteArray> ids = groupId.split (';');
        if (not ids.isEmpty())
        {
            groupId = ids.first();
        }
        if (groupId.isEmpty())
        {
            groupId = "0";
        }
        if (not groups.isEmpty())
        {
            foreach (QGraphicsSimpleTextItem *it, groups)
            {
                if (it->data (1).toByteArray() == groupId)
                {
                    groupItem = it;
                    break;
                }
            }
        }
        if (groupItem)
        {
            item = new ContactItem (groupItem);
            item->setParentItem (groupItem);
        }
        else // should not been called, just in case
        {
            qDebug() << "XXXXXXXXXXXXX groupId" << groupId;
            item = new ContactItem (groups.first());
            item->setParentItem (groups.first());
        }
        item->setSipuri (contact);
        item->updateContact (contactInfo);
        item->setZValue (0);
        item->setVisible (true);
        connect (item, SIGNAL (sendMessage (QByteArray, QByteArray)),
                 this, SLOT (onSendMessage (QByteArray, QByteArray)));
        itemList.append (item);
        //TODO adjust size
    }
}

void SidepanelView::addGroup (const QByteArray &id, const QByteArray &name)
{
    foreach (QGraphicsSimpleTextItem *it, groups)
    {
        if (it->data (1).toByteArray() == id)
        {
            it->setText (QString::fromUtf8 (name));
            return;
        }
    }
    QGraphicsSimpleTextItem *item = new QGraphicsSimpleTextItem;
    item->setFont (QFont ("Times", 18, QFont::Bold));
    item->setPen (QPen (QColor (0x88888888)));
    item->setText (QString::fromUtf8 (name));
    item->setData (1, id);
    item->setZValue (0);
    groups.append (item);
    contactsScene->addItem (item);
}

void SidepanelView::onLoginCommit (const QByteArray &n, const QByteArray &p)
{
    emit toLogin (n, p);
}

void SidepanelView::onVerifycommit (const QByteArray &c)
{
    emit toVerify (c);
}

void SidepanelView::onSendMessage (const QByteArray &sipuri,
                                   const QByteArray &message)
{
    emit sendMessage (sipuri, message);
}


//TODO onContactRemoved
void SidepanelView::setRenderingSystem()
{
//     QWidget *viewport = 0;
//
// //     #ifndef QT_NO_OPENGL
// //         QGLWidget *glw = new QGLWidget(QGLFormat(QGL::SampleBuffers));
// //         if (Colors::noScreenSync)
// //             glw->format().setSwapInterval(0);
// //         glw->setAutoFillBackground(false);!@#
// //         viewport = glw;
// //         setCacheMode(QGraphicsView::CacheNone);
// //
// //     #endif
//     // software rendering
//     viewport = new QWidget;
//     setViewport (viewport);
    setAlignment (Qt::AlignLeft | Qt::AlignTop);
    setWindowFlags (Qt::Window);
    setMaximumWidth (600);
    setContentsMargins (0,0,0,0);
    setBackgroundRole (QPalette::BrightText);
    setForegroundRole (QPalette::NoRole);
    setRenderHints (QPainter::Antialiasing |
                    QPainter::SmoothPixmapTransform|
                    QPainter::HighQualityAntialiasing);
    setCacheMode (QGraphicsView::CacheNone);
    setViewportUpdateMode (QGraphicsView::FullViewportUpdate);
    setDragMode (QGraphicsView::ScrollHandDrag);

}

void SidepanelView::updateContactPortrait (const QByteArray &contact,
                                           const QByteArray &portrait)
{
    if (not itemList.isEmpty())
    {
        int itemlists = itemList.size();
        for (int i = 0; i < itemlists; i++)
        {
            if (itemList.at (i)->getSipuri() == contact)
            {
                // update this
                itemList.at (i)->updatePortrait (portrait);
                break;
            }
        }
    }
}


void SidepanelView::setupContactsScene()
{
    // indirectly pass through

    int itemlists = itemList.size();
    for (int i = 0; i < itemlists; i++)
    {
        itemList.at (i)->setHostSipuri (hostSipuri);
        itemList.at (i)->setHostName (myNickname);
    }

    setScene (contactsScene);
    resizeScene();
    setMinimumSize (contactsScene->itemsBoundingRect().width() *3, 500);
    viewport()->resize (contactsScene->width(), contactsScene->height());

}

void SidepanelView::activateLogin (bool ok)
{
    loginScene->setEnable (ok);
}

void SidepanelView::onIncomeMessage (const QByteArray &contact,
                                     const QByteArray &datetime,
                                     const QByteArray &content)
{
    ContactItem *item;
    int itemlists = itemList.size();
    for (int i = 0; i < itemlists; i++)
    {
        if (itemList.at (i)->getSipuri() == contact)
        {
            item = itemList.at (i);
            break;
        }
    }
    if (item)
    {
        item->onIncomeMessage (datetime, content);
    }
    else // TODO if contact is from stranger
    {

    }
}

void SidepanelView::requestVerify (const QByteArray &datum)
{
    loginScene->onRequseVerify (datum);
}


void SidepanelView::setupSceneItems()
{
    // setup buttons
    //
}

void SidepanelView::resizeScene()
{
    //FIXME
    qDebug() << "resizeScene called";
    qreal height = 0;
    qreal subHeight = 0;
    QList<QGraphicsSimpleTextItem *>::iterator it = groups.begin();
    while (it not_eq groups.end())
    {

        (*it)->setPos (0, height);
        height += (*it)->boundingRect().height();
        // update positions of it's childItems
        if (not (*it)->childItems().isEmpty())
        {
            subHeight = (*it)->boundingRect().height();
            foreach (ContactItem *item, itemList)
            {
                if (item->parentItem() == (*it))
                {
                    item->setPos (5, subHeight);
                    subHeight += item->boundingRect().height() +3;
                }
            }

        }
        height += (*it)->childrenBoundingRect().height();
        ++it;
    }
    if (height > 600)
    {
        contactsScene->setSceneRect (0, 0, contactsScene->width(), height);
    }
}


void SidepanelView::resizeEvent (QResizeEvent *event)
{

    QGraphicsView::resizeEvent (event);
    int w = event->size().width();
    if (w)
    {
        foreach (ContactItem *item, itemList)
        {
            item->setTextWidth (w - 10);
            item->update();
        }
    }
}

void SidepanelView::closeEvent (QCloseEvent *event)
{
    // TODO
    if (scene() == contactsScene)
    {
        event->ignore();
        hide();
    }
    else
    {
        // TODO  ask for confirm to close
    }
//     QWidget::closeEvent (event);
}

}
#include "sidepanelview.moc"
