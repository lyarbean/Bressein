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

#ifndef GROUPITEM_H
#define GROUPITEM_H

#include <QGraphicsTextItem>
#include <QPen>
namespace Bressein
{
class GroupItem : public QGraphicsTextItem
{
    Q_OBJECT
public:
    enum { Type = UserType + 1};
    int type() const
    {
        return Type;
    }
    GroupItem (QGraphicsTextItem *parent = 0, QGraphicsScene *scene = 0);
    ~GroupItem();
    virtual QRectF boundingRect() const;
    void setGroupName (const QByteArray &name);
    void setText();
    bool isShowChildItems() const
    {
        return showChildItems;
    }
protected:
    virtual void mouseDoubleClickEvent (QGraphicsSceneMouseEvent *event);
    virtual void contextMenuEvent (QGraphicsSceneContextMenuEvent *event);
    void paint (QPainter *, const QStyleOptionGraphicsItem *, QWidget *);
private:
    bool showChildItems;
    QByteArray groupName;
    QPen pen;
};
}
#endif // GROUPITEM_H
