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

#ifndef CONTACTITEM_H
#define CONTACTITEM_H
#include "sipc/types.h"

#include <QGraphicsTextItem>

namespace Bressein
{
class ChatView;
/**
shows portrait and local name or nick name or mobile / fetion number
 * */
class ContactItem : public QGraphicsTextItem
{
    Q_OBJECT
public:
    enum { ContactItemType = UserType + 5, };
    ContactItem (QGraphicsItem *parent = 0);
    ~ContactItem();

    int type () const
    {
        return ContactItemType;
    }
    void paint (QPainter *painter,
                const QStyleOptionGraphicsItem *option,
                QWidget *widget = 0);
    QRectF boundingRect() const;
//     virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
//     virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
//     ContactItem();
//     virtual ~ContactItem();
    void setHostSipuri (const QByteArray &sipuri);
    void setSipuri (const QByteArray &sipuri);
    void setHostName (const QByteArray &);
    const QByteArray &getSipuri () const ;
    void updatePortrait (const QByteArray &);
    void updateView();
    void updateContact (ContactInfo *contactInfo);

private slots:
    void setupChatView();
    void activateChatView (bool ok);
    void closeChatView();
signals:
    void sendMessage (const QByteArray &, const QByteArray &);
    //private:
    void incomeMessage (const QByteArray &, const QByteArray &);
public slots:
    void onSendMessage (const QByteArray &);
    void onIncomeMessage (const QByteArray &, const QByteArray &);
protected:
    void mouseDoubleClickEvent (QGraphicsSceneMouseEvent *event);
    void contextMenuEvent (QGraphicsSceneContextMenuEvent *event);
    void focusInEvent (QFocusEvent *event);
    void hoverEnterEvent (QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent (QGraphicsSceneHoverEvent *event);
    void hoverMoveEvent (QGraphicsSceneHoverEvent *event);
private:
    QByteArray hostSipuri;
    QByteArray hostName;
    QByteArray sipuri;
    ContactInfo *contactInfo;
    QString imagePath;
    ChatView *chatView;
};
}
#endif // CONTACTITEM_H
