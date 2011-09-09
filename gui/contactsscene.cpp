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


#include "contactsscene.h"
#include "contactitem.h"
#include "groupitem.h"

#include <QResizeEvent>
namespace Bressein
{
ContactsScene::ContactsScene (QObject *parent) : QGraphicsScene (parent)
{
    setSceneRect (0, 0, itemsBoundingRect().width(), itemsBoundingRect().height());
    setItemIndexMethod (QGraphicsScene::BspTreeIndex);
    setSortCacheEnabled (false);
}

ContactsScene::~ContactsScene()
{

}

void ContactsScene::addGroup (const QByteArray &id, const QByteArray &name)
{
    qDebug() << "To add group" << "id" << QString::fromUtf8 (name);
    GroupItem *groupItem = 0;
    foreach (QGraphicsItem *gitem, items())
    {
        groupItem = qgraphicsitem_cast<GroupItem *> (gitem);
        if (groupItem and groupItem->data (1).toByteArray() == id)
        {
            groupItem->setGroupName (name);
            return;
        }
    }
    groupItem = new GroupItem (0, this);
    groupItem->setGroupName (name);
    groupItem->setData (1, id);
    groupItem->setZValue (0);
    groupItem->setVisible (true);
    resizeScene();
    qDebug() << "group" << "id" << id << QString::fromUtf8 (name) << "Added";
}

void ContactsScene::updateContact (const QByteArray &contact,
                                   ContactInfo *contactInfo)
{
    if (items().isEmpty())
    {
        return;
    }
    ContactItem *item = 0;
    GroupItem *groupItem = 0;
    QByteArray groupId = contactInfo->groupId;
    // one may have multi-groups, like, 1;0;2
    // we take the first group id
    if (groupId.isEmpty())
    {
        groupId = "0";
    }
    else if (groupId.contains (';'))
    {
        QList<QByteArray> ids = groupId.split (';');
        groupId = ids.first();
    }
    foreach (QGraphicsItem *gitem, items())
    {
        groupItem = qgraphicsitem_cast<GroupItem *> (gitem);
        if (groupItem and groupItem->data (1).toByteArray() == groupId)
        {
            foreach (QGraphicsItem *citem, groupItem->childItems())
            {
                item = qgraphicsitem_cast<ContactItem *> (citem);
                if (item and item->getSipuri() == contact)
                {
                    // found
                    // update this
                    item->updateContact (contactInfo);
                    resizeScene();
                    qDebug() << "contactInfo updated!";
                    return;
                }
            }
            // if not found
            item = new ContactItem (groupItem, this);
            item->setSipuri (contact);
            item->updateContact (contactInfo);
            item->setZValue (1);
            item->setVisible (true);
            connect (item, SIGNAL (sendMessage (QByteArray, QByteArray)),
                     this, SLOT (onSendMessage (QByteArray, QByteArray)));
            qDebug() << "New contactItem added!";
            resizeScene();
            return;
        }
        groupItem = 0;
    }
}

void ContactsScene::updateContactPortrait (const QByteArray &contact,
                                           const QByteArray &portrait)
{
    ContactItem *item = 0;
    GroupItem *groupItem = 0;
    if (not items().isEmpty())
    {
        foreach (QGraphicsItem *gitem, items())
        {
            groupItem = qgraphicsitem_cast<GroupItem *> (gitem);
            if (groupItem)
            {
                foreach (QGraphicsItem *citem, groupItem->childItems())
                {
                    item = qgraphicsitem_cast<ContactItem *> (citem);
                    if (item and item->getSipuri() == contact)
                    {
                        // update this
                        item->updatePortrait (portrait);
                        break;
                    }
                    item = 0;
                }
                groupItem = 0;
            }
        }
    }
}

void ContactsScene::resizeScene()
{
    qDebug() << "resizeScene called";
    if (items().isEmpty())
    {
        return;
    }
    qreal height = 0;
    qreal subHeight = 0;
    GroupItem *groupItem = 0;
    ContactItem *contactItem = 0;
    int itemsCount = items().size();
    int subItemsCount = 0;
    int index = 0;
    int subIndex = 0;
    while (index < itemsCount)
    {
        groupItem = qgraphicsitem_cast<GroupItem * > (items().at (index));
        if (groupItem)
        {
            groupItem->setPos (0, height);
            height += groupItem->boundingRect().height();
            // update positions of it's childItems
            if (groupItem->isShowChildItems())
            {
                if (not groupItem->childItems().isEmpty())
                {
                    subHeight = groupItem->boundingRect().height();
                    subItemsCount = groupItem->childItems().size();
                    while (subIndex < subItemsCount)
                    {
                        contactItem = qgraphicsitem_cast<ContactItem *>
                                      (groupItem->childItems().at (subIndex));
                        if (contactItem and contactItem->isVisible())
                        {
                            contactItem->setPos (5, subHeight);
                            subHeight += contactItem->boundingRect().height();
                            subHeight += 3;
                            contactItem = 0;
                        }
                        ++subIndex;
                    }
                    subIndex = 0;
                    height += groupItem->childrenBoundingRect().height() + 3;
                }
            }
            groupItem = 0;
        }
        ++index;
    }
    setSceneRect (0, 0, width(), height);
}
void ContactsScene::setWidth (int w)
{
    ContactItem *item = 0;
    GroupItem *groupItem = 0;
    if (w > 250 and not items().isEmpty())
    {
        foreach (QGraphicsItem *gitem, items())
        {
            groupItem = qgraphicsitem_cast<GroupItem *> (gitem);
            if (groupItem)
            {
                foreach (QGraphicsItem *citem, groupItem->childItems())
                {
                    item = qgraphicsitem_cast<ContactItem *> (citem);
                    if (item)
                    {
                        item->setTextWidth (w - 10);
                        item->update();
                        item = 0;
                    }
                }
                groupItem = 0;
            }
        }
    }
//         resizeScene();
}

void ContactsScene::onIncomeMessage (const QByteArray &contact,
                                     const QByteArray &datetime,
                                     const QByteArray &content)
{
    ContactItem *item = 0;
    GroupItem *groupItem = 0;
    if (not items().isEmpty())
    {
        foreach (QGraphicsItem *gitem, items())
        {
            groupItem = qgraphicsitem_cast<GroupItem *> (gitem);
            if (groupItem)
            {
                foreach (QGraphicsItem *citem, groupItem->childItems())
                {
                    item = qgraphicsitem_cast<ContactItem *> (citem);
                    if (item and item->getSipuri() == contact)
                    {
                        // update this
                        item->onIncomeMessage (datetime, content);
                        return;
                    }
                }
                groupItem = 0;
            }
        }
    }
}

void ContactsScene::onSendMessage (const QByteArray &sipuri,
                                   const QByteArray &message)
{
    emit sendMessage (sipuri, message);
}


void ContactsScene::contextMenuEvent (QGraphicsSceneContextMenuEvent *event)
{
    QGraphicsScene::contextMenuEvent (event);
    // add some functionalities here
}

}
#include "contactsscene.moc"
