#include <QtGui/QApplication>
#include "Bressein.h"
#include "sipc/user.h"

int main (int argc, char** argv)
{
    QApplication app (argc, argv);
    Bressein::Bressein foo;
    foo.show();
    return app.exec();
}
