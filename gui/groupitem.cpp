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

#include "groupitem.h"
#include "contactsscene.h"
#include <QPainter>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QTextDocument>
#include <QTextCursor>
namespace Bressein
{
GroupItem::GroupItem (QGraphicsTextItem *parent, QGraphicsScene *scene)
    : QGraphicsTextItem (parent, scene), showChildItems (true)
{
//     setFlags (QGraphicsItem::ItemIsFocusable);
    setCacheMode (QGraphicsItem::DeviceCoordinateCache);
}


GroupItem::~GroupItem()
{
}

QRectF GroupItem::boundingRect() const
{
    return QGraphicsTextItem::boundingRect();
}

void GroupItem::setText()
{
    document()->clear();
    prepareGeometryChange();
    QTextCursor cursor = textCursor();
    cursor.movePosition (QTextCursor::End);
    // TODO add style sheet
    QString html = "<div id='GroupItem'>" +
                   QString::fromUtf8 (groupName) + "</div>";
    if (not showChildItems)
    {
        html.append ("<span id='itemCount'>" +
                     QString::number (childItems().size()) + "</span>");
    }
    cursor.insertHtml (html);
    setTextCursor (cursor);
}

void GroupItem::mouseDoubleClickEvent (QGraphicsSceneMouseEvent *event)
{
    foreach (QGraphicsItem *item, childItems())
    {
        item->setVisible (not item->isVisible());
    }
    showChildItems = not showChildItems;
    setText();
    if (scene())
    {
        ContactsScene *contactsScene = static_cast<ContactsScene *> (scene());
        if (contactsScene)
        {
            contactsScene->resizeScene();
        }
    }
}

void GroupItem::contextMenuEvent (QGraphicsSceneContextMenuEvent *event)
{
    QGraphicsTextItem::contextMenuEvent (event);
}

void GroupItem::paint (QPainter *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget *widget)
{
    painter->setRenderHints (QPainter::RenderHint (0x07));
    QGraphicsTextItem::paint (painter, option, widget);
    QPen pen;  // creates a default pen
    pen.setStyle (Qt::SolidLine);
    pen.setCapStyle (Qt::RoundCap);
    pen.setWidth (2);
    pen.setBrush (QColor::fromRgba (0xFF008FFF));
    painter->setPen (pen);
    QRectF rect = boundingRect().adjusted (0,0,-1,-1);
    painter->drawLine (rect.bottomLeft(),rect.bottomRight());
}

}

#include "groupitem.moc"
