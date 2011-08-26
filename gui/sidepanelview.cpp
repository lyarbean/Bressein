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
#include "singleton.h"
#include "sipc/account.h"
#include "loginscene.h"
#include "contactsscene.h"
#include "contactitem.h"
#include "chatview.h"

namespace Bressein
{
SidepanelView::SidepanelView (QWidget *parent)
    : QGraphicsView (parent)
    , loginScene (new LoginScene (this))
    , contactsScene (new ContactsScene (this))

{
    setAlignment (Qt::AlignLeft | Qt::AlignTop);
    setRenderingSystem();
    setScene (loginScene);
    connect (loginScene,
             SIGNAL (loginCommit (const QByteArray &, const QByteArray &)),
             this,SLOT (onLoginCommit (const QByteArray &, const QByteArray &)));
    // add default group
    QByteArray id = "0";
    QByteArray name = "Untitled";
    addGroup (id, name);
}

SidepanelView::~SidepanelView()
{
    loginScene->deleteLater();
    contactsScene->deleteLater();
    itemList.clear();
}


void SidepanelView::updateContact (const QByteArray &contact,
                                   const ContactInfo &contactInfo)
{
    qDebug() << "SidepanelView::updateContact";
    bool updated = false;
    int itemlists = itemList.size();

    updated = false;
    for (int i = 0; i < itemlists; i++)
    {
        if (itemList.at (i)->getSipuri() == contact)
        {
            // update this
            itemList.at (i)->updateContact (contactInfo);
            updated = true;
            break;
        }
    }
    if (not updated)
    {
        ContactItem *item;
        QGraphicsSimpleTextItem *groupItem = 0;
        QByteArray groupId = contactInfo.basic.groupId;
        // one may have multi-groups, like, 1;0;2
        // we take the first group id
        QList<QByteArray> ids = groupId.split (';');
        if (not ids.isEmpty())
            groupId = ids.first();
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
            item->setPos (5, 10 + groupItem->childrenBoundingRect().height());
            // for each other groups whose id is great than that of this,
            // arrange them
        }
        else // should not been called, just in case
        {
            qDebug() << "XXXXXXXXXXXXX groupId" << groupId;
            // TODO make a new group?
            item = new ContactItem (groups.first());
            item->setPos (5, 10 + groups.first()->childrenBoundingRect().height());
        }
        item->setSipuri (contact);
        item->updateContact (contactInfo);
        item->setZValue (1);
        item->setVisible (true);
        itemList.append (item);
        //TODO adjust size
    }
    resizeScene();
}

void SidepanelView::addGroup (const QByteArray &id, const QByteArray &name)
{

    QGraphicsSimpleTextItem *item = new QGraphicsSimpleTextItem;
    item->setFont (QFont ("Times", 14, QFont::Bold));
    item->setText (QString::fromUtf8 (name));
    item->setData (1, id);
    item->setZValue (0);
    groups.append (item);
    contactsScene->addItem (item);
    resizeScene();
//     linearLayout->addItem(item);
//     item->setPos(0,gscene->itemsBoundingRect().height());

}

void SidepanelView::onLoginCommit (const QByteArray &n, const QByteArray &p)
{
    emit toLogin (n,p);
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
    setRenderHints (QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setCacheMode (QGraphicsView::CacheBackground);
    setViewportUpdateMode (QGraphicsView::BoundingRectViewportUpdate);
    setDragMode (QGraphicsView::ScrollHandDrag);

}

void SidepanelView::setupContactsScene()
{
    // indirectly pass through
    setScene (contactsScene);
    setSceneRect (contactsScene->itemsBoundingRect());
    resize(contactsScene->itemsBoundingRect().size().toSize());
}


void SidepanelView::setupSceneItems()
{
    // setup buttons
    //
}

void SidepanelView::resizeScene()
{
    //FIXME
    int height = 0;
    QList<QGraphicsSimpleTextItem *>::iterator it = groups.begin();
    while (it not_eq groups.end())
    {
        (*it)->setPos (0, height);
        while (not (*it)->collidingItems().isEmpty())
        {
            foreach (QGraphicsItem *item, (*it)->collidingItems())
            {
                item->setPos (item->pos().x(), item->pos().y() + 1);
            }
        }
        // update positions of it's childItems
        foreach (QGraphicsItem *citem, (*it)->childItems())
        {
            while (not citem->collidingItems().isEmpty())
            {
                foreach (QGraphicsItem *ccitem, citem->collidingItems())
                {
                    ccitem->setPos (ccitem->pos().x(), ccitem->pos().y() + 1);
                }
            }
        }
        height += (*it)->childrenBoundingRect().height();
        if (not (*it)->childItems().isEmpty())
            height += (*it)->childItems().last()->boundingRect().height();
        ++it;
    }
    if (not itemList.isEmpty())
    {
//         ensureVisible (itemList.last());
        height += itemList.last()->boundingRect().height();
    }
    if (height > 600)
        contactsScene->setSceneRect (0, 0, contactsScene->width(), height);
    updateGeometry();
}


void SidepanelView::resizeEvent (QResizeEvent *event)
{
    QGraphicsView::resizeEvent (event);
//     if (scene())
//     {
//         scene()->invalidate (scene()->sceneRect());
//     }
}

void SidepanelView::closeEvent (QCloseEvent *event)
{
    // TODO
    event->ignore();
    hide();
//     QWidget::closeEvent (event);
}

}
#include "sidepanelview.moc"
