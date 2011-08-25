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

#ifndef TEXTWIDGET_H
#define TEXTWIDGET_H

#include <QtGui/QGraphicsTextItem>
#include <QTextImageFormat>
namespace Bressein
{

// TextWidget is a QGraphicsWidget with a QGraphicsTextItem inside
class TextWidget : public QGraphicsTextItem
{
    Q_OBJECT
public:
    enum { TextWidgetType = UserType + 2 };
    TextWidget (QGraphicsItem *parent = 0);
    ~TextWidget();
    int type () const
    {
        return TextWidgetType;
    }
    void setEditable();
    void addText (const QByteArray &,
                  const QByteArray &,
                  const QByteArray &,
                  const QTextImageFormat &);
    void addText (const QByteArray &,
                  const QByteArray &,
                  const QByteArray &,
                  const QByteArray &);
    const QByteArray plainText() const;
protected:
//     virtual QSizeF sizeHint (Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;
    void paint (QPainter *painter,
                const QStyleOptionGraphicsItem *option,
                QWidget *widget = 0);
};
}
#endif // TEXTWIDGET_H