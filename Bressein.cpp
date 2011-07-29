#include "Bressein.h"
#include "utils/singleton.h"
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
    user = Singleton<User>::instance();
    user->setAccount(qgetenv ("FETIONNUMBER"), qgetenv ("FETIONPASSWORD"));
    user->login();
}

Bressein::~Bressein()
{
    user->close();
}
}
#include "Bressein.moc"
