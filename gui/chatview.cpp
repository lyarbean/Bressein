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

#include "chatview.h"
#include <QGraphicsTextItem>
#include <QDateTime>
#include <QKeyEvent>
namespace Bressein
{
ChatView::ChatView (QWidget *parent) : QGraphicsView (parent),
    gscene (new QGraphicsScene (this)),
    showArea (new QGraphicsItemGroup),
    inputArea (new QGraphicsTextItem)
{
    setAlignment (Qt::AlignLeft | Qt::AlignTop);
    setRenderHints (QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setCacheMode (QGraphicsView::CacheNone);
    setViewportUpdateMode (QGraphicsView::FullViewportUpdate);
    setDragMode (QGraphicsView::ScrollHandDrag);
    setScene (gscene);
    gscene->setSceneRect (0, 0, 400, 400); // reset at runtime!
    gscene->addItem (showArea);
    showArea->setPos (0,0);
    gscene->addItem (inputArea);
    inputArea->setFlags (QGraphicsItem::ItemIsFocusable|
                         QGraphicsItem::ItemAcceptsInputMethod);
    inputArea->setTextInteractionFlags (Qt::TextEditable);
    inputArea->setTextWidth (400);
    inputArea->setPlainText ("Bressein");
    inputArea->setPos (0,showArea->boundingRect().height()-32);
    gscene->setActivePanel (inputArea);
}

ChatView::~ChatView()
{

}

//public slots

void ChatView::incomeMessage (const QDateTime &datetime,
                              const QByteArray &message)
{
    //TODO make a MessageBlockItem with text datetime and message and ...

}

void ChatView::keyReleaseEvent (QKeyEvent *event)
{
    switch (event->key())
    {
        case Qt::Key_Control :
            inputArea->setTextInteractionFlags (Qt::TextSelectableByMouse);
            showArea->addToGroup (inputArea);
            inputArea = new QGraphicsTextItem;
            inputArea->setFlags (QGraphicsItem::ItemIsFocusable|
                                 QGraphicsItem::ItemAcceptsInputMethod);
            inputArea->setTextInteractionFlags (Qt::TextEditable);
            inputArea->setTextWidth (sceneRect().width());
            inputArea->setPlainText ("Bressein");
            gscene->setSceneRect (0, 0, 400, showArea->boundingRect().height() +1);
            gscene->addItem (inputArea);
            // HACK  never setPos before added to scene!!
            inputArea->setPos (0,gscene->sceneRect().height()-32);
            centerOn (inputArea);
            gscene->setActivePanel (inputArea);
            gscene->setFocusItem (inputArea);
            update();
            break;
        default:
            break;
    }
}

void ChatView::resizeEvent (QResizeEvent *event)
{
    QGraphicsView::resizeEvent (event);

}

}

