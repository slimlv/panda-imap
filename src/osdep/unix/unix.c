/*
 * Program:	UNIX mail routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	20 December 1989
 * Last Edited:	14 October 1999
 *
 * Copyright 1999 by the University of Washington
 *
 *  Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appears in all copies and that both the
 * above copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the University of Washington not be
 * used in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  This software is made
 * available "as is", and
 * THE UNIVERSITY OF WASHINGTON DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT LIMITATION ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, AND IN
 * NO EVENT SHALL THE UNIVERSITY OF WASHINGTON BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, TORT
 * (INCLUDING NEGLIGENCE) OR STRICT LIABILITY, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */


/*				DEDICATION
 *
 *  This file is dedicated to my dog, Unix, also known as Yun-chan and
 * Unix J. Terwilliker Jehosophat Aloysius Monstrosity Animal Beast.  Unix
 * passed away at the age of 11 1/2 on September 14, 1996, 12:18 PM PDT, after
 * a two-month bout with cirrhosis of the liver.
 *
 *  He was a dear friend, and I miss him terribly.
 *
 *  Lift a leg, Yunie.  Luv ya forever!!!!
 */

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
extern int errno;		/* just in case */
#include <signal.h>
#include "mail.h"
#include "osdep.h"
#include <time.h>
#include <sys/stat.h>
#include "unix.h"
#include "pseudo.h"
#include "fdstring.h"
#include "misc.h"
#include "dummy.h"

/* UNIX mail routines */


/* Driver dispatch used by MAIL */

DRIVER unixdriver = {
  "unix",			/* driver name */
  DR_LOCAL|DR_MAIL,		/* driver flags */
  (DRIVER *) NIL,		/* next driver */
  unix_valid,			/* mailbox is valid for us */
  unix_parameters,		/* manipulate parameters */
  unix_scan,			/* scan mailboxes */
  unix_list,			/* list mailboxes */
  unix_lsub,			/* list subscribed mailboxes */
  NIL,				/* subscribe to mailbox */
  NIL,				/* unsubscribe from mailbox */
  unix_create,			/* create mailbox */
  unix_delete,			/* delete mailbox */
  unix_rename,			/* rename mailbox */
  NIL,				/* status of mailbox */
  unix_open,			/* open mailbox */
  unix_close,			/* close mailbox */
  NIL,				/* fetch message "fast" attributes */
  NIL,				/* fetch message flags */
  NIL,				/* fetch overview */
  NIL,				/* fetch message envelopes */
  unix_header,			/* fetch message header */
  unix_text,			/* fetch message text */
  NIL,				/* fetch partial message text */
  NIL,				/* unique identifier */
  NIL,				/* message number */
  NIL,				/* modify flags */
  unix_flagmsg,			/* per-message modify flags */
  NIL,				/* search for message based on criteria */
  NIL,				/* sort messages */
  NIL,				/* thread messages */
  unix_ping,			/* ping mailbox to see if still alive */
  unix_check,			/* check for new messages */
  unix_expunge,			/* expunge deleted messages */
  unix_copy,			/* copy messages to another mailbox */
  unix_append,			/* append string message to mailbox */
  NIL				/* garbage collect stream */
};

				/* prototype stream */
MAILSTREAM unixproto = {&unixdriver};

				/* driver parameters */
static long unix_fromwidget = T;

/* UNIX mail validate mailbox
 * Accepts: mailbox name
 * Returns: our driver if name is valid, NIL otherwise
 */

DRIVER *unix_valid (char *name)
{
  int fd;
  DRIVER *ret = NIL;
  char *t,file[MAILTMPLEN];
  struct stat sbuf;
  time_t tp[2];
  errno = EINVAL;		/* assume invalid argument */
				/* must be non-empty file */
  if ((t = dummy_file (file,name)) && !stat (t,&sbuf)) {
    if (!sbuf.st_size)errno = 0;/* empty file */
    else if ((fd = open (file,O_RDONLY,NIL)) >= 0) {
				/* OK if mailbox format good */
      if (unix_isvalid_fd (fd)) ret = &unixdriver;
      else errno = -1;		/* invalid format */
      close (fd);		/* close the file */
      tp[0] = sbuf.st_atime;	/* preserve atime and mtime */
      tp[1] = sbuf.st_mtime;
      utime (file,tp);		/* set the times */
    }
  }
				/* in case INBOX but not unix format */
  else if ((errno == ENOENT) && ((name[0] == 'I') || (name[0] == 'i')) &&
	   ((name[1] == 'N') || (name[1] == 'n')) &&
	   ((name[2] == 'B') || (name[2] == 'b')) &&
	   ((name[3] == 'O') || (name[3] == 'o')) &&
	   ((name[4] == 'X') || (name[4] == 'x')) && !name[5]) errno = -1;
  return ret;			/* return what we should */
}

/* UNIX mail test for valid mailbox
 * Accepts: file descriptor
 *	    scratch buffer
 * Returns: T if valid, NIL otherwise
 */

long unix_isvalid_fd (int fd)
{
  int zn;
  int ret = NIL;
  char tmp[MAILTMPLEN],*s,*t,c = '\n';
  memset (tmp,'\0',MAILTMPLEN);
  if (read (fd,tmp,MAILTMPLEN-1) >= 0) {
    for (s = tmp; (*s == '\r') || (*s == '\n') || (*s == ' ') || (*s == '\t');)
      c = *s++;
    if (c == '\n') VALID (s,t,ret,zn);
  }
  return ret;			/* return what we should */
}


/* UNIX manipulate driver parameters
 * Accepts: function code
 *	    function-dependent value
 * Returns: function-dependent return value
 */

void *unix_parameters (long function,void *value)
{
  switch ((int) function) {
  case SET_FROMWIDGET:
    unix_fromwidget = (long) value;
    break;
  case GET_FROMWIDGET:
    value = (void *) unix_fromwidget;
    break;
  default:
    value = NIL;		/* error case */
    break;
  }
  return value;
}

/* UNIX mail scan mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 *	    string to scan
 */

void unix_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents)
{
  if (stream) dummy_scan (NIL,ref,pat,contents);
}


/* UNIX mail list mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void unix_list (MAILSTREAM *stream,char *ref,char *pat)
{
  if (stream) dummy_list (NIL,ref,pat);
}


/* UNIX mail list subscribed mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void unix_lsub (MAILSTREAM *stream,char *ref,char *pat)
{
  if (stream) dummy_lsub (NIL,ref,pat);
}

/* UNIX mail create mailbox
 * Accepts: MAIL stream
 *	    mailbox name to create
 * Returns: T on success, NIL on failure
 */

long unix_create (MAILSTREAM *stream,char *mailbox)
{
  char *s,mbx[MAILTMPLEN],tmp[MAILTMPLEN];
  long ret = NIL;
  int i,fd;
  time_t ti = time (0);
  if (!(s = dummy_file (mbx,mailbox))) {
    sprintf (tmp,"Can't create %.80s: invalid name",mailbox);
    mm_log (tmp,ERROR);
  }
				/* create underlying file */
  else if (dummy_create_path (stream,s)) {
				/* done if made directory */
    if ((s = strrchr (s,'/')) && !s[1]) return T;
    if ((fd = open (mbx,O_WRONLY,
		    (int) mail_parameters (NIL,GET_MBXPROTECTION,NIL))) < 0) {
      sprintf (tmp,"Can't reopen mailbox node %.80s: %s",mbx,strerror (errno));
      mm_log (tmp,ERROR);
      unlink (mbx);		/* delete the file */
    }
				/* in case a whiner with no life */
    else if (mail_parameters (NIL,GET_USERHASNOLIFE,NIL)) ret = T;
    else {			/* initialize header */
      memset (tmp,'\0',MAILTMPLEN);
      sprintf (tmp,"From %s %sDate: ",pseudo_from,ctime (&ti));
      rfc822_fixed_date (s = tmp + strlen (tmp));
				/* write the pseudo-header */
      sprintf (s += strlen (s),
	       "\nFrom: %s <%s@%s>\nSubject: %s\nX-IMAP: %010lu 0000000000",
	       pseudo_name,pseudo_from,mylocalhost (),pseudo_subject,
	       (unsigned long) ti);
      for (i = 0; i < NUSERFLAGS; ++i) if (default_user_flag (i))
	sprintf (s += strlen (s)," %s",default_user_flag (i));
      sprintf (s += strlen (s),"\nStatus: RO\n\n%s\n\n",pseudo_msg);
      if ((write (fd,tmp,strlen (tmp)) < 0) || close (fd)) {
	sprintf (tmp,"Can't initialize mailbox node %.80s: %s",mbx,
		 strerror (errno));
	mm_log (tmp,ERROR);
	unlink (mbx);		/* delete the file */
      }
      else ret = T;		/* success */
    }
    close (fd);			/* close file, set proper protections */
  }
  return ret ? set_mbx_protections (mailbox,mbx) : NIL;
}


/* UNIX mail delete mailbox
 * Accepts: MAIL stream
 *	    mailbox name to delete
 * Returns: T on success, NIL on failure
 */

long unix_delete (MAILSTREAM *stream,char *mailbox)
{
  return unix_rename (stream,mailbox,NIL);
}

/* UNIX mail rename mailbox
 * Accepts: MAIL stream
 *	    old mailbox name
 *	    new mailbox name (or NIL for delete)
 * Returns: T on success, NIL on failure
 */

