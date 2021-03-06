/******************************************************************************
 *
 * Copyright (C) 1990-2018, Condor Team, Computer Sciences Department,
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
 ******************************************************************************/

#ifndef _CONDOR_FILE_MODIFIED_TRIGGER_H
#define _CONDOR_FILE_MODIFIED_TRIGGER_H

class FileModifiedTrigger {
	public:
		FileModifiedTrigger( const std::string & filename );
		virtual ~FileModifiedTrigger( void );

		bool isInitialized( void ) const { return initialized; }
		void releaseResources();

		// Timeout is in milliseconds.  Returns -1 if invalid, 0 if timed
		// out, 1 if file has changed (like poll()).
		int wait( int timeout = -1 );

	private:
		// Only needed for better log messages.
		std::string filename;
		bool initialized;

		FileModifiedTrigger( const FileModifiedTrigger & fmt );
		FileModifiedTrigger & operator =( const FileModifiedTrigger & fmt );

#if defined( LINUX )
		int read_inotify_events( void );
		int inotify_fd;
#else
		int statfd;
		off_t lastSize;
#endif
};

#endif
