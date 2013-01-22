#ifndef SYNCHRONIZATION_QEMU_H 
#define SYNCHRONIZATION_QEMU_H  

#include "monitor.h"

/* 
 * SliceTime synchronization for Android emulator (qemu)
 * Aalto University School of Science
 * Department of Computer Science and Engineering.
 * Kimmo Ahokas
 */


//Define callback function which will be run when emulator receives run permission.
typedef void(*SliceTime_runfor)(uint32_t);


/*
 * Initialize SliceTime client. This should be called in emulator initialization
 * code. It connects emulator to SliceTime synchronization server and return the
 * opened socket.
 */
void slicetime_init_client(Monitor *mon, const char *host, const char *host_port, 
			  const char *client_port, int client_id);

/*
 * Allow the emulator to run for given amount of microseconds
 */
void slicetime_run_for(uint32_t microseconds);

void slicetime_stop_sync(Monitor* mon);

void slicetime_sock_read_cb(void*);


#endif