long unix_rename (MAILSTREAM *stream,char *old,char *newname)
{
  long ret = NIL;
  char c,*s = NIL;
  char tmp[MAILTMPLEN],file[MAILTMPLEN],lock[MAILTMPLEN];
  DOTLOCK lockx;
  int fd,ld;
  long i;
  struct stat sbuf;
  mm_critical (stream);		/* get the c-client lock */
  if (newname && !((s = dummy_file (tmp,newname)) && *s))
    sprintf (tmp,"Can't rename mailbox %.80s to %.80s: invalid name",
	     old,newname);
				/* lock out other c-clients */
  else if ((ld = lockname (lock,dummy_file (file,old),LOCK_EX|LOCK_NB,&i)) < 0)
    sprintf (tmp,"Mailbox %.80s is in use by another process",old);
  else {
    if ((fd = unix_lock (file,O_RDWR,S_IREAD|S_IWRITE,&lockx,LOCK_EX)) < 0)
      sprintf (tmp,"Can't lock mailbox %.80s: %s",old,strerror (errno));
    else {
      if (newname) {		/* want rename? */
				/* found superior to destination name? */
	if (s = strrchr (s,'/')) {
	  c = *++s;		/* remember first character of inferior */
	  *s = '\0';		/* tie off to get just superior */
				/* name doesn't exist, create it */
	  if ((stat (tmp,&sbuf) || ((sbuf.st_mode & S_IFMT) != S_IFDIR)) &&
	      !dummy_create (stream,tmp)) {
	    unix_unlock (fd,NIL,&lockx);
	    unix_unlock (ld,NIL,NIL);
	    unlink (lock);
	    mm_nocritical (stream);
	    return ret;		/* return success or failure */
	  }
	  *s = c;		/* restore full name */
	}
	if (rename (file,tmp))
	  sprintf (tmp,"Can't rename mailbox %.80s to %.80s: %s",old,newname,
		   strerror (errno));
	else ret = T;		/* set success */
      }
      else if (unlink (file))
	sprintf (tmp,"Can't delete mailbox %.80s: %s",old,strerror (errno));
      else ret = T;		/* set success */
      unix_unlock (fd,NIL,&lockx);
    }
    unix_unlock (ld,NIL,NIL);	/* flush the lock */
    unlink (lock);
  }
  mm_nocritical (stream);	/* no longer critical */
  if (!ret) mm_log (tmp,ERROR);	/* log error */
  return ret;			/* return success or failure */
}

/* UNIX mail open
 * Accepts: Stream to open
 * Returns: Stream on success, NIL on failure
 */

MAILSTREAM *unix_open (MAILSTREAM *stream)
{
  long i;
  int fd;
  char tmp[MAILTMPLEN];
  DOTLOCK lock;
  long retry;
				/* return prototype for OP_PROTOTYPE call */
  if (!stream) return user_flags (&unixproto);
  retry = stream->silent ? 1 : KODRETRY;
  if (stream->local) fatal ("unix recycle stream");
  stream->local = memset (fs_get (sizeof (UNIXLOCAL)),0,sizeof (UNIXLOCAL));
				/* note if an INBOX or not */
  stream->inbox = !strcmp (ucase (strcpy (tmp,stream->mailbox)),"INBOX");
				/* canonicalize the stream mailbox name */
  dummy_file (tmp,stream->mailbox);
				/* flush old name */
  fs_give ((void **) &stream->mailbox);
				/* save canonical name */
  stream->mailbox = cpystr (tmp);
  LOCAL->fd = LOCAL->ld = -1;	/* no file or state locking yet */
  LOCAL->buf = (char *) fs_get ((LOCAL->buflen = CHUNK) + 1);
  stream->sequence++;		/* bump sequence number */

				/* make lock for read/write access */
  if (!stream->rdonly) while (retry) {
				/* try to lock file */
    if ((fd = lockname (tmp,stream->mailbox,LOCK_EX|LOCK_NB,&i)) < 0) {
      if (retry-- == KODRETRY) {/* no, first time through? */
	if (i) {		/* learned the other guy's PID? */
	  kill ((int) i,SIGUSR2);
	  sprintf (tmp,"Trying to get mailbox lock from process %ld",i);
	  mm_log (tmp,WARN);
	}
	else retry = 0;		/* give up */
      }
      if (!stream->silent) {	/* nothing if silent stream */
	if (retry) sleep (1);	/* wait a second before trying again */
	else mm_log ("Mailbox is open by another process, access is readonly",
		     WARN);
      }
    }
    else {			/* got the lock, nobody else can alter state */
      LOCAL->ld = fd;		/* note lock's fd and name */
      LOCAL->lname = cpystr (tmp);
				/* make sure mode OK (don't use fchmod()) */
      chmod (LOCAL->lname,(int) mail_parameters (NIL,GET_LOCKPROTECTION,NIL));
      if (stream->silent) i = 0;/* silent streams won't accept KOD */
      else {			/* note our PID in the lock */
	sprintf (tmp,"%d",getpid ());
	write (fd,tmp,(i = strlen (tmp))+1);
      }
      ftruncate (fd,i);		/* make sure tied off */
      fsync (fd);		/* make sure it's available */
      retry = 0;		/* no more need to try */
    }
  }

				/* parse mailbox */
  stream->nmsgs = stream->recent = 0;
				/* will we be able to get write access? */
  if ((LOCAL->ld >= 0) && access (stream->mailbox,W_OK) && (errno == EACCES)) {
    mm_log ("Can't get write access to mailbox, access is readonly",WARN);
    flock (LOCAL->ld,LOCK_UN);	/* release the lock */
    close (LOCAL->ld);		/* close the lock file */
    LOCAL->ld = -1;		/* no more lock fd */
    unlink (LOCAL->lname);	/* delete it */
  }
				/* reset UID validity */
  stream->uid_validity = stream->uid_last = 0;
  if (stream->silent && !stream->rdonly && (LOCAL->ld < 0))
    unix_abort (stream);	/* abort if can't get RW silent stream */
				/* parse mailbox */
  else if (unix_parse (stream,&lock,LOCK_SH)) {
    unix_unlock (LOCAL->fd,stream,&lock);
    mail_unlock (stream);
    mm_nocritical (stream);	/* done with critical */
  }
  if (!LOCAL) return NIL;	/* failure if stream died */
				/* make sure upper level knows readonly */
  stream->rdonly = (LOCAL->ld < 0);
				/* notify about empty mailbox */
  if (!(stream->nmsgs || stream->silent)) mm_log ("Mailbox is empty",NIL);
  if (!stream->rdonly) {	/* flags stick if readwrite */
    stream->perm_seen = stream->perm_deleted =
      stream->perm_flagged = stream->perm_answered = stream->perm_draft = T;
    if (!stream->uid_nosticky) {/* users with lives get permanent keywords */
      stream->perm_user_flags = 0xffffffff;
				/* and maybe can create them too! */
      stream->kwd_create = stream->user_flags[NUSERFLAGS-1] ? NIL : T;
    }
  }
  return stream;		/* return stream alive to caller */
}


/* UNIX mail close
 * Accepts: MAIL stream
 *	    close options
 */

void unix_close (MAILSTREAM *stream,long options)
{
  int silent = stream->silent;
  stream->silent = T;		/* go silent */
				/* expunge if requested */
  if (options & CL_EXPUNGE) unix_expunge (stream);
				/* else dump final checkpoint */
  else if (LOCAL->dirty) unix_check (stream);
  stream->silent = silent;	/* restore old silence state */
  unix_abort (stream);		/* now punt the file and local data */
}

/* UNIX mail fetch message header
 * Accepts: MAIL stream
 *	    message # to fetch
 *	    pointer to returned header text length
 *	    option flags
 * Returns: message header in RFC822 format
 */

				/* lines to filter from header */
static STRINGLIST *unix_hlines = NIL;

char *unix_header (MAILSTREAM *stream,unsigned long msgno,
		   unsigned long *length,long flags)
{
  MESSAGECACHE *elt;
  char *s;
  *length = 0;			/* default to empty */
  if (flags & FT_UID) return "";/* UID call "impossible" */
  elt = mail_elt (stream,msgno);/* get cache */
  if (!unix_hlines) {		/* once only code */
    STRINGLIST *lines = unix_hlines = mail_newstringlist ();
    lines->text.size = strlen ((char *) (lines->text.data =
					 (unsigned char *) "Status"));
    lines = lines->next = mail_newstringlist ();
    lines->text.size = strlen ((char *) (lines->text.data =
					 (unsigned char *) "X-Status"));
    lines = lines->next = mail_newstringlist ();
    lines->text.size = strlen ((char *) (lines->text.data =
					 (unsigned char *) "X-Keywords"));
    lines = lines->next = mail_newstringlist ();
    lines->text.size = strlen ((char *) (lines->text.data =
					 (unsigned char *) "X-UID"));
  }
				/* go to header position */
  lseek (LOCAL->fd,elt->private.special.offset +
	 elt->private.msg.header.offset,L_SET);

  if (flags & FT_INTERNAL) {	/* initial data OK? */
    if (elt->private.msg.header.text.size > LOCAL->buflen) {
      fs_give ((void **) &LOCAL->buf);
      LOCAL->buf = (char *) fs_get ((LOCAL->buflen =
				     elt->private.msg.header.text.size) + 1);
    }
				/* read message */
    read (LOCAL->fd,LOCAL->buf,elt->private.msg.header.text.size);
				/* got text, tie off string */
    LOCAL->buf[*length = elt->private.msg.header.text.size] = '\0';
				/* squeeze out CRs (in case from PC) */
    if (s = strchr (LOCAL->buf,'\r')) {
      char *t,*tl;
      for (t = s,tl = LOCAL->buf + *length; t <= tl; t++)
	if ((*t != '\r') || (t[1] != '\n')) *s++ = *t;
				/* adjust length */
      LOCAL->buf[*length = s - LOCAL->buf - 1] = '\0';
    }
  }
  else {			/* need to make a CRLF version */
    read (LOCAL->fd,s = (char *) fs_get (elt->private.msg.header.text.size+1),
	  elt->private.msg.header.text.size);
				/* tie off string, and convert to CRLF */
    s[elt->private.msg.header.text.size] = '\0';
    *length = strcrlfcpy (&LOCAL->buf,&LOCAL->buflen,s,
			  elt->private.msg.header.text.size);
    fs_give ((void **) &s);	/* free readin buffer */
  }
  *length = mail_filter (LOCAL->buf,*length,unix_hlines,FT_NOT);
  return LOCAL->buf;		/* return processed copy */
}

/* UNIX mail fetch message text
 * Accepts: MAIL stream
 *	    message # to fetch
 *	    pointer to returned stringstruct
 *	    option flags
 * Returns: T on success, NIL if failure
 */

long unix_text (MAILSTREAM *stream,unsigned long msgno,STRING *bs,long flags)
{
  char *s;
  unsigned long i;
  MESSAGECACHE *elt;
				/* UID call "impossible" */
  if (flags & FT_UID) return NIL;
  elt = mail_elt (stream,msgno);/* get cache element */
				/* if message not seen */
  if (!(flags & FT_PEEK) && !elt->seen) {
				/* mark message seen and dirty */
    elt->seen = elt->private.dirty = LOCAL->dirty = T;
    mm_flags (stream,msgno);
  }
  s = unix_text_work (stream,elt,&i,flags);
  INIT (bs,mail_string,s,i);	/* set up stringstruct */
  return T;			/* success */
}

