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
#include "textitem.h"
#include <sipc/aux.h>


#include <QApplication>
#include <QDesktopWidget>
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
#include <QIcon>

#include <QDebug>
namespace Bressein
{
ChatView::ChatView (QWidget *parent) :
    QGraphicsView (parent),
    gscene (new QGraphicsScene (this)),
    showArea (new TextItem),
    inputArea (new TextItem),
    self (false) // FIXME what is the first message from
{
    setBackgroundBrush (Qt::NoBrush);
    setAlignment (Qt::AlignLeft | Qt::AlignTop);
    setRenderHints (QPainter::RenderHints (0xF));
    setCacheMode (QGraphicsView::CacheBackground);
    setViewportUpdateMode (QGraphicsView::FullViewportUpdate);
    setDragMode (QGraphicsView::NoDrag);
    setFocusPolicy (Qt::StrongFocus);
    setMinimumSize (300, 300);
    setGeometry (qApp->activeWindow()->pos().x() - 150,
                 qApp->activeWindow()->pos().y() + 150,
                 width(), height());
    setWindowTitle (tr ("Bressein"));
    gscene->addItem (inputArea);
    gscene->addItem (showArea);
    gscene->setActivePanel (inputArea);
    showArea->setPos (0, 0);
    inputArea->setEditable();
    inputArea->setFocus();
    inputArea->setToolTip
    (tr ("Input your message here and press Ctrl+Enter to send"));
    setScene (gscene);
    viewport()->setAutoFillBackground (0);
    connect (inputArea->document(), SIGNAL (contentsChanged()),
             this, SLOT (adjustSize()));
}

ChatView::~ChatView()
{
}

void ChatView::setNames (const QByteArray &otherName,
                         const QByteArray &myName)
{
    this->otherName = otherName;
    this->myName = myName;
    setWindowTitle (tr ("Bressein: chatting with ").
                    append (QString::fromUtf8 (otherName)));
    adjustSize();
}

void ChatView::setPortraits (const QByteArray &otherSipuri,
                             const QByteArray &mySipuri)
{
    QString iconPath = QDir::homePath().append ("/.bressein/icons/");
    otherPortraitName = iconPath.append (sipToFetion (otherSipuri)).append (".jpeg");
    if (not QDir::root().exists (otherPortraitName))
    {
        otherPortraitName = ":/images/envelop_32.png";
    }
    myPortraitName = iconPath.append (sipToFetion (mySipuri)).append (".jpeg");
    if (not QDir::root().exists (myPortraitName))
    {
        myPortraitName = ":/images/envelop_32.png";
    }
    setWindowIcon (QIcon (otherPortraitName));
}

//public slots

void ChatView::incomeMessage (const QByteArray &datetime,
                              const QByteArray &message)
{
    if (self)
    {
        showArea->addText (otherName, datetime, message,
                           otherPortraitName.toLocal8Bit());
        self = false;
    }
    else
    {
        showArea->addText (datetime, message);
    }
    adjustSize();
}

void ChatView::keyReleaseEvent (QKeyEvent *event)
{
    // use ctrl+enter to commit
    if (event->key() == Qt::Key_Return and
        event->modifiers() == Qt::ControlModifier)
    {
        QByteArray text = inputArea->plainText();
        if (not text.isEmpty())
        {
            if (self)
            {
                showArea->addText (QDateTime::currentDateTime().
                                   toString().toUtf8(), text);
            }
            else
            {
                showArea->addText (myName, QDateTime::currentDateTime().
                                   toString().toUtf8(), text, myPortraitName.toLocal8Bit());
                self = true;
            }
            inputArea->document()->clear();
            emit sendMessage (text);
            adjustSize();
        }
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
    // hide!
    event->ignore();
    hide();
}

void ChatView::adjustSize()
{
    int showAreaHeight = showArea->document()->size().height();
    int inputAreaHeight = inputArea->document()->size().height();
    int leftTop = showAreaHeight > 200 ? showAreaHeight + 5 : 205;
    inputArea->setPos (0, leftTop);
    gscene->setSceneRect (0, 0, showArea->textWidth() - 10, leftTop +
                          (inputAreaHeight > 100 ? inputAreaHeight + 20 : 120));
    ensureVisible (inputArea);
    updateGeometry();
}

}
#include "chatview.moc"
