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

#include "sidepanelview.h"
#include "sipc/account.h"
#include "loginscene.h"
#include "contactsscene.h"
#include "contactitem.h"
#include "chatview.h"
#include "groupitem.h"
#include <QGraphicsWidget>
namespace Bressein
{
SidepanelView::SidepanelView (QWidget *parent)
    : QGraphicsView (parent)
    , loginScene (new LoginScene (this))
{
    setWindowTitle ("Bressein");
    setRenderingSystem();
    setForegroundBrush (Qt::white);
    QLinearGradient linearGrad (QPointF (-100, 0), QPointF (700, 0));
    linearGrad.setColorAt (0, Qt::black);
    linearGrad.setColorAt (1, Qt::red);
    linearGrad.setSpread (QGradient::ReflectSpread);
    setBackgroundBrush (linearGrad);
    setScene (loginScene);
    connect (loginScene,
             SIGNAL (loginCommit (const QByteArray &, const QByteArray &)),
             this,
             SIGNAL (toLogin (const QByteArray &, const QByteArray &)));
    connect (loginScene,
             SIGNAL (verifyCommit (const QByteArray &)),
             this,
             SIGNAL (toVerify (QByteArray)));
    setMinimumSize (loginScene->itemsBoundingRect().size().toSize());

}

SidepanelView::~SidepanelView()
{
    loginScene->deleteLater();
}

//TODO onContactRemoved
void SidepanelView::setRenderingSystem()
{
    setAlignment (Qt::AlignLeft | Qt::AlignTop);
//     setWindowFlags (Qt::Window);
    setMaximumWidth (600);
    setContentsMargins (0,0,0,0);
//     setBackgroundRole (QPalette::BrightText);
//     setForegroundRole (QPalette::NoRole);
    setRenderHints (QPainter::RenderHints (0xF));
    setCacheMode (QGraphicsView::CacheBackground);
    setViewportUpdateMode (QGraphicsView::FullViewportUpdate);
    setDragMode (QGraphicsView::ScrollHandDrag);
}


void SidepanelView::activateLogin (bool ok)
{
    loginScene->setEnable (ok);
}


void SidepanelView::requestVerify (const QByteArray &datum)
{
    loginScene->onRequseVerify (datum);
}


void SidepanelView::setupSceneItems()
{
    // setup buttons
    //
}

void SidepanelView::closeEvent (QCloseEvent *event)
{
    // TODO
    if (scene() and scene() not_eq loginScene)
    {
        event->ignore();
        hide();
    }
    else
    {
        // TODO  ask for confirm to close
    }
}
void SidepanelView::resizeEvent (QResizeEvent *event)
{
    QGraphicsView::resizeEvent (event);
    if (event->oldSize().width() == event->size().width())
    {
        return;
    }
    //FIXME if fail to cast?
    if (scene() and scene() not_eq loginScene)
    {
        ContactsScene *contactsScene = static_cast<ContactsScene *> (scene());
        if (contactsScene)
        {
            contactsScene->setWidth (event->size().width());
        }
    }
}


}
#include "sidepanelview.moc"
