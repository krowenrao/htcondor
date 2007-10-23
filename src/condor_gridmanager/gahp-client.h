/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef CONDOR_GAHP_CLIENT_H
#define CONDOR_GAHP_CLIENT_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "gahp_common.h"

#include "classad_hashtable.h"
#include "globus_utils.h"
#include "proxymanager.h"
#include "condor_arglist.h"


struct GahpProxyInfo
{
	Proxy *proxy;
	int cached_expiration;
	int num_references;
};

typedef void (* globus_gt4_gram_callback_func_t)(void * user_callback_arg,
												 const char * job_contact,
												 const char * state,
												 const char * fault,
												 const int exit_code);

typedef void (* unicore_gahp_callback_func_t)(const char *update_ad_string);

static const char *GAHPCLIENT_DEFAULT_SERVER_ID = "DEFAULT";
static const char *GAHPCLIENT_DEFAULT_SERVER_PATH = "DEFAULT";

// Additional error values that GAHP calls can return
///
static const int GAHPCLIENT_COMMAND_PENDING = -100;
///
static const int GAHPCLIENT_COMMAND_NOT_SUPPORTED = -101;
///
static const int GAHPCLIENT_COMMAND_NOT_SUBMITTED = -102;
///
static const int GAHPCLIENT_COMMAND_TIMED_OUT = -103;

static const int GT4_NO_EXIT_CODE = -1;

void GahpReconfig();

class GahpClient;

class GahpServer : public Service {
 public:
	static GahpServer *FindOrCreateGahpServer(const char *id,
											  const char *path,
											  const ArgList *args = NULL);
	static HashTable <HashKey, GahpServer *> GahpServersById;

	GahpServer(const char *id, const char *path, const ArgList *args = NULL);
	~GahpServer();

	bool Startup();
	bool Initialize(Proxy * proxy);

	static const int m_buffer_size;
	char *m_buffer;
	int m_buffer_pos;
	int m_buffer_end;
	int buffered_read( int fd, void *buf, int count );
	int buffered_peek();

	bool m_in_results;

	void read_argv(Gahp_Args &g_args);
	void read_argv(Gahp_Args *g_args) { read_argv(*g_args); }
	void write_line(const char *command);
	void write_line(const char *command,int req,const char *args);
	void Reaper(int pid,int status);
	int pipe_ready();
	int err_pipe_ready();

	void AddGahpClient();
	void RemoveGahpClient();

	int doProxyCheck();
	GahpProxyInfo *RegisterProxy( Proxy *proxy );
	void UnregisterProxy( Proxy *proxy );

	/** Set interval to automatically poll the Gahp Server for results.
		If the Gahp server supports async result notification, then
		the poll interval defaults to zero (disabled).  Otherwise,
		it will default to 5 seconds.  
	 	@param interval Polling interval in seconds, or zero
		to disable polling all together.  
		@return true on success, false otherwise.
		@see getPollInterval
	*/
	void setPollInterval(unsigned int interval);

	/** Retrieve the interval used to auto poll the Gahp Server 
		for results.  Also used to determine if async notification
		is in effect.
	 	@return Polling interval in seconds, or a zero
		to represent auto polling is disabled (likely if
		the Gahp server supports async notification).
		@see setPollInterval
	*/
	unsigned int getPollInterval();

	/** Immediately poll the Gahp Server for results.  Normally,
		this method is invoked automatically either by a timer set
		via setPollInterval or by a Gahp Server async result
		notification.
		@return The number of pending commands which have completed
		since the last poll (note: could easily be zero).	
		@see setPollInterval
	*/
	int poll();

	void poll_real_soon();


	bool cacheProxyFromFile( GahpProxyInfo *new_proxy );
	bool uncacheProxy( GahpProxyInfo *gahp_proxy );
	bool useCachedProxy( GahpProxyInfo *new_proxy, bool force = false );

	bool command_cache_proxy_from_file( GahpProxyInfo *new_proxy );
	bool command_use_cached_proxy( GahpProxyInfo *new_proxy );

		// Methods for private GAHP commands
	bool command_version();
	bool command_initialize_from_file(const char *proxy_path,
									  const char *command=NULL);
	bool command_commands();
	bool command_async_mode_on();
	bool command_response_prefix(const char *prefix);

