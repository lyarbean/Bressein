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

#include "messagerecorder.h"
#include <QtSql>
#include <QDir>
#include <QDebug>
MessageRecorder::MessageRecorder()
{
    database = QSqlDatabase::addDatabase ("QSQLITE");
    database.setDatabaseName (QDir::homePath() +"/.bressein/bressein.db");
    openDatabase();
}

MessageRecorder::~MessageRecorder()
{
    database.close();
}

//public
void MessageRecorder::insertRecord (QString sender, QString receiver,
                                    QString date, QString content)
{
    QSqlQuery query (database);
    query.prepare ("INSERT INTO messages (sender, receiver, date, content,status) "
                   "VALUES (?,?,?,?,?)");
    query.addBindValue (sender);
    query.addBindValue (receiver);
    query.addBindValue (date);
    query.addBindValue (content);
    query.addBindValue ("New");
    if (query.exec())
    {
        qDebug() << "Fails to insert record!";
    }
}



//private
void MessageRecorder::openDatabase()
{
    if (not database.open())
    {
        qDebug() << "Fail to open" << database.databaseName();
        return;
    }
    // initialize
    // sender receiver date content status(replied)
    QSqlQuery query (database);
    query.prepare ("CREATE TABLE IF NOT EXISTS messages(sender TEXT, "
                   "receiver TEXT, date TEXT, content TEXT, status TEXT)");
    if (not query.exec())
    {
        qDebug() << "fail to create table messages";
    }
}


#include "messagerecorder.moc"


