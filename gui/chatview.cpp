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
#include <sipc/aux.h>
#include <QDebug>
namespace Bressein
{
ChatView::ChatView (QWidget *parent) :
    QGraphicsView (parent),
    gscene (new QGraphicsScene (this)),
    showArea (new TextWidget),
    inputArea (new TextWidget)
{
    setBackgroundBrush (Qt::NoBrush);
//     setForegroundBrush(Qt::black);
    setAlignment (Qt::AlignLeft | Qt::AlignTop);
    setRenderHints (QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setCacheMode (QGraphicsView::CacheBackground);
    setViewportUpdateMode (QGraphicsView::FullViewportUpdate);
    setDragMode (QGraphicsView::RubberBandDrag);
    gscene->addItem (inputArea);
    gscene->addItem (showArea);
    gscene->setActivePanel (inputArea);
    showArea->setPos (0, 0);
    inputArea->setEditable();
    inputArea->setFocus();
    setMinimumSize (300, 300);
    adjustSize();
    setScene (gscene);
    viewport()->setAutoFillBackground (0);
    connect (inputArea->document(), SIGNAL (contentsChanged()),
             this, SLOT (adjustSize()));
}

ChatView::~ChatView()
{
    gscene->deleteLater();
}

void ChatView::setContact (const QByteArray &contact,
                           const QByteArray &otherName,
                           const QByteArray &myName)
{
    sipuri = contact;
    this->otherName = otherName;
    this->myName = myName;
//     showArea->setImage (sipuri);
    QString path = QDir::homePath().append ("/.bressein/icons/").
                   append (sipToFetion (sipuri)).append (".jpeg");
    if (not QFile (path).open (QIODevice::ReadOnly))
    {
        path = "/usr/share/icons/oxygen/32x32/emotes/face-smile.png";
    }
    QImage image = QImage (path);
    otherPortrait = "img:/other";
    showArea->document()->addResource (QTextDocument::ImageResource,
                                       QUrl (otherPortrait), QVariant (image));
    setWindowTitle (tr ("Chatting with ").append (QString::fromUtf8 (otherName)));
}

void ChatView::setPortrait (const QByteArray &myPortraitName, const QImage &portrait)
{
    myPortrait = myPortraitName;
    showArea->document()->addResource (QTextDocument::ImageResource,
                                       QUrl (myPortraitName), QVariant (portrait));
}

//public slots

void ChatView::incomeMessage (const QByteArray &datetime,
                              const QByteArray &message)
{
    //TODO make a MessageBlockItem with text datetime and message and ...
    showArea->addText (otherName, datetime, message, otherPortrait);
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
            showArea->addText (myName, QDateTime::currentDateTime().
                               toString().toUtf8(), text, myPortrait);
            inputArea->document()->clear();
            sendMessage (sipuri, text);
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
//     emit toClose (sipuri);
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