/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_socket_types.h"
#include "condor_netdb.h"

#ifndef   NI_MAXHOST
#define   NI_MAXHOST 1025
#endif

/* SPECIAL NAMES:
 *
 * If NO_DNS is configured we do some special name/ip handling. IP
 * addresses given to gethostbyaddr, XXX.XXX.XXX.XXX, will return
 * XXX-XXX-XXX-XXX.DEFAULT_DOMAIN, and an attempt will be made to
 * parse gethostbyname names from XXX-XXX-XXX-XXX.DEFAULT_DOMAIN into
 * XXX.XXX.XXX.XXX.
 */

/* param_boolean() and param_boolean_int() are C++-only functions, so
 * we roll our own version for checking NO_DNS. :-(
 */
static int
nodns_enabled( void )
{
	int rc = 0;
	char *value = param( "NO_DNS" );
	if ( value && ( value[0] == 'T' || value[0] == 't' || value[0] == '1' ) ) {
		rc = 1;
	}
	if ( value ) {
		free( value );
	}
	return rc;
}

/* WARNING: None of these functions properly set h_error, or errno */

int
convert_ip_to_hostname(const char *addr,
					   char *h_name,
					   int maxlen);

int
convert_hostname_to_ip(const char *name,
					   char **h_addr_list,
					   int maxaddrs);

struct hostent * get_nodns_hostent(const char* name)
{
		// We simulate at most 1 addr
	#define MAXSIMULATEDADDRS 2

	static struct hostent hostent;
	static char *h_aliases[1] = {NULL};
	static char *h_addr_list[MAXSIMULATEDADDRS];
	static char h_name[MAXSIMULATEDADDRS];

    if (convert_hostname_to_ip(name, h_addr_list, MAXSIMULATEDADDRS)) {
            // We've failed
        return NULL;
    } else {
        memset(h_name, 0, MAXSIMULATEDADDRS);
        strncpy(h_name, name, MAXSIMULATEDADDRS - 1);
        hostent.h_name = h_name;
        hostent.h_aliases = h_aliases;
        hostent.h_addrtype = AF_INET;
        hostent.h_length = sizeof(struct in_addr);
        hostent.h_addr_list = h_addr_list;

        return &hostent;
    }
}

struct hostent *
condor_gethostbyname_ipv4(const char *name) {
	if (nodns_enabled()) {
	    return get_nodns_hostent(name);
	} else {
		return gethostbyname(name);
	}
}

