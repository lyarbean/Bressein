/*
 *  This file is part of Bressein.
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


#include "contactitem.h"
#include <QtGui>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include "sipc/user.h"
#include "singleton.h"
#include <QApplication>
namespace Bressein
{
ContactItem::ContactItem (QGraphicsItem *parent, QGraphicsScene *scene)
    : QGraphicsItem (parent, scene), state (OFFLINE)
{
}

ContactItem::~ContactItem()
{

}

void ContactItem::paint (QPainter *painter,
                         const QStyleOptionGraphicsItem *option,
                         QWidget *widget)
{
    if (contact.sipuri.isEmpty())
    {
        return;
    }
//     QMatrix m = painter->worldMatrix();
//     painter->setWorldMatrix (QMatrix());
    painter->drawRect (boundingRect());
    painter->setPen (Qt::red);
    if (not contact.localName.isEmpty())
    {
        painter->drawText (25-QApplication::desktop()->screenGeometry().width()
        /2, 14, QString::fromUtf8 (contact.localName));
    }
    else
    {
        painter->drawText (25-QApplication::desktop()->screenGeometry().width()
        /2, 14, QString::fromUtf8 (contact.sipuri));
    }
    painter->drawText (25-QApplication::desktop()->screenGeometry().width() /2,
                       28, QString::fromUtf8 (contact.userId));
    if (not contact.imprea.isEmpty())
    {
        painter->drawText (25-QApplication::desktop()->screenGeometry().width()
        /2, 42, QString::fromUtf8 (contact.imprea));
    }
}

void ContactItem::mousePressEvent (QGraphicsSceneMouseEvent *event)
{
    state = PRESSED;
}

void ContactItem::mouseReleaseEvent (QGraphicsSceneMouseEvent *event)
{
    if (state == PRESSED)
    {
        if (this->boundingRect().contains (event->pos()))
        {
            //TODO
            // call singleton to open chat room
            qDebug() << "to call" << contact.sipuri;
            Singleton<User>::instance()->startChat (contact.sipuri);
        }
    }
}

QRectF ContactItem::boundingRect() const
{
    return QRectF (25-QApplication::desktop()->screenGeometry().width() /2, -25,
                   QApplication::desktop()->screenGeometry().width(),50);
}

void ContactItem::setData (const Contact &contact)
{
    this->contact = contact;
}

const Contact &ContactItem::data()
{
    return contact;
}


}
