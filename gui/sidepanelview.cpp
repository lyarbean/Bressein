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
#include "contactsscene.h"
#include "contactitem.h"
#include "chatview.h"
#include <QGraphicsLinearLayout>


namespace Bressein
{
SidepanelView::SidepanelView (QWidget *parent)
    : QGraphicsView (parent)
    , gscene (new ContactsScene (this))
    , linearLayout (new QGraphicsLinearLayout)
{
    setAlignment (Qt::AlignLeft | Qt::AlignTop);
    setRenderingSystem();
    setupScene();
    // add default group
    QByteArray id = "0";
    QByteArray name = "untitled";
    addGroup (id,name);
}

SidepanelView::~SidepanelView()
{
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
        ContactItem *groupItem = 0;
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
            foreach (ContactItem *it, groups)
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
            qDebug() << "XXXXXXXXXXXXX groupId" <<groupId;
            // TODO make a new group?
            item = new ContactItem (groups.first());
            item->setPos (5, 10 + groups.first()->childrenBoundingRect().height());
        }
        item->setSipuri (contact);
        item->updateContact (contactInfo);
        item->setZValue (10);
        item->setVisible (true);
        itemList.append (item);
        //TODO adjust size
    }
    resizeScene();
}

void SidepanelView::addGroup (const QByteArray &id, const QByteArray &name)
{

    ContactItem *item = new ContactItem;
    item->setFont (QFont ("Times",14, QFont::Bold));
    item->setPlainText (QString::fromUtf8 (name));
    item->setData (1,id);
    qDebug() << "addGroup"<< QString::fromUtf8 (name);
    groups.append (item);
    gscene->addItem (item);
    resizeScene();
//     linearLayout->addItem(item);
//     item->setPos(0,gscene->itemsBoundingRect().height());

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
    setViewportUpdateMode (QGraphicsView::FullViewportUpdate);
    setDragMode (QGraphicsView::ScrollHandDrag);

}

void SidepanelView::setupScene()
{
    // indirectly pass through
    gscene->setSceneRect (0, 0, 300, 600);
    gscene->setItemIndexMethod (QGraphicsScene::NoIndex);
//     QGraphicsWidget *form = new QGraphicsWidget;
//     form->setLayout(linearLayout);
//     gscene->addItem(form);
    setScene (gscene);
}

void SidepanelView::setupSceneItems()
{
    // setup buttons
    //
}

void SidepanelView::resizeScene()
{
    int height = 0;
    QList<ContactItem *>::iterator it = groups.begin();
    while (it not_eq groups.end())
    {
        (*it)->setPos (0,height);
        height += (*it)->childrenBoundingRect().height();
        if (not (*it)->childItems().isEmpty())
            height += (*it)->childItems().last()->boundingRect().height();
        ++it;
    }
    if (not itemList.isEmpty())
    {
        ensureVisible (itemList.last());
        height += itemList.last()->boundingRect().height();
    }
    if (height > 600)
        gscene->setSceneRect (0,0, 300, height);

    updateGeometry();
}


void SidepanelView::resizeEvent (QResizeEvent *event)
{
    QGraphicsView::resizeEvent (event);
    if (scene())
    {
        scene()->invalidate (scene()->sceneRect());
    }
}

}
#include "sidepanelview.moc"
