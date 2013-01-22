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
Monitor* monitor;

static void slicetime_stop_timer_cb(void* opaque)
{
    //monitor_printf(monitor, "slicetime timer callback called, stopping vm.\n");
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

void slicetime_init_client(Monitor *mon, const char *host, const char *host_port, 
			  const char *client_port, int client_id)
{
    monitor = mon;

    monitor_printf(monitor, "Bind the timer\n");
    ts_stop_timer = qemu_new_timer(vm_clock, SCALE_NS, slicetime_stop_timer_cb, 0);
    monitor_printf(monitor, "Unbind the timer\n");    

    slicetime_initialized = 1;
    vm_running = 1;
    monitor_printf(monitor, "Init slicetime client\n");

    slicetime_socket = register_client(monitor, host, host_port, client_port, client_id,
				 slicetime_run_for);
    if (slicetime_socket < 0)
    {
    	monitor_printf(monitor, "Init failed!\n");
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
    	monitor_printf(monitor, "SliceTime not initialized!\n");
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


void slicetime_stop_sync(Monitor* mon)
{
    monitor = mon; 
    monitor_printf(monitor, "Stoppin sync..\n");
    if (slices_run > 0) {
        monitor_printf(monitor, "Total realtime: %ld, average: %ld\n",
               r_total, r_total/slices_run);
        monitor_printf(monitor, "Total virtualtime: %ld, average: %ld\n",
               v_total, v_total/slices_run);
    } else {
        monitor_printf(monitor, "No timeslices run!\n");
    }
    qemu_set_fd_handler(slicetime_socket, NULL, NULL, NULL);
    unregister_client(0);
    qemu_free_timer(ts_stop_timer);
    slicetime_initialized = 0;
    vm_running = 0;
}


