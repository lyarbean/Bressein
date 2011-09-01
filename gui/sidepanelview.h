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

#ifndef BRESSEINVIEW_H
#define BRESSEINVIEW_H

#include <QtGui/QGraphicsView>
#include <QtGui/QGraphicsWidget>

namespace Bressein
{
class ContactItem;
class ContactInfo;
class ContactsScene;
class LoginScene;
//TODO move contactsScene to BresseinManager
/**
 * @brief The side Panel of Bressein.
 **/
// There are two scenes, one is a dialog-alike loginScene
// and the other is contactsScene that what gscene is.

class SidepanelView : public QGraphicsView
{
    Q_OBJECT
public:
    SidepanelView (QWidget *parent = 0);
    virtual ~SidepanelView();
    void setHostSipuri (const QByteArray &);
    void setNickname (const QByteArray &);
    void updateContact (const QByteArray &, const ContactInfo &);
    void addGroup (const QByteArray &,const QByteArray &);
signals:
    void toLogin (const QByteArray &,
                  const QByteArray &);
    void sendMessage (const QByteArray &sipuri,
                      const QByteArray &message);
public slots:
    void setupContactsScene();
    void activateLogin (bool ok);
    void onIncomeMessage (const QByteArray &,
                          const QByteArray &,
                          const QByteArray &);
private slots:
    void onLoginCommit (const QByteArray &,
                        const QByteArray &);
    void onSendMessage (const QByteArray &,
                        const QByteArray &);
    void setRenderingSystem();
    void setupSceneItems();
    void resizeScene();
protected:
    void resizeEvent (QResizeEvent *event);
    void closeEvent (QCloseEvent *event);
private:
    QByteArray myNickname;
    QByteArray hostSipuri;
    LoginScene *loginScene;
    ContactsScene *contactsScene;
    QList<ContactItem *> itemList;
    QList<QGraphicsSimpleTextItem *> groups;
};
}
#endif // BRESSEINVIEW_H
