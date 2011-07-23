#ifndef BRESSIEN_TYPES_H
#define BRESSIEN_TYPES_H
class QByteArray;
namespace Bressein
{
    // from levin's openfetion
    /**
     * @brief Presence states
     **/
    enum StateType
    {
        ONLINE =       400,
        RIGHTBACK =    300,
        AWAY =         100,
        BUSY =         600,
        OUTFORLUNCH =  500,
        ONTHEPHONE =   150,
        MEETING =      850,
        DONOTDISTURB = 800,
        HIDDEN =       0,
        OFFLINE =      -1
    };
    /**
     * @brief Type to indicate user`s service status
     **/
    enum StatusType
    {
        STATUS_NORMAL = 1,       //normal status
        STATUS_OFFLINE,          //user offline , deleted you from his list or out of service
        STATUS_NOT_AUTHENTICATED,//user has not accept your add buddy request
        STATUS_SMS_ONLINE,       //user has not start fetion service
        STATUS_REJECTED,         //user rejected your add buddy request,wait for deleting
        STATUS_SERVICE_CLOSED,   //user has closed his fetion service
        STATUS_NOT_BOUND         //user doesn`t bound fetion number to a mobile number
    };
    /**
     * @brief some other buddylists
     **/
    enum BuddyListType
    {
        BUDDY_LIST_NOT_GROUPED = 0 ,
        BUDDY_LIST_STRANGER = -1 ,
        BUDDY_LIST_PGGROUP = -2
    };
    /**
     * @brief Type used to indicate whether user`s portrait has been changed
     **/
    enum ImageChangedType
    {
        IMAGE_NOT_INITIALIZED = -1 ,//portrait has not been initialized
        IMAGE_NOT_CHANGED ,         //portrait does not change
        IMAGE_CHANGED ,             //portrait has been changed
        IMAGE_ALLREADY_SET
    };

    struct contact
    {
        // from contact list
        // i: userId,  n:local name
        // o: group id, p: identity
        // r; relationStatus, u: sipuri
        QByteArray userId;
        QByteArray localName;
        QByteArray groupId;
        QByteArray identity;
        QByteArray relationStatus;
        // this is determinant
        QByteArray sipuri;     //sipuri like 'sip:sId@fetion.com.cn'
        // contact info when query
        QByteArray imprea;
        QByteArray mobileno;   //mobile phone number
        QByteArray devicetype; //user`s client type , like PC and J2ME,etc
        QByteArray portraitCrc;//a number generated by crc algorithm
        QByteArray birthdate;   //user`s bitrhday
        QByteArray country;    //user`s country`s simplified form,like CN
        QByteArray province;   //user`s province`s simplified form,like bj
        QByteArray city;       //user`s city`s code ,like 10 for beijing
// other stuffs
        int scoreLevel;        //user`s score level,unused now
        int serviceStatus;     //basic service status
        int carrierStatus;
        QByteArray carrier;
        StateType state;       //state type like online,busy,etc
        int gender;            //gender 1 for male 2 for female,0 for private
        int imageChanged;      //whether user`s portrait has changed
        int dirty;             //whether the contact read from the server is latest
    };

    struct group
    {
        QByteArray groupname; // current buddy list name
        int groupId;          // current buddy list Id
        int dirty;
    };
    //TODO
    // pggroupmember
    // pggroup
}
#endif
