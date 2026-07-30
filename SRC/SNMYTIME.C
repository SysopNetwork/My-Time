/* 26.07.29.3 10:54PM */
/*****************************************************************************
 *   SNMYTIME.C                                     My Time - Core           *
 *                                                                           *
 *   Copyright (C) 2026 Sysop Network.  All Rights Reserved.                 *
 *                                                                           *
 *   My Time is a module for The Major BBS v10 that lets every user run     *
 *   their own clock: a personal display-time offset from the BBS clock,    *
 *   for callers who live in a different time zone than the board. The      *
 *   offset changes DISPLAY ONLY - the BBS's own timekeeping is never       *
 *   touched.                                                                *
 *                                                                           *
 *   Commands (available everywhere, via a global command handler):         *
 *                                                                           *
 *       /time            show the actual BBS time                          *
 *       /mytime          show the user's time (BBS time + their offset)    *
 *       /mytime +2       run two hours ahead of the BBS                    *
 *       /mytime -1       run one hour behind the BBS                       *
 *       /mytime +9:30    half-hour zones work too                          *
 *       /mytime reset    match the BBS clock again (offset zero)           *
 *       /mytime ?        usage summary                                     *
 *       /mytime audit on|off   Sysop only: toggle Audit Trail logging      *
 *                                                                           *
 *   The module also registers named text variables (usable in ANY message  *
 *   text on the board, and freely movable by the sysop):                   *
 *                                                                           *
 *       MYTIME     the user's time, h:mm with a/p  (e.g. 10:00a, 9:05p)    *
 *       MYT_BBS    the BBS time in the same format                         *
 *       MYT_OFS    the user's offset, signed       (e.g. +2:00, -1:00)     *
 *                                                                           *
 *   Storage: one small record per user in the host's general user          *
 *   database (the same file the host's own paging preferences live in),    *
 *   keyed by user-id + module name. New users have no record and so        *
 *   default to BBS time. Offsets are written through to disk the moment    *
 *   they change, and cached per channel for cheap display.                 *
 *****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "gcomm.h"
#include "majorbbs.h"
#include "usracc.h"                  /* usaptr - current user's account      */
#include "dfaapi.h"                  /* general user database access         */
#include "locknkey.h"                /* haskey() - sysop check               */
#include "SNMYTIME.H"                /* version + generated message numbers  */

#define MYTMNM "SNMYTIME"            /* our key in the general user database */
#define SYSKEY "SYSOP"               /* key required for /mytime audit       */

#define MINSPERHOUR 60
#define MAXOFS (23*MINSPERHOUR+59)   /* offsets run -23:59 .. +23:59         */

/* ----- module lifecycle routines (registered with the BBS) --------------- */
GBOOL myt_logon(VOID);               /* per-user logon hook: load offset     */
GBOOL myt_sttrou(VOID);              /* menu entry (module has no menu UI)   */
SHORT myt_logoff(VOID);              /* logoff: clear the channel's cache    */
VOID  myt_hangup(VOID);              /* hangup: clear the channel's cache    */
VOID  myt_delacc(CHAR *uid);         /* account deleted: drop stored offset  */
VOID  myt_shutdown(VOID);            /* clean shutdown: close message file   */

/* ----- module interface block -------------------------------------------- */
struct module SNMYTIME = {
    "",                              /* description (filled from the MDF)    */
    myt_logon,                       /* user logon supplemental routine      */
    myt_sttrou,                      /* input routine if our verb selected   */
    NULL,                            /* status-input routine                 */
    NULL,                            /* injoth routine                       */
    myt_logoff,                      /* user logoff supplemental routine     */
    myt_hangup,                      /* hangup (lost carrier) routine        */
    NULL,                            /* midnight cleanup routine             */
    myt_delacc,                      /* delete-account routine               */
    myt_shutdown                     /* finish-up (system shutdown) routine  */
};

/* ----- module-wide state -------------------------------------------------- */
static INT      myt_state;           /* this module's user-state handle      */
static HMCVFILE myt_mb;              /* open handle to our message file      */

/* Per-channel cache of each online user's offset, in minutes from BBS time.
   Loaded from disk at logon, written through on change, cleared at logoff
   and hangup so a recycled channel can never show a stale value.           */
static INT chanofs[MAXNTERM];

/* ----- on-disk record (general user database) ----------------------------- *
 * The general user database is shared by many facilities; records are keyed *
 * by user-id + module name, so ours never collide with anyone else's. The   *
 * sysop-wide settings live in one extra record under an empty user-id       *
 * (no real account can have an empty id).                                   */