/* UNIX mail fetch message text worker routine
 * Accepts: MAIL stream
 *	    message cache element
 *	    pointer to returned header text length
 *	    option flags
 */

char *unix_text_work (MAILSTREAM *stream,MESSAGECACHE *elt,
		      unsigned long *length,long flags)
{
  FDDATA d;
  STRING bs;
  char *s,tmp[CHUNK];
				/* go to text position */
  lseek (LOCAL->fd,elt->private.special.offset +
	 elt->private.msg.text.offset,L_SET);
  if (flags & FT_INTERNAL) {	/* initial data OK? */
    if (elt->private.msg.text.text.size > LOCAL->buflen) {
      fs_give ((void **) &LOCAL->buf);
      LOCAL->buf = (char *) fs_get ((LOCAL->buflen =
				     elt->private.msg.text.text.size) + 1);
    }
				/* read message */
    read (LOCAL->fd,LOCAL->buf,elt->private.msg.text.text.size);
				/* got text, tie off string */
    LOCAL->buf[*length = elt->private.msg.text.text.size] = '\0';
				/* squeeze out CRs (in case from PC) */
    if (s = strchr (LOCAL->buf,'\r')) {
      char *t,*tl;
      for (t = s,tl = LOCAL->buf + *length; t <= tl; t++)
	if ((*t != '\r') || (t[1] != '\n')) *s++ = *t;
				/* adjust length */
      *length = s - LOCAL->buf - 1;
    }
  }

  else {			/* need to make a CRLF version */
    if (elt->rfc822_size > LOCAL->buflen) {
      /* excessively conservative, but the right thing is too hard to do */
      fs_give ((void **) &LOCAL->buf);
      LOCAL->buf = (char *) fs_get ((LOCAL->buflen = elt->rfc822_size) + 1);
    }
    d.fd = LOCAL->fd;		/* yes, set up file descriptor */
    d.pos = elt->private.special.offset + elt->private.msg.text.offset;
    d.chunk = tmp;		/* initial buffer chunk */
    d.chunksize = CHUNK;	/* file chunk size */
    INIT (&bs,fd_string,&d,elt->private.msg.text.text.size);
    for (s = LOCAL->buf; SIZE (&bs);) switch (CHR (&bs)) {
    case '\r':			/* carriage return seen */
      *s++ = SNX (&bs);		/* copy it and any succeeding LF */
      if (SIZE (&bs) && (CHR (&bs) == '\n')) *s++ = SNX (&bs);
      break;
    case '\n':
      *s++ = '\r';		/* insert a CR */
    default:
      *s++ = SNX (&bs);		/* copy characters */
    }
    *s = '\0';			/* tie off buffer */
    *length = s - LOCAL->buf;	/* calculate length */
  }
  return LOCAL->buf;
}

/* UNIX per-message modify flag
 * Accepts: MAIL stream
 *	    message cache element
 */

void unix_flagmsg (MAILSTREAM *stream,MESSAGECACHE *elt)
{
				/* only after finishing */
  if (elt->valid) elt->private.dirty = LOCAL->dirty = T;
}


/* UNIX mail ping mailbox
 * Accepts: MAIL stream
 * Returns: T if stream alive, else NIL
 */

long unix_ping (MAILSTREAM *stream)
{
  DOTLOCK lock;
  struct stat sbuf;
				/* big no-op if not readwrite */
  if (LOCAL && (LOCAL->ld >= 0) && !stream->lock) {
    if (stream->rdonly) {	/* does he want to give up readwrite? */
				/* checkpoint if we changed something */
      if (LOCAL->dirty) unix_check (stream);
      flock (LOCAL->ld,LOCK_UN);/* release readwrite lock */
      close (LOCAL->ld);	/* close the readwrite lock file */
      LOCAL->ld = -1;		/* no more readwrite lock fd */
      unlink (LOCAL->lname);	/* delete the readwrite lock file */
    }
    else {			/* get current mailbox size */
      if (LOCAL->fd >= 0) fstat (LOCAL->fd,&sbuf);
      else stat (stream->mailbox,&sbuf);
				/* parse if mailbox changed */
      if ((sbuf.st_size != LOCAL->filesize) &&
	  unix_parse (stream,&lock,LOCK_SH)) {
				/* unlock mailbox */
	unix_unlock (LOCAL->fd,stream,&lock);
	mail_unlock (stream);	/* and stream */
	mm_nocritical (stream);	/* done with critical */
      }
    }
  }
  return LOCAL ? LONGT : NIL;	/* return if still alive */
}

/* UNIX mail check mailbox
 * Accepts: MAIL stream
 */

void unix_check (MAILSTREAM *stream)
{
  DOTLOCK lock;
				/* parse and lock mailbox */
  if (LOCAL && (LOCAL->ld >= 0) && !stream->lock &&
      unix_parse (stream,&lock,LOCK_EX)) {
				/* any unsaved changes? */
    if (LOCAL->dirty && unix_rewrite (stream,NIL,&lock)) {
      if (!stream->silent) mm_log ("Checkpoint completed",NIL);
    }
				/* no checkpoint needed, just unlock */
    else unix_unlock (LOCAL->fd,stream,&lock);
    mail_unlock (stream);	/* unlock the stream */
    mm_nocritical (stream);	/* done with critical */
  }
}


/* UNIX mail expunge mailbox
 * Accepts: MAIL stream
 */

void unix_expunge (MAILSTREAM *stream)
{
  unsigned long i;
  DOTLOCK lock;
  char *msg = NIL;
				/* parse and lock mailbox */
  if (LOCAL && (LOCAL->ld >= 0) && !stream->lock &&
      unix_parse (stream,&lock,LOCK_EX)) {
				/* count expunged messages if not dirty */
    if (!LOCAL->dirty) for (i = 1; i <= stream->nmsgs; i++)
      if (mail_elt (stream,i)->deleted) LOCAL->dirty = T;
    if (!LOCAL->dirty) {	/* not dirty and no expunged messages */
      unix_unlock (LOCAL->fd,stream,&lock);
      msg = "No messages deleted, so no update needed";
    }
    else if (unix_rewrite (stream,&i,&lock)) {
      if (i) sprintf (msg = LOCAL->buf,"Expunged %lu messages",i);
      else msg = "Mailbox checkpointed, but no messages expunged";
    }
				/* rewrite failed */
    else unix_unlock (LOCAL->fd,stream,&lock);
    mail_unlock (stream);	/* unlock the stream */
    mm_nocritical (stream);	/* done with critical */
    if (msg && !stream->silent) mm_log (msg,NIL);
  }
  else if (!stream->silent) mm_log("Expunge ignored on readonly mailbox",WARN);
}

/* UNIX mail copy message(s)
 * Accepts: MAIL stream
 *	    sequence
 *	    destination mailbox
 *	    copy options
 * Returns: T if copy successful, else NIL
 */

long unix_copy (MAILSTREAM *stream,char *sequence,char *mailbox,long options)
{
  struct stat sbuf;
  int fd;
  char *s,file[MAILTMPLEN];
  DOTLOCK lock;
  time_t tp[2];
  unsigned long i,j;
  MESSAGECACHE *elt;
  long ret = T;
  mailproxycopy_t pc =
    (mailproxycopy_t) mail_parameters (stream,GET_MAILPROXYCOPY,NIL);
  if (!((options & CP_UID) ? mail_uid_sequence (stream,sequence) :
	mail_sequence (stream,sequence))) return NIL;
				/* make sure valid mailbox */
  if (!unix_valid (mailbox)) switch (errno) {
  case ENOENT:			/* no such file? */
    mm_notify (stream,"[TRYCREATE] Must create mailbox before copy",NIL);
    return NIL;
  case 0:			/* merely empty file? */
    break;
  case EINVAL:
    if (pc) return (*pc) (stream,sequence,mailbox,options);
    sprintf (LOCAL->buf,"Invalid UNIX-format mailbox name: %.80s",mailbox);
    mm_log (LOCAL->buf,ERROR);
    return NIL;
  default:
    if (pc) return (*pc) (stream,sequence,mailbox,options);
    sprintf (LOCAL->buf,"Not a UNIX-format mailbox: %.80s",mailbox);
    mm_log (LOCAL->buf,ERROR);
    return NIL;
  }
  LOCAL->buf[0] = '\0';
  mm_critical (stream);		/* go critical */
  if ((fd = unix_lock (dummy_file (file,mailbox),O_WRONLY|O_APPEND|O_CREAT,
		       S_IREAD|S_IWRITE,&lock,LOCK_EX)) < 0) {
    mm_nocritical (stream);	/* done with critical */
    sprintf (LOCAL->buf,"Can't open destination mailbox: %s",strerror (errno));
    mm_log (LOCAL->buf,ERROR);	/* log the error */
    return NIL;			/* failed */
  }
  fstat (fd,&sbuf);		/* get current file size */

				/* write all requested messages to mailbox */
  for (i = 1; ret && (i <= stream->nmsgs); i++)
    if ((elt = mail_elt (stream,i))->sequence) {
      lseek (LOCAL->fd,elt->private.special.offset,L_SET);
      read (LOCAL->fd,LOCAL->buf,elt->private.special.text.size);
      if (write (fd,LOCAL->buf,elt->private.special.text.size) < 0) ret = NIL;
      else {			/* internal header succeeded */
	s = unix_header (stream,i,&j,FT_INTERNAL);
				/* header size, sans trailing newline */
	if (j && (s[j - 2] == '\n')) j--;
	if (write (fd,s,j) < 0) ret = NIL;
	else {			/* message header succeeded */
	  j = unix_xstatus (stream,LOCAL->buf,elt,NIL);
	  if (write (fd,LOCAL->buf,j) < 0) ret = NIL;
	  else {		/* message status succeeded */
	    s = unix_text_work (stream,elt,&j,FT_INTERNAL);
	    if ((write (fd,s,j) < 0) || (write (fd,"\n",1) < 0)) ret = NIL;
	  }
	}
      }
    }
  if (!ret || fsync (fd)) {	/* force out the update */
    sprintf (LOCAL->buf,"Message copy failed: %s",strerror (errno));
    ftruncate (fd,sbuf.st_size);
    ret = NIL;
  }
  tp[0] = sbuf.st_atime;	/* preserve atime */
  tp[1] = time (0);		/* set mtime to now */
  utime (file,tp);		/* set the times */
  unix_unlock (fd,NIL,&lock);	/* unlock and close mailbox */
  mm_nocritical (stream);	/* release critical */
				/* log the error */
  if (!ret) mm_log (LOCAL->buf,ERROR);
				/* delete if requested message */
  else if (options & CP_MOVE) for (i = 1; i <= stream->nmsgs; i++)
    if ((elt = mail_elt (stream,i))->sequence)
      elt->deleted = elt->private.dirty = LOCAL->dirty = T;
  return ret;
}

