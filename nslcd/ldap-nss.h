/*
   ldap-nss.c - main file for NSS interface
   This file was part of the nss_ldap library which has been
   forked into the nss-ldapd library.

   Copyright (C) 1997-2005 Luke Howard
   Copyright (C) 2006, 2007 West Consulting
   Copyright (C) 2006, 2007 Arthur de Jong

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301 USA
*/

#ifndef _LDAP_NSS_LDAP_LDAP_NSS_H
#define _LDAP_NSS_LDAP_LDAP_NSS_H

/* for glibc, use weak aliases to pthreads functions */
#ifdef HAVE_LIBC_LOCK_H
#include <libc-lock.h>
#elif defined(HAVE_BITS_LIBC_LOCK_H)
#include <bits/libc-lock.h>
#endif

#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif

#include <netdb.h>
#include <netinet/in.h>
#include <syslog.h>

#include <nss.h>
#include <ldap.h>

#include "ldap-schema.h"

#ifndef LDAP_FILT_MAXSIZ
#define LDAP_FILT_MAXSIZ 1024
#endif /* not LDAP_FILT_MAXSIZ */

#ifndef LDAPS_PORT
#define LDAPS_PORT 636
#endif /* not LDAPS_PORT */

#ifdef __GNUC__
#define alignof(ptr) __alignof__(ptr)
#elif defined(HAVE_ALIGNOF_H)
#include <alignof.h>
#else
#define alignof(ptr) (sizeof(char *))
#endif /* __GNUC__ */

#define align(ptr, blen, TYPE)\
  { \
      char *qtr = ptr; \
      ptr += alignof(TYPE) - 1; \
      ptr -= ((ptr - (char *)NULL) % alignof(TYPE)); \
      blen -= (ptr - qtr); \
  }

/* worst case */
#define bytesleft(ptr, blen, TYPE) \
  ( (blen < alignof(TYPE)) ? 0 : (blen - alignof(TYPE) + 1))

/* selectors for different maps */
enum ldap_map_selector
{
  LM_PASSWD,
  LM_SHADOW,
  LM_GROUP,
  LM_HOSTS,
  LM_SERVICES,
  LM_NETWORKS,
  LM_PROTOCOLS,
  LM_RPC,
  LM_ETHERS,
  LM_NETMASKS,
  LM_BOOTPARAMS,
  LM_ALIASES,
  LM_NETGROUP,
  LM_NONE
};

enum ldap_userpassword_selector
{
  LU_RFC2307_USERPASSWORD,
  LU_RFC3112_AUTHPASSWORD,
  LU_OTHER_PASSWORD
};

enum ldap_shadow_selector
{
  LS_RFC2307_SHADOW,
  LS_AD_SHADOW,
  LS_OTHER_SHADOW
};

/*
 * POSIX profile information (not used yet)
 * see draft-joslin-config-schema-00.txt
 */
struct ldap_service_search_descriptor
{
  /* search base, qualified */
  char *lsd_base;
  /* scope */
  int lsd_scope;
  /* filter */
  char *lsd_filter;
  /* next */
  struct ldap_service_search_descriptor *lsd_next;
};

#include "cfg.h"

#if defined(__GLIBC__) && __GLIBC_MINOR__ > 1
#else
#define ss_family sa_family
#endif /* __GLIBC__ */

enum ldap_session_state
{
  LS_UNINITIALIZED = -1,
  LS_INITIALIZED,
  LS_CONNECTED_TO_DSA
};

/*
 * convenient wrapper around pointer into global config list, and a
 * connection to an LDAP server.
 */
struct ldap_session
{
  /* the connection */
  LDAP *ls_conn;
  /* timestamp of last activity */
  time_t ls_timestamp;
  /* has session been connected? */
  enum ldap_session_state ls_state;
  /* index into ldc_uris: currently connected DSA */
  int ls_current_uri;
};

enum ldap_args_types
{
  LA_TYPE_STRING,
  LA_TYPE_NUMBER,
  LA_TYPE_STRING_AND_STRING,
  LA_TYPE_NUMBER_AND_STRING,
  LA_TYPE_TRIPLE,
  LA_TYPE_STRING_LIST_OR,
  LA_TYPE_STRING_LIST_AND,
  LA_TYPE_NONE
};

enum ldap_map_type
{
  MAP_ATTRIBUTE = 0,
  MAP_OBJECTCLASS,
  MAP_OVERRIDE,
  MAP_DEFAULT,
  MAP_ATTRIBUTE_REVERSE,
  MAP_OBJECTCLASS_REVERSE, /* XXX not used yet? */
  MAP_MAX = MAP_OBJECTCLASS_REVERSE
};

