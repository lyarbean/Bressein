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
#include "textwidget.h"
#include <QGraphicsLinearLayout>
#include <QGraphicsTextItem>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextCursor>
#include <QTextBlock>
#include <QImageReader>
#include <QUrl>
#include <QDir>
#include <QDateTime>
#include <QKeyEvent>
#include <QDebug>
namespace Bressein
{
ChatView::ChatView (QWidget *parent) :
    QGraphicsView (parent),
    gscene (new QGraphicsScene (this)),
    showArea (new TextWidget),
    inputArea (new TextWidget)
{
    setAlignment (Qt::AlignLeft | Qt::AlignTop);
    setRenderHints (QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setCacheMode (QGraphicsView::CacheNone);
    setViewportUpdateMode (QGraphicsView::FullViewportUpdate);
    setDragMode (QGraphicsView::RubberBandDrag);
    gscene->addItem (inputArea);
    gscene->addItem (showArea);
    gscene->setActivePanel (inputArea);
    showArea->setPos (0, 0);
    inputArea->setEditable();
    inputArea->setFocus();
    adjustSize();
//     inputArea->setTextWidth(400);
//     inputArea->setPlainText("Hello");
//     inputArea->setTextInteractionFlags (Qt::TextEditable);
    setScene (gscene);
//     gscene->setActivePanel (inputArea);
    connect (inputArea->document(), SIGNAL (contentsChanged()),
             this, SLOT (adjustSize()));
}

ChatView::~ChatView()
{
    gscene->deleteLater();
}

void ChatView::setContact (const QByteArray &contact)
{
    sipuri = contact;
    showArea->setImage (sipuri);
}

//public slots

void ChatView::incomeMessage (const QByteArray &datetime,
                              const QByteArray &message)
{
    //TODO make a MessageBlockItem with text datetime and message and ...
    showArea->addText (datetime,message);
    adjustSize();
}

void ChatView::keyReleaseEvent (QKeyEvent *event)
{
    switch (event->key())
    {
        case Qt::Key_Meta:
            {
                QByteArray text = inputArea->plainText();
                if (not text.isEmpty())
                {
                    showArea->addText (QDateTime::currentDateTime().
                                       toString().toUtf8(),text);
                    inputArea->setPlainText ("");
                    sendMessage (sipuri,text);
                    adjustSize();
                }
            }
            break;
        default:
            break;
    }
}

void ChatView::resizeEvent (QResizeEvent *event)
{
    QGraphicsView::resizeEvent (event);
    int w = event->size().width();
    showArea->setTextWidth (w);
    inputArea->setTextWidth (w);
    adjustSize();
}

void ChatView::closeEvent (QCloseEvent *event)
{
    event->ignore();
    emit toClose (sipuri);
}

void ChatView::adjustSize()
{
    int showAreaHeight = showArea->document()->size().height();
    int inputAreaHeight = inputArea->document()->size().height();

    int leftTop = showAreaHeight > 200 ? showAreaHeight+5 : 205;
    inputArea->setPos (0, leftTop);
    gscene->setSceneRect (0,0,showArea->textWidth()-10, leftTop +
                          (inputAreaHeight > 100 ? inputAreaHeight+20 : 120));
    ensureVisible (inputArea);
    updateGeometry();
}

}
#include "chatview.moc"