/* UNIX mail append message from stringstruct
 * Accepts: MAIL stream
 *	    destination mailbox
 *	    initial flags
 *	    internal date
 *	    stringstruct of messages to append
 * Returns: T if append successful, else NIL
 */

#define BUFLEN 8*MAILTMPLEN

long unix_append (MAILSTREAM *stream,char *mailbox,char *flags,char *date,
		  STRING *message)
{
  MESSAGECACHE elt;
  struct stat sbuf;
  int fd,ti,zn;
  long f,i,ok;
  unsigned long j,n,uf,size;
  char c,*x,buf[BUFLEN],tmp[MAILTMPLEN],file[MAILTMPLEN];
  DOTLOCK lock;
  time_t tp[2];
				/* default stream to prototype */
  if (!stream) stream = user_flags (&unixproto);
				/* get flags */
  f = mail_parse_flags (stream,flags,&uf);
				/* parse date */
  if (!date) rfc822_date (date = tmp);
  if (!mail_parse_date (&elt,date)) {
    sprintf (buf,"Bad date in append: %.80s",date);
    mm_log (buf,ERROR);
    return NIL;
  }
				/* make sure valid mailbox */
  if (!unix_valid (mailbox)) switch (errno) {
  case ENOENT:			/* no such file? */
    if (((mailbox[0] == 'I') || (mailbox[0] == 'i')) &&
	((mailbox[1] == 'N') || (mailbox[1] == 'n')) &&
	((mailbox[2] == 'B') || (mailbox[2] == 'b')) &&
	((mailbox[3] == 'O') || (mailbox[3] == 'o')) &&
	((mailbox[4] == 'X') || (mailbox[4] == 'x')) && !mailbox[5])
      unix_create (NIL,"INBOX");
    else {
      mm_notify (stream,"[TRYCREATE] Must create mailbox before append",NIL);
      return NIL;
    }
				/* falls through */
  case 0:			/* INBOX ENOENT or empty file? */
    break;
  case EINVAL:
    sprintf (buf,"Invalid UNIX-format mailbox name: %.80s",mailbox);
    mm_log (buf,ERROR);
    return NIL;
  default:
    sprintf (buf,"Not a UNIX-format mailbox: %.80s",mailbox);
    mm_log (buf,ERROR);
    return NIL;
  }

  mm_critical (stream);		/* go critical */
  if ((fd = unix_lock (dummy_file (file,mailbox),O_WRONLY|O_APPEND|O_CREAT,
		       S_IREAD|S_IWRITE,&lock,LOCK_EX)) < 0) {
    mm_nocritical (stream);	/* done with critical */
    sprintf (buf,"Can't open append mailbox: %s",strerror (errno));
    mm_log (buf,ERROR);
    return NIL;
  }
  fstat (fd,&sbuf);		/* get current file size */
  sprintf (buf,"From %s@%s ",myusername (),mylocalhost ());
				/* user wants to suppress time zones? */
  if (mail_parameters (NIL,GET_NOTIMEZONES,NIL)) {
    time_t when = mail_longdate (&elt);
    strcat (buf,ctime (&when));
  }
				/* write the date given */
  else mail_cdate (buf + strlen (buf),&elt);
  sprintf (buf + strlen (buf),"Status: %s\nX-Status: %s%s%s%s\nX-Keywords:",
	   f&fSEEN ? "R" : "",f&fDELETED ? "D" : "",f&fFLAGGED ? "F" : "",
	   f&fANSWERED ? "A" : "",f&fDRAFT ? "T" : "");
  while (uf)			/* write user flags */
    sprintf(buf+strlen(buf)," %s",stream->user_flags[find_rightmost_bit(&uf)]);
  strcat (buf,"\n");		/* tie off flags */
				/* copy text, tossing out CRs */
  for (i = strlen (buf), ok = T, size = SIZE (message); ok && size; ) {
    for (j = 0, c = '\0'; size && (c != '\n') && (j < MAILTMPLEN); size--)
      if (((c = SNX (message)) != '\r') || !(SIZE (message)) ||
	  (CHR (message) != '\n')) tmp[j++] = c;
				/* possible "From " line? */
    if ((j > 4) && (tmp[0] == 'F') && (tmp[1] == 'r') && (tmp[2] == 'o') &&
	(tmp[3] == 'm') && (tmp[4] == ' ')) {
				/* see if need to write a widget */
      if (!(ti = unix_fromwidget || (c != '\n'))) VALID (tmp,x,ti,zn);
      if (ti) ok = unix_append_putc (fd,buf,&i,'>');
    }
				/* write the line */
    for (n = 0; ok && (n < j); n++) ok = unix_append_putc (fd,buf,&i,tmp[n]);
				/* handle pathologically long line */
    if (ok && (c != '\n') && size) do {
      if (((c = SNX (message)) != '\r') || !(SIZE (message)) ||
	  (CHR (message) != '\n')) ok = unix_append_putc (fd,buf,&i,c);
    } while (ok && --size && (c != '\n'));
  }
				/* write trailing newline */
  if (!(ok && (ok = unix_append_putc (fd,buf,&i,'\n') &&
	       (i ? (write (fd,buf,i) >= 0) : T) && !fsync (fd)))) {
    sprintf (buf,"Message append failed: %s",strerror (errno));
    mm_log (buf,ERROR);
    ftruncate (fd,sbuf.st_size);
  }
  tp[0] = sbuf.st_atime;	/* preserve atime */
  tp[1] = time (0);		/* set mtime to now */
  utime (file,tp);		/* set the times */
  unix_unlock (fd,NIL,&lock);	/* unlock and close mailbox */
  mm_nocritical (stream);	/* release critical */
  return ok;			/* return success */
}

/* UNIX mail append character
 * Accepts: file descriptor
 *	    output buffer
 *	    pointer to current size of output buffer
 *	    character to append
 * Returns: T if append successful, else NIL
 */

long unix_append_putc (int fd,char *s,long *i,char c)
{
  s[(*i)++] = c;
  if (*i == BUFLEN) {		/* dump if buffer filled */
    if (write (fd,s,*i) < 0) return NIL;
    *i = 0;			/* reset */
  }
  return T;
}

/* Internal routines */


/* UNIX mail abort stream
 * Accepts: MAIL stream
 */

void unix_abort (MAILSTREAM *stream)
{
  if (LOCAL) {			/* only if a file is open */
    if (LOCAL->fd >= 0) close (LOCAL->fd);
    if (LOCAL->ld >= 0) {	/* have a mailbox lock? */
      flock (LOCAL->ld,LOCK_UN);/* yes, release the lock */
      close (LOCAL->ld);	/* close the lock file */
      unlink (LOCAL->lname);	/* and delete it */
    }
    if (LOCAL->lname) fs_give ((void **) &LOCAL->lname);
				/* free local text buffers */
    if (LOCAL->buf) fs_give ((void **) &LOCAL->buf);
    if (LOCAL->line) fs_give ((void **) &LOCAL->line);
				/* nuke the local data */
    fs_give ((void **) &stream->local);
    stream->dtb = NIL;		/* log out the DTB */
  }
}

/* UNIX open and lock mailbox
 * Accepts: file name to open/lock
 *	    file open mode
 *	    destination buffer for lock file name
 *	    type of locking operation (LOCK_SH or LOCK_EX)
 */

int unix_lock (char *file,int flags,int mode,DOTLOCK *lock,int op)
{
  int fd;
  blocknotify_t bn = (blocknotify_t) mail_parameters (NIL,GET_BLOCKNOTIFY,NIL);
  (*bn) (BLOCK_FILELOCK,NIL);
				/* open file */
  if ((fd = open (file,flags,mode)) >= 0) {
    flock (fd,op);		/* lock the file */
    dotlock_lock (file,lock,fd);/* make a dot lock file */
  }
  (*bn) (BLOCK_NONE,NIL);
  return fd;
}


/* UNIX unlock and close mailbox
 * Accepts: file descriptor
 *	    (optional) mailbox stream to check atime/mtime
 *	    (optional) lock file name
 */

void unix_unlock (int fd,MAILSTREAM *stream,DOTLOCK *lock)
{
  struct stat sbuf;
  time_t tp[2];
  fstat (fd,&sbuf);		/* get file times */
				/* if stream and csh would think new mail */
  if (stream && (sbuf.st_atime <= sbuf.st_mtime)) {
    tp[0] = time (0);		/* set atime to now */
				/* set mtime to (now - 1) if necessary */
    tp[1] = tp[0] > sbuf.st_mtime ? sbuf.st_mtime : tp[0] - 1;
				/* set the times, note change */
    if (!utime (stream->mailbox,tp)) LOCAL->filetime = tp[1];
  }
  flock (fd,LOCK_UN);		/* release flock'ers */
  if (!stream) close (fd);	/* close the file if no stream */
  dotlock_unlock (lock);	/* flush the lock file if any */
}

/* UNIX mail parse and lock mailbox
 * Accepts: MAIL stream
 *	    space to write lock file name
 *	    type of locking operation
 * Returns: T if parse OK, critical & mailbox is locked shared; NIL if failure
 */