struct ldap_args
{
  enum ldap_args_types la_type;
  union
  {
    const char *la_string;
    long la_number;
    struct {
      /* for Solaris netgroup support */
      const char *host;
      const char *user;
      const char *domain;
    } la_triple;
    const char **la_string_list;
  }
  la_arg1;
  union
  {
    const char *la_string;
  }
  la_arg2;
  const char *la_base; /* override default base */
};

#define LA_INIT(q)                              do { \
                                                (q).la_type = LA_TYPE_STRING; \
                                                (q).la_arg1.la_string = NULL; \
                                                (q).la_arg2.la_string = NULL; \
                                                (q).la_base = NULL; \
                                                } while (0)
#define LA_TYPE(q)                              ((q).la_type)
#define LA_STRING(q)                            ((q).la_arg1.la_string)
#define LA_NUMBER(q)                            ((q).la_arg1.la_number)
#define LA_TRIPLE(q)                            ((q).la_arg1.la_triple)
#define LA_STRING_LIST(q)                       ((q).la_arg1.la_string_list)
#define LA_STRING2(q)                           ((q).la_arg2.la_string)
#define LA_BASE(q)                              ((q).la_base)

/*
 * the state consists of the desired attribute value or an offset into a list of
 * values for the desired attribute. This is necessary to support services.
 *
 * Be aware of the arbitary distinction between state and context. Context is
 * the enumeration state of a lookup subsystem (which may be per-subsystem,
 * or per-subsystem/per-thread, depending on the OS). State is the state
 * of a particular lookup, and is only concerned with resolving and enumerating
 * services. State is represented as instances of struct ldap_state; context as
 * instances of struct ent_context. The latter contains the former.
 */
struct ldap_state
{
  int ls_type;
  int ls_retry;
#define LS_TYPE_KEY     (0)
#define LS_TYPE_INDEX   (1)
  union
  {
    /* ls_key is the requested attribute value.
       ls_index is the desired offset into the value list.
     */
    const char *ls_key;
    int ls_index;
  }
  ls_info;
};

/*
 * thread specific context: result chain, and state data
 */
struct ent_context
{
  struct ldap_state ec_state;        /* eg. for services */
  int ec_msgid;                      /* message ID */
  LDAPMessage *ec_res;               /* result chain */
  struct ldap_service_search_descriptor *ec_sd;  /* current sd */
  struct berval *ec_cookie;          /* cookie for paged searches */
};

struct name_list
{
  char *name;
  struct name_list *next;
};

typedef enum nss_status (*parser_t) (LDAPMessage *, struct ldap_state *, void *,
                                char *, size_t);

typedef int (*NEWparser_t)(LDAPMessage *e,struct ldap_state *pvt,FILE *fp);

/*
 * Portable locking macro.
 */
#if defined(HAVE_THREAD_H)
#define NSS_LDAP_LOCK(m)                mutex_lock(&m)
#define NSS_LDAP_UNLOCK(m)              mutex_unlock(&m)
#define NSS_LDAP_DEFINE_LOCK(m)         static mutex_t m = DEFAULTMUTEX
#elif defined(HAVE_LIBC_LOCK_H) || defined(HAVE_BITS_LIBC_LOCK_H)
#define NSS_LDAP_LOCK(m)                __libc_lock_lock(m)
#define NSS_LDAP_UNLOCK(m)              __libc_lock_unlock(m)
#define NSS_LDAP_DEFINE_LOCK(m)         static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER
#elif defined(HAVE_PTHREAD_H)
#define NSS_LDAP_LOCK(m)               pthread_mutex_lock(&m)
#define NSS_LDAP_UNLOCK(m)             pthread_mutex_unlock(&m)
#define NSS_LDAP_DEFINE_LOCK(m)                static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER
#else
#define NSS_LDAP_LOCK(m)
#define NSS_LDAP_UNLOCK(m)
#define NSS_LDAP_DEFINE_LOCK(m)
#endif

/*
 * Acquire global nss_ldap lock and blocks SIGPIPE.
 * Generally this should only be done within ldap-nss.c.
 */
void _nss_ldap_enter (void);

/*
 * Release global nss_ldap lock and blocks SIGPIPE.
 * Generally this should only be done within ldap-nss.c.
 */
void _nss_ldap_leave (void);

/*
 * _nss_ldap_ent_context_init() is called for each getXXent() call
 * This will acquire the global mutex.
 */
struct ent_context *_nss_ldap_ent_context_init (struct ent_context **);

/*
 * _nss_ldap_ent_context_init_locked() has the same behaviour
 * as above, except it assumes that the caller has acquired
 * the lock
 */

struct ent_context *_nss_ldap_ent_context_init_locked (struct ent_context **);

/*
 * _nss_ldap_ent_context_release() is used to manually free a context
 */
