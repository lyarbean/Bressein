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

#include "bressein.h"
#include "singleton.h"
#include "sipc/user.h"
#include "contactsscene.h"
#include "contactitem.h"

namespace Bressein
{
BresseinView::BresseinView (QWidget *parent)
    : QGraphicsView (parent) , gwidget (new QGraphicsWidget)
{
    setRenderingSystem();
    setupScene();
    // demo
    Singleton<User>::instance()->setAccount
    (qgetenv ("FETIONNUMBER"), qgetenv ("FETIONPASSWORD"));

//     connect (Singleton<User>::instance(), SIGNAL (contactsChanged()),
//             this, SLOT (onDataChanged()));
    connect (Singleton<User>::instance(),
             SIGNAL (contactChanged (const QByteArray &)),
             this, SLOT (onDatumChanged (const QByteArray &)));
}

BresseinView::~BresseinView()
{
    Singleton<User>::instance()->close();
}

void BresseinView::login()
{
    Singleton<User>::instance()->login();
}


//TODO move to gscene
void BresseinView::onDataChanged ()
{
    //TODO add lock
    const Contacts &list = Singleton<User>::instance()->getContacts();

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
                itemList.at (i)->updateContact();
                updated = true;
                break;
            }
        }
        if (not updated)
        {
            ContactItem *item;
            item = new ContactItem (gwidget, gscene);
            item->setSipuri (keys.at (i));
            item->setZValue (10);
            item->setVisible (true);
            item->setPos (10, item->boundingRect().height() * i);
            itemList.append (item);
        }
    }
    updateSceneRect (this->viewport()->rect());
}

void BresseinView::onDatumChanged (const QByteArray &siprui)
{
    bool updated = false;
    int itemlists = itemList.size();

    updated = false;
    for (int i = 0; i < itemlists; i++)
    {
        if (itemList.at (i)->getSipuri() == siprui)
        {
            // update this
            itemList.at (i)->updateContact();
            updated = true;
            break;
        }
    }
    if (not updated)
    {
        ContactItem *item;
        item = new ContactItem (gwidget, gscene);
        item->setSipuri (siprui);
        item->setZValue (10);
        item->setVisible (true);
        item->setPos (10, item->boundingRect().height() * itemlists);
        itemList.append (item);
    }

    updateSceneRect (this->viewport()->rect());
}

//TODO onContactRemoved
void BresseinView::setRenderingSystem()
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

void BresseinView::setupScene()
{
    gscene = new ContactsScene (this);
    gscene->setSceneRect (0, 0, 100, 600);
    setScene (gscene);
    gscene->setItemIndexMethod (QGraphicsScene::NoIndex);
    gscene->addItem (gwidget);
}

void BresseinView::setupSceneItems()
{
    // setup buttons
    //
}

}
#include "bressein.moc"
