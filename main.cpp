#include <QtGui/QApplication>
#include <QtGui/QGraphicsView>
#include "Bressein.h"
#include "sipc/user.h"

int main (int argc, char** argv)
{
    QApplication app (argc, argv);
    Bressein::Bressein foo;
    QGraphicsView view(&foo);
    view.setMinimumSize(128,64);
    view.show();
    return app.exec();
}