	int new_reqid();

	int next_reqid;
	bool rotated_reqids;

	unsigned int m_reference_count;
	HashTable<int,GahpClient*> *requestTable;
	Queue<int> waitingToSubmit;

	int m_gahp_pid;
	int m_reaperid;
	int m_gahp_readfd;
	int m_gahp_writefd;
	int m_gahp_errorfd;
	MyString m_gahp_error_buffer;
	char m_gahp_version[150];
	StringList * m_commands_supported;
	bool use_prefix;
	unsigned int m_pollInterval;
	int poll_tid;
	bool poll_pending;

	int max_pending_requests;
	int num_pending_requests;
	GahpProxyInfo *current_proxy;
	bool skip_next_r;
	char *binary_path;
	ArgList binary_args;
	char *my_id;

	char *globus_gass_server_url;
	char *globus_gt2_gram_callback_contact;
	void *globus_gt2_gram_user_callback_arg;
	globus_gram_client_callback_func_t globus_gt2_gram_callback_func;
	int globus_gt2_gram_callback_reqid;

	char *globus_gt4_gram_callback_contact;
	void *globus_gt4_gram_user_callback_arg;
	globus_gt4_gram_callback_func_t globus_gt4_gram_callback_func;
	int globus_gt4_gram_callback_reqid;

	unicore_gahp_callback_func_t unicore_gahp_callback_func;
	int unicore_gahp_callback_reqid;

	GahpProxyInfo *master_proxy;
	int proxy_check_tid;
	bool is_initialized;
	bool can_cache_proxies;
	HashTable<HashKey,GahpProxyInfo*> *ProxiesByFilename;
}; // end of class GahpServer
	
///
class GahpClient : public Service {
	
	friend class GahpServer;
	public:
		
		/** @name Instantiation. 
		 */
		//@{
	
			/// Constructor
		GahpClient(const char *id=GAHPCLIENT_DEFAULT_SERVER_ID,
				   const char *path=GAHPCLIENT_DEFAULT_SERVER_PATH,
				   const ArgList *args=NULL);
			/// Destructor
		~GahpClient();
		
		//@}

		///
		bool Startup();

		///
		bool Initialize(Proxy *proxy);

		///
		void purgePendingRequests() { clear_pending(); }

		/** @name Mode methods.
		 * Methods to set/get the mode.
		 */
		//@{
	
		/// Enum used by setMode() method
		enum mode {
				/** */ normal,
				/** */ results_only,
				/** */ blocking
		};

		/**
		*/
		void setMode( mode m ) { m_mode = m; }

		/**
		*/
		mode getMode() { return m_mode; }
	
		//@}

		/** @name Timeout methods.
		 * Methods to set/get the timeout on pending async commands.
		 */
		//@{
	
		/** Set the timeout.
			@param t timeout in seconds, or zero to disable timeouts.
			@see getTimeout
		*/
		void setTimeout(int t) { m_timeout = t; }

		/** Get the currently timeout value.
			@return timeout in seconds, or zero if no timeout set.
			@see setTimeout
		*/
		unsigned int getTimeout() { return m_timeout; }
	
		//@}

		/** @name Async results methods.
		   Methods to control how to fetch and/or be notified about
		   pending asynchronous results.
		 */
		//@{
		
		/** Reset the specified timer to go off immediately when a
			pending command has completed.
			@param tid The timer id to reset via DaemonCore's Reset_Timer 
			method, or a -1 to disable.
			@see Reset_Timer
			@see getNotificationTimerId
		*/
		void setNotificationTimerId(int tid) { user_timerid = tid; }

		/** Return the timer id previously set with method 
			setNotificationTimerId.
			@param tid The timer id which will be reset via DaemonCore's 
			Reset_Timer method, or a -1 if deactivated.
			@see setNotificationTimerId
			@see Reset_Timer
		*/
		int getNotificationTimerId() { return user_timerid; }

		//@}

		void setNormalProxy( Proxy *proxy );

		void setDelegProxy( Proxy *proxy );

		Proxy *getMasterProxy();

		bool isStarted() { return server->m_gahp_pid != -1; }
		bool isInitialized() { return server->is_initialized; }

		const char *getErrorString();

