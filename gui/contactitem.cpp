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
// #include "sipc/account.h"
#include "sipc/aux.h"
#include "chatview.h"
#include "singleton.h"
#include "bresseinmanager.h"
namespace Bressein
{
ContactItem::ContactItem (QGraphicsItem *parent)
        : QGraphicsTextItem (parent), chatView (0)
{
    qDebug() << "new ContactItem";
    setFlags (QGraphicsItem::ItemIsFocusable |
            QGraphicsItem::ItemIsSelectable);
    setTextInteractionFlags (Qt::TextBrowserInteraction);
    setTextWidth (boundingRect().width());
    setCacheMode (QGraphicsItem::ItemCoordinateCache);
}

ContactItem::~ContactItem()
{

}

void ContactItem::paint (QPainter *painter,
        const QStyleOptionGraphicsItem *option,
        QWidget *widget)
{
    painter->drawRect (boundingRect().adjusted (2, 2, -2, -2));
    QGraphicsTextItem::paint (painter, option, widget);
}

QRectF ContactItem::boundingRect() const
{
    return QRectF (0, 0, 130, document()->size().height());
}

void ContactItem::setHostSipuri (const QByteArray& sipuri)
{
    this->hostSipuri = sipuri;
}

void ContactItem::setSipuri (const QByteArray &sipuri)
{
    this->sipuri = sipuri;
}

void ContactItem::setHostName (const QByteArray &name)
{
    this->hostName = name;
}


const QByteArray &ContactItem::getSipuri() const
{
    return sipuri;
}
// FIXME
void ContactItem::updateContact (const ContactInfo &contactInfo)
{
    prepareGeometryChange();
    document()->clear();

    QString iconPath = QDir::homePath().append ("/.bressein/icons/");
    QString other = iconPath.append (sipToFetion (sipuri)).append (".jpeg");
    if (not QFile (other).open (QIODevice::ReadOnly))
    {
        other = ":/images/envelop_128.png";
    }
    sipuriImage = QImage (other);
    // TODO using Html
    // retain a copy
    contact = contactInfo;
    QTextBlockFormat blockFormat;
    blockFormat.setAlignment (Qt::AlignCenter);
    textCursor().beginEditBlock();
    textCursor().insertImage (sipuriImage);
    textCursor().setBlockFormat (blockFormat);
    if (not contact.basic.localName.isEmpty())
    {
        textCursor().insertText (QString::fromUtf8 (contact.basic.localName));
        textCursor().insertText ("\n");
    }
    else if (not contact.detail.nickName.isEmpty())
    {
        textCursor().insertText (QString::fromUtf8 (contact.detail.nickName));
        textCursor().insertText ("\n");
    }
    if (not contact.detail.mobileno.isEmpty())
    {
        textCursor().insertText (QString::fromUtf8 (contact.detail.mobileno));
        textCursor().insertText ("\n");
    }
    textCursor().insertText (QString::fromUtf8 (sipToFetion (sipuri)));
    textCursor().endEditBlock();
    //
    //TODO show imprea when get hovered
    setToolTip (QString::fromUtf8 (contact.basic.imprea));
}

void ContactItem::setupChatView()
{
    chatView = new ChatView;
    connect (chatView, SIGNAL (sendMessage (QByteArray)),
            this, SLOT (onSendMessage (QByteArray)));
    // should query for info, such as hostName
    QByteArray otherName = contact.basic.localName;
    if (otherName.isEmpty())
    {
        otherName = contact.detail.nickName;
    }
    chatView->setNames(otherName,hostName);
    chatView->setPortraits (sipuri, hostSipuri);
}

void ContactItem::activateChatView (bool ok)
{
    if (not chatView and ok)
    {
        setupChatView();
    }
    if (chatView and not ok)
    {
        chatView->hide();
    }
    else
    {
        chatView->show();
    }
}

void ContactItem::onIncomeMessage (const QByteArray &datetime,
        const QByteArray &message)
{
    activateChatView (true);
    chatView->incomeMessage (datetime, message);
}

void ContactItem::onSendMessage (const QByteArray &message)
{
    emit sendMessage (sipuri, message);
}

void ContactItem::closeChatView()
{
    if (chatView)
    {
        chatView->disconnect();
        chatView->deleteLater();
        chatView = 0;
    }
}


void ContactItem::mouseDoubleClickEvent (QGraphicsSceneMouseEvent *event)
{
    // should not call directly, just for convenience
    activateChatView (true);
//     Singleton<BresseinManager>::instance()->onChatSpawn (sipuri);
}

void ContactItem::hoverEnterEvent (QGraphicsSceneHoverEvent *event)
{
    setOpacity (1.0);
    QGraphicsTextItem::hoverEnterEvent (event);
}

void ContactItem::hoverLeaveEvent (QGraphicsSceneHoverEvent *event)
{
    setOpacity (0.9);
    QGraphicsTextItem::hoverLeaveEvent (event);
}

}

#include "contactitem.moc"
