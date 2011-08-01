/*
 *  An scene to display contact in views*
 *  Copyright (C) 2011  颜烈彬 <slbyan@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 *  OpenSSL linking exception
 *  --------------------------
 *  If you modify this Program, or any covered work, by linking or
 *  combining it with the OpenSSL project's "OpenSSL" library (or a
 *  modified version of that library), containing parts covered by
 *  the terms of OpenSSL/SSLeay license, the licensors of this
 *  Program grant you additional permission to convey the resulting
 *  work. Corresponding Source for a non-source form of such a
 *  combination shall include the source code for the parts of the
 *  OpenSSL library used as well as that of the covered work.
 */


#include <QGraphicsSceneMouseEvent>
#include "contactsscene.h"
#include "contactitem.h"

namespace Bressein
{
ContactsScene::ContactsScene()
{
    qDebug() <<"on ctor";
    QGraphicsItem *item = addText("Hello Bressein");
    QGraphicsItem *item2 = addText("Author: lyarbean");
    item->setPos(item2->pos()+QPointF(0,32));
    qDebug() << "right after addText";
}

ContactsScene::~ContactsScene()
{

}
void ContactsScene::mouseMoveEvent (QGraphicsSceneMouseEvent *mouseEvent)
{
    QGraphicsScene::mouseMoveEvent (mouseEvent);
}

void ContactsScene::mousePressEvent (QGraphicsSceneMouseEvent *mouseEvent)
{
    QGraphicsScene::mousePressEvent (mouseEvent);
}

void ContactsScene::mouseReleaseEvent (QGraphicsSceneMouseEvent *mouseEvent)
{
    QGraphicsScene::mouseReleaseEvent (mouseEvent);
}

void ContactsScene::mouseDoubleClickEvent
(QGraphicsSceneMouseEvent *mouseEvent)
{
    ContactItem *item = qgraphicsitem_cast<ContactItem *>
                        (itemAt (mouseEvent->pos()));
    if (item)
    {
        emit contactActivated (item->data().userId);
        qDebug() << "item->data()->userId" << item->data().userId;
    }
}

void ContactsScene::onDataChanged (const Contact& contact)
{
    qDebug() << "onDataChanged" << contact.sipuri;
    bool updated = false;
    for (int i=0, s=itemList.size(); i < s; i++)
    {
        if (itemList.at(i)->data().sipuri == contact.sipuri)
        {
            // update this
            itemList.at(i)->setData(contact);
            updated = true;
        }
    }
    if (not updated)
    {
        ContactItem * item;
        if ( itemList.isEmpty())
        {
            item = new ContactItem;
        } else {
            item = new ContactItem(itemList.last());
        }
        item->setData(contact);
        this->addItem(item);
    }
}

void ContactsScene::onDataRemoved (const Contact& contact)
{
    for (int i=0; i< itemList.size(); i++)
    {
        if (itemList.at(i)->data().sipuri == contact.sipuri)
        {
            // update this
            removeItem(itemList.at(i));
            itemList.removeAt(i);
        }
    }

}

}
#include "contactsscene.moc"