		const char *getVersion();

		//-----------------------------------------------------------
		
		/**@name Globus Methods
		 * These methods have the exact same API as their native Globus
		 * Toolkit counterparts.  
		 */
		//@{

		const char *
			getGlobusGassServerUrl() { return server->globus_gass_server_url; }

		/// cache it from the gahp
		const char *
		globus_gram_client_error_string(int error_code);

		///
		int 
		globus_gram_client_callback_allow(
			globus_gram_client_callback_func_t callback_func,
			void * user_callback_arg,
			char ** callback_contact);

		///
		int 
		globus_gram_client_job_request(const char * resource_manager_contact,
			const char * description,
			const int job_state_mask,
			const char * callback_contact,
			char ** job_contact);

		///
		int 
		globus_gram_client_job_cancel(const char * job_contact);

		///
		int
		globus_gram_client_job_status(const char * job_contact,
			int * job_status,
			int * failure_code);

		///
		int
		globus_gram_client_job_signal(const char * job_contact,
			globus_gram_protocol_job_signal_t signal,
			const char * signal_arg,
			int * job_status,
			int * failure_code);

		///
		int
		globus_gram_client_job_callback_register(const char * job_contact,
			const int job_state_mask,
			const char * callback_contact,
			int * job_status,
			int * failure_code);

		///
		int 
		globus_gram_client_ping(const char * resource_manager_contact);

		///
		int 
		globus_gram_client_job_contact_free(char *job_contact) 
			{ free(job_contact); return 0; }


		///
		int
		globus_gram_client_job_refresh_credentials(const char *job_contact);

		///
		int
		globus_gass_server_superez_init( char **gass_url, int port );


		///
		int
		gt4_generate_submit_id (char ** submit_id);


		int
		gt4_gram_client_callback_allow(
			globus_gt4_gram_callback_func_t callback_func,
			void * user_callback_arg,
			char ** callback_contact);

		///
		int 	
		gt4_gram_client_job_create(
								   const char * submit_id,
								   const char * resource_manager_contact,
								   const char * jobmanager_type,
								   const char * callback_contact,
								   const char * rsl,
								   time_t termination_time,
								   char ** job_contact);

		int 
		gt4_gram_client_job_create(const char * resource_manager_contact,
			const char * description,
			const char * callback_contact,
			char ** job_contact);

		///
		int
		gt4_gram_client_job_start(const char *job_contact);

		///
		int 
		gt4_gram_client_job_destroy(const char * job_contact);

		///
		int
		gt4_gram_client_job_status(const char * job_contact,
			char ** job_status, char ** job_fault, int * exit_code);

		///
		int
		gt4_gram_client_job_callback_register(const char * job_contact,
			const char * callback_contact);

		///
		int 
		gt4_gram_client_ping(const char * resource_manager_contact);

		///
		int
		gt4_gram_client_delegate_credentials(const char *delegation_service_url,
											 char ** delegation_uri);

		///
		int
		gt4_gram_client_refresh_credentials(const char *delegation_uri);

		///
		int
		gt4_set_termination_time(const char *resource_uri,
								 time_t &new_termination_time);



		int
		condor_job_submit(const char *schedd_name, ClassAd *job_ad,
						  char **job_id);

		int
		condor_job_update_constrained(const char *schedd_name,
									  const char *constraint,
									  ClassAd *update_ad);

		int
		condor_job_status_constrained(const char *schedd_name,
									  const char *constraint,
									  int *num_ads, ClassAd ***ads);

		int
		condor_job_remove(const char *schedd_name, PROC_ID job_id,
						  const char *reason);

		int
		condor_job_update(const char *schedd_name, PROC_ID job_id,
						  ClassAd *update_ad);

		int
		condor_job_hold(const char *schedd_name, PROC_ID job_id,
						const char *reason);

		int
		condor_job_release(const char *schedd_name, PROC_ID job_id,
						   const char *reason);

		int
		condor_job_stage_in(const char *schedd_name, ClassAd *job_ad);

		int
		condor_job_stage_out(const char *schedd_name, PROC_ID job_id);

		int
		condor_job_refresh_proxy(const char *schedd_name, PROC_ID job_id,
								 const char *proxy_file);

