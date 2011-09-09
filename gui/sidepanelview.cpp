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
    setScene (loginScene);
    connect (loginScene,
             SIGNAL (loginCommit (const QByteArray &, const QByteArray &)),
             this,
             SLOT (onLoginCommit (const QByteArray &, const QByteArray &)));
    connect (loginScene,
             SIGNAL (verifyCommit (const QByteArray &)),
             this,
             SLOT (onVerifycommit (QByteArray)));
    setMinimumSize (loginScene->itemsBoundingRect().size().toSize());
}

SidepanelView::~SidepanelView()
{
    loginScene->deleteLater();
}

void SidepanelView::setHostSipuri (const QByteArray &sipuri)
{
    hostSipuri = sipuri;
}

void SidepanelView::setNickname (const QByteArray &nickname)
{
    myNickname = nickname;
}

void SidepanelView::onLoginCommit (const QByteArray &n, const QByteArray &p)
{
    emit toLogin (n, p);
}

void SidepanelView::onVerifycommit (const QByteArray &c)
{
    emit toVerify (c);
}

void SidepanelView::onSendMessage (const QByteArray &sipuri,
                                   const QByteArray &message)
{
    emit sendMessage (sipuri, message);
}


//TODO onContactRemoved
void SidepanelView::setRenderingSystem()
{
//     QWidget *viewport = 0;
//
// //     #ifndef QT_NO_OPENGL
// //         QGLWidget *glw = new QGLWidget(QGLFormat(QGL::SampleBuffers));
// //         if (Colors::noScreenSync)
// //             glw->format().setSwapInterval(0);
// //         glw->setAutoFillBackground(false);!@#
// //         viewport = glw;
// //         setCacheMode(QGraphicsView::CacheNone);
// //
// //     #endif
//     // software rendering
//     viewport = new QWidget;
//     setViewport (viewport);
    setAlignment (Qt::AlignLeft | Qt::AlignTop);
    setWindowFlags (Qt::Window);
    setMaximumWidth (600);
    setContentsMargins (0,0,0,0);
    setBackgroundRole (QPalette::BrightText);
    setForegroundRole (QPalette::NoRole);
    setRenderHints ( (QPainter::RenderHints) 0x07);
    setCacheMode (QGraphicsView::CacheNone);
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
//     QWidget::closeEvent (event);
}
void SidepanelView::resizeEvent (QResizeEvent *event)
{
    QGraphicsView::resizeEvent (event);
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
