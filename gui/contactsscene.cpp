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


#include "contactsscene.h"
#include "contactitem.h"

namespace Bressein
{
ContactsScene::ContactsScene (QObject *parent) : QGraphicsScene (parent)
{

}

ContactsScene::~ContactsScene()
{

}

void ContactsScene::drawItems (QPainter *painter, int numItems,
                               QGraphicsItem *items[],
                               const QStyleOptionGraphicsItem options[],
                               QWidget *widget)
{
//     for (int i=0; i<numItems; ++i)
//     {
//         painter->save();
//         painter->setMatrix (items[i]->sceneMatrix(), true);
//         items[i]->paint (painter, &options[i], widget);
//         painter->restore();
//     }
}


}
#include "contactsscene.moc"