		int
		condor_job_update_lease(const char *schedd_name,
								const SimpleList<PROC_ID> &jobs,
								const SimpleList<int> &expirations,
								SimpleList<PROC_ID> &updated );

		int
		blah_job_submit(ClassAd *job_ad, char **job_id);

		int
		blah_job_status(const char *job_id, ClassAd **status_ad);

		int
		blah_job_cancel(const char *job_id);

		int
		blah_job_refresh_proxy(const char *job_id, const char *proxy_file);

		int
		nordugrid_submit(const char *hostname, const char *rsl, char *&job_id);

		int
		nordugrid_status(const char *hostname, const char *job_id,
						 char *&status);

		int
		nordugrid_cancel(const char *hostname, const char *job_id);

		int
		nordugrid_stage_in(const char *hostname, const char *job_id,
						   StringList &files);

		int
		nordugrid_stage_out(const char *hostname, const char *job_id,
							StringList &files);

		int
		nordugrid_stage_out2(const char *hostname, const char *job_id,
							 StringList &src_files, StringList &dest_files);

		int
		nordugrid_exit_info(const char *hostname, const char *job_id,
							bool &normal_exit, int &exit_code,
							float &wallclock, float &sys_cpu,
							float &user_cpu );

		int
		nordugrid_ping(const char *hostname);

		///
		int 
		unicore_job_create(const char * description,
						   char ** job_contact);

		///
		int
		unicore_job_start(const char *job_contact);

		///
		int 
		unicore_job_destroy(const char * job_contact);

		///
		int
		unicore_job_status(const char * job_contact,
						   char **job_status);

		///
		int
		unicore_job_recover(const char * description);

		int
		unicore_job_callback(unicore_gahp_callback_func_t callback_func);

		int cream_delegate(const char *delg_service, const char *delg_id);
		
		int cream_job_register(const char *service, const char *delg_service, const char *delg_id, 
							   const char *jdl, time_t lease_time, char **job_id, char **upload_url);
		
		int cream_job_start(const char *service, const char *job_id);
		
		int cream_job_purge(const char *service, const char *job_id);

		int cream_job_cancel(const char *service, const char *job_id);

		int cream_job_suspend(const char *service, const char *job_id);

		int cream_job_resume(const char *service, const char *job_id);

		int cream_job_status(const char *service, const char *job_id, 
							 char **job_status, int *exit_code, char **failure_reason);
		
		int cream_proxy_renew(const char *service, const char *delg_service, const char *delg_id, StringList &job_ids);
		
		int cream_ping(const char * service);
		
		int cream_job_lease(const char *service, const char *job_id, time_t &lease_incr);

#ifdef CONDOR_GLOBUS_HELPER_WANT_DUROC
	// Not yet ready for prime time...
	globus_duroc_control_barrier_release();
	globus_duroc_control_init();
	globus_duroc_control_job_cancel();
	globus_duroc_control_job_request();
	globus_duroc_control_subjob_states();
	globus_duroc_error_get_gram_client_error();
	globus_duroc_error_string();
#endif /* ifdef CONDOR_GLOBUS_HELPER_WANT_DUROC */

		//@}

	private:

			// Various Private Methods
		void clear_pending();
		bool is_pending(const char *command, const char *buf);
		void now_pending(const char *command,const char *buf,
						 GahpProxyInfo *proxy = NULL);
		Gahp_Args* get_pending_result(const char *,const char *);
		bool check_pending_timeout(const char *,const char *);
		int reset_user_timer(int tid);
		int reset_user_timer_alarm();

			// Private Data Members
		unsigned int m_timeout;
		mode m_mode;
		char pending_command[150];
		char *pending_args;
		int pending_reqid;
		Gahp_Args* pending_result;
		time_t pending_timeout;
		int pending_timeout_tid;
		bool pending_submitted_to_gahp;
		int user_timerid;
		GahpProxyInfo *normal_proxy;
		GahpProxyInfo *deleg_proxy;
		GahpProxyInfo *pending_proxy;
		MyString error_string;

			// These data members all deal with the GAHP
			// server.  Since there is only one instance of the GAHP
			// server, all the below data members are static.
		GahpServer *server;

};	// end of class GahpClient


#endif /* ifndef CONDOR_GAHP_CLIENT_H */