int unix_parse (MAILSTREAM *stream,DOTLOCK *lock,int op)
{
  int zn;
  unsigned long i,j,k,m;
  char c,*s,*t,*u,tmp[MAILTMPLEN],date[30];
  int ti = 0,pseudoseen = NIL;
  unsigned long nmsgs = stream->nmsgs;
  unsigned long prevuid = nmsgs ? mail_elt (stream,nmsgs)->private.uid : 0;
  unsigned long recent = stream->recent;
  unsigned long oldnmsgs = stream->nmsgs;
  short silent = stream->silent;
  struct stat sbuf;
  STRING bs;
  FDDATA d;
  MESSAGECACHE *elt;
  mail_lock (stream);		/* guard against recursion or pingers */
				/* toss out previous descriptor */
  if (LOCAL->fd >= 0) close (LOCAL->fd);
  mm_critical (stream);		/* open and lock mailbox (shared OK) */
  if ((LOCAL->fd = unix_lock (stream->mailbox,(LOCAL->ld >= 0) ?
			      O_RDWR : O_RDONLY,NIL,lock,op)) < 0) {
    sprintf (tmp,"Mailbox open failed, aborted: %s",strerror (errno));
    mm_log (tmp,ERROR);
    unix_abort (stream);
    mail_unlock (stream);
    mm_nocritical (stream);	/* done with critical */
    return NIL;
  }
  fstat (LOCAL->fd,&sbuf);	/* get status */
				/* validate change in size */
  if (sbuf.st_size < LOCAL->filesize) {
    sprintf (tmp,"Mailbox shrank from %lu to %lu bytes, aborted",
	     (unsigned long) LOCAL->filesize,(unsigned long) sbuf.st_size);
    mm_log (tmp,ERROR);		/* this is pretty bad */
    unix_unlock (LOCAL->fd,stream,lock);
    unix_abort (stream);
    mail_unlock (stream);
    mm_nocritical (stream);	/* done with critical */
    return NIL;
  }

				/* new data? */
  else if (i = sbuf.st_size - LOCAL->filesize) {
    d.fd = LOCAL->fd;		/* yes, set up file descriptor */
    d.pos = LOCAL->filesize;	/* get to that position in the file */
    d.chunk = LOCAL->buf;	/* initial buffer chunk */
    d.chunksize = CHUNK;	/* file chunk size */
    INIT (&bs,fd_string,&d,i);	/* initialize stringstruct */
				/* skip leading whitespace for broken MTAs */
    while (((c = CHR (&bs)) == '\n') || (c == '\r') ||
	   (c == ' ') || (c == '\t')) SNX (&bs);
    if (SIZE (&bs)) {		/* read new data */
				/* remember internal header position */
      j = LOCAL->filesize + GETPOS (&bs);
      s = unix_mbxline (stream,&bs,&i);
      t = NIL,zn = 0;
      if (i) VALID (s,t,ti,zn);	/* see if valid From line */
      if (!ti) {		/* someone pulled the rug from under us */
	sprintf(tmp,"Unexpected changes to mailbox (try restarting): %.20s",s);
	mm_log (tmp,ERROR);
	unix_unlock (LOCAL->fd,stream,lock);
	unix_abort (stream);
	mail_unlock (stream);
	mm_nocritical (stream);	/* done with critical */
	return NIL;
      }
      stream->silent = T;	/* quell main program new message events */
      do {			/* found a message */
				/* instantiate first new message */
	mail_exists (stream,++nmsgs);
	(elt = mail_elt (stream,nmsgs))->valid = T;
	recent++;		/* assume recent by default */
	elt->recent = T;
				/* note position/size of internal header */
	elt->private.special.offset = j;
	elt->private.msg.header.offset = elt->private.special.text.size = i;

				/* generate plausible IMAPish date string */
	date[2] = date[6] = date[20] = '-'; date[11] = ' ';
	date[14] = date[17] = ':';
				/* dd */
	date[0] = t[ti - 2]; date[1] = t[ti - 1];
				/* mmm */
	date[3] = t[ti - 6]; date[4] = t[ti - 5]; date[5] = t[ti - 4];
				/* hh */
	date[12] = t[ti + 1]; date[13] = t[ti + 2];
				/* mm */
	date[15] = t[ti + 4]; date[16] = t[ti + 5];
	if (t[ti += 6] == ':') {/* ss */
	  date[18] = t[++ti]; date[19] = t[++ti];
	  ti++;			/* move to space */
	}
	else date[18] = date[19] = '0';
				/* yy -- advance over timezone if necessary */
	if (zn == ti) ti += (((t[zn+1] == '+') || (t[zn+1] == '-')) ? 6 : 4);
	date[7] = t[ti + 1]; date[8] = t[ti + 2];
	date[9] = t[ti + 3]; date[10] = t[ti + 4];
				/* zzz */
	t = zn ? (t + zn + 1) : "LCL";
	date[21] = *t++; date[22] = *t++; date[23] = *t++;
	if ((date[21] != '+') && (date[21] != '-')) date[24] = '\0';
	else {			/* numeric time zone */
	  date[24] = *t++; date[25] = *t++;
	  date[26] = '\0'; date[20] = ' ';
	}
				/* set internal date */
	if (!mail_parse_date (elt,date)) {
	  sprintf (tmp,"Unable to parse internal date: %s",date);
	  mm_log (tmp,WARN);
	}

	do {			/* look for message body */
	  s = t = unix_mbxline (stream,&bs,&i);
	  if (i) switch (*s) {	/* check header lines */
	  case 'X':		/* possible X-???: line */
	    if (s[1] == '-') {	/* must be immediately followed by hyphen */
				/* X-Status: becomes Status: in S case */
	      if (s[2] == 'S' && s[3] == 't' && s[4] == 'a' && s[5] == 't' &&
		  s[6] == 'u' && s[7] == 's' && s[8] == ':') s += 2;
				/* possible X-Keywords */
	      else if (s[2] == 'K' && s[3] == 'e' && s[4] == 'y' &&
		       s[5] == 'w' && s[6] == 'o' && s[7] == 'r' &&
		       s[8] == 'd' && s[9] == 's' && s[10] == ':') {
		char uf[MAILTMPLEN];
		s += 11;	/* flush leading whitespace */
		while (*s && (*s != '\n') && (*s != '\r')) {
		  while (*s == ' ') s++;
				/* find end of keyword */
		  if (!(u = strpbrk (s," \n\r"))) u = s + strlen (s);
				/* got a keyword? */
		  if ((k = (u - s)) && (k < MAILTMPLEN)) {
				/* copy keyword */
		    strncpy (uf,s,k);
		    uf[k] = '\0';	/* make sure tied off */
		    ucase (uf);	/* coerce upper case */
		    for (j = 0; (j<NUSERFLAGS) && stream->user_flags[j]; ++j)
		      if (!strcmp(uf,
				  ucase (strcpy(tmp,stream->user_flags[j])))) {
			elt->user_flags |= ((long) 1) << j;
			break;
		      }
				/* need to create it? */
		    if (!stream->rdonly && (j < NUSERFLAGS) &&
			!stream->user_flags[j]) {
				/* set the bit */
		      *uf |= 1 << j;
		      stream->user_flags[j] = (char *) fs_get (k + 1);
		      strncpy (stream->user_flags[j],s,k);
		      stream->user_flags[j][k] = '\0';
				/* if now out of user flags */
		      if (j == NUSERFLAGS - 1) stream->kwd_create = NIL;
		    }
		  }
		  s = u;	/* advance to next keyword */
		}
		break;
	      }

				/* possible X-IMAP */
	      else if ((nmsgs == 1) && !stream->uid_validity &&
		       (s[2] == 'I') && (s[3] == 'M') && (s[4] == 'A') &&
		       (s[5] == 'P') && (s[6] == ':')) {
		s += 7;		/* advance to data */
				/* flush whitespace */
		while (*s == ' ') s++;
		j = 0;		/* slurp UID validity */
				/* found a digit? */
		while (isdigit (*s)) {
		  j *= 10;	/* yes, add it in */
		  j += *s++ - '0';
		}
		if (!j) break;	/* punt if invalid UID validity */
		stream->uid_validity = j;
				/* flush whitespace */
		while (*s == ' ') s++;
				/* must have UID last too */
		if (isdigit (*s)) {
		  j = 0;	/* slurp UID last */
		  while (isdigit (*s)) {
		    j *= 10;	/* yes, add it in */
		    j += *s++ - '0';
		  }
		  stream->uid_last = j;
				/* process keywords */
		  for (j = 0; (*s != '\n') && (*s != '\r'); j++) {
				/* flush leading whitespace */
		    while (*s == ' ') s++;
		    u = strpbrk (s," \n\r");
				/* got a keyword? */
		    if ((k = (u - s)) && j < NUSERFLAGS) {
		      if (stream->user_flags[j])
			fs_give ((void **) &stream->user_flags[j]);
		      stream->user_flags[j] = (char *) fs_get (k + 1);
		      strncpy (stream->user_flags[j],s,k);
		      stream->user_flags[j][k] = '\0';
		    }
		    s = u;	/* advance to next keyword */
		  }
				/* pseudo-header seen */
		  pseudoseen = T;
		}
		break;
	      }

				/* possible X-UID */
	      else if (s[2] == 'U' && s[3] == 'I' && s[4] == 'D' &&
		       s[5] == ':') {
				/* only believe if have a UID validity */
		if (stream->uid_validity && (nmsgs > 1)) {
		  s += 6;	/* advance to UID value */
				/* flush whitespace */
		  while (*s == ' ') s++;
		  j = 0;
				/* found a digit? */
		  while (isdigit (*s)) {
		    j *= 10;	/* yes, add it in */
		    j += *s++ - '0';
		  }
				/* flush remainder of line */
		  while (*s != '\n') s++;
				/* make sure not duplicated */
		  if (elt->private.uid)
		    sprintf (tmp,"Message %lu UID %lu already has UID %lu",
			     pseudoseen ? elt->msgno - 1 : elt->msgno,
			     j,elt->private.uid);
				/* make sure UID doesn't go backwards */
		  else if (j <= prevuid)
		    sprintf (tmp,"Message %lu UID %lu less than %lu",
			     pseudoseen ? elt->msgno - 1 : elt->msgno,
			     j,prevuid + 1);
				/* or skip by mailbox's recorded last */
		  else if (j > stream->uid_last)
		    sprintf (tmp,"Message %lu UID %lu greater than last %lu",
			     pseudoseen ? elt->msgno - 1 : elt->msgno,
			     j,stream->uid_last);
		  else {	/* normal UID case */
		    prevuid = elt->private.uid = j;
		    break;		/* exit this cruft */
		  }
		  mm_log (tmp,WARN);
				/* invalidate UID validity */
		  stream->uid_validity = 0;
		  elt->private.uid = 0;
		}
		break;
	      }
	    }
				/* otherwise fall into S case */

	  case 'S':		/* possible Status: line */
	    if (s[0] == 'S' && s[1] == 't' && s[2] == 'a' && s[3] == 't' &&
		s[4] == 'u' && s[5] == 's' && s[6] == ':') {
	      s += 6;		/* advance to status flags */
	      do switch (*s++) {/* parse flags */
	      case 'R':		/* message read */
		elt->seen = T;
		break;
	      case 'O':		/* message old */
		if (elt->recent) {
		  elt->recent = NIL;
		  recent--;	/* it really wasn't recent */
		}
		break;
	      case 'D':		/* message deleted */
		elt->deleted = T;
		break;
	      case 'F':		/* message flagged */
		elt->flagged = T;
		break;
	      case 'A':		/* message answered */
		elt->answered = T;
		break;
	      case 'T':		/* message is a draft */
		elt->draft = T;
		break;
	      default:		/* some other crap */
		break;
	      } while (*s && (*s != '\n') && (*s != '\r'));
	      break;		/* all done */
	    }
				/* otherwise fall into default case */
	  default:		/* ordinary header line */
				/* line length in LF format newline */
	    k =  i - (((i >= 2) && (s[i - 2] == '\r')) ? 1 : 0);
				/* "internal" header size */
	    elt->private.data += k;
				/* message size */
	    elt->rfc822_size += k + 1;
	    break;
	  }
	} while (i && (*t != '\n') && (*t != '\r'));
				/* "internal" header sans trailing newline */
	if (i) elt->private.data--;
				/* assign a UID if none found */
	if (((nmsgs > 1) || !pseudoseen) && !elt->private.uid) {
	  prevuid = elt->private.uid = ++stream->uid_last;
	  elt->private.dirty = T;
	}
	else elt->private.dirty = elt->recent;

				/* note size of header, location of text */
	elt->private.msg.header.text.size = 
	  (elt->private.msg.text.offset =
	   (LOCAL->filesize + GETPOS (&bs)) - elt->private.special.offset) -
	     elt->private.special.text.size;
	k = m = 0;		/* no previous line size yet */
				/* note current position */
	j = LOCAL->filesize + GETPOS (&bs);
	if (i) do {		/* look for next message */
	  s = unix_mbxline (stream,&bs,&i);
	  if (i) {		/* got new data? */
	    VALID (s,t,ti,zn);	/* yes, parse line */
	    if (!ti) {		/* not a header line, add it to message */
	      elt->rfc822_size += 
		k = i + (m = (((i < 2) || s[i - 2] != '\r') ? 1 : 0));
				/* update current position */
	      j = LOCAL->filesize + GETPOS (&bs);
	    }
	  }
	} while (i && !ti);	/* until found a header */
	elt->private.msg.text.text.size = j -
	  (elt->private.special.offset + elt->private.msg.text.offset);
	if (k == 2) {		/* last line was blank? */
	  elt->private.msg.text.text.size -= (m ? 1 : 2);
	  elt->rfc822_size -= 2;
	}
      } while (i);		/* until end of buffer */
      if (pseudoseen) {		/* flush pseudo-message if present */
				/* decrement recent count */
	if (mail_elt (stream,1)->recent) recent--;
				/* and the exists count */
	mail_exists (stream,nmsgs--);
	mail_expunged(stream,1);/* fake an expunge of that message */
      }
				/* need to start a new UID validity? */
      if (!stream->uid_validity) {
	stream->uid_validity = time (0);
				/* in case a whiner with no life */
	if (mail_parameters (NIL,GET_USERHASNOLIFE,NIL))
	  stream->uid_nosticky = T;
	else LOCAL->dirty = T;	/* make dirty to create pseudo-message */
      }
      stream->nmsgs = oldnmsgs;	/* whack it back down */
      stream->silent = silent;	/* restore old silent setting */
				/* notify upper level of new mailbox sizes */
      mail_exists (stream,nmsgs);
      mail_recent (stream,recent);
				/* mark dirty so O flags are set */
      if (recent) LOCAL->dirty = T;
    }
  }
				/* no change, don't babble if never got time */
  else if (LOCAL->filetime && LOCAL->filetime != sbuf.st_mtime)
    mm_log ("New mailbox modification time but apparently no changes",WARN);
				/* update parsed file size and time */
  LOCAL->filesize = sbuf.st_size;
  LOCAL->filetime = sbuf.st_mtime;
  return T;			/* return the winnage */
}

