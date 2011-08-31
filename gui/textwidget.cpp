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

#include "textwidget.h"
#include <QPainter>
#include <QTextDocument>
#include <QTextCursor>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QDebug>
#include <QDir>
const QString  QSTYLESHEET =
    "*{font:18pt Tahoma;color:#FF8F00;margin:0 0 0 0;text-align:left;}\
    #Container {width:100%;margin:0 0 0 0;}\
    #DateTime {width:90%;margin:0 0 0 0;color:#EFAF00;font:10pt Sans;}\
    #Header {width:90%;margin:0 0 0 0;height:48px;}\
    #mainbody {width:90%;margin:0 0 0 48;text-align:left;}";
namespace Bressein
{
TextWidget::TextWidget (QGraphicsItem *parent)
    : QGraphicsTextItem (parent)
{
//     setFlags (QGraphicsItem::ItemIsSelectable);
    setTextInteractionFlags (Qt::TextSelectableByMouse);
    document()->setDefaultStyleSheet (QSTYLESHEET);
}

TextWidget::~TextWidget()
{

}

void TextWidget::setEditable ()
{
    //TODO
    setFlags (QGraphicsItem::ItemAcceptsInputMethod |
              QGraphicsItem::ItemIsFocusable | flags());
    setTextInteractionFlags (Qt::TextEditable | Qt::TextEditorInteraction);
    setPanelModality (QGraphicsItem::SceneModal);

}

void TextWidget::addText (const QByteArray &from,
                          const QByteArray &datetime,
                          const QByteArray &content,
                          const QByteArray &image)
{

    QTextCursor cursor = textCursor();
    cursor.movePosition (QTextCursor::End);
    QString html;
    html.append ("<div id='Container'>");//container
    html.append ("<div id='Header'>"); //header
    html.append (QString::fromUtf8 (from));
    html.append ("<br/>");
    html.append ("<span id='DateTime'>");
    html.append (QString::fromUtf8 (datetime));
    html.append ("</span>");
    html.append ("</div>");//heder
    html.append ("<div id='Mainbody'>");//body
    html.append (QString::fromUtf8 (content).replace ("\n","<br/>"));
    html.append ("</div>");//body
    html.append ("</div>");//container
    cursor.insertHtml ("<img  width='48' src=\"" + image + "\" />");
    cursor.insertHtml (html);
    cursor.insertText ("\n");
    setTextCursor (cursor);
    update();
}

void TextWidget::addText (const QByteArray &datetime,
                          const QByteArray &content)
{
    QTextCursor cursor = textCursor();
    cursor.movePosition (QTextCursor::End);
    QString html;
    html.append ("<div id='Container'>");//container
    html.append ("<span id='DateTime'>"); //header
    html.append (QString::fromUtf8 (datetime));
    html.append ("</span>");//heder
    html.append ("<div id='Mainbody'>");//body
    html.append (QString::fromUtf8 (content).replace ("\n","<br/>"));
    html.append ("</div>");//body
    html.append ("</div>");//container
    cursor.insertHtml (html);
    cursor.insertText ("\n");
    setTextCursor (cursor);
    update();
}

const QByteArray TextWidget::plainText() const
{
    return toPlainText().toUtf8();
}


/*
QSizeF TextWidget::sizeHint (Qt::SizeHint which, const QSizeF &constraint) const
{

}*/

void TextWidget::paint (QPainter *painter,
                        const QStyleOptionGraphicsItem *option,
                        QWidget *widget)
{
    painter->drawRect (boundingRect());
    QGraphicsTextItem::paint (painter, option, widget);
}


}
#include "textwidget.moc"
