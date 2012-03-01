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


 

//////////////////////////////////////////////////////////////////////
// Methods to manipulate and manage daemon names
//////////////////////////////////////////////////////////////////////

#include "condor_common.h"
#include "condor_config.h"
#include "condor_string.h"
#include "my_hostname.h"
#include "my_username.h"
#include "condor_uid.h"
#include "ipv6_hostname.h"

extern "C" {

// Return the host portion of a daemon name string.  Either the name
// includes an "@" sign, in which case we return whatever is after it,
// or it doesn't, in which case we just return what we got passed.
const char*
get_host_part( const char* name )
{
	char const *tmp;
	if (name == NULL) return NULL;
	tmp = strrchr( name, '@' );
	if( tmp ) {
		return ++tmp;
	} else {
		return name;
	}
}


// Return a pointer to a newly allocated string that contains the
// valid daemon name that corresponds with the given name.  Basically,
// see if there's an '@'.  If so, leave the name alone. If not,
// resolve what we were passed as a hostname.  The string is allocated
// with strnewp() (or it's equivalent), so you should deallocate it
// with delete [].
char*
get_daemon_name( const char* name )
{
	char *tmp, *tmpname, *daemon_name = NULL;

	dprintf( D_HOSTNAME, "Finding proper daemon name for \"%s\"\n",
			 name ); 

		// First, check for a '@' in the name. 
	tmpname = strdup( name );
	tmp = strrchr( tmpname, '@' );
	if( tmp ) {
			// There's a '@'.
		dprintf( D_HOSTNAME, "Daemon name has an '@', we'll leave it alone\n" );
		daemon_name = strnewp( name );
	} else {
			// There's no '@', just try to resolve the hostname.
		dprintf( D_HOSTNAME, "Daemon name contains no '@', treating as a "
				 "regular hostname\n" );

		//MyString hostname(tmpname);
		MyString fqdn = get_fqdn_from_hostname(tmpname);
		//daemon_name = get_full_hostname( tmpname );
		daemon_name = strnewp(fqdn.Value());
	}
	free( tmpname );

		// If there was an error, this will still be NULL.
	if( daemon_name ) { 
		dprintf( D_HOSTNAME, "Returning daemon name: \"%s\"\n", daemon_name );
	} else {
		dprintf( D_HOSTNAME, "Failed to construct daemon name, "
				 "returning NULL\n" );
	}
	return daemon_name;
}


// Given some name, create a valid name for ourself with our full
// hostname.  If the name contains an '@', leave it alone.  If there's
// no '@', try to resolve what we have and see if it's
// my_full_hostname.  If so, use it, otherwise, use
// name@my_full_hostname().  We return the answer in a string which
// should be deallocated w/ delete [].
char*
build_valid_daemon_name( const char* name ) 
{
	char *tmp, *tmpname = NULL, *daemon_name = NULL;
	int size;

		// This flag determines if we want to just return a copy of
		// my_full_hostname(), or if we want to append
		// "@my_full_hostname" to the name we were given.  The name we
		// were given might include an '@', in which case, we leave it
		// alone.
	bool just_host = false;

	bool just_name = false;

	if( name && *name ) {
		tmpname = strnewp( name );
		tmp = strrchr( tmpname, '@' );
		if( tmp ) {
				// name we were passed has an '@', we just want to
				// leave the name alone
			just_name = true;
		} else {
				// no '@', see if what we have is our hostname
			MyString fqdn = get_fqdn_from_hostname(name);
			if( fqdn.Length() > 0 ) {
				if( get_local_fqdn() != fqdn ) {
						// Yup, so just the full hostname.
					just_host = true;
				}					
			}
		}
	} else {
			// Passed NULL for the name.
		just_host = true;
	}

	if( just_host ) {
		daemon_name = strnewp( my_full_hostname() );
	} else {
		if( just_name ) {
			daemon_name = strnewp( name );
		} else {
			size = strlen(tmpname) + strlen(my_full_hostname()) + 2; 
			daemon_name = new char[size];
			sprintf( daemon_name, "%s@%s", tmpname, my_full_hostname() ); 
		}
	}
	delete [] tmpname;
	return daemon_name;
}


/* 
   Return a string on the heap (must be deallocated with delete()) that 
   contains the default daemon name for the calling process.  If we're
   root (additionally, on UNIX, if we're condor), we default to the
   full hostname.  Otherwise, we default to username@full.hostname.
*/
char*
default_daemon_name( void )
{
	if( is_root() ) {
		return strnewp( my_full_hostname() );
	}
#ifndef WIN32
	if( getuid() == get_real_condor_uid() ) {
		return strnewp( my_full_hostname() );
	}
#endif /* ! LOSE32 */
	char* name = my_username();
	if( ! name ) {
		return NULL;
	}
	const char* host = my_full_hostname();
	if( ! host ) {
		free( name );
		return NULL;
	}
	int size = strlen(name) + strlen(host) + 2;
	char* ans = new char[size];
	if( ! ans ) {
		free( name );
		return NULL;
	}
	sprintf( ans, "%s@%s", name, host );
	free(name);
	return ans;
}


} /* extern "C" */