/* UNIX read line from mailbox
 * Accepts: mail stream
 *	    stringstruct
 *	    pointer to line size
 * Returns: pointer to input line
 */

char *unix_mbxline (MAILSTREAM *stream,STRING *bs,unsigned long *size)
{
  unsigned long i,j,k,m;
  char *s,*t,*te,p1[CHUNK];
  char *ret = "";
				/* flush old buffer */
  if (LOCAL->line) fs_give ((void **) &LOCAL->line);
				/* if buffer needs refreshing */
  if (!bs->cursize) SETPOS (bs,GETPOS (bs));
  if (SIZE (bs)) {		/* find newline */
				/* end of fast scan */
    te = (t = (s = bs->curpos) + bs->cursize) - 12;
    while (s < te) if ((*s++ == '\n') || (*s++ == '\n') || (*s++ == '\n') ||
		       (*s++ == '\n') || (*s++ == '\n') || (*s++ == '\n') ||
		       (*s++ == '\n') || (*s++ == '\n') || (*s++ == '\n') ||
		       (*s++ == '\n') || (*s++ == '\n') || (*s++ == '\n')) {
      --s;			/* back up */
      break;			/* exit loop */
    }
				/* final character-at-a-time scan */
    while ((s < t) && (*s != '\n')) ++s;
				/* difficult case if line spans buffer */
    if ((i = s - bs->curpos) == bs->cursize) {
      memcpy (p1,bs->curpos,i);	/* remember what we have so far */
				/* load next buffer */
      SETPOS (bs,k = GETPOS (bs) + i);
				/* end of fast scan */
      te = (t = (s = bs->curpos) + bs->cursize) - 12;
				/* fast scan in overlap buffer */
      while (s < te) if ((*s++ == '\n') || (*s++ == '\n') || (*s++ == '\n') ||
			 (*s++ == '\n') || (*s++ == '\n') || (*s++ == '\n') ||
			 (*s++ == '\n') || (*s++ == '\n') || (*s++ == '\n') ||
			 (*s++ == '\n') || (*s++ == '\n') || (*s++ == '\n')) {
	--s;			/* back up */
	break;			/* exit loop */
      }
				/* final character-at-a-time scan */
      while ((s < t) && (*s != '\n')) ++s;
				/* huge line? */
      if ((j = s - bs->curpos) == bs->cursize) {
	SETPOS (bs,GETPOS (bs) + j);
				/* look for end of line (s-l-o-w!!) */
	for (m = SIZE (bs); m && (SNX (bs) != '\n'); --m,++j);
	SETPOS (bs,k);		/* go back to where it started */
      }

				/* got size of data, make buffer for return */
      ret = LOCAL->line = (char *) fs_get (i + j + 2);
      memcpy (ret,p1,i);	/* copy first chunk */
      while (j) {		/* copy remainder */
	if (!bs->cursize) SETPOS (bs,GETPOS (bs));
	memcpy (ret + i,bs->curpos,k = min (j,bs->cursize));
	i += k;			/* account for this much read in */
	j -= k;
	bs->curpos += k;	/* increment new position */
	bs->cursize -= k;	/* eat that many bytes */
      }
      if (SIZE (bs)) SNX (bs);	/* skip over newline if one seen */
      ret[i++] = '\n';		/* make sure newline at end */
      ret[i] = '\0';		/* makes debugging easier */
    }
    else {			/* this is easy */
      ret = bs->curpos;		/* string it at this position */
      bs->curpos += ++i;	/* increment new position */
      bs->cursize -= i;		/* eat that many bytes */
    }
    *size = i;			/* return that to user */
  }
  else *size = 0;		/* end of data, return empty */
  return ret;
}

/* UNIX make pseudo-header
 * Accepts: MAIL stream
 *	    buffer to write pseudo-header
 * Returns: length of pseudo-header
 */

unsigned long unix_pseudo (MAILSTREAM *stream,char *hdr)
{
  int i;
  char *s,tmp[MAILTMPLEN];
  time_t now = time (0);
  rfc822_fixed_date (tmp);
  sprintf (hdr,"From %s %.24s\nDate: %s\nFrom: %s <%s@%.80s>\nSubject: %s\nMessage-ID: <%lu@%.80s>\nX-IMAP: %010lu %010lu",
	   pseudo_from,ctime (&now),
	   tmp,pseudo_name,pseudo_from,mylocalhost (),pseudo_subject,
	   (unsigned long) now,mylocalhost (),stream->uid_validity,
	   stream->uid_last);
  for (s = hdr,i = 0; i < NUSERFLAGS; ++i) if (stream->user_flags[i])
    sprintf (s += strlen (s)," %s",stream->user_flags[i]);
  sprintf (s += strlen (s),"\nStatus: RO\n\n%s\n\n",pseudo_msg);
  return strlen (hdr);		/* return header length */
}

/* UNIX make status string
 * Accepts: MAIL stream
 *	    destination string to write
 *	    message cache entry
 *	    non-zero flag to write UID as well
 * Returns: length of string
 */

unsigned long unix_xstatus (MAILSTREAM *stream,char *status,MESSAGECACHE *elt,
			    long flag)
{
  char *t;
  char *s = status;
  unsigned long n = elt->user_flags;
  /* This used to be an sprintf(), but thanks to certain cretinous C libraries
     with horribly slow implementations of sprintf() I had to change it to this
     mess.  At least it should be fast. */
  *s++ = 'S'; *s++ = 't'; *s++ = 'a'; *s++ = 't'; *s++ = 'u'; *s++ = 's';
  *s++ = ':'; *s++ = ' ';
  if (elt->seen) *s++ = 'R';
  *s++ = 'O'; *s++ = '\n';
  *s++ = 'X'; *s++ = '-'; *s++ = 'S'; *s++ = 't'; *s++ = 'a'; *s++ = 't';
  *s++ = 'u'; *s++ = 's'; *s++ = ':'; *s++ = ' ';
  if (elt->deleted) *s++ = 'D';
  if (elt->flagged) *s++ = 'F';
  if (elt->answered) *s++ = 'A';
  if (elt->draft) *s++ = 'T';
  *s++ = '\n';
  if (!stream->uid_nosticky) {	/* cretins with no life can't use this */
    *s++ = 'X'; *s++ = '-'; *s++ = 'K'; *s++ = 'e'; *s++ = 'y'; *s++ = 'w';
    *s++ = 'o'; *s++ = 'r'; *s++ = 'd'; *s++ = 's'; *s++ = ':';
    if (n) do {
      *s++ = ' ';
      for (t = stream->user_flags[find_rightmost_bit (&n)]; *t; *s++ = *t++);
    } while (n);
				/* pad X-Keywords to make size constant */
    else for (n = 50 - (s - status); n > 0; --n) *s++ = ' ';
    *s++ = '\n';
    if (flag) {			/* want to include UID? */
      char stack[64];
      char *p = stack;
      n = elt->private.uid;	/* push UID digits on the stack */
      do *p++ = (char) (n % 10) + '0';
      while (n /= 10);
      *s++ = 'X'; *s++ = '-'; *s++ = 'U'; *s++ = 'I'; *s++ = 'D'; *s++ = ':';
      *s++ = ' ';
				/* pop UID from stack */
      while (p > stack) *s++ = *--p;
      *s++ = '\n';
    }
  }
  *s++ = '\n'; *s = '\0';	/* end of extended message status */
  return s - status;		/* return size of resulting string */
}

