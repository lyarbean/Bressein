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
    setFlags (QGraphicsItem::ItemIsFocusable);
    setTextInteractionFlags (Qt::TextBrowserInteraction);
    setTextWidth (boundingRect().width());
    setCacheMode (QGraphicsItem::DeviceCoordinateCache);
}

ContactItem::~ContactItem()
{

}

void ContactItem::paint (QPainter *painter,
                         const QStyleOptionGraphicsItem *option,
                         QWidget *widget)
{
    QGraphicsTextItem::paint (painter, option, widget);
    if (option->state.testFlag (QStyle::State_HasFocus) or
        option->state.testFlag (QStyle::State_MouseOver) or
        option->state.testFlag (QStyle::QStyle::State_Sunken))
    {
        painter->setPen (QColor (0xFFFF8F00));
    }
    else
    {
        painter->setPen (QColor (0xFF008FFF));
    }
    painter->drawRect (boundingRect().adjusted (1, 1, -1, -1));

}

QRectF ContactItem::boundingRect() const
{
    return  QRectF (0, 0, textWidth(), 64);
}

void ContactItem::setHostSipuri (const QByteArray &sipuri)
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
    // TODO assignment
    //contact = contactInfo;
    document()->clear();
    QString iconPath = QDir::homePath().append ("/.bressein/icons/");
    QString iconFullPath =
        iconPath.append (sipToFetion (sipuri)).append (".jpeg");
    QImageReader reader (iconFullPath);
    QImage image (120, 120, QImage::Format_RGB32);
    if (reader.read (&image))
    {
        document()->addResource (QTextDocument::ImageResource,
                                 QUrl (imagePath),
                                 QVariant (image));
    }
    else
    {
        imagePath = ":/images/envelop_32.png";
    }
    QString text;
    text.append ("<div style='display:inline'><img width='48' src=\"" +
                 imagePath + "\" style='float:left;clear:right;"
                 "text-align:center;'/>");
    text.append ("<div style='margin:0 0 0 0;font-size:12px;color:#333;"
                 "padding-top:30%;'>");
    if (not contact.localName.isEmpty())
    {
        text.append (QString::fromUtf8 (contact.localName));
    }
    else if (not contact.nickName.isEmpty())
    {
        text.append (QString::fromUtf8 (contact.nickName));
    }
    if (not contact.mobileno.isEmpty())
    {
        text.append ("[");
        text.append (QString::fromUtf8 (contact.mobileno));
        text.append ("]");
    }
//     text.append ("(");
//     text.append (QString::fromUtf8 (sipToFetion (sipuri)));
//     text.append (")");
    text.append ("</div>");
    if (not contact.impresa.isEmpty())
    {
        text.append ("<div style='margin:0 0 3px 0;overflow:scroll;"
                     "font-size:10px;color:#F80;display:block'>");
        text.append (QString::fromUtf8 (contact.impresa));
        text.append ("</div>");
    }
    text.append ("</div>");
    prepareGeometryChange();
    QTextCursor cursor = textCursor();
    cursor.movePosition (QTextCursor::End);
    cursor.insertHtml (text);
    setTextCursor (cursor);
}

void ContactItem::setupChatView()
{
    chatView = new ChatView;
    connect (chatView, SIGNAL (sendMessage (QByteArray)),
             this, SLOT (onSendMessage (QByteArray)));
    // should query for info, such as hostName
    QByteArray otherName = contact.localName;
    if (otherName.isEmpty())
    {
        otherName = contact.nickName;
    }
    chatView->setNames (otherName, hostName);
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

void ContactItem::focusInEvent (QFocusEvent *event)
{
    update();
}

void ContactItem::hoverEnterEvent (QGraphicsSceneHoverEvent *event)
{
    update();
//     QGraphicsTextItem::hoverLeaveEvent (event);
}

void ContactItem::hoverLeaveEvent (QGraphicsSceneHoverEvent *event)
{
    update();
//     QGraphicsTextItem::hoverLeaveEvent (event);
}
void ContactItem::hoverMoveEvent (QGraphicsSceneHoverEvent *event)
{
    update();
}

}

#include "contactitem.moc"