struct mytdisk {
    CHAR userid[UIDSIZ];             /*   key part 1: user-id ("" = config)  */
    CHAR modnam[MNMSIZ];             /*   key part 2: always "SNMYTIME"      */
    SHORT ofsmin;                    /*   user's offset in minutes           */
    SHORT auditflg;                  /*   config record: log changes? 1/0    */
    CHAR resv[24];                   /*   reserved for future settings       */
};

/* ========================================================================= *
 *  Time formatting                                                          *
 * ========================================================================= */

/*
 * fmt12 - format a moment as h:mm with an 'a' or 'p' tail (10:00a, 9:05p).
 * This is the display format the MYTIME text variable promises.
 */
static VOID
fmt12(time_t t, CHAR *buf)
{
    struct tm *tp;
    INT hour12;

    tp=localtime(&t);
    hour12=tp->tm_hour%12;
    if (hour12 == 0) {               /* 0 and 12 both display as 12          */
        hour12=12;
    }
    sprintf(buf,"%d:%02d%c",hour12,tp->tm_min,tp->tm_hour < 12 ? 'a' : 'p');
}

/*
 * fmtofs - format an offset in minutes as a signed h:mm string (+2:00,
 * -1:00, +9:30). Zero formats as +0:00 (callers use a different message
 * for that case anyway).
 */
static VOID
fmtofs(INT ofsmin, CHAR *buf)
{
    INT mag=ofsmin < 0 ? -ofsmin : ofsmin;

    sprintf(buf,"%c%d:%02d",ofsmin < 0 ? '-' : '+',
            mag/MINSPERHOUR,mag%MINSPERHOUR);
}

/* ========================================================================= *
 *  Named text variables                                                     *
 *                                                                           *
 *  The host expands these at message-emit time, in the context of the user  *
 *  the output is for, so reading this channel's cached offset is correct.   *
 * ========================================================================= */

static CHAR *
tvMYTIME(VOID)                       /* the user's time (offset applied)     */
{
    static CHAR buf[12];

    fmt12(time(NULL)+(time_t)chanofs[usrnum]*60,buf);
    return(buf);
}

static CHAR *
tvMYTBBS(VOID)                       /* the actual BBS time                  */
{
    static CHAR buf[12];

    fmt12(time(NULL),buf);
    return(buf);
}

static CHAR *
tvMYTOFS(VOID)                       /* the user's offset, signed h:mm       */
{
    static CHAR buf[12];

    fmtofs(chanofs[usrnum],buf);
    return(buf);
}

/* ========================================================================= *
 *  Storage (general user database)                                          *
 * ========================================================================= */

/*
 * mytkey - fill a record with the two key fields for a lookup.
 */
static VOID
mytkey(struct mytdisk *rec, const CHAR *userid)
{
    setmem(rec,sizeof(struct mytdisk),0);
    stlcpy(rec->userid,userid,UIDSIZ);
    stlcpy(rec->modnam,MYTMNM,MNMSIZ);
}

/*
 * mytload - fetch a user's stored offset, in minutes. Users without a
 * record (anyone who never touched /mytime) are on BBS time: zero.
 */
static INT
mytload(const CHAR *userid)
{
    struct mytdisk rec;
    INT ofs=0;

    mytkey(&rec,userid);
    dfaSetBlk(genbb);
    if (dfaAcqEQ(&rec,&rec,0)) {
        ofs=rec.ofsmin;
    }
    dfaRstBlk();
    return(ofs);
}

/*
 * mytsave - write a user's offset through to disk (insert or update).
 */
static VOID
mytsave(const CHAR *userid, INT ofsmin)
{
    struct mytdisk rec;
    GBOOL recxst;

    mytkey(&rec,userid);
    dfaSetBlk(genbb);
    recxst=dfaAcqEQ(&rec,&rec,0);
    rec.ofsmin=(SHORT)ofsmin;
    if (recxst) {
        dfaUpdateV(&rec,sizeof(struct mytdisk));
    }
    else {
        dfaInsertV(&rec,sizeof(struct mytdisk));
    }
    dfaRstBlk();
}

/*
 * mytdrop - remove a user's record entirely (account deletion).
 */
static VOID
mytdrop(const CHAR *userid)
{
    struct mytdisk rec;

    mytkey(&rec,userid);
    dfaSetBlk(genbb);
    if (dfaAcqEQ(&rec,&rec,0)) {
        dfaDelete();
    }
    dfaRstBlk();
}