/* Rewrite mailbox file
 * Accepts: MAIL stream, must be critical and locked
 *	    return pointer to number of expunged messages if want expunge
 *	    lock file name
 * Returns: T if success and mailbox unlocked, NIL if failure
 */

int unix_old_algorithm = NIL;	/* non-zero to use old algorithm */

long unix_rewrite (MAILSTREAM *stream,unsigned long *nexp,DOTLOCK *lock)
{
  time_t tp[2];
  unsigned long size = 0;
  unsigned long recent = stream->recent;
  long ret;
  if (nexp) *nexp = 0;		/* initially nothing expunged */
  if (ret = unix_old_algorithm ? /* rewrite using selected algorithm */
      unix_rewrite_old (stream,nexp,&size,&recent) :
      unix_rewrite_new (stream,nexp,&size,&recent)) {
				/* make sure tied off */
    ftruncate (LOCAL->fd,LOCAL->filesize = size);
    fsync (LOCAL->fd);		/* make sure the updates take */
    LOCAL->dirty = NIL;		/* no longer dirty */
  				/* notify upper level of new mailbox sizes */
    mail_exists (stream,stream->nmsgs);
    mail_recent (stream,recent);
				/* set atime to now, mtime a second earlier */
    tp[1] = (tp[0] = time (0)) - 1;
				/* set the times, note change */
    if (!utime (stream->mailbox,tp)) LOCAL->filetime = tp[1];
    close (LOCAL->fd);		/* close and reopen file */
    if ((LOCAL->fd = open (stream->mailbox,O_RDWR,NIL)) < 0) {
      sprintf (LOCAL->buf,"Mailbox open failed, aborted: %s",strerror (errno));
      mm_log (LOCAL->buf,ERROR);
      unix_abort (stream);
    }
    dotlock_unlock (lock);	/* flush the lock file */
  }
  return ret;			/* return state from algorithm */
}

/* Extend UNIX mailbox file
 * Accepts: MAIL stream
 *	    new desired size
 * Return: T if success, else NIL
 */

long unix_extend (MAILSTREAM *stream,unsigned long size)
{
  unsigned long i = (size > LOCAL->filesize) ? size - LOCAL->filesize : 0;
  if (i) {			/* does the mailbox need to grow? */
    if (i > LOCAL->buflen) {	/* make sure have enough space */
				/* this user won the lottery all right */
      fs_give ((void **) &LOCAL->buf);
      LOCAL->buf = (char *) fs_get ((LOCAL->buflen = i) + 1);
    }
    memset (LOCAL->buf,'\0',i);	/* get a block of nulls */
    while (T) {			/* until write successful or punt */
      lseek (LOCAL->fd,LOCAL->filesize,L_SET);
      if ((write (LOCAL->fd,LOCAL->buf,i) >= 0) && !fsync (LOCAL->fd)) break;
      else {
	long e = errno;		/* note error before doing ftruncate */
	ftruncate (LOCAL->fd,LOCAL->filesize);
	if (mm_diskerror (stream,e,NIL)) {
	  fsync (LOCAL->fd);	/* user chose to punt */
	  sprintf (LOCAL->buf,"Unable to extend mailbox: %s",strerror (e));
	  mm_log (LOCAL->buf,ERROR);
	  return NIL;
	}
      }
    }
  }
  return LONGT;
}

/* Rewrite mailbox file (new, fast, dangerous algorithm)
 * Accepts: MAIL stream, must be critical and locked
 *	    return pointer to number of expunged messages if want expunge
 *	    return pointer to size of mailbox
 *	    return pointer to number of recent messages
 * Returns: T if success, NIL if failure
 */

#define OVERFLOWBUFLEN 8192	/* initial overflow buffer length */

long unix_rewrite_new (MAILSTREAM *stream,unsigned long *nexp,
		       unsigned long *size,unsigned long *recent)
{
  MESSAGECACHE *elt;
  UNIXFILE f;
  char *s;
  unsigned long i,j;
				/* size of mailbox starts with pseudo */
  *size = stream->uid_nosticky ? 0 : unix_pseudo (stream,LOCAL->buf);
				/* calculate size of mailbox after rewrite */
  for (i = 1; i <= stream->nmsgs; i++)
    if (!(elt = mail_elt (stream,i))->deleted || !nexp) {
				/* add RFC822 size of this message */
      *size += elt->private.special.text.size + elt->private.data +
	unix_xstatus (stream,LOCAL->buf,elt,T) +
	  elt->private.msg.text.text.size + 1;
    }
				/* extend the file as necessary */
  if (!unix_extend (stream,*size)) return NIL;
  /* Set up buffered I/O file structure
   * curpos	current position being written through buffering
   * filepos	current position being written physically to the disk
   * bufpos	current position being written in the buffer
   * protect	current maximum position that can be written to the disk
   *		before buffering is forced
   * The code tries to buffer so that that disk is written in multiples of
   * OVERBLOWBUFLEN bytes.
   */
  f.stream = stream;		/* note mail stream */
  f.curpos = f.filepos = 0;	/* start of file */
  f.protect = stream->nmsgs ?	/* initial protection pointer */
    mail_elt (stream,1)->private.special.offset : 8192;
  f.bufpos = f.buf = (char *) fs_get (f.buflen = OVERFLOWBUFLEN);
  if (!stream->uid_nosticky)	/* write pseudo-header */
    unix_write (&f,LOCAL->buf,unix_pseudo (stream,LOCAL->buf));

				/* loop through all messages */
  for (i = 1; i <= stream->nmsgs;) {
    elt = mail_elt (stream,i);	/* get cache */
    if (nexp && elt->deleted) {	/* expunge this message? */
      if (elt->recent)--*recent;/* one less recent message */
      mail_expunged (stream,i);	/* notify upper levels */
      ++*nexp;			/* count up one more expunged message */
    }
    else {			/* preserve this message */
      i++;			/* advance to next message */
				/* need to rewrite message? */
      if (elt->private.dirty || (f.curpos != elt->private.special.offset) ||
	  (elt->private.msg.header.text.size !=
	   (elt->private.data + unix_xstatus (stream,LOCAL->buf,elt,T)))) {
	unsigned long newoffset = f.curpos;
				/* yes, seek to internal header */
	lseek (LOCAL->fd,elt->private.special.offset,L_SET);
	read (LOCAL->fd,LOCAL->buf,elt->private.special.text.size);
				/* see if need to squeeze out a CR */
	if (LOCAL->buf[elt->private.special.text.size - 2] == '\r') {
	  LOCAL->buf[--elt->private.special.text.size - 1] = '\n';
	  --*size;		/* squeezed out a CR from PC */
	}
				/* protection pointer moves to RFC822 header */
	f.protect = elt->private.special.offset +
	  elt->private.msg.header.offset;
				/* write internal header */
	unix_write (&f,LOCAL->buf,elt->private.special.text.size);
				/* get RFC822 header */
	s = unix_header (stream,elt->msgno,&j,FT_INTERNAL);
				/* in case this got decremented */
	elt->private.msg.header.offset = elt->private.special.text.size;
				/* header size, sans trailing newline */
	if ((j < 2) || (s[j - 2] == '\n')) j--;
	if (j != elt->private.data) fatal ("header size inconsistant");
				/* protection pointer moves to RFC822 text */
	f.protect = elt->private.special.offset + elt->private.msg.text.offset;
	unix_write (&f,s,j);	/* write RFC822 header */
				/* write status and UID */
	unix_write (&f,LOCAL->buf,j = unix_xstatus (stream,LOCAL->buf,elt,T));
				/* new file header size */
	elt->private.msg.header.text.size = elt->private.data + j;

				/* did text move? */
	if (f.curpos != f.protect) {
				/* get message text */
	  s = unix_text_work (stream,elt,&j,FT_INTERNAL);
				/* this can happen if CRs were squeezed */
	  if (j < elt->private.msg.text.text.size) {
				/* so fix up counts */
	    *size -= elt->private.msg.text.text.size - j;
	    elt->private.msg.text.text.size = j;
	  }
				/* can't happen it says here */
	  else if (j > elt->private.msg.text.text.size)
	    fatal ("text size inconsistant");
				/* new text offset, status/UID may change it */
	  elt->private.msg.text.offset = f.curpos - newoffset;
				/* protection pointer moves to next message */
	  f.protect = (i <= stream->nmsgs) ?
	    mail_elt (stream,i)->private.special.offset : (f.curpos + j + 1);
	  unix_write (&f,s,j);	/* write text */
				/* write trailing newline */
	  unix_write (&f,"\n",1);
	}
	else {			/* tie off header and status */
	  unix_write (&f,NIL,NIL);
	  f.curpos = f.protect =/* restart everything at end of message */
	    f.filepos += elt->private.msg.text.text.size + 1;
	}
				/* new internal header offset */
	elt->private.special.offset = newoffset;
	elt->private.dirty =NIL;/* message is now clean */
      }
      else {			/* no need to rewrite */
	unix_write (&f,NIL,NIL);/* tie off previous message if needed */
	f.curpos = f.protect =	/* restart everything at end of message */
	  f.filepos += elt->private.special.text.size +
	    elt->private.msg.header.text.size +
	      elt->private.msg.text.text.size + 1;
      }
    }
  }
  unix_write (&f,NIL,NIL);	/* tie off final message */
  if (*size != f.filepos) fatal ("file size inconsistant");
  fs_give ((void **) &f.buf);	/* free buffer */
  return LONGT;			/* looks good */
}

/* Write data to buffered file
 * Accepts: buffered file pointer
 *	    file data or NIL to indicate "flush buffer"
 *	    date size (ignored for "flush buffer")
 * Does not return until success
 */

