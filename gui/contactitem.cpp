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
#include "sipc/aux.h"
#include "chatview.h"
#include "singleton.h"
#include "bresseinmanager.h"

#include <QApplication>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QTextDocument>
#include <QTextCursor>
namespace Bressein
{
ContactItem::ContactItem (QGraphicsItem *parent, QGraphicsScene *scene)
        : QGraphicsTextItem (parent, scene), contactInfo (0), chatView (new ChatView)
{
    setTextInteractionFlags (Qt::TextBrowserInteraction);
    setCacheMode (QGraphicsItem::DeviceCoordinateCache);
    connect (chatView, SIGNAL (sendMessage (QByteArray)),
            this, SLOT (onSendMessage (QByteArray)));
}

ContactItem::~ContactItem()
{
}

void ContactItem::paint (QPainter *painter,
        const QStyleOptionGraphicsItem *option,
        QWidget *widget)
{
    painter->setRenderHints (QPainter::RenderHints (0xF));
    QGraphicsTextItem::paint (painter, option, widget);
    QPen pen;  // creates a default pen
    pen.setStyle (Qt::SolidLine);
    pen.setCapStyle (Qt::RoundCap);
    pen.setJoinStyle (Qt::RoundJoin);
    if (option->state.testFlag (QStyle::State_HasFocus))
    {
        pen.setWidth (3);
        pen.setBrush (QColor::fromRgba (0xFF008FFF));
        painter->setPen (pen);
        painter->drawRect (boundingRect().adjusted (1, 1, -1, -1));
    }
    if (option->state.testFlag (QStyle::State_Sunken))
    {
        pen.setWidth (3);
        pen.setBrush (Qt::darkCyan);
        painter->setPen (pen);
        painter->drawRoundedRect (boundingRect().adjusted (1, 1, -1, -1), 2, 2);
    }
    if (option->state == QStyle::State_Enabled)
    {
        pen.setWidth (2);
        pen.setBrush (QColor::fromRgba (0xFFFF8F00));
        painter->setPen (pen);
        painter->drawRoundedRect (boundingRect().adjusted (1, 1, -1, -1), 1, 1);
    }
    if (option->state.testFlag (QStyle::State_MouseOver))
    {
        pen.setWidth (2);
        pen.setBrush (QColor::fromRgba (0xFF008FFF));
        painter->setPen (pen);
        painter->drawRoundedRect (boundingRect().adjusted (1, 1, -1, -1), 1, 1);
    }
}

QRectF ContactItem::boundingRect() const
{
    return  QRectF (0, 0, textWidth(), 56);
}

void ContactItem::setHostSipuri (const QByteArray &sipuri)
{
    this->hostSipuri = sipuri;
    chatView->setPortraits (sipuri, hostSipuri);
}

void ContactItem::setSipuri (const QByteArray &sipuri)
{
    this->sipuri = sipuri;
    chatView->setPortraits (sipuri, hostSipuri);
}

void ContactItem::setHostName (const QByteArray &name)
{
    this->hostName = name;
}


const QByteArray &ContactItem::getSipuri() const
{
    return sipuri;
}

void ContactItem::updatePortrait (const QByteArray &portrait)
{
    imagePath = portrait;
    if (contactInfo)
    {
        updateView();
    }
}

// TODO make style sheet configurable
void ContactItem::updateView()
{
    document()->clear();
    // create message item class and add it instance to this
    QString text;
    // TODO moves to stylesheet
    text.append ("<div style='display:inline;height:54;'><img width='48' src='"
            + imagePath + "' style='margin:0 0 0 0;float:left;clear:right;"
            "text-align:center;'/>");
    text.append ("<div style='margin:0 0 0 0;font-size:12px;color:#CCC;'>");
    if (not contactInfo->preferedName.isEmpty())
    {
        text.append (QString::fromUtf8 (contactInfo->preferedName));
    }
    else
    {
        text.append (tr ("No name set!"));
    }
    if (not contactInfo->mobileno.isEmpty())
    {
        text.append ("[");
        text.append (QString::fromUtf8 (contactInfo->mobileno));
        text.append ("]");
    }
    else
    {
        text.append ("[sip:");
        text.append (QString::fromUtf8 (sipToFetion (sipuri)));
        text.append ("]");
    }
    // TODO some descriptions some from contactInfo
    QString stateString;
    switch (contactInfo->state)
    {
        case StateType::ONLINE:
            stateString = tr ("online");
            break;
        case StateType::RIGHTBACK:
            stateString = tr ("right back");
            break;
        case StateType::AWAY:
            stateString = tr ("away");
            break;
        case StateType::BUSY:
            stateString = tr ("busy");
            break;
        case StateType::OUTFORLUNCH:
            stateString = tr ("out for lunch");
            break;
        case StateType::ONTHEPHONE:
            stateString = tr ("on the phone");
            break;
        case StateType::DONOTDISTURB:
            stateString = tr ("don't disturb");
            break;
        case StateType::MEETING:
            stateString = tr ("metting");
            break;
        case StateType::HIDDEN:
            stateString = tr ("hidden");
            break;
        case StateType::OFFLINE:
            stateString = tr ("offline");
            break;
        default:
            stateString = QString::number (contactInfo->state);
    }
    if (not contactInfo->devicetype.isEmpty())
    {
        stateString.append (" - ");
        stateString.append (contactInfo->devicetype);
    }
    if (not stateString.isEmpty())
    {
        text.append ("(");
        text.append (stateString);
        text.append (")");
    }
    text.append ("</div>");
    if (not contactInfo->impresa.isEmpty())
    {
        text.append ("<div style='margin:0 0 0 1px;overflow:hidden;"
                "font-size:10px;color:#F80;display:box'>");
        text.append (QString::fromUtf8 (contactInfo->impresa));
        text.append ("</div>");
    }
    text.append ("</div>");
    prepareGeometryChange();
    QTextCursor cursor = textCursor();
    cursor.movePosition (QTextCursor::End);
    cursor.insertHtml (text);
    setTextCursor (cursor);
    update();
}

void ContactItem::updateContact (ContactInfo *contactInfo)
{
    if (contactInfo)
    {
        this->contactInfo = contactInfo;
        if (not contactInfo->localName.isEmpty())
        {
            chatView->setNames (contactInfo->localName, hostName);
        }
        else if (not contactInfo->nickName.isEmpty())
        {
            chatView->setNames (contactInfo->nickName, hostName);
        }
        else
        {
            chatView->setNames (contactInfo->mobileno, hostName);
        }
        updateView();
    }
}

void ContactItem::activateChatView (bool ok)
{
    if (not ok)
    {
        chatView->hide();
    }
    else
    {
        chatView->showNormal();
        chatView->raise();
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

void ContactItem::mouseDoubleClickEvent (QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        activateChatView (true);
    }
}

void ContactItem::contextMenuEvent (QGraphicsSceneContextMenuEvent *event)
{
    QGraphicsTextItem::contextMenuEvent (event);
    //TODO add menu with actions: remove, query, move to group ...
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
