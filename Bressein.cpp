#include "Bressein.h"

#include <QtGui/QLabel>
#include <QtGui/QMenu>
#include <QtGui/QMenuBar>
#include <QtGui/QAction>

namespace Bressein
{
Bressein::Bressein()
{

    addText ("Hey, I'm Bressein!");
    setForegroundBrush (QColor (255, 255, 255, 127));
    user = new User (qgetenv("FETIONNUMBER"), qgetenv("FETIONPASSWORD"));
    user->login();
}

Bressein::~Bressein()
{
    user->close();
}
}
#include "Bressein.moc"
