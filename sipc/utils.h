#ifndef BRESSIEN_UTILS_H
#define BRESSIEN_UTILS_H
#include <QDateTime>
#include <QCryptographicHash>
#include <openssl/rsa.h>
#include <QDebug>
/*! \defgroup utils "Utils"
*  utils provide public declarations and functions.
*  @{
* */

namespace Bressein
{
    /**
     * @brief The current Fetion's version
     **/
    const static QByteArray PROTOCOL_VERSION = "4.3.0980";
    const static QByteArray NAVIGATION = "nav.fetion.com.cn";
    const static QByteArray CONFIG_URI = "/nav/getsystemconfig.aspx";
    const static QByteArray DOMAIN = "fetion.com.cn";
    const static QByteArray UID_URI = "uid.fetion.com.cn";
    const static QByteArray SSI_URI = "/ssiportal/SSIAppSignInV4.aspx";

    /** \fn static QByteArray hashV1 ( QByteArray &userIdInt, QByteArray &password )
     * @brief Returns the Sha1 in Hex, of the catenation of userIdInt and password
     *
     * @param userId
     * @param password
     * @return QByteArray in Hex
     **/
    static QByteArray hashV1 (QByteArray &userId, QByteArray &password)
    {
        return QCryptographicHash::hash (userId.append (password),
                                         QCryptographicHash::Sha1).toHex().toUpper();
    }

    /**
     * \fn static QByteArray hashV2 ( QByteArray &userId, QByteArray &hashV4 )
     * @brief hashV2
     *
     * @param userId in decimal
     * @param password
     * @return QByteArray
     **/
    static QByteArray hashV2 (QByteArray &userId, QByteArray &hashV4)
    {
        QByteArray left;
        bool ok;
        unsigned int value = userId.toInt (&ok, 10);

        if (!ok)
        {
            printf ("Failed to convert userId to Integer\n");
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
     * \fn static QByteArray hashV4 ( QByteArray &userId, QByteArray &password )
     * @brief If userId is null then returns  hashV1 ( prefix, password ),
     *  otherwise returns hashV2 ( userId, hashV1 ( prefix,password )),
     *  where prefix is 'fetion.com.cn:'.
     * \sa hashV1
     * \sa hashV2
     * @param userId ...
     * @param password ...
     * @return QByteArray
     **/
    static QByteArray hashV4 (QByteArray &userId, QByteArray &password)
    {
        QByteArray prefix = DOMAIN;
        prefix.append (':');
        QByteArray res = hashV1 (prefix, password);

        if (userId.isNull() || userId.isEmpty())
            return res;

        return hashV2 (userId, res);
    }

    static QByteArray RSAPublicEncrypt (QByteArray userId,
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
        memset (modulus, 0, sizeof (modulus));
        memset (exponent, 0, sizeof (exponent));
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
        qDebug() << "toEncrypt " << toEncrypt.constData();
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
        int i = 0;
        memset (result , 0 , ret * 2 + 1);

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

    /** \fn static QByteArray cnouce(quint16 time = 2)
     * @brief generate a QByteArray in hex of length 16 * time
     *
     * @param time  2 as default.
     * @return QByteArray
     **/
    static QByteArray cnouce (quint16 time = 2)
    {
        QByteArray t;
        qsrand (QDateTime::currentMSecsSinceEpoch ());

        for (quint16 i = 0; i < time; i++)
        {
            t.append (QByteArray::number (qrand()).toHex());
        }

        return t;
    }

    /** \fn static QByteArray configData(QByteArray& number)
     * @brief Generate a body for fetching server's configuration
     *
     * @param number The phone number or fetion number
     * @return QByteArray
     **/
    static QByteArray configData (QByteArray& number)
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

        part.append ("\"/> <client type=\"PC\" version=\"");
        part.append (PROTOCOL_VERSION);
        part.append ("\" platform=\"W5.1\"/><servers version=\"0\"/>"
                     "<parameters version=\"0\"/><hints version=\"0\"/></config>");
        QByteArray data ("POST /nav/getsystemconfig.aspx HTTP/1.1\r\n");
        data.append ("User-Agent: IIC2.0/PC ");
        data.append (PROTOCOL_VERSION);
        data.append ("\r\nHost: ");
        data.append ("\r\nConnection: Close\r\n");
        data.append ("Content-length: ");
        data.append (QByteArray::number (part.size()));
        data.append ("\r\n\r\n");
        data.append (part);
        return data;
    }

    /** \fn static QByteArray ssiLoginData (QByteArray& number, QByteArray& passwordhashed4)
     * @brief A body that written to sslsocket on SsiLogin
     *
     * @param number The phone number or fetion number.
     * @param passwordhashed4 The value of hashV4(UserId, password).
     * @return QByteArray
     **/
    static QByteArray ssiLoginData (QByteArray& number,
                                    QByteArray& passwordhashed4,
                                    QByteArray passwordType = "1")
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

    static QByteArray SsiPicData (QByteArray algorithm, QByteArray ssic)
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

    /** \fn static QByteArray ssiVerifyData (QByteArray& number, QByteArray& passwordha*shed4, QByteArray& guid, QByteArray& code, QByteArray& algorithm)
     * @brief A http body for Ssi Verification
     *
     * @param number The phone number or fetion number.
     * @param passwordhashed4 The value of hashV4(UserId, password).
     * @param guid ...
     * @param code ...
     * @param algorithm ...
     * @return QByteArray
     **/
    static QByteArray ssiVerifyData (QByteArray& number,
                                     QByteArray& passwordhashed4,
                                     QByteArray& guid,
                                     QByteArray& code,
                                     QByteArray& algorithm,
                                     QByteArray passwordType = "1")
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

    /** \fn static QByteArray sipAuthorizeBody(QByteArray& mobileNumber, QByteArray& userId*, QByteArray& personalVersion, QByteArray& customConfigVersion, QByteArray& contactVersion, QByteArray& state)
     * @brief Generate a body for sip authorization
     * @param mobileNumber ...
     * @param userId ...
     * @param personalVersion ...
     * @param customConfigVersion ...
     * @param contactVersion ...
     * @param state ...
     * @return QByteArray
     **/
    static QByteArray sipAuthorizeBody (QByteArray& mobileNumber,
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
}

/*! @} */
#endif
