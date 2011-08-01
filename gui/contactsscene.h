/*
 *  An scene to display contact in views*
 *  Copyright (C) 2011  颜烈彬 <slbyan@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 *  OpenSSL linking exception
 *  --------------------------
 *  If you modify this Program, or any covered work, by linking or
 *  combining it with the OpenSSL project's "OpenSSL" library (or a
 *  modified version of that library), containing parts covered by
 *  the terms of OpenSSL/SSLeay license, the licensors of this
 *  Program grant you additional permission to convey the resulting
 *  work. Corresponding Source for a non-source form of such a
 *  combination shall include the source code for the parts of the
 *  OpenSSL library used as well as that of the covered work.
 */


#ifndef CONTACTSSCENE_H
#define CONTACTSSCENE_H

#include <QtGui/QGraphicsScene>
#include "sipc/types.h"

namespace Bressein
{
class ContactItem;
class ContactsScene : public QGraphicsScene
{
    Q_OBJECT
public:
    ContactsScene();
    virtual ~ContactsScene();
public slots:
    // just pass one contact each time
    void onDataChanged (const Contact& contact);
    void onDataRemoved (const Contact& contact);
signals:
    void contactActivated (QByteArray userId);
protected:
    void mousePressEvent (QGraphicsSceneMouseEvent *mouseEvent);
    void mouseMoveEvent (QGraphicsSceneMouseEvent *mouseEvent);
    void mouseReleaseEvent (QGraphicsSceneMouseEvent *mouseEvent);
    void mouseDoubleClickEvent (QGraphicsSceneMouseEvent *mouseEvent);
private:
    QList<ContactItem *> itemList;
};
}

#endif // CONTACTSSCENE_H