/*
 * cfgaudit / setaudit - the one sysop setting: log offset changes to the
 * Audit Trail? The live value lives in the config record (empty user-id)
 * and is read at point of use, so a change applies the moment it is made.
 * The CNF option is only the install default, used to seed the record the
 * first time the module ever loads (see init__snmytime).
 */
static GBOOL
cfgaudit(VOID)
{
    struct mytdisk rec;
    GBOOL on=TRUE;

    mytkey(&rec,"");
    dfaSetBlk(genbb);
    if (dfaAcqEQ(&rec,&rec,0)) {
        on=(rec.auditflg != 0);
    }
    dfaRstBlk();
    return(on);
}

static VOID
setaudit(GBOOL on)
{
    struct mytdisk rec;
    GBOOL recxst;

    mytkey(&rec,"");
    dfaSetBlk(genbb);
    recxst=dfaAcqEQ(&rec,&rec,0);
    rec.auditflg=(SHORT)(on ? 1 : 0);
    if (recxst) {
        dfaUpdateV(&rec,sizeof(struct mytdisk));
    }
    else {
        dfaInsertV(&rec,sizeof(struct mytdisk));
    }
    dfaRstBlk();
}

/* ========================================================================= *
 *  Command handling                                                         *
 * ========================================================================= */

/*
 * parseofs - parse a user-typed offset: an optional + or - sign, hours
 * 0-23, and optionally a colon and minutes 00-59. Returns TRUE and the
 * offset in minutes (signed) if the text is a well-formed offset.
 */
static GBOOL
parseofs(const CHAR *stg, INT *ofsmin)
{
    const CHAR *p=stg;
    INT sign=1,hrs=0,mins=0,ndig;

    if (*p == '+') {
        p++;
    }
    else if (*p == '-') {
        sign=-1;
        p++;
    }
    for (ndig=0 ; isdigit(*p) ; ndig++) {
        hrs=hrs*10+(*p++-'0');
    }
    if (ndig < 1 || ndig > 2 || hrs > 23) {
        return(FALSE);
    }
    if (*p == ':') {
        p++;
        for (ndig=0 ; isdigit(*p) ; ndig++) {
            mins=mins*10+(*p++-'0');
        }
        if (ndig != 2 || mins > 59) {
            return(FALSE);
        }
    }
    if (*p != '\0') {
        return(FALSE);
    }
    *ofsmin=sign*(hrs*MINSPERHOUR+mins);
    return(TRUE);
}

/*
 * saynow - report the user's current time: one message when an offset is
 * in effect, a simpler one when they are on BBS time.
 */
static VOID
saynow(VOID)
{
    prfmsg(chanofs[usrnum] != 0 ? MYTNOW : MYTSAME);
}

/*
 * setofs - store a new offset for the current user (cache + disk), report
 * it, and note it in the Audit Trail when the sysop has that enabled.
 */
static VOID
setofs(INT ofsmin)
{
    CHAR ofsbuf[12];

    chanofs[usrnum]=ofsmin;
    mytsave(usaptr->userid,ofsmin);
    prfmsg(ofsmin != 0 ? MYTSET : MYTRESET);
    if (cfgaudit()) {
        if (ofsmin != 0) {
            fmtofs(ofsmin,ofsbuf);
            shocst("My Time",spr("%s set their time offset to %s",
                                 usaptr->userid,ofsbuf));
        }
        else {
            shocst("My Time",spr("%s reset their time to BBS time",
                                 usaptr->userid));
        }
    }
}

/*
 * audcmd - the sysop's online settings editor: /mytime audit on|off.
 * The change applies immediately and is always recorded in the Audit
 * Trail (settings changes are audited regardless of the setting itself).
 */
static VOID
audcmd(VOID)
{
    if (!haskey(SYSKEY)) {
        prfmsg(MYTNOKEY);
        return;
    }
    if (margc >= 3 && sameas(margv[2],"on")) {
        setaudit(TRUE);
        prfmsg(MYTAUDON);
        shocst("My Time",spr("%s turned offset-change auditing ON",
                             usaptr->userid));
    }
    else if (margc >= 3 && sameas(margv[2],"off")) {
        setaudit(FALSE);
        prfmsg(MYTAUDOF);
        shocst("My Time",spr("%s turned offset-change auditing OFF",
                             usaptr->userid));
    }
    else {
        prfmsg(MYTHELP);
    }
}