void unix_write (UNIXFILE *f,char *buf,unsigned long size)
{
  unsigned long i,j,k;
  if (buf) {			/* doing buffered write? */
    i = f->bufpos - f->buf;	/* yes, get size of current buffer data */
				/* yes, have space in current buffer chunk? */
    if (j = i ? ((f->buflen - i) % OVERFLOWBUFLEN) : f->buflen) {
				/* yes, fill up buffer as much as we can */
      memcpy (f->bufpos,buf,k = min (j,size));
      f->bufpos += k;		/* new buffer position */
      f->curpos += k;		/* new current position */
      if (j -= k) return;	/* all done if still have buffer free space */
      buf += k;			/* full, get new unwritten data pointer */
      size -= k;		/* new data size */
      i += k;			/* new buffer data size */
    }
    /* This chunk of the buffer is full.  See if can make some space by
     * writing to the disk, if there's enough unprotected space to do so.
     * Try to fill out any unaligned chunk, along with any subsequent full
     * chunks that will fit in unprotected space.
     */
				/* any unprotected space we can write to? */
    if (j = min (i,f->protect - f->filepos)) {
				/* yes, filepos not at chunk boundary? */
      if ((k = f->filepos % OVERFLOWBUFLEN) && ((k = OVERFLOWBUFLEN - k) < j))
	j -= k;			/* yes, and can write out partial chunk */
      else k = 0;		/* no partial chunk to write */
				/* if at least a chunk free, write that too */
      if (j > OVERFLOWBUFLEN) k += j - (j % OVERFLOWBUFLEN);
      if (k) {			/* write data if there is anything we can */
	unix_phys_write (f,f->buf,k);
				/* slide buffer */
	if (i -= k) memmove (f->buf,f->buf + k,i);
	f->bufpos = f->buf + i;	/* new end of buffer */
      }
    }

    /* Have flushed the buffer as best as possible.  All done if no more
     * data to write.  Otherwise, if the buffer is empty AND if the unwritten
     * data is larger than a chunk AND the unprotected space is also larger
     * than a chunk, then write as many chunks as we can directly from the
     * data.  Buffer the rest, expanding the buffer as needed.
     */
    if (size) {			/* have more data that we need to buffer? */
				/* can write any of it to disk instead? */
      if ((f->bufpos == f->buf) && 
	  ((j = min (f->protect - f->filepos,size)) > OVERFLOWBUFLEN)) {
				/* write as much as we can right now */
	unix_phys_write (f,buf,j -= (j % OVERFLOWBUFLEN));
	buf += j;		/* new data pointer */
	size -= j;		/* new data size */
	f->curpos += j;		/* advance current pointer */
      }
      if (size) {		/* still have data that we need to buffer? */
				/* yes, need to expand the buffer? */
	if ((i = ((f->bufpos + size) - f->buf)) > f->buflen) {
				/* note current position in buffer */
	  j = f->bufpos - f->buf;
	  i += OVERFLOWBUFLEN;	/* yes, grow another chunk */
	  fs_resize ((void **) &f->buf,f->buflen = i - (i % OVERFLOWBUFLEN));
				/* in case buffer relocated */
	  f->bufpos = f->buf + j;
	}
				/* buffer remaining data */
	memcpy (f->bufpos,buf,size);
	f->bufpos += size;	/* new end of buffer */
	f->curpos += size;	/* advance current pointer */
      }
    }
  }
  else {			/* flush buffer to disk */
    unix_phys_write (f,f->buf,i = f->bufpos - f->buf);
    f->bufpos = f->buf;		/* reset buffer */
				/* update positions */
    f->curpos = f->protect = f->filepos;
  }
}

/* Physical disk write
 * Accepts: buffered file pointer
 *	    buffer address
 *	    buffer size
 * Does not return until success
 */

void unix_phys_write (UNIXFILE *f,char *buf,size_t size)
{
  MAILSTREAM *stream = f->stream;
				/* write data at desired position */
  while (size && ((lseek (LOCAL->fd,f->filepos,L_SET) < 0) ||
		  (write (LOCAL->fd,buf,size) < 0))) {
    int e;
    char tmp[MAILTMPLEN];
    sprintf (tmp,"Unable to write to mailbox: %s",strerror (e = errno));
    mm_log (tmp,ERROR);
    mm_diskerror (NIL,e,T);	/* serious problem, must retry */
  }
  f->filepos += size;		/* update file position */
}

/* Rewrite mailbox file (old, slow, safe algorithm)
 * Accepts: MAIL stream, must be critical and locked
 *	    return pointer to number of expunged messages if want expunge
 *	    return pointer to size of mailbox
 *	    return pointer to number of recent messages
 * Returns: T if success, NIL if failure
 */

long unix_rewrite_old (MAILSTREAM *stream,unsigned long *nexp,
		       unsigned long *size,unsigned long *recent)
{
  unsigned long i,j;
  int e,retry;
  struct stat sbuf;
  FILE *f;
  MESSAGECACHE *elt;
				/* open scratch file */
  if (!(f = tmpfile ())) return unix_punt_scratch (NIL);
  if (!(stream->uid_nosticky ||	/* write pseudo-header */
	unix_fwrite (f,LOCAL->buf,unix_pseudo (stream,LOCAL->buf),size)))
    return unix_punt_scratch (f);
  if (nexp) {			/* expunging */
    for (i = 1; i <= stream->nmsgs; i++)
      if (!(elt = mail_elt (stream,i))->deleted &&
	  !unix_write_message (f,stream,elt,size))
	return unix_punt_scratch (f);
  }
  else for (i = 1; i <= stream->nmsgs; i++)
    if (!unix_write_message (f,stream,mail_elt (stream,i),size))
      return unix_punt_scratch (f);
				/* write remaining data */
  if (fflush (f) || fstat (fileno (f),&sbuf)) return unix_punt_scratch (f);
  if (*size != sbuf.st_size) {	/* make damn sure stdio isn't lying */
    char tmp[MAILTMPLEN];
    sprintf (tmp,"Checkpoint file size mismatch (%lu != %lu)",
	     (unsigned long) size,(unsigned long) sbuf.st_size);
    mm_log (tmp,ERROR);
    fclose (f);			/* flush the output file */
    return NIL;
  }

				/* extend the file as necessary */
  if (!unix_extend (stream,*size)) return NIL;
				/* update the cache */
  for (i = 1; i <= stream->nmsgs;) {
    elt = mail_elt (stream,i);	/* get cache */
    if (nexp && elt->deleted) {	/* expunge this message? */
      if (elt->recent)--*recent;/* one less recent message */
      mail_expunged (stream,i);	/* notify upper levels */
      ++*nexp;			/* count up one more expunged message */
    }
    else {			/* update file pointers from kludgey places */
      elt->private.special.offset = elt->private.msg.full.offset;
      elt->private.msg.text.offset = elt->private.msg.full.text.size;
				/* in case header grew */
      elt->private.msg.header.text.size = elt->private.msg.text.offset -
	elt->private.msg.header.offset;
				/* stomp on these two kludges */
      elt->private.msg.full.offset = elt->private.msg.full.text.size = 0;
      i++;			/* preserved message */
    }
  }
  do {				/* restart point if failure */
    retry = NIL;		/* no need to retry yet */
    fseek (f,0,L_SET);		/* rewind files */
    lseek (LOCAL->fd,0,L_SET);
    for (i = *size; i; i -= j)
      if (!((j = fread (LOCAL->buf,1,min ((long) CHUNK,i),f)) &&
	    (write (LOCAL->fd,LOCAL->buf,j) >= 0))) {
	sprintf (LOCAL->buf,"Mailbox rewrite error: %s",strerror (e = errno));
	mm_notify (stream,LOCAL->buf,WARN);
	mm_diskerror (stream,e,T);
	retry = T;		/* must retry */
	break;
      }
  } while (retry);		/* in case need to retry */
  fclose (f);			/* finished with scratch file */
  return LONGT;
}

/* Write message (old algorithm)
 * Accepts: destination file
 *	    MAIL stream
 *	    message number
 *	    pointer to current filesize tally
 * Returns: T if success, NIL if failure
 */

long unix_write_message (FILE *f,MAILSTREAM *stream,MESSAGECACHE *elt,
			 unsigned long *size)
{
  char *s;
  unsigned long i;
				/* (kludge alert) note new message offset */
  elt->private.msg.full.offset = *size;
				/* internal header */
  lseek (LOCAL->fd,elt->private.special.offset,L_SET);
  read (LOCAL->fd,LOCAL->buf,elt->private.special.text.size);
  if (unix_fwrite (f,LOCAL->buf,elt->private.special.text.size,size)) {
				/* get header */
    s = unix_header (stream,elt->msgno,&i,FT_INTERNAL);
				/* header size, sans trailing newline */
    if (i && (s[i - 2] == '\n')) i--;
				/* write header */
    if (unix_fwrite (f,s,i,size) &&
	unix_fwrite (f,LOCAL->buf,unix_xstatus(stream,LOCAL->buf,elt,T),size)){
				/* (kludge alert) note new text offset */
      elt->private.msg.full.text.size = *size - elt->private.msg.full.offset;
				/* get text */
      s = unix_text_work (stream,elt,&i,FT_INTERNAL);
				/* write text and trailing newline */
      if (unix_fwrite (f,s,i,size) && unix_fwrite (f,"\n",1,size)) return T;
    }
  }
  return NIL;			/* failed */
}

/* Safely write buffer (old algorithm)
 * Accepts: destination file
 *	    buffer pointer
 *	    number of octets
 *	    pointer to current filesize tally
 * Returns: T if successful, NIL if failure
 */

long unix_fwrite (FILE *f,char *s,unsigned long i,unsigned long *size)
{
  unsigned long j;
  while (i && ((j = fwrite (s,1,i,f)) || (errno == EINTR))) {
    *size += j;
    s += j;
    i -= j;
  }
  return i ? NIL : T;		/* success if wrote all requested data */
}


/* Punt scratch file (old algorithm)
 * Accepts: file pointer
 * Returns: NIL, always
 */

long unix_punt_scratch (FILE *f)
{
  char tmp[MAILTMPLEN];
  sprintf (tmp,"Checkpoint file failure: %s",strerror (errno));
  mm_log (tmp,ERROR);
  if (f) fclose (f);		/* flush the output file */
  return NIL;
}
