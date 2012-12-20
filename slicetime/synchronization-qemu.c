/* 
 * SliceTime synchronization for Android emulator (qemu)
 * Aalto University School of Science
 * Department of Computer Science and Engineering.
 * Kimmo Ahokas
 */


#include <stdio.h>
#include <stdlib.h>

#include "qemu-timer.h"
#include "sysemu.h"
#include "qemu-char.h"

#include "synchronization.h"
#include "synchronization-qemu.h"


struct QEMUTimer *ts_stop_timer;
int slicetime_initialized = 0;
int slicetime_socket;
int vm_running = 0;
int64_t r_start_time;
int64_t v_start_time;
int64_t r_total = 0;
int64_t v_total = 0;
int64_t slices_run = 0;
uint32_t slice;

static void slicetime_stop_timer_cb(void* opaque)
{
    //printf("slicetime timer callback called, stopping vm.\n");
    vm_stop(RUN_STATE_PAUSED);
    vm_running = 0;
    int64_t r_ended = qemu_get_clock_ns(rt_clock);
    r_total += r_ended - r_start_time;
    int64_t v_ended = qemu_get_clock_ns(vm_clock);
    v_total += v_ended - v_start_time;
    
    period_finished(slice, r_total);
}

void slicetime_sock_read_cb(void *opaque) {
    handle_socket_read();
}

void slicetime_init_client(const char *host, const char *host_port, 
			  const char *client_port, int client_id)
{
printf("Bind the timer");
    ts_stop_timer = qemu_new_timer(vm_clock, SCALE_NS, slicetime_stop_timer_cb, 0);
printf("Unbind the timer");    
slicetime_initialized = 1;
    vm_running = 1;
    printf("Init slicetime client\n");
    slicetime_socket = register_client(host, host_port, client_port, client_id,
				 slicetime_run_for);
    if (slicetime_socket < 0)
    {
	printf("Init failed!\n");
	slicetime_initialized = 0;
	return;
    }
   
   IOHandler *sock_read = slicetime_sock_read_cb;
   qemu_set_fd_handler(slicetime_socket, sock_read, NULL, NULL);
}

/*
 * Allow the emulator to run for given amount of microseconds and then stop.
 */
void slicetime_run_for(uint32_t microseconds)
{
    //printf("slicetime_run_for called, time: %u\n", microseconds);
    if (!slicetime_initialized)
    {
	printf("SliceTime not initialized!\n");
	return;
    }
    slice = microseconds;
    slices_run++;
    v_start_time = qemu_get_clock_ns(vm_clock);
    //TODO: Figure out correct scaling for end time. Propably depends on cpu etc.
    int64_t v_end_time = v_start_time + (int64_t)microseconds * SCALE_US;
    
    qemu_mod_timer(ts_stop_timer, v_end_time);
    if (!vm_running)
    {
       vm_running = 1;
	   vm_start();
    }
    r_start_time = qemu_get_clock_ns(rt_clock);
}


void slicetime_stop_sync(void)
{
    printf("Stoppin sync..\n");
    if (slices_run > 0) {
        printf("Total realtime: %ld, average: %ld\n",
               r_total, r_total/slices_run);
        printf("Total virtualtime: %ld, average: %ld\n",
               v_total, v_total/slices_run);
    } else {
        printf("No timeslices run!\n");
    }
    qemu_set_fd_handler(slicetime_socket, NULL, NULL, NULL);
    unregister_client(0);
    qemu_free_timer(ts_stop_timer);
    slicetime_initialized = 0;
    vm_running = 0;
}


