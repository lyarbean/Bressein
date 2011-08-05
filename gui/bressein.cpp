/*
 *  This file is part of Bressein.
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
#include "bressein.h"
#include "singleton.h"

namespace Bressein
{
Bressein::Bressein (QWidget *parent)
    : QGraphicsView (parent) , gwidget (new QGraphicsWidget)
{
    setRenderingSystem();
    setupScene();
    user = Singleton<User>::instance();
    user->setAccount (qgetenv ("FETIONNUMBER"), qgetenv ("FETIONPASSWORD"));
    connect (user, SIGNAL (contactsChanged()),
             this, SLOT (onDataChanged()));
}

Bressein::~Bressein()
{
    user->close();
}

void Bressein::login()
{
    user->login();
}


//TODO move to gscene
void Bressein::onDataChanged ()
{
    //TODO add lock
    const Contacts &list = user->getContacts();

    // N**2
    bool updated = false;
    int itemlists = itemList.size();

    QList<QByteArray> keys = list.keys();
    int keysCount = keys.size();
    for (int i = 0; i < keysCount; i++)
    {
        updated = false;
        for (int j = 0; j < itemlists; j++)
        {
            if (itemList.at (i)->getSipuri() == keys.at (i))
            {
                // update this
                itemList.at (i)->setSipuri (keys.at (i));
                itemList.at (i)->setContact (*list.value (keys.at (i)));
                updated = true;
                break;
            }
        }
        if (not updated)
        {
            ContactItem *item;
            item = new ContactItem (gwidget, gscene);
            item->setSipuri (keys.at (i));
            item->setContact (*list.value (keys.at (i)));
            item->setZValue (10);
            item->setVisible (true);
            item->setPos (10, item->boundingRect().height() * i);
            itemList.append (item);
        }
    }
}

//TODO onContactRemoved
void Bressein::setRenderingSystem()
{
    QWidget *viewport = 0;

//     #ifndef QT_NO_OPENGL
//         QGLWidget *glw = new QGLWidget(QGLFormat(QGL::SampleBuffers));
//         if (Colors::noScreenSync)
//             glw->format().setSwapInterval(0);
//         glw->setAutoFillBackground(false);!@#
//         viewport = glw;
//         setCacheMode(QGraphicsView::CacheNone);
//
//     #endif
    // software rendering
    viewport = new QWidget;
    setRenderHints (QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setCacheMode (QGraphicsView::CacheBackground);
    setViewportUpdateMode (QGraphicsView::BoundingRectViewportUpdate);
    setDragMode (QGraphicsView::ScrollHandDrag);
    setViewport (viewport);
}

void Bressein::setupScene()
{
    gscene = new ContactsScene (this);
    gscene->setSceneRect (0, 0, 100, 600);
    setScene (gscene);
    gscene->setItemIndexMethod (QGraphicsScene::NoIndex);
    gscene->addItem (gwidget);
}

void Bressein::setupSceneItems()
{
    // setup buttons
    //
}

}
#include "bressein.moc"