/*
 * glomyt - the global command handler: fields /time and /mytime typed
 * anywhere on the board. Returns 1 when the input was ours (the host then
 * redisplays the caller's prompt), 0 to let other handlers look at it.
 */
static INT
glomyt(VOID)
{
    INT ofsmin;

    if (margc < 1 || usrptr->usrcls != ACTUSR) {
        return(0);                   /* not logged on: not our business      */
    }
    if (sameas(margv[0],"/time")) {
        setmbk(myt_mb);
        prfmsg(BBSTIME);
        rstmbk();
        outprf(usrnum);
        return(1);
    }
    if (!sameas(margv[0],"/mytime")) {
        return(0);
    }
    setmbk(myt_mb);
    if (margc == 1) {
        saynow();
    }
    else if (sameas(margv[1],"reset")) {
        setofs(0);
    }
    else if (sameas(margv[1],"?") || sameas(margv[1],"help")) {
        prfmsg(MYTHELP);
    }
    else if (sameas(margv[1],"audit")) {
        audcmd();
    }
    else if (parseofs(margv[1],&ofsmin)) {
        setofs(ofsmin);
    }
    else {
        prfmsg(MYTBADOF);
    }
    rstmbk();
    outprf(usrnum);
    return(1);
}

/* ========================================================================= *
 *  Module lifecycle                                                         *
 * ========================================================================= */

/*
 * init__snmytime - module entry point, called once by the BBS at startup.
 */
VOID EXPORT
init__snmytime(VOID)
{
    struct mytdisk rec;
    GBOOL cfgxst;

    /* Register with the BBS; description text comes from SNMYTIME.MDF.     */
    stzcpy(SNMYTIME.descrp,gmdnam("SNMYTIME.MDF"),MNMSIZ);
    myt_state=register_module(&SNMYTIME);

    /* Open our compiled message file so prfmsg() and options work.         */
    myt_mb=opnmsg("SNMYTIME.MCV");

    /* Named text variables - usable in any message text on the board.      */
    register_textvar("MYTIME",tvMYTIME);
    register_textvar("MYT_BBS",tvMYTBBS);
    register_textvar("MYT_OFS",tvMYTOFS);

    /* Field /time and /mytime typed anywhere on the board.                 */
    globalcmd(glomyt);

    /* First load ever: seed the live settings record from the CNF install
       default. After this the CNF option is never consulted again - the
       sysop edits the live value online with /mytime audit on|off.         */
    mytkey(&rec,"");
    dfaSetBlk(genbb);
    cfgxst=dfaAcqEQ(&rec,&rec,0);
    dfaRstBlk();
    if (!cfgxst) {
        setmbk(myt_mb);
        setaudit(ynopt(AUDITOPT));
        rstmbk();
    }

    /* Announce the running build in the Audit Trail: two entries, each on
       its own time-stamped line.                                           */
    shocst(spr("My Time v%s",MYT_VERSION),"");
    shocst("By SysopNetwork.com","");
}

/*
 * myt_logon - runs for every user at logon: load their stored offset into
 * this channel's cache. Users with no record land on zero - BBS time -
 * which is exactly what a brand-new user should see.
 */
GBOOL EXPORT
myt_logon(VOID)
{
    chanofs[usrnum]=mytload(usaptr->userid);
    return(0);
}

/*
 * myt_sttrou - input routine if a sysop wires My Time into the menu tree.
 * The module is really a set of global commands, so a menu visit just
 * shows the user their time and the usage summary, then returns.
 */
GBOOL EXPORT
myt_sttrou(VOID)
{
    setmbk(myt_mb);
    saynow();
    prfmsg(MYTHELP);
    rstmbk();
    outprf(usrnum);
    return(0);                       /* straight back to the menu            */
}

/*
 * myt_logoff / myt_hangup - clear the channel's cached offset the moment
 * the user leaves, so a recycled channel can never display a previous
 * occupant's time before the next logon reloads it.
 */
SHORT EXPORT
myt_logoff(VOID)
{
    chanofs[usrnum]=0;
    return(0);
}

VOID EXPORT
myt_hangup(VOID)
{
    chanofs[usrnum]=0;
}

/*
 * myt_delacc - a user account was deleted: drop their stored offset too.
 */
VOID EXPORT
myt_delacc(CHAR *uid)
{
    mytdrop(uid);
}

/*
 * myt_shutdown - called at BBS shutdown: release what init acquired.
 */
VOID EXPORT
myt_shutdown(VOID)
{
    if (myt_mb != NULL) {
        clsmsg(myt_mb);
        myt_mb=NULL;
    }
}
