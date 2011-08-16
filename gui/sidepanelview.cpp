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
#include "sipc/account.h"
#include "contactsscene.h"
#include "contactitem.h"
#include "chatview.h"
namespace Bressein
{
SidepanelView::SidepanelView (QWidget *parent)
    : QGraphicsView (parent) , gwidget (new QGraphicsWidget)
{
    setAlignment (Qt::AlignLeft | Qt::AlignTop);
    setRenderingSystem();
    setupScene();
    gwidget->resize (200, 200);
    // demo
    Singleton<Account>::instance()->setAccount
    (qgetenv ("FETIONNUMBER"), qgetenv ("FETIONPASSWORD"));
    connect (Singleton<Account>::instance(),
             SIGNAL (contactChanged (const QByteArray &)),
             this, SLOT (onDatumChanged (const QByteArray &)));
    ChatView *chatview =new ChatView;
    chatview->show();
}

SidepanelView::~SidepanelView()
{
    disconnect (Singleton<Account>::instance(),
                SIGNAL (contactChanged (const QByteArray &)),
                this, SLOT (onDatumChanged (const QByteArray &)));
    Singleton<Account>::instance()->close();
}

void SidepanelView::login()
{
    Singleton<Account>::instance()->login();
}


//TODO move to gscene
void SidepanelView::onDataChanged ()
{

    const Contacts &list = Singleton<Account>::instance()->getContacts();

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
            item = new ContactItem (gwidget);
            item->setSipuri (keys.at (i));
            item->setZValue (1);
            item->setVisible (true);
            item->setPos (10, item->boundingRect().height() * i + 1);
            itemList.append (item);
        }
    }
    updateSceneRect (this->viewport()->rect());
}

void SidepanelView::onDatumChanged (const QByteArray &siprui)
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
        item = new ContactItem (gwidget);
        item->setSipuri (siprui);
        item->setZValue (10);
        item->setVisible (true);
        item->setPos (10, item->boundingRect().height() * itemlists + 1);
        itemList.append (item);
    }
}

//TODO onContactRemoved
void SidepanelView::setRenderingSystem()
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
    setViewport (viewport);
    setRenderHints (QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setCacheMode (QGraphicsView::CacheBackground);
    setViewportUpdateMode (QGraphicsView::BoundingRectViewportUpdate);
    setDragMode (QGraphicsView::ScrollHandDrag);

}

void SidepanelView::setupScene()
{
    gscene = new ContactsScene (this);
    gscene->setSceneRect (0, 0, 300, 768);
    setScene (gscene);
    gscene->setItemIndexMethod (QGraphicsScene::NoIndex);
    gscene->addItem (gwidget);
    gwidget->setPos (10, 10);
}

void SidepanelView::setupSceneItems()
{
    // setup buttons
    //
}

void SidepanelView::resizeEvent (QResizeEvent *event)
{
    QGraphicsView::resizeEvent (event);
    if (scene())
    {
        scene()->invalidate (scene()->sceneRect());
    }
}

}
#include "bressein.moc"
