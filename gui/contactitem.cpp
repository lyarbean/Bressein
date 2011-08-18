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


#include "contactitem.h"
#include <QtGui>
#include <QApplication>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include "sipc/account.h"
#include "singleton.h"
#include "bresseinmanager.h"
namespace Bressein
{
ContactItem::ContactItem (QGraphicsItem *parent)
    : QGraphicsObject (parent)
{
}

ContactItem::~ContactItem()
{

}

void ContactItem::paint (QPainter *painter,
                         const QStyleOptionGraphicsItem *option,
                         QWidget *widget)
{
    if (sipuri.isEmpty())
    {
        return;
    }
//     QMatrix m = painter->worldMatrix();
//     painter->setWorldMatrix (QMatrix());
    painter->drawRect (boundingRect().adjusted (-3, -3, 3, 3));

    if (contact.basic.state == StateType::ONLINE)
        painter->setPen (Qt::red);
    if (not contact.detail.nickName.isEmpty())
    {
        painter->drawText (10, 15, QString::fromUtf8 (contact.detail.nickName));
    }
    else if (not contact.basic.localName.isEmpty())
    {
        painter->drawText (10, 15, QString::fromUtf8 (contact.basic.localName));
    }
    else
    {
        painter->drawText (10, 15, QString::fromUtf8 (sipuri));
    }
//     painter->drawText (25-QApplication::desktop()->screenGeometry().width() /2,
//                        28, QString::fromUtf8 (contact.basic.userId));
//     if (not contact.basic.imprea.isEmpty())
//     {
//         painter->drawText (25-QApplication::desktop()->screenGeometry().width()
//                            /2, 42, QString::fromUtf8 (contact.basic.imprea));
//     }
}

QRectF ContactItem::boundingRect() const
{
    return QRectF (5, 0, 320, 60);
}


void ContactItem::setSipuri (const QByteArray &sipuri)
{
    this->sipuri = sipuri;
}

const QByteArray &ContactItem::getSipuri() const
{
    return sipuri;
}

void ContactItem::updateContact (const ContactInfo &contactInfo)
{
    contact = contactInfo;
}

void ContactItem::mouseDoubleClickEvent (QGraphicsSceneMouseEvent *event)
{
    Singleton<BresseinManager>::instance()->onChatSpawn (sipuri);
}

}
