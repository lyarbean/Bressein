#ifndef Bressein_H
#define Bressein_H

#include <QtGui/QMainWindow>
#include "sipc/user.h"
// This is for test, will be rewritten with Qt graphics view framework
// and will be moved to gui;
namespace Bressein
{

    class Bressein : public QMainWindow
    {
        Q_OBJECT

    public:
        Bressein();
        virtual ~Bressein();
    private:
        User* user;
    };
}
#endif // Bressein_H
