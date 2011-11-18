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

#include "aux.h"
#include <QDateTime>
#include <QCryptographicHash>
#include <QDebug>
#include <openssl/rsa.h>


namespace Bressein
{
const QByteArray sipToFetion (const QByteArray &sipuri)
{
    int a = sipuri.indexOf (":");
    int b = sipuri.indexOf ("@");
    return sipuri.mid (a + 1, b - a - 1);
}

const QByteArray hashV1 (const QByteArray &userId, const QByteArray &password)
{
    QByteArray data (userId);
    data.append (password);
    return QCryptographicHash::hash (data, QCryptographicHash::Sha1)
           .toHex().toUpper();
}

const QByteArray hashV2 (const QByteArray &userId, const QByteArray &hashV4)
{
    QByteArray left;
    bool ok;
    unsigned int value = userId.toInt (&ok, 10);
    if (not ok)
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

const QByteArray hashV4 (const QByteArray &userId, const QByteArray &password)
{
    QByteArray prefix = DOMAIN_URI;
    prefix.append (':');
    QByteArray res = hashV1 (prefix, password);
    if (userId.isNull() or userId.isEmpty())
        return res;
    return hashV2 (userId, res);
}

/**
 * @brief This generates a response for sipc authorization,
 * implemented following that of libofetion's.
 **/
void RSAPublicEncrypt (const QByteArray &userId,
                       const QByteArray &password,
                       const QByteArray &nonce,
                       const QByteArray &aeskey,
                       const QByteArray &key,
                       QByteArray &result,
                       bool &ok)
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
    char *keyC = (char *) (key.data());
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
    int flen = RSA_size (r);   //128
    unsigned char *out = (unsigned char *) malloc (flen);
    memset (out , 0 , flen);
    int ret = RSA_public_encrypt (toEncrypt.size(),
                                  (unsigned char *) toEncrypt.data(),
                                  out, r, RSA_PKCS1_PADDING);
    RSA_free (r);
    if (ret < 0)
    {
        free (out);
        ok = false;
        return;
    }
    // split ret into hex bytearray
    Q_ASSERT (ret == 128);
    char *res = (char *) malloc (ret * 2 + 1);
    memset (res , 0 , ret * 2 + 1);
    int i = 0;
    while (i < ret)
    {
        sprintf (res + i * 2 , "%02x" , out[i]);
        i++;
    };
    result = QByteArray (res);
    ok = true;
    free (out);
    free (res);
}

/**
 * @brief generates a random string in hex with length of 8*time
 **/
const QByteArray cnonce (quint16 time)
{
    QByteArray t;
    qsrand (QDateTime::currentDateTime().toTime_t());
    for (quint16 i = 0; i < time; i++)
    {
        t.append (QByteArray::number (qrand(), 16));
    }
    return t.toUpper();
}

const QByteArray configData (QByteArray number)
{
    QByteArray numberString;
    if (number.size() == 11)
    {
        numberString.append ("mobile-no=\"");
    }
    else
    {
        numberString.append ("sid=\"");
    }
    numberString.append (number);
    QByteArray part = "<config><user ";
    part.append (numberString);
    part.append ("\"/><client type=\"Bressein\" version=\"");
    part.append (PROTOCOL_VERSION);
    part.append ("\" platform=\"W5.1\" /><servers version=\"0\" />"
                 "<parameters version=\"0\" /><client-config version=\"0\" />"
                 "<hints version=\"0\" /></config>");
    QByteArray data ("POST /nav/getsystemconfig.aspx HTTP/1.1\r\n"
                     "User-Agent: IIC2.0/PC ");
    data.append (PROTOCOL_VERSION);
    data.append ("\r\nHost: ");
    data.append (NAVIGATION);
    data.append ("\r\nConnection: Keep-Alive\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Content-length: ");
    data.append (QByteArray::number (part.size()));
    data.append ("\r\n\r\n");
    data.append (part);
    data.append ("\r\n");
    return data;
}

const QByteArray SsiPicData (const QByteArray &algorithm, const QByteArray &ssic)
{
    QByteArray data ("GET /nav/GetPicCodeV4.aspx?algorithm=");
    data.append (algorithm);
    data.append (" HTTP/1.1\r\n");
    data.append (ssic);
    data.append ("Host: nav.fetion.com.cn\r\n"   // NAVIGATION_URI
                 "User-Agent: IIC2.0/PC ");
    data.append (PROTOCOL_VERSION);
    data.append ("\r\nConnection: close\r\n\r\n");
    return data;
}
// TODO merged to ssiLoginData
const QByteArray ssiData (const QByteArray &number,
                          const QByteArray &passwordhashed4,
                          const QByteArray &id,
                          const QByteArray &code,
                          const QByteArray &algorithm)
{
    QByteArray numberString;
    QByteArray passwordType;
    if (number.size() == 11)
    {
        numberString.append ("mobileno=");
        passwordType = "1";
    }
    else
    {
        numberString.append ("sid=");
        passwordType = "2";
    }
    numberString.append (number);
    QByteArray data ("GET /ssiportal/SSIAppSignInV4.aspx?");
    data.append (numberString);
    data.append ("&domains=");
    data.append (DOMAIN_URI);
    if (not code.isEmpty())
    {
        data.append ("&pid=");
        data.append (id);
        data.append ("&pic=");
        data.append (code);
        data.append ("&algorithm=");
        data.append (algorithm);
    }
    data.append ("&v4digest-type=");
    data.append (passwordType);
    data.append ("&v4digest=");
    data.append (passwordhashed4);
    data.append ("\r\nUser-Agent: IIC2.0/pc ");
    data.append (PROTOCOL_VERSION);
    data.append ("\r\nHost: uid.fetion.com.cn\r\n"
                 "Cache-Control: private\r\n"
                 "Connection: Keep-Alive\r\n\r\n");
    return data;
}

const QByteArray percentEncodingLowercase (const QByteArray &string)
{
    QByteArray result = string;
    result.replace ("/", "%2f");
    result.replace ("@", "%40");
    result.replace ("=", "%3d");
    result.replace (":", "%3a");
    result.replace (";", "%3b");
    result.replace ("+", "%2b");
    return result;
}
const QByteArray downloadPortraitData (const QByteArray &portraitName,
                                       const QByteArray &portraitPath,
                                       const QByteArray &sipuri,
                                       const QByteArray &ssic)
{
    QByteArray data ("GET ");
    data.append (portraitPath);
    data.append ("/getportrait.aspx?Uri=");
    data.append (percentEncodingLowercase (sipuri));
    data.append ("&Size=120&c=");
    data.append (percentEncodingLowercase (ssic));
    data.append (" HTTP/1.1\r\nUser-Agent: IIC2.0/PC ");
    data.append (PROTOCOL_VERSION);
    data.append ("\r\nAccept: image/pjpeg;image/jpeg;image/bmp;"
                 "image/x-windows-bmp;image/png;image/gif\r\n"
                 "Host: ");
    data.append (portraitName);
    data.append ("\r\nConnection: Keep-Alive\r\n\r\n");
    return data;
}

const QByteArray downloadPortraitAgainData (const QByteArray &path,
                                            const QByteArray &host)
{
    QByteArray data ("GET ");
    data.append (path);
    data.append (" HTTP/1.1\r\nUser-Agent: IIC2.0/PC ");
    data.append (PROTOCOL_VERSION);
    data.append ("\r\nAccept: image/pjpeg;image/jpeg;image/bmp;"
                 "image/x-windows-bmp;image/png;image/gif\r\n"
                 "Cache-Control: private\r\n"
                 "Host: ");
    data.append (host);
    data.append ("\r\nConnection: Keep-Alive\r\n\r\n");
    return data;
}

const QByteArray sipcAuthorizeData (const QByteArray &mobileNumber,
                                    const QByteArray &fetionNumber,
                                    const QByteArray &userId,
                                    int &callId,
                                    const QByteArray &response,
                                    const QByteArray &personalVersion,
                                    const QByteArray &customConfigVer,
                                    const QByteArray &contactVersion,
                                    const QByteArray &state,
                                    const QByteArray &ackData)
{
    QByteArray body;
    body.append ("<args><device machine-code=\"001676C0E351\"/>"
                 "<caps value=\"1ff\"/><events value=\"7f\"/>"
                 "<user-info mobile-no=\"");
    body.append (mobileNumber);
    body.append ("\" user-id=\"");
    body.append (userId);
    body.append ("\"><personal version=\"");
    body.append (personalVersion);
    body.append ("\" attributes=\"v4default\"/><custom-config version=\"");
    body.append (customConfigVer);
    body.append ("\"/><contact-list version=\"");
    body.append (contactVersion);
    body.append ("\" buddy-attributes=\"v4default\"/>"
                 "</user-info><credentials domains=\"");
    body.append (DOMAIN_URI);
    body.append ("\"/><presence><basic value=\"");
    body.append (state);
    body.append ("\" desc=\"\"/></presence></args>\r\n");
    QByteArray data ("R fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QString::number (callId++)).append ("\r\n");
    data.append ("Q: 2 R\r\n"
                 "A: Digest response=\"");
    data.append (response);
    data.append ("\",algorithm=\"SHA1-sess-v4\"\r\n"
                 "AK: ak-value\r\n");
    if (not ackData.isEmpty())
    {
        data.append (ackData);
    }
    data.append ("L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

const QByteArray sipcAckData (const QByteArray &number,
                              const QByteArray &passwordhashed4,
                              const QByteArray &type,
                              const QByteArray &id,
                              const QByteArray &code,
                              const QByteArray &algorithm)
{
    QByteArray data = "A: Verify response=\"";
    data.append (code);
    data.append ("\",algorithm=\"");
    data.append (algorithm);
    data.append ("\",type=\"");
    data.append (type);
    data.append ("\",chid=\"");
    data.append (id);
    data.append ("\"\r\n");
    return data;
}

const QByteArray keepAliveData (const QByteArray &fetionNumber, int &callId)
{
    // body can be empty, that help save flow
    static QByteArray body = "";
    //"<args><credentials domains=\"fetion.com.cn\"/></args>\r\n";
    QByteArray data ("R fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 R\r\n"
                 "N: KeepAlive\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

const QByteArray chatKeepAliveData (const QByteArray &fetionNumber, int &callId)
{
    QByteArray data ("R fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 1 R\r\n"
                 "N: KeepConnectionBusy\r\n\r\n");
    return data;
}

const QByteArray catMsgData (const QByteArray &fromFetionNumber,
                             const QByteArray &toSipuri,
                             int &callId,
                             const QByteArray &message)  // utf-8 encoded
{
    QByteArray data ("M fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fromFetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 M\r\n"
                 "T: ");
    data.append (toSipuri);
    data.append ("\r\n");
    data.append ("C: text/html-fragment\r\n"//text/html-fragment
                 "K: SaveHistory\r\n"
                 "N: CatMsg\r\n");//K: NewEmotion
    data.append ("L: ");
    data.append (QByteArray::number (message.size()));
    data.append ("\r\n\r\n");
    data.append (message);
    return data;
}

const QByteArray sendCatMsgSelfData (const QByteArray &fetionNumber,
                                     const QByteArray &sipuri,
                                     int &callId,
                                     const QByteArray &message)
{
    QByteArray data ("M fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 M\r\n"
                 "T: ");
    data.append (sipuri);
    data.append ("\r\nN: SendCatSMS\r\n"
                 "L: ");
    data.append (QByteArray::number (message.size()));
    data.append ("\r\n\r\n");
    data.append (message);
    return data;
}

const QByteArray sendCatMsgPhoneData (const QByteArray &fromFetionNumber,
                                      const QByteArray &toSipuri,
                                      int &callId,
                                      const QByteArray &message,
                                      const QByteArray &id,
                                      const QByteArray &code)
{
    QByteArray data ("M fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fromFetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 M\r\n"
                 "T: ");
    data.append (toSipuri);
    data.append ("\r\n");
    if (not id.isEmpty() and not code.isEmpty())
    {
        data.append ("A: Verify algorithm=\"picc\",chid=\"");
        data.append (id);
        data.append ("\",response=\"");
        data.append (code);
        data.append ("\"\r\n");
    }
    data.append ("N: SendCatSMS\r\n"
                 "L: ");
    data.append (QByteArray::number (message.size()));
    data.append ("\r\n\r\n");
    data.append (message);
    return data;
}

const QByteArray addBuddyV4Data (const QByteArray &fromFetionNumber,
                                 const QByteArray &buddyNumber,
                                 int &callId,
                                 const QByteArray &buddyLists,
                                 const QByteArray &localName,
                                 const QByteArray &desc,
                                 const QByteArray &phraseId)
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
    body.append (desc);
    // in form &#xABCD;, where ABCD is the utf8 code of character
    body.append ("\" expose-mobile-no=\"1\" expose-name=\"1\""
                 " addbuddy-phrase-id=\"");
    body.append (phraseId);
    body.append ("\"/></buddies></contacts></args>");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fromFetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 //TODO if verify
                 "N: AddBuddyV4\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

const QByteArray contactInfoData (const QByteArray &fetionNumber,
                                  const QByteArray &userId,
                                  int &callId)
{
    QByteArray body = "<args><contact user-id=\"";
    body.append (userId);
    body.append ("\"/></args>\r\n");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: GetContactInfoV4\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}


const QByteArray contactInfoData (const QByteArray &fetionNumber,
                                  const QByteArray &number,
                                  int &callId,
                                  bool mobile)
{
    QByteArray uri;
    uri = mobile ? "tel:" : "sip:";
    uri.append (number);
    QByteArray body = "<args><contact uri=\"";
    body.append (uri);
    body.append ("\"/></args>\r\n");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: GetContactInfoV4\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

const QByteArray deleteBuddyV4Data (const QByteArray &fetionNumber,
                                    const QByteArray &userId,
                                    int &callId)
{
    QByteArray body = "<args><contacts><buddies><buddy user-id=\"";
    body.append (userId);
    body.append ("\"/></buddies></contacts></args>");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: DeleteBuddyV4\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}
//subscription
const QByteArray presenceV4Data (const QByteArray &fetionNumber, int &callId)
{
    QByteArray body = "<args><subscription self=\"v4default;mail-count\" "
                      "buddy=\"v4default\" version=\"0\"/></args>";
    //<args><subscription self="v4default;mail-count;impresa;sms-online-status;
    //feed-version;feed-type;es2all;birthday" buddy="v4default;feed-version;
    //feed-type;es2all;email-enable;rtm;dynamic-title;dynamic-id;birthday"
    // version="" /></args>
    QByteArray data ("SUB fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 SUB\r\n"
                 "N: PresenceV4\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

const QByteArray startChatData (const QByteArray &fetionNumber,
                                int &callId)
{
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 1 S\r\n"
                 "N: StartChat\r\n\r\n");
    return data;
}

const QByteArray registerData (const QByteArray &fetionNumber,
                               int &callId,
                               const QByteArray &credential)
{
    QByteArray data ("R fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 1 R\r\n"
                 "A: TICKS auth=\"");
    data.append (credential);
    data.append ("\"\r\n");
    data.append ("K: text/html-fragment\r\nK: multiparty\r\nK: nudge\r\n"
                 "K: share-background\r\nK: fetion-show\r\nK: ExModulesApp\r\n"
                 "K: FileTransferV4\r\n\r\n");
    return data;
}

//

const QByteArray inviteBuddyData (const QByteArray &fromFetionNumber,
                                  int &callId,
                                  const QByteArray &toSipUri)
{
    QByteArray body = "<args><contacts><contact uri=\"";
    body.append (toSipUri);
    body.append ("\"/></contacts></args>");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fromFetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: InviteBuddy\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

// Whether show mobile number to userId
const QByteArray SetContactInfoV4 (const QByteArray &fromFetionNumber,
                                   const QByteArray &userId,
                                   int &callId,
                                   bool show)
{
    QByteArray body = "<args><contacts><contact user-id=\"";
    body.append (userId);
    body.append ("\" permission=\"identity=");
    if (show)
    {
        body.append ("1");
    }
    else
    {
        body.append ("0");
    }
    body.append ("\"/></contacts></args>");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fromFetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: SetContactInfoV4\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

const QByteArray SetContactInfoV4 (const QByteArray &fromFetionNumber,
                                   const QByteArray &userId,
                                   int &callId,
                                   const QByteArray &name)
{
    QByteArray body = "<args><contacts><contact user-id=\"";
    body.append (userId);
    body.append ("\" local-name=\"");
    body.append (name);
    body.append ("\"/></contacts></args>");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fromFetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: SetContactInfoV4\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

const QByteArray SetContactInfoV4 (const QByteArray &fromFetionNumber,
                                   const QByteArray &userId,
                                   int &callId,
                                   const int &moveGroup)
{
    QByteArray body = "<args><contacts><contact user-id=\"";
    body.append (userId);
    body.append ("\" buddy-lists=\"");
    body.append (QByteArray::number (moveGroup));
    body.append ("\"/></contacts></args>");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fromFetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: SetContactInfoV4\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}
const QByteArray createBuddyListData (const QByteArray &fetionNumber,
                                      int &callId,
                                      const QByteArray &name)
{
    QByteArray body = "<args><contacts><buddy-lists><buddy-list name=\"";
    body.append (name);
    body.append ("\"/></buddy-lists></contacts></args>");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: CreateBuddyList\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

const QByteArray deleteBuddyListData (const QByteArray &fetionNumber,
                                      int &callId,
                                      const QByteArray &id)
{
    QByteArray body = "<args><contacts><buddy-lists><buddy-list id=\"";
    body.append (id);
    body.append ("\"/></buddy-lists></contacts></args>");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: DeleteBuddyList\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

const QByteArray setBuddyListInfoData (const QByteArray &fetionNumber,
                                       int &callId,
                                       const QByteArray &id,
                                       const QByteArray &name)
{
    QByteArray body = "<args><contacts><buddy-lists><buddy-list name=\"";
    body.append (name);
    body.append ("\" id=\"");
    body.append (id);
    body.append ("\"/></buddy-lists></contacts></args>");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: SetBuddyListInfo\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

const QByteArray handleContactRequestV4Data (const QByteArray &fetionNumber,
                                             int &callId,
                                             const QByteArray &userId,
                                             const QByteArray &sipuri,
                                             const QByteArray &localName,
                                             const QByteArray &buddyLists,
                                             const QByteArray &result)
{
    QByteArray body = "<args><contacts><buddies><buddy user-id=\"";
    body.append (userId);
    body.append ("\" uri=\"");
    body.append (sipuri);
    body.append ("\" result=\"");
    body.append (result);
    body.append ("\" buddylists=\"");
    body.append (buddyLists);
    body.append ("\" expose-mobile-no=\"1\" expose-name=\"1\" local-name=\"");
    body.append (localName);
    body.append ("\"/></buddies></contacts></args>");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: HandleContactRequestV4\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

const QByteArray directSMSData (const QByteArray &fetionNumber,
                                int &callId,
                                const QByteArray &algorithm,
                                const QByteArray &type,
                                const QByteArray &id,
                                const QByteArray &response)
{
    QByteArray data ("O fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 M\r\n"
                 "N: DirectSMS\r\n");
    if (not response.isEmpty())
    {
        QByteArray A = "A: Verify algorithm=\"";
        A.append (algorithm);
        A.append ("\",type=\"");
        A.append (type);
        A.append ("\",response=\"");
        A.append (response);
        A.append ("\",chid=\"");
        A.append (id);
        A.append ("\"\r\n");
        data.append (A);
    }
    data.append ("\r\n");
    return data;
}
const QByteArray sendDirectCatSMSData (const QByteArray &fetionNumber,
                                       int &callId,
                                       const QByteArray &mobile,
                                       const QByteArray &message)
{
    QByteArray data ("M fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 M\r\n"
                 "T: tel:");
    data.append (mobile);
    data.append ("\r\n"
                 "SV: 1\r\n"
                 "N: SendDirectCatSMS\r\n"
                 "L: ");
    data.append (QByteArray::number (message.size()));
    data.append ("\r\n\r\n");
    data.append (message);
    return data;
}
//PG

// N.B. pgGroupCallId = callId
const QByteArray pgGetGroupListData (const QByteArray &fetionNumber,
                                     int &callId)
{
    QByteArray body = "<args><group-list /></args>";
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: PGGetGroupList\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

const QByteArray pgGetGroupInfoData (const QByteArray &fetionNumber,
                                     int &callId,
                                     const QList<QByteArray> &pguris)
{
    QByteArray body = "<args><groups attributes =\"all\">";
    foreach (const QByteArray& pguri, pguris)
    {
        body.append ("<group uri=\"" + pguri + "\">");
    }
    body.append ("</groups></args>");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: PGGetGroupInfo\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

const QByteArray pgPresenceData (const QByteArray &fetionNumber,
                                 int &callId,
                                 const QByteArray &pguri)
{
    QByteArray body = "<args><subscription><groups><group uri=\"";
    body.append (pguri);
    body.append ("\"/></groups><presence><basic attributes=\"all\"/>"
                 "<member attributes=\"identity\"/>"
                 "<management attributes=\"all\"/>"
                 "</presence></subscription></args>");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: PGPresence\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

const QByteArray pgGetGroupMembersData (const QByteArray &fetionNumber,
                                        int &callId,
                                        const QByteArray &pguri)
{
    QByteArray body = "<args><groups attributes=\"member-uri;member-nickname;"
                      "member-iicnickname;member-identity;member-t6svcid\">"
                      "<group uri=\"";
    body.append (pguri);
    body.append ("\"/></groups></args>");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: PGGetGroupMembers\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

const QByteArray pgSendCatSMSData (const QByteArray &fetionNumber,
                                   int &callId,
                                   const QByteArray &pguri,
                                   const QByteArray &message)
{
    QByteArray data ("M fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 M\r\n"
                 "T: ");
    data.append (pguri);
    data.append ("\r\n"
                 "N: PGSendCatSMS\r\n"
                 "L: ");
    data.append (QByteArray::number (message.size()));
    data.append ("\r\n\r\n");
    data.append (message);
    return data;
}
//User
//update info
const QByteArray setUserInfoV4Data (const QByteArray &fetionNumber,
                                    int &callId,
                                    const QByteArray &impresa,
                                    const QByteArray &nickName,
                                    const QByteArray &gender,
                                    const QByteArray &customConfig,
                                    const QByteArray &customConfigVer)
{
    QByteArray body = "<args><userinfo><personal impresa=\"";
    body.append (impresa);
    body.append ("\" nickName=\"");
    body.append (nickName);
    body.append ("\" gender=\"");
    body.append (gender);
    body.append ("\" version=\"0\"/>");
    body.append ("<custom-config type=\"Bressein\" version=\"");
    body.append (customConfigVer);
    body.append ("\"/></userinfo></args>");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: SetUserInfoV4\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

//set impresa
const QByteArray setUserInfoV4Data (const QByteArray &fetionNumber,
                                    int &callId,
                                    const QByteArray &impresa,
                                    const QByteArray &personalVersion,
                                    const QByteArray &customConfig,
                                    const QByteArray &customConfigVer)
{
    QByteArray body = "<args><userinfo><personal impresa=\"";
    body.append (impresa);
    body.append ("\" version=\"");
    body.append (personalVersion);
    body.append ("\"/><custom-config type=\"Bressein\" version=\"");
    body.append (customConfigVer);
    body.append ("\"/></userinfo></args>");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: SetUserInfoV4\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

// set SMS status
const QByteArray setUserInfoV4Data (const QByteArray &fetionNumber,
                                    int &callId, const int days)
{
    /** <args><userinfo><configs>
     * <config name="sms-notify">0</config>
     * <config name="directsms-rec">0</config> 1 if only receive sms fetion, 0 if client then phone, 2 if not to receive sms from stranger
     * <config name="multi-client">0</config>
     * <config name="private-presence"></config>
     * <config name="addbuddy-app-sms">0</config>
     * <config name="permission-request">0</config>
     * <config name="schedule-sms-receipt">1</config> 1 for only send to fetion, 0 will fallback to phone
     * <config name="rev-offlinefile-sms">1</config>
     * <config name="send-offlinefile-sms">1</config> **/
    QByteArray body = "<args><userinfo><personal sms-online-status=\"";
    body.append (QByteArray::number (days));
    body.append (".00:00:00\"/></userinfo></args>");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: SetUserInfoV4\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

//
const QByteArray setPresenceV4Data (const QByteArray &fetionNumber,
                                    int &callId,
                                    const QByteArray &state)
{
    QByteArray body = "<args><presence><basic value=\"";
    // <args><presence><extends><extend type="rtm"/></extends></presence><args>
    body.append (state);
    body.append ("\"/></presence></args>");
    QByteArray data ("S fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 S\r\n"
                 "N: SetPresenceV4\r\n"
                 "L: ");
    data.append (QByteArray::number (body.size()));
    data.append ("\r\n\r\n");
    data.append (body);
    return data;
}

const QByteArray inviteAckData (const QByteArray &fetionNumber,
                                int &callId,
                                const QByteArray &sipuri)
{
    QByteArray data ("A fetion.com.cn SIP-C/4.0\r\n");
    data.append ("F: ").append (fetionNumber).append ("\r\n");
    data.append ("I: ").append (QByteArray::number (callId++)).append ("\r\n");
    data.append ("Q: 2 A\r\n"
                 "T ");
    data.append (sipuri);
    data.append ("\r\n\r\n");
    return data;
}

}
