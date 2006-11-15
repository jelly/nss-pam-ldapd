/*
   protocol.c - network address entry lookup routines
   This file was part of the nss-ldap library (as ldap-proto.c)
   which has been forked into the nss-ldapd library.

   Copyright (C) 1997-2005 Luke Howard
   Copyright (C) 2006 West Consulting
   Copyright (C) 2006 Arthur de Jong

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
   MA 02110-1301 USA
*/

/*
   Determine the canonical name of the RPC with _nss_ldap_getrdnvalue(),
   and assign any values of "cn" which do NOT match this canonical name
   as aliases.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#ifdef HAVE_LBER_H
#include <lber.h>
#endif
#ifdef HAVE_LDAP_H
#include <ldap.h>
#endif
#if defined(HAVE_THREAD_H)
#include <thread.h>
#elif defined(HAVE_PTHREAD_H)
#include <pthread.h>
#endif

#include "ldap-nss.h"
#include "util.h"

static struct ent_context *proto_context = NULL;

static enum nss_status _nss_ldap_parse_proto (LDAPMessage *e,
                       struct ldap_state *pvt,
                       void *result, char *buffer, size_t buflen)
{

  struct protoent *proto = (struct protoent *) result;
  char *number;
  enum nss_status stat;

  stat =
    _nss_ldap_getrdnvalue (e, ATM (LM_PROTOCOLS, cn), &proto->p_name,
                           &buffer, &buflen);
  if (stat != NSS_STATUS_SUCCESS)
    return stat;

  stat =
    _nss_ldap_assign_attrval (e, AT (ipProtocolNumber), &number, &buffer,
                              &buflen);
  if (stat != NSS_STATUS_SUCCESS)
    return stat;

  proto->p_proto = atoi (number);

  stat =
    _nss_ldap_assign_attrvals (e, ATM (LM_PROTOCOLS, cn), proto->p_name,
                               &proto->p_aliases, &buffer, &buflen, NULL);
  if (stat != NSS_STATUS_SUCCESS)
    return stat;

  return NSS_STATUS_SUCCESS;
}

enum nss_status _nss_ldap_getprotobyname_r(const char *name,struct protoent *result,
                            char *buffer,size_t buflen,int *errnop)
{
  LOOKUP_NAME (name, result, buffer, buflen, errnop,
               _nss_ldap_filt_getprotobyname, LM_PROTOCOLS,
               _nss_ldap_parse_proto, LDAP_NSS_BUFLEN_DEFAULT);
}

enum nss_status _nss_ldap_getprotobynumber_r(int number,struct protoent *result,
                              char *buffer,size_t buflen,int *errnop)
{
  LOOKUP_NUMBER (number, result, buffer, buflen, errnop,
                 _nss_ldap_filt_getprotobynumber, LM_PROTOCOLS,
                 _nss_ldap_parse_proto, LDAP_NSS_BUFLEN_DEFAULT);
}

enum nss_status _nss_ldap_setprotoent(void)
{
  LOOKUP_SETENT (proto_context);
}

enum nss_status _nss_ldap_getprotoent_r(struct protoent *result,char *buffer,size_t buflen,
                        int *errnop)
{
  LOOKUP_GETENT (proto_context, result, buffer, buflen, errnop,
                 _nss_ldap_filt_getprotoent, LM_PROTOCOLS,
                 _nss_ldap_parse_proto, LDAP_NSS_BUFLEN_DEFAULT);
}

enum nss_status _nss_ldap_endprotoent(void)
{
  LOOKUP_ENDENT (proto_context);
}