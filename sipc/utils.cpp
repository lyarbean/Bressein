/*
 *  These static functions generate data that written to socket.
 *  Copyright (C) 2011  颜烈彬 <slbyan@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "utils.h"

namespace Bressein
{
    /** @fn static QByteArray hashV1 ( QByteArray &userIdInt, QByteArray &password )
     * @brief Returns the Sha1 in Hex, of the catenation of userIdInt and password
     *
     * @param userId
     * @param password
     * @return QByteArray in Hex
     **/
    QByteArray hashV1 (QByteArray &userId, QByteArray &password)
    {
        return QCryptographicHash::hash (userId.append (password),
                                         QCryptographicHash::Sha1).toHex().toUpper();
    }

    /**
     * @fn static QByteArray hashV2 ( QByteArray &userId, QByteArray &hashV4 )
     * @brief hashV2
     *
     * @param userId in decimal
     * @param password
     * @return QByteArray
     **/
    QByteArray hashV2 (QByteArray &userId, QByteArray &hashV4)
    {
        QByteArray left;
        bool ok;
        unsigned int value = userId.toInt (&ok, 10);

        if (!ok)
        {
            return QByteArray ("");
        }

        left.append ( (char) ( (value & 0xFF)));

        left.append ( (char) ( (value & 0xFF00) >> 8));
        left.append ( (char) ( (value & 0xFF0000) >> 16));
        left.append ( (char) ( (value & 0xFF000000) >> 24));
        QByteArray right = QByteArray::fromHex (hashV4);
        return  hashV1 (left, right);
    }

    /**
     * @fn static QByteArray hashV4 ( QByteArray &userId, QByteArray &password )
     * @brief If userId is null then returns  hashV1 ( prefix, password ),
     *  otherwise returns hashV2 ( userId, hashV1 ( prefix,password )),
     *  where prefix is 'fetion.com.cn:'.
     * \sa hashV1
     * \sa hashV2
     * @param userId ...
     * @param password ...
     * @return QByteArray
     **/
    QByteArray hashV4 (QByteArray &userId, QByteArray &password)
    {
        QByteArray prefix = DOMAIN;
        prefix.append (':');
        QByteArray res = hashV1 (prefix, password);

        if (userId.isNull() || userId.isEmpty())
            return res;

        return hashV2 (userId, res);
    }

    QByteArray RSAPublicEncrypt (QByteArray userId,
                                 QByteArray password,
                                 QByteArray nonce,
                                 QByteArray aeskey,
                                 QByteArray key/*public key*/)
    {
        // Follow openfetion's method
        // in hex
        // nonce + password(doubly hashed) + aeskey (random)
        // catenate parts in bytes
        // in hex
        QByteArray psd = QByteArray::fromHex (hashV4 (userId, password));
        QByteArray toEncrypt = nonce;
        toEncrypt.append (psd);
        toEncrypt.append (aeskey);
        char *keyC = key.data(); // it points to key's data, never free it;
        char modulus[257];
        char exponent[7];
        memcpy (modulus , keyC, 256);
        memcpy (exponent, keyC + 256, 6);
        BIGNUM *bnn, *bne;
        bnn = BN_new();
        bne = BN_new();
        BN_hex2bn (&bnn, modulus);
        BN_hex2bn (&bne, exponent);
        RSA *r = RSA_new();
        r->n = bnn;
        r->e = bne;
        r->d = 0;
        int flen = RSA_size (r);//128
        unsigned char *out = (unsigned char*) malloc (flen);
        memset (out , 0 , flen);

        int ret = RSA_public_encrypt (toEncrypt.size(), (unsigned char *) toEncrypt.data(), out, r, RSA_PKCS1_PADDING);

        if (ret < 0)
        {
            printf ("encrypt failed");
            return QByteArray ("");
        }

        RSA_free (r);

        //clean up
        // split ret into hex bytearray
        Q_ASSERT (ret == 128);
        char* result = (char*) malloc (ret * 2 + 1);
        memset (result , 0 , ret * 2 + 1);
        int i = 0;

        while (i < ret)
        {
            sprintf (result + i * 2 , "%02x" , out[i]);
            i++;
        };

        QByteArray resultHex (result);

        free (out);

        free (result);

        return resultHex.toUpper();
    }

    /** @fn static QByteArray cnouce(quint16 time = 2)
     * @brief generate a QByteArray in hex of length 16 * time
     *
     * @param time  2 as default.
     * @return QByteArray
     **/
    QByteArray cnouce (quint16 time)
    {
        QByteArray t;
        qsrand (QDateTime::currentMSecsSinceEpoch ());

        for (quint16 i = 0; i < time; i++)
        {
            t.append (QByteArray::number (qrand(), 16));
        }

        return t.toUpper();
    }

    /** @fn static QByteArray configData(QByteArray& number)
     * @brief Generate a body for fetching server's configuration
     *
     * @param number The phone number or fetion number
     * @return QByteArray
     **/
    QByteArray configData (QByteArray& number)
    {
        QByteArray part = ("<config><user ");

        if (number.size() == 11) //mobileNumber
        {
            part.append ("mobile-no=\"");
        }
        else
        {
            part.append ("user-id=\"");
        }

        part.append (number);

        part.append ("\"/><client type=\"PC\" version=\"");
        part.append (PROTOCOL_VERSION);
        part.append ("\" platform=\"W5.1\"/><servers version=\"0\"/>"
                     "<parameters version=\"0\"/><hints version=\"0\"/></config>");
        QByteArray data ("POST /nav/getsystemconfig.aspx HTTP/1.1\r\n");
        data.append ("User-Agent: IIC2.0/PC ");
        data.append (PROTOCOL_VERSION);
        data.append ("\r\nHost: ");
        data.append (NAVIGATION);
        data.append ("\r\nConnection: Close\r\n");
        data.append ("Content-length: ");
        data.append (QByteArray::number (part.size()));
        data.append ("\r\n\r\n");
        data.append (part);
        data.append ("\r\n");
        return data;
    }

    /** @fn static QByteArray ssiLoginData (QByteArray& number, QByteArray& passwordhashed4)
     * @brief A body that written to sslsocket on SsiLogin
     *
     * @param number The phone number or fetion number.
     * @param passwordhashed4 The value of hashV4(UserId, password).
     * @return QByteArray
     **/
    QByteArray ssiLoginData (QByteArray& number,
                             QByteArray& passwordhashed4,
                             QByteArray passwordType)
    {
        QByteArray numberString;

        if (number.size() == 11)
        {
            numberString.append ("mobileno=");
        }
        else
        {
            numberString.append ("sid=");
        }

        numberString.append (number);

        QByteArray data ("GET /ssiportal/SSIAppSignInV4.aspx?");
        data.append (numberString);
        data.append ("&domains=");
        data.append (DOMAIN);
        data.append ("&v4digest-type=");
        data.append (passwordType);
        data.append ("&v4digest="); // TODO v4digest-type
        data.append (passwordhashed4);
        data.append ("\r\n"
                     "User-Agent: IIC2.0/pc ");
        data.append (PROTOCOL_VERSION);
        data.append ("\r\nHost: uid.fetion.com.cn\r\n" // UID_URI
                     "Cache-Control: private\r\n"
                     "Connection: Keep-Alive\r\n\r\n");
        return data;
    }

    QByteArray SsiPicData (QByteArray algorithm, QByteArray ssic)
    {
        QByteArray data ("GET /nav/GetPicCodeV4.aspx?algorithm=");
        data.append (algorithm);
        data.append (" HTTP/1.1\r\n");
        data.append (ssic);
        data.append ("Host: nav.fetion.com.cn\r\n");// NAVIGATION_URI
        data.append ("User-Agent: IIC2.0/PC ");
        data.append (PROTOCOL_VERSION);
        data.append ("\r\nConnection: close\r\n\r\n");
        return data;
    }

    /** @fn static QByteArray ssiVerifyData (QByteArray& number, QByteArray& passwordha*shed4, QByteArray& guid, QByteArray& code, QByteArray& algorithm)
     * @brief A http body for Ssi Verification
     *
     * @param number The phone number or fetion number.
     * @param passwordhashed4 The value of hashV4(UserId, password).
     * @param guid ...
     * @param code ...
     * @param algorithm ...
     * @return QByteArray
     **/
    QByteArray ssiVerifyData (QByteArray& number,
                              QByteArray& passwordhashed4,
                              QByteArray& guid,
                              QByteArray& code,
                              QByteArray& algorithm,
                              QByteArray passwordType)
    {
        QByteArray numberString;

        if (number.size() == 11)
        {
            numberString.append ("mobileno=");
        }
        else
        {
            numberString.append ("sid=");
        }

        numberString.append (number);

        QByteArray data ("GET /ssiportal/SSIAppSignInV4.aspx?");
        data.append (numberString);
        data.append ("&domains=");
        data.append (DOMAIN);
        data.append ("&pid=");
        data.append (guid);
        data.append ("&pic=");
        data.append (code);
        data.append ("&algorithm=");
        data.append (algorithm);
        data.append ("&v4digest-type=");
        data.append (passwordType);
        data.append ("&v4digest="); // TODO v4digest-type
        data.append (passwordhashed4);
        data.append (" HTTP/1.1\r\n"
                     "User-Agent: IIC2.0/pc ");
        data.append (PROTOCOL_VERSION);
        data.append ("\r\nHost: uid.fetion.com.cn\r\n"
                     "Cache-Control: private\r\n"
                     "Connection: Keep-Alive\r\n\r\n");
        return data;
    }

    /** @fn static QByteArray sipcAuthorizeBody(QByteArray& mobileNumber, QByteArray& userId*, QByteArray& personalVersion, QByteArray& customConfigVersion, QByteArray& contactVersion, QByteArray& state)
     * @brief Generate a body for sipc authorization
     * @param mobileNumber ...
     * @param userId ...
     * @param personalVersion ...
     * @param customConfigVersion ...
     * @param contactVersion ...
     * @param state ...
     * @return QByteArray
     **/
    QByteArray sipcAuthorizeBody (QByteArray& mobileNumber,
                                  QByteArray& userId,
                                  QByteArray& personalVersion,
                                  QByteArray& customConfigVersion,
                                  QByteArray& contactVersion,
                                  QByteArray& state)
    {
        QByteArray body = "<args><device machine-code=\"001676C0E351\"/>";
        body.append ("<caps value=\"1ff\"/><events value=\"7f\"/>"
                     "<user-info mobile-no=\"");
        body.append (mobileNumber);
        body.append ("\" user-id=\"");
        body.append (userId);
        body.append ("\"><personal version=\"");
        body.append (personalVersion);
        body.append ("\" attributes=\"v4default\"/><custom-config version=\"");
        body.append (customConfigVersion);
        body.append ("\"/><contact-list version=\"");
        body.append (contactVersion);
        body.append ("\" buddy-attributes=\"v4default\"/>"
                     "</user-info><credentials domains=\"");
        body.append (DOMAIN);
        body.append ("\"/><presence><basic value=\"");
        body.append (state);
        body.append ("\" desc=\"\"/></presence></args>\r\n");
        return body;
    }

    QByteArray keepAliveData (QByteArray &fetionNumber, int& callId)
    {
        static QByteArray keepAliveBody = "<args><credentials domains=\"fetion.com.cn\"/></args>\r\n";
        QByteArray data ("R fetion.com.cn SIP-C/4.0\r\n");
        data.append ("F: ").append (fetionNumber).append ("\r\n");
        data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
        data.append ("Q: 2 ").append ("R\r\n");
        data.append ("N: KeepAlive\r\n");
        data.append ("L: ");
        data.append (QByteArray::number (keepAliveBody.size()));
        data.append ("\r\n\r\n");
        data.append (keepAliveBody);
        return data;
    }

    /** @fn
     * @brief ...
     *
     * @param fromFetionNumber ...
     * @param toSipuri ...
     * @param callId ...
     * @param message ...
     * @return QByteArray
     **/
    QByteArray messagedata (QByteArray &fromFetionNumber, QByteArray &toSipuri, int& callId, QByteArray& message)
    {
        QByteArray data ("M fetion.com.cn SIP-C/4.0\r\n");
        data.append ("F: ").append (fromFetionNumber).append ("\r\n");
        data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
        data.append ("Q: 2 M\r\n");
        data.append ("T: ");
        data.append (toSipuri);
        data.append ("\r\n");
        data.append ("C: text/plain\r\n");
        data.append ("K: SaveHistory");
        data.append ("N: CatMsg\r\n");
        data.append ("L: ");
        data.append (QByteArray::number (message.size()));
        data.append ("\r\n\r\n");
        data.append (message);
        return data;
    }

    QByteArray addBuddyData (QByteArray& fromFetionNumber, QByteArray& buddyNumber,
                             int& callId, QByteArray& buddyLists, QByteArray& localName,
                             QByteArray& desc,  QByteArray& phraseId)
    {
        QByteArray body = "<args><contacts><buddies><buddy uri=\"";

        if (buddyNumber.size() == 11)
        {
            body.append ("tel:").append (buddyNumber);
        }
        else
        {
            body.append ("sip:").append (buddyNumber);
        }

        body.append ("\" localName=\"");

        body.append (localName);
        body.append ("\" buddy-lists=\"");
        body.append (buddyLists);
        body.append ("\" desc=\"");
        body.append (desc);// in form &#xABCD;, where ABCD is the utf8 code of character
        body.append ("\" expose-mobile-no=\"1\" expose-name=\"1\" addbuddy-phrase-id=\"");
        body.append (phraseId);
        body.append ("\"/></buddies></contacts></args>");

        QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
        data.append ("F: ").append (fromFetionNumber).append ("\r\n");
        data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
        data.append ("Q: 2 S\r\n");
        //TODO if verify
        data.append ("N: AddBuddyV4\r\n");
        data.append ("L: ");
        data.append (QByteArray::number (body.size()));
        data.append ("\r\n\r\n");
        data.append (body);
        return data;
    }

    QByteArray contactInfoData (QByteArray& fetionNumber, QByteArray &userId, int& callId)
    {
        QByteArray body = "<args><contact user-id=\"";
        body.append (userId);
        body.append ("\"/></args>\r\n");
        QByteArray data ("R fetion.com.cn SIP-C/4.0\r\n");
        data.append ("F: ").append (fetionNumber).append ("\r\n");
        data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
        data.append ("Q: 2 R\r\n");
        data.append ("N: GetContactInfoV4\r\n");
        data.append ("L: ");
        data.append (QByteArray::number (body.size()));
        data.append ("\r\n\r\n");
        data.append (body);
        return data;
    }
}