struct hostent*
condor_gethostbyname_ipv6(const char* name) {
    #define MAXADDR 16
    static struct hostent hostent;
	static struct in_addr addr_list[MAXADDR];
	static char *h_addr_list[MAXADDR+1];
	static char h_name[NI_MAXHOST];
	struct addrinfo hints;
	struct addrinfo* res = NULL;
	struct addrinfo* iter;
	struct hostent* hostent_alias = NULL;
	int e;
	int addrcount = 0;
	int first = 1;

    if (nodns_enabled()) {
        return get_nodns_hostent(name);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
#ifdef WIN32
	// AI_ADDRCONFIG is supported since Windows Server 2008 SDK.
    hints.ai_flags = AI_CANONNAME;
#else
    hints.ai_flags = AI_ADDRCONFIG | AI_CANONNAME;
#endif 

    e = getaddrinfo(name, NULL, &hints, &res);
    if (e != 0)
        return NULL;

    memset(h_addr_list, 0, sizeof(h_addr_list));
    memset(h_name, 0, sizeof(h_name));
    memset(&hostent, 0, sizeof(hostent));
    hostent.h_name = h_name;

	// fill up h_aliases field by calling gethostbyname().
	// it seems there is no way to get DNS aliases from IPv6 functions.
	// **YOU SHOULD NOT CALL gethostbyname() inside this fn after this point**
	hostent_alias = gethostbyname(name);
	if (hostent_alias) {
		hostent.h_aliases = hostent_alias->h_aliases;
	}

    hostent.h_addrtype = AF_INET;
    hostent.h_length = sizeof(struct in_addr);
    hostent.h_addr_list = &h_addr_list[0];

    for ( iter = res; iter != NULL; iter = iter->ai_next ) {
        if (first) {
            // first canonical name is primary name for the host.
            // rest is aliases
            if ( iter->ai_canonname ) {
                strncpy(h_name, iter->ai_canonname, sizeof(h_name)-1);
                first = 0;
            }
        }
        // pick only IPv4 address
        if (iter->ai_addr && iter->ai_addr->sa_family == AF_INET) {
            struct sockaddr_in* _sin = (struct sockaddr_in*)iter->ai_addr;
            memcpy(&addr_list[addrcount], &_sin->sin_addr, sizeof(struct in_addr));
            h_addr_list[addrcount] = (char*)&addr_list[addrcount];
            addrcount++;
            if (addrcount == MAXADDR)
                break;
        }
    }

    h_addr_list[addrcount] = NULL;

cleanup:
    freeaddrinfo(res);
    return &hostent;
}

struct hostent *
condor_gethostbyname(const char *name)
{
    return condor_gethostbyname_ipv6(name);
}

struct hostent* get_nodns_addr(const char* addr) {
	static struct hostent hostent;
	static char *h_aliases[1] = {NULL};
	static char h_name[NI_MAXHOST]; // from /usr/include/sys/param.h

    if (convert_ip_to_hostname(addr, h_name, MAXHOSTNAMELEN)) {
            // We've failed
        return NULL;
    } else {
        hostent.h_name = h_name;
        hostent.h_aliases = h_aliases;
        hostent.h_addrtype = AF_INET;
        hostent.h_length = 0;
        hostent.h_addr_list = NULL;

        return &hostent;
    }
}

struct hostent *
condor_gethostbyaddr_ipv4(const char *addr, SOCKET_LENGTH_TYPE len, int type) {
	if (nodns_enabled()) {
	    return get_nodns_addr(addr);
	} else {
		return gethostbyaddr(addr, len, type);
	}
}

struct hostent *
condor_gethostbyaddr_ipv6(const char *addr, SOCKET_LENGTH_TYPE len, int type) {
    char _hostname[NI_MAXHOST];
    struct sockaddr_in sinaddr;
    int e;

    if (type != AF_INET || type != PF_INET) {
        // fallback
        return condor_gethostbyaddr_ipv4(addr, len, type);
    }

	if (nodns_enabled()) {
	    return get_nodns_addr(addr);
	}

	memset(&sinaddr, 0, sizeof(sinaddr));
	sinaddr.sin_family = type;
	sinaddr.sin_addr = *(const struct in_addr*)addr;

	e = getnameinfo((const struct sockaddr*)&sinaddr, sizeof(sinaddr), _hostname, sizeof(_hostname), NULL, 0, 0);
	if (e != 0)
        return NULL;

    // getnameinfo only returns hostname. in order to fill up struct hostent, call condor_gethotsbyname_ipv6
    return condor_gethostbyname_ipv6(_hostname);
}

struct hostent *
condor_gethostbyaddr(const char *addr, SOCKET_LENGTH_TYPE len, int type) {
    return condor_gethostbyaddr_ipv6(addr, len, type);
    //return condor_gethostbyaddr_ipv4(addr, len, type);
}

int
condor_gethostname(char *name, size_t namelen) {

	if (nodns_enabled()) {

		char tmp[MAXHOSTNAMELEN];
		char *param_buf;

			// First, we try NETWORK_INTERFACE
		if ( (param_buf = param( "NETWORK_INTERFACE" )) ) {
			char ip_str[MAXHOSTNAMELEN];
			struct in_addr sin_addr;

			dprintf( D_HOSTNAME, "NO_DNS: Using NETWORK_INTERFACE='%s' "
					 "to determine hostname\n", param_buf );

			snprintf( ip_str, MAXHOSTNAMELEN, "%s", param_buf );
			free( param_buf );

			if ( !is_ipaddr( ip_str, &sin_addr ) ) {
				dprintf(D_HOSTNAME,
						"NO_DNS: NETWORK_INTERFACE is invalid: %s\n",
						ip_str);
				return -1;
			}

			if (convert_ip_to_hostname((char *) &sin_addr,
									   name,
									   namelen)) {
					// convert_ip_to_hostname() already printed an error
				return -1;
			}

			return 0;
		}

			// Second, we try COLLECTOR_HOST

			// To get the hostname with NODNS we are going to jump
			// through some hoops. We will try to make a UDP
			// connection to the COLLECTOR_HOST. This will result in not
			// only letting us call getsockname to find an IP for this
			// machine, but it will select the proper IP that we can
			// use to contact the COLLECTOR_HOST
		if ( (param_buf = param( "COLLECTOR_HOST" )) ) {

			struct hostent *collector_ent;
			int s;
			SOCKET_LENGTH_TYPE addr_len;
			char collector_host[MAXHOSTNAMELEN];
			char *idx;
			struct sockaddr_in addr, collector_addr;

			dprintf( D_HOSTNAME, "NO_DNS: Using COLLECTOR_HOST='%s' "
					 "to determine hostname\n", param_buf );

				// Get only the name portion of the COLLECTOR_HOST
			if ((idx = index(param_buf, ':'))) {
				*idx = '\0';
			}
			snprintf( collector_host, MAXHOSTNAMELEN, "%s", param_buf );
			free( param_buf );

				// Now that we have the name we need an IP
			if (!(collector_ent = condor_gethostbyname(collector_host))) {
				dprintf(D_HOSTNAME,
						"NO_DNS: Failed to get IP address of collector "
						"host '%s'\n", collector_host);
				return -1;
			}

				// We are doing UDP, the benefit is connect will not send
				// any network traffic on a UDP socket
			if (-1 == (s = socket(AF_INET, SOCK_DGRAM, 0))) {
				dprintf(D_HOSTNAME,
						"NO_DNS: Failed to create socket, errno=%d (%s)\n",
						errno, strerror(errno));
				return -1;
			}

			memset((char *) &collector_addr, 0, sizeof(struct sockaddr_in));
			memcpy(&collector_addr.sin_addr,
				   collector_ent->h_addr_list[0],
				   sizeof(struct in_addr));
				//collector_addr.sin_len = sizeof(struct sockaddr_in);
			collector_addr.sin_family = AF_INET;
			collector_addr.sin_port = htons(1980);
			if (connect(s,
						(struct sockaddr *) &collector_addr,
						sizeof(struct sockaddr_in))) {
				perror("connect");
				dprintf(D_HOSTNAME,
						"NO_DNS: Failed to bind socket, errno=%d (%s)\n",
						errno, strerror(errno));
				return -1;
			}

			addr_len = sizeof(struct sockaddr_in);
			if (getsockname(s,
							(struct sockaddr *) &addr,
							&addr_len)) {
				dprintf(D_HOSTNAME,
						"NO_DNS: Failed to get socket name, errno=%d (%s)\n",
						errno, strerror(errno));
				return -1;
			}

			if (convert_ip_to_hostname((char *) &addr.sin_addr,
									   name,
									   namelen)) {
					// convert_ip_to_hostname() already printed an error
				return -1;
			}

			return 0;
		}

			// Last, we try gethostname()
		if ( gethostname( tmp, MAXHOSTNAMELEN ) == 0 ) {

			struct hostent *h_ent;
			struct in_addr *inaddr;

			dprintf( D_HOSTNAME, "NO_DNS: Using gethostname()='%s' "
					 "to determine hostname\n", tmp );

			if ( (h_ent = gethostbyname( tmp )) == NULL) {
				dprintf(D_HOSTNAME,
						"NO_DNS: gethostbyname() failed, errno=%d (%s)\n",
						errno, strerror(errno));
				return -1;
			}

			inaddr = (struct in_addr *)h_ent->h_addr_list[0];
			if (convert_ip_to_hostname((char *) inaddr,
									   name,
									   namelen)) {
					// convert_ip_to_hostname() already printed an error
				return -1;
			}

			return 0;
		}

		dprintf(D_HOSTNAME,
				"Failed in determining hostname for this machine\n");
		return -1;

	} else {

		return gethostname(name, namelen);

	}
}

int
convert_ip_to_hostname(const char *addr,
					   char *h_name,
					   int maxlen) {
	char *default_domain_name;

	if (NULL != (default_domain_name = param("DEFAULT_DOMAIN_NAME"))) {
		int h_name_len;
		int i;
		memset(h_name, 0, maxlen);
		strncpy(h_name, inet_ntoa(*((struct in_addr *) addr)), maxlen - 1);
		for (i = 0; h_name[i]; i++) {
			if ('.' == h_name[i]) {
				h_name[i] = '-';
			}
		}
		h_name_len = strlen(h_name);
		snprintf(&(h_name[h_name_len]), maxlen - h_name_len, ".%s",
				 default_domain_name);

		free(default_domain_name);
		return 0;
	} else {
		dprintf(D_HOSTNAME,
				"NO_DNS: DEFAULT_DOMAIN_NAME must be defined in your "
				"top-level config file\n");
		return -1;
	}
}

int
convert_hostname_to_ip(const char *name,
					   char **h_addr_list,
					   int maxaddrs) {
	static struct in_addr addr;
	char tmp_name[MAXHOSTNAMELEN]; // could get away with 16 for IPv4
	char *default_domain_name;
	int ret;

		// We need at least one place for an address
	if (2 > maxaddrs) {
		return -1;
	}

		// We do at most one address
	h_addr_list[1] = NULL;

	if (NULL != (default_domain_name = param("DEFAULT_DOMAIN_NAME"))) {
		int i;
		char *idx;
			// XXX: What I really want to do here is to not try to
			// parse names that do not contain the DEFAULT_DOMAIN_NAME
			// suffix, but for some reason line 517 of condor_config.C
			// tries to reconfigure the full_hostname and subsequently
			// calls this function with a hostname that is of the form
			// XXX-XXX-XXX-XXX except without a suffix. Argh, so
			// instead of failing when there is no suffix I must try
			// to parse anyway!
// This is the code I want, but it's not what I get to use (below)
// 		if (NULL != (idx = strstr(name, default_domain_name))) {
//			int i;
//    		memset(tmp_name, 0, MAXHOSTNAMELEN);
// 			strncpy(tmp_name, name, idx - name);
// 			for (i = 0; tmp_name[i]; i++) {
// 				if ('-' == tmp_name[i]) {
// 					tmp_name[i] = '.';
// 				}
// 			}
//
//			addr.s_addr = inet_addr(tmp_name);
// 			if (addr.s_addr == (unsigned long)(-1L)) {
// 					// Parsing failed, so we will too
// 				h_addr_list[0] = NULL;
//
// 				return -1;
// 			} else {
// 				h_addr_list[0] = (char *) &addr;
//
// 				return 0;
// 			}
// 		} else {
// 			dprintf(D_HOSTNAME,
// 					"All hostnames must end with '%s' "
// 					"(DEFAULT_DOMAIN_NAME) in NODNS operation\n",
// 					default_domain_name);
//
// 			return -1;
// 		}

		memset(tmp_name, 0, MAXHOSTNAMELEN);
		if (NULL != (idx = strstr(name, default_domain_name))) {
			strncpy(tmp_name, name, idx - name - 1);
		} else {
			strncpy(tmp_name, name, MAXHOSTNAMELEN - 1);
		}

		free( default_domain_name );

		for (i = 0; tmp_name[i]; i++) {
			if ('-' == tmp_name[i]) {
				tmp_name[i] = '.';
			}
		}

        ret = inet_pton( AF_INET, tmp_name, &addr.s_addr);
		if (ret<=0) {
				// Parsing failed, so we will too
			h_addr_list[0] = NULL;

			return -1;
		} else {
			h_addr_list[0] = (char *) &addr;

			return 0;
		}
	} else {
		dprintf(D_HOSTNAME,
				"NO_DNS: DEFAULT_DOMAIN_NAME must be defined in your "
				"top-level config file\n");
		return -1;
	}	
}
