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
    : QGraphicsTextItem (parent)
{
    qDebug() << "new ContactItem";
    setFlags (QGraphicsItem::ItemIsFocusable |
              QGraphicsItem::ItemIsSelectable | flags());
    setTextInteractionFlags (Qt::TextBrowserInteraction);
    setTextWidth (boundingRect().width());

}

ContactItem::~ContactItem()
{

}

void ContactItem::paint (QPainter *painter,
                         const QStyleOptionGraphicsItem *option,
                         QWidget *widget)
{
    painter->drawRect (boundingRect());
    QGraphicsTextItem::paint (painter, option, widget);
}

QRectF ContactItem::boundingRect() const
{
    return QRectF (2, 0, 200, document()->size().height());
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
    setPlainText ("");
    int a = sipuri.indexOf (":");
    int b = sipuri.indexOf ("@");
    QString path = QDir::homePath().append ("/.bressein/icons/").
                   append (sipuri.mid (a + 1, b - a - 1)).append (".jpeg");
    if (not QFile (path).open (QIODevice::ReadOnly))
    {
        path = "/usr/share/icons/oxygen/32x32/emotes/face-smile.png";
    }
    QImage image = QImageReader (path).read();
    document()->addResource (QTextDocument::ImageResource,
                             QUrl ("logo"), QVariant (image));
    imageFormat.setName (path);
    imageFormat.setHeight (32);
    imageFormat.setWidth (32);
    contact = contactInfo;
    textCursor().beginEditBlock();
    textCursor().insertImage (imageFormat);
    //TODO localize
    if (not contact.basic.localName.isEmpty())
    {
        textCursor().insertText (QString::fromUtf8 (contact.basic.localName));
    }
    else
    {
        textCursor().insertText (QString::fromUtf8 (contact.detail.nickName));
    }
    textCursor().insertText ("\t");
    textCursor().insertText (QString::fromUtf8 (contact.basic.userId));

    textCursor().insertText ("\n");
    //TODO show imprea when get hovered
    textCursor().insertText (QString::fromUtf8 (contact.basic.imprea));
    textCursor().insertText ("\n");
    textCursor().endEditBlock();
}

void ContactItem::mouseDoubleClickEvent (QGraphicsSceneMouseEvent *event)
{
    Singleton<BresseinManager>::instance()->onChatSpawn (sipuri);
}

}

#include "contactitem.moc"
