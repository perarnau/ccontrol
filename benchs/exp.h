#ifndef _EXP_H
#define _EXP_H

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#ifdef USE_PAPI

#include <papi.h>

#define NUM_EVENTS 1
//#define CODE_EVENTS  PAPI_RES_STL, PAPI_TOT_CYC
//#define NAME_EVENTS  "RES_STL","TOT_CYC"
//#define CODE_EVENTS  PAPI_RES_STL, 0x40000004
//#define NAME_EVENTS  "RES_STL","LLC_TCM"
//#define CODE_EVENTS PAPI_RES_STL,
//#define CODE_EVENTS  PAPI_RES_STL, PAPI_L2_TCM
#define NAME_EVENTS  "L3_CACHE_MISSES"
//#define CODE_EVENTS  PAPI_L2_TCM, 0x40000004
//#define NAME_EVENTS  "L2_TCM","LLC_TCM"
//#define CODE_EVENTS  0x40000003, 0x40000004
//#define NAME_EVENTS  "LLC_REFS","LLC_TCM"

#define BEGIN_MAIN \
struct timespec main_s, main_f ; \
clock_getres( CLOCK_REALTIME, &main_s);\
printf("resolution: %ld\n",main_s.tv_nsec);\
clock_gettime ( CLOCK_REALTIME, &main_s ) ; \
struct timespec m_time_s, m_time_f ; \
double exp_time ; \
int event_idx ; \
int Events[NUM_EVENTS]; \
long_long values[NUM_EVENTS] ; \
char name_events[NUM_EVENTS][15] = { NAME_EVENTS } ; \
if (PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT) { \
	printf("problem in PAPI_library_init\n"); \
	exit(1); \
} \
for(event_idx=0;event_idx<NUM_EVENTS;event_idx++){\
	if(PAPI_event_name_to_code(name_events[event_idx],Events+event_idx) != PAPI_OK){ \
		printf("problem translating event name to code %s\n", name_events[event_idx]); \
		exit(1); \
	} \
}\
if (PAPI_thread_init(pthread_self) != PAPI_OK) { \
	printf("problem in PAPI_thread_init\n"); \
	exit(1); \
} \
if(PAPI_start_counters(Events, NUM_EVENTS )!= PAPI_OK ) { \
	printf ( "problem in PAPI_start_counters: \n") ; \
	exit(1) ; \
}
#define END_MAIN \
if ( PAPI_stop_counters(values, NUM_EVENTS ) != PAPI_OK ) { \
	printf ( "problem in PAPI_stop_counters\n" ) ; \
	exit(1) ; \
} \
clock_gettime ( CLOCK_REALTIME, &main_f ) ; \
exp_time = (double)(main_f.tv_sec - main_s.tv_sec) \
		   + (double)(main_f.tv_nsec - main_s.tv_nsec) / 1e9 ; \
printf ( "total ----> %f (ms)\n", 1e3 * exp_time ) ;

#define BEGIN_EXPERIMENT \
if ( PAPI_read_counters(values, NUM_EVENTS ) != PAPI_OK ) { \
	printf ( "problem in PAPI_read_counters\n" ) ; \
	exit(1); \
} \
clock_gettime(CLOCK_REALTIME, &m_time_s ) ;

#define END_EXPERIMENT \
clock_gettime(CLOCK_REALTIME, &m_time_f ) ; \
if ( PAPI_read_counters(values, NUM_EVENTS ) != PAPI_OK ) { \
	printf ( "problem in PAPI_read_counters\n" ) ; \
	exit(1); \
} \
exp_time = (double)(m_time_f.tv_sec - m_time_s.tv_sec) \
		   + (double)(m_time_f.tv_nsec - m_time_s.tv_nsec) / 1e9 ; \
printf ( "----------> %f (ms)\n", 1e3 * exp_time ) ; \
printf( "COUNTERS "); \
for( event_idx = 0 ; event_idx < NUM_EVENTS ; ++event_idx ) \
	printf ( "%lld\t", values[event_idx] ) ; \
printf ( "\n" ) ;

#else // USE_PAPI

#define BEGIN_MAIN \
struct timespec main_s, main_f ; \
clock_gettime ( CLOCK_REALTIME, &main_s ) ; \
struct timespec m_time_s, m_time_f ; \
double exp_time ;

#define END_MAIN \
clock_gettime ( CLOCK_REALTIME, &main_f ) ; \
exp_time = (double)(main_f.tv_sec - main_s.tv_sec) \
		   + (double)(main_f.tv_nsec - main_s.tv_nsec) / 1e9 ; \
printf ( "total ----> %f (ms)\n", 1e3 * exp_time ) ;

#define BEGIN_EXPERIMENT \
clock_gettime(CLOCK_REALTIME, &m_time_s ) ;

#define END_EXPERIMENT \
clock_gettime(CLOCK_REALTIME, &m_time_f ) ; \
exp_time = (double)(m_time_f.tv_sec - m_time_s.tv_sec) \
		   + (double)(m_time_f.tv_nsec - m_time_s.tv_nsec) / 1e9 ; \
printf ( "----------> %f (ms)\n", 1e3 * exp_time ) ;

#endif // USE PAPI

#endif // _EXP_H

