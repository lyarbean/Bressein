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

#ifndef CHATVIEW_H
#define CHATVIEW_H

#include <QGraphicsView>

namespace Bressein
{
//TODO provide Edit Tools
class TextItem;

class ChatView : public QGraphicsView
{
    Q_OBJECT
public:
    ChatView (QWidget *parent = 0);
    virtual ~ChatView();
    void setNames (const QByteArray &, const QByteArray &);
    void setPortraits (const QByteArray &,
                       const QByteArray &);
public slots:
    void incomeMessage (const QByteArray &, const QByteArray &);
signals:
    void sendMessage (const QByteArray &);
protected:
    void keyReleaseEvent (QKeyEvent *event);
    void resizeEvent (QResizeEvent *event);
    void closeEvent (QCloseEvent *event);
private slots:
    void adjustSize();
private:
    QString otherPortraitName;// a resource path
    QString myPortraitName;// a resource path
    QByteArray otherName; // literal name
    QByteArray myName;
    QGraphicsScene *gscene;
    TextItem *showArea;
    TextItem *inputArea;
    bool self;
};
}

#endif // CHATVIEW_H
