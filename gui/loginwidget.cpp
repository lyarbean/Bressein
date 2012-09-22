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

#include "loginwidget.h"
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QGridLayout>
namespace Bressein
{
LoginWidget::LoginWidget (QWidget *parent, Qt::WindowFlags f)
    : QWidget (parent, f),
      numberEdit (new QLineEdit (this)),
      passwordEdit (new QLineEdit (this)),
      codeEdit (new QLineEdit (this)),
      commitButton (new QPushButton (this)),
      verifyButton (new QPushButton (this)),
      messageLabel (new QLabel (this))
{
    QGridLayout *gridLayout = new QGridLayout (this);
    gridLayout->addWidget (new QLabel (tr ("Number")), 0, 0);
    gridLayout->addWidget (numberEdit, 0, 1);
    gridLayout->addWidget (new QLabel (tr ("Password")), 1, 0);
    gridLayout->addWidget (passwordEdit, 1, 1);
    gridLayout->addWidget (commitButton, 2, 0);
    gridLayout->addWidget (messageLabel, 2, 1);
    gridLayout->addWidget (codeEdit, 3, 0);
    gridLayout->addWidget (verifyButton, 3, 1);
    codeEdit->setEnabled (false);
    verifyButton->setEnabled (false);
    passwordEdit->setEchoMode (QLineEdit::Password);
    commitButton->setText (tr ("Login"));
    verifyButton->setText (tr ("Confirm"));
    connect (commitButton, SIGNAL (clicked ()),
             this, SLOT (onCommitButtonClicked ()));
    connect (verifyButton, SIGNAL (clicked ()),
             this, SLOT (onverifyButtonClicked()));
    connect (passwordEdit, SIGNAL (returnPressed()),
             commitButton, SLOT (click()));
    connect (numberEdit, SIGNAL (selectionChanged()),
             messageLabel, SLOT (clear()));
    setLayout (gridLayout);
    setFixedSize (gridLayout->sizeHint());
    setBackgroundRole (QPalette::WindowText);
}

LoginWidget::~LoginWidget()
{

}

void LoginWidget::setEnable (bool ok)
{
    commitButton->setEnabled (ok);
    passwordEdit->setEnabled (ok);
    numberEdit->setEnabled (ok);
}

void LoginWidget::requestVerify (const QByteArray &data)
{
    QImage img = QImage::fromData (QByteArray::fromBase64 (data));
    if (img.isNull())
    {
        // FIXME
        return;
    }
    messageLabel->setPixmap (QPixmap::fromImage (img));
    codeEdit->setEnabled (true);
    verifyButton->setEnabled (true);
    codeEdit->show();
    verifyButton->show();
    connect (commitButton, SIGNAL (clicked ()),
             this, SLOT (onCommitButtonClicked ()));
}


// slots
void LoginWidget::onCommitButtonClicked ()
{
    QRegExp expression ("[1-9][0-9]*");
    QByteArray number = numberEdit->text().toLocal8Bit();
    QByteArray password = passwordEdit->text().toLocal8Bit();
    if (not expression.exactMatch (number) or
        (number.size() not_eq 11 and number.size() not_eq 9))
    {
        messageLabel->setText ("Invalid Number!");
        return;
    }
    setEnable (false);
    emit commit (number, password);

}

void LoginWidget::onverifyButtonClicked()
{
    QByteArray code = codeEdit->text().toLocal8Bit();
    if (code.isEmpty())
    {
        messageLabel->setText ("Please enter code!");
    }
    codeEdit->setEnabled (false);
    verifyButton->setEnabled (false);
    emit verify (code);
}

}
#include "loginwidget.moc"

