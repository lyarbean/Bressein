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

// see http://www.qtcentre.org/wiki/index.php?title=Singleton_pattern
// This is a thread-safe singleton template
#ifndef SINGLETON_H
#define SINGLETON_H

#include <QObject>
#include <QMutex>

template <class T>
class Singleton
{
public:
    static T *instance()
    {
        static QMutex mutex;
        if (not _instance)
        {
            mutex.lock();
            if (not _instance)
            {
                _instance = new T;
            }
            mutex.unlock();
        }
        return _instance;
    }
private:
    // requires c++0x or gnu++0x support
    Singleton() = delete;
    ~Singleton() = delete;
    Singleton (const Singleton &) = delete; // hide copy constructor
    Singleton &operator= (const Singleton &) = delete; // hide assign op
    void *operator new (size_t) = delete; // new
    static T *_instance;
};
template <class T>
T *Singleton<T>::_instance = 0;
#endif
