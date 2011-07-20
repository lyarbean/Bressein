#include "Bressein.h"

#include <QtGui/QLabel>
#include <QtGui/QMenu>
#include <QtGui/QMenuBar>
#include <QtGui/QAction>

namespace Bressein
{
    Bressein::Bressein()
    {
        QLabel* l = new QLabel (this);
        l->setText ("Hello World!");
        setCentralWidget (l);
        QAction* a = new QAction (this);
        a->setText ("Quit");
        connect (a, SIGNAL (triggered()), SLOT (close()));
        menuBar()->addMenu ("File")->addAction (a);
        user = new User("13710940390","3200614fetion");
        user->login();
    }

    Bressein::~Bressein()
    {}
}
#include "Bressein.moc"
