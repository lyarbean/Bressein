#ifndef BRESSIEN_UTILS_H
#define BRESSIEN_UTILS_H
#include <QDateTime>
#include <QCryptographicHash>
#include <openssl/rsa.h>
/*! \defgroup utils
*  utils provide public declarations and functions.
*  @{
* */

/**
 * @brief The current Fetion's version
 **/
const static QByteArray PROTOCOL_VERSION = "4.3.0980";
const static QByteArray NAVIGATION = "nav.fetion.com.cn";
const static QByteArray CONFIG_URI = "/nav/getsystemconfig.aspx";
const static QByteArray DOMAIN = "fetion.com.cn";
const static QByteArray UID_URI = "uid.fetion.com.cn";
const static QByteArray SSI_URI = "/ssiportal/SSIAppSignInV4.aspx";

/** \fn static QByteArray hashV1 ( QByteArray &sid, QByteArray &password )
 * @brief Returns the Sha1 in Hex, of the catenation of sid and password
 *
 * @param sid
 * @param password
 * @return QByteArray
 **/
static QByteArray hashV1 (QByteArray &sid, QByteArray &password)
{
    return QCryptographicHash::hash (sid.append (password),
                                     QCryptographicHash::Sha1).toHex().toUpper();
}

/**
 * \fn static QByteArray hashV2 ( QByteArray &sid, QByteArray &password )
 * @brief hashV2
 *
 * @param sid ...
 * @param password ...
 * @return QByteArray
 **/
static QByteArray hashV2 (QByteArray &sid, QByteArray &password)
{
    QByteArray h = QByteArray::fromHex (password);
    bool ok;
    u_int32_t sidInt = sid.toUInt (&ok, 10);
    Q_ASSERT (ok);
    QByteArray out;
    out.append (char (sidInt & 0xFF)).append (char ( (sidInt & 0xFF00) >> 8))
    .append (char ( (sidInt & 0xFF000) >> 16))
    .append (char ( (sidInt & 0xFF000) >> 24));
    return  hashV1 (out, h);;
}

/**
 * \fn static QByteArray hashV4 ( QByteArray &sid, QByteArray &password )
 * @brief If sid is null then returns  hashV1 ( prefix, password ),
 *  otherwise returns hashV2 ( sid, hashV1 ( prefix,password )), where prefix is
 *  'fetion.com.cn:'.
 * \sa hashV1
 * \sa hashV2
 * @param sid ...
 * @param password ...
 * @return QByteArray
 **/
static QByteArray hashV4 (QByteArray &sid, QByteArray &password)
{
    QByteArray prefix = DOMAIN;
    prefix.append (':');
    QByteArray res = hashV1 (prefix, password);

    if (sid.isNull() || sid.isEmpty())
        return res;

    return hashV2 (sid, res);
}

/** \fn static QByteArray cnouce(uint time = 2)
 * @brief generate a QByteArray in hex of length 4 * time
 *
 * @param time ... 默认为 2。
 * @return QByteArray
 **/
static QByteArray cnouce (uint time = 2)
{
    QByteArray t;
    qsrand (QDateTime::currentMSecsSinceEpoch ());

    for (uint i = 0; i < time; i++)
    {
        t.append (QString::number (qrand(), 16));
    }

    return t;
}

/** \fn static QByteArray configBody(QByteArray& number)
 * @brief Generate a body for fetching server's configuration
 *
 * @param number The phone number or fetion number
 * @return QByteArray
 **/
static QByteArray configBody (QByteArray& number)
{
    QByteArray body = ("<config><user ");

    if (number.size() == 11) //mobileNumber
    {
        body.append ("mobile-no=\"");
    }
    else
    {
        body.append ("user-id=\"");
    }

    body.append (number);

    body.append ("\"/> <client type=\"PC\" version=\"");
    body.append (PROTOCOL_VERSION);
    body.append ("\"platform=\"W5.1\"/><servers version=\"0\"/>");
    body.append ("<parameters version=\"0\"/><hints version=\"0\"/></config>");
    return body;
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
    body.append ("<caps value=\"1ff\"/><events value=\"7f\"/>");
    body.append ("<user-info mobile-no=\"");
    body.append (mobileNumber);
    body.append ("\" user-id=\"");
    body.append (userId);
    body.append ("\"><personal version=\"");
    body.append (personalVersion);
    body.append ("\" attributes=\"v4default\"/><custom-config version=\"");
    body.append (customConfigVersion);
    body.append ("\"/><contact-list version=\"");
    body.append (contactVersion);
    body.append ("\" buddy-attributes=\"v4default\"/></user-info><credentials domains=\"");
    body.append (DOMAIN);
    body.append ("\"/><presence><basic value=\"");
    body.append (state);
    body.append ("\" desc=\"\"/></presence></args>\r\n");
    return body;
}

/*! @} */
#endif
// kate: indent-mode cstyle; space-indent on; indent-width 4; replace-tabs on;  replace-tabs on;  replace-tabs on;  replace-tabs on;           indent-wsidth 0;