void _nss_ldap_ent_context_release (struct ent_context *);

/*
 * these are helper functions for ldap-grp.c only on Solaris
 */
char **_nss_ldap_get_values (LDAPMessage * e, const char *attr);
char *_nss_ldap_get_dn (LDAPMessage * e);
LDAPMessage *_nss_ldap_first_entry (LDAPMessage * res);
LDAPMessage *_nss_ldap_next_entry (LDAPMessage * res);
char *_nss_ldap_first_attribute (LDAPMessage * entry, BerElement **berptr);
char *_nss_ldap_next_attribute (LDAPMessage * entry, BerElement *ber);

/*
 * Synchronous search cover (caller acquires lock).
 */
enum nss_status _nss_ldap_search_s (const struct ldap_args * args,   /* IN */
                               const char *filterprot,  /* IN */
                               enum ldap_map_selector sel, /* IN */
                               const char **user_attrs, /* IN */
                               int sizelimit,   /* IN */
                               LDAPMessage ** pRes /* OUT */ );

/*
 * Emulate X.500 read operation.
 */
enum nss_status _nss_ldap_read (const char *dn, /* IN */
                           const char **attributes,     /* IN */
                           LDAPMessage ** pRes /* OUT */ );

/*
 * extended enumeration routine; uses asynchronous API.
 * Caller must have acquired the global mutex
 */
enum nss_status _nss_ldap_getent_ex (struct ldap_args * args, /* IN */
                                struct ent_context ** key,   /* IN/OUT */
                                void *result,   /* IN/OUT */
                                char *buffer,   /* IN */
                                size_t buflen,  /* IN */
                                int *errnop,    /* OUT */
                                const char *filterprot, /* IN */
                                enum ldap_map_selector sel,        /* IN */
                                const char **user_attrs, /* IN */
                                parser_t parser /* IN */ );

/*
 * common enumeration routine; uses asynchronous API.
 * Acquires the global mutex
 */
enum nss_status _nss_ldap_getent (struct ent_context ** key, /* IN/OUT */
                             void *result,      /* IN/OUT */
                             char *buffer,      /* IN */
                             size_t buflen,     /* IN */
                             int *errnop,       /* OUT */
                             const char *filterprot,    /* IN */
                             enum ldap_map_selector sel,   /* IN */
                             parser_t parser /* IN */ );

/*
 * common lookup routine; uses synchronous API.
 */
enum nss_status _nss_ldap_getbyname (struct ldap_args * args,        /* IN/OUT */
                                void *result,   /* IN/OUT */
                                char *buffer,   /* IN */
                                size_t buflen,  /* IN */
                                int *errnop,    /* OUT */
                                const char *filterprot, /* IN */
                                enum ldap_map_selector sel,        /* IN */
                                parser_t parser /* IN */ );


/* parsing utility functions */
enum nss_status _nss_ldap_assign_attrvals (LDAPMessage * e,     /* IN */
                                      const char *attr, /* IN */
                                      const char *omitvalue,    /* IN */
                                      char ***valptr,   /* OUT */
                                      char **buffer,    /* IN/OUT */
                                      size_t * buflen,  /* IN/OUT */
                                      size_t * pvalcount /* OUT */ );


enum nss_status _nss_ldap_assign_attrval (LDAPMessage * e,      /* IN */
                                     const char *attr,  /* IN */
                                     char **valptr,     /* OUT */
                                     char **buffer,     /* IN/OUT */
                                     size_t * buflen /* IN/OUT */ );


enum nss_status _nss_ldap_assign_userpassword (LDAPMessage * e, /* IN */
                                          const char *attr,     /* IN */
                                          char **valptr,        /* OUT */
                                          char **buffer,        /* IN/OUT */
                                          size_t * buflen);     /* IN/OUT */

/* check that the entry has the specified objectclass
   return 0 for false, not-0 for true */ 
int has_objectclass(LDAPMessage *entry,const char *objectclass);

int _nss_ldap_shadow_date(const char *val);
void _nss_ldap_shadow_handle_flag(struct spwd *sp);

enum nss_status _nss_ldap_map_get(enum ldap_map_selector sel,
                                  enum ldap_map_type map,
                                  const char *key, const char **value);

const char *_nss_ldap_map_at (enum ldap_map_selector sel, const char *pChar2);
const char *_nss_ldap_unmap_at (enum ldap_map_selector sel, const char *attribute);

const char *_nss_ldap_map_oc (enum ldap_map_selector sel, const char *pChar);
const char *_nss_ldap_unmap_oc (enum ldap_map_selector sel, const char *pChar);

enum nss_status _nss_ldap_init (void);

#endif /* _LDAP_NSS_LDAP_LDAP_NSS_H */
