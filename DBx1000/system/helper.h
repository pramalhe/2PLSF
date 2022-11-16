#pragma once 

#include <cstdlib>
#include <iostream>
#include <stdint.h>
#include "global.h"


/************************************************/
// atomic operations
/************************************************/
#define ATOM_ADD(dest, value) \
	__sync_fetch_and_add(&(dest), value)
#define ATOM_SUB(dest, value) \
	__sync_fetch_and_sub(&(dest), value)
// returns true if cas is successful
#define ATOM_CAS(dest, oldval, newval) \
	__sync_bool_compare_and_swap(&(dest), oldval, newval)
#define ATOM_ADD_FETCH(dest, value) \
	__sync_add_and_fetch(&(dest), value)
#define ATOM_FETCH_ADD(dest, value) \
	__sync_fetch_and_add(&(dest), value)
#define ATOM_SUB_FETCH(dest, value) \
	__sync_sub_and_fetch(&(dest), value)

#define COMPILER_BARRIER asm volatile("" ::: "memory");
#define PAUSE { __asm__ ( "pause;" ); }
//#define PAUSE usleep(1);

/************************************************/
// ASSERT Helper
/************************************************/
#define M_ASSERT(cond, ...) \
	if (!(cond)) {\
		printf("ASSERTION FAILURE [%s : %d] ", \
		__FILE__, __LINE__); \
		printf(__VA_ARGS__);\
		assert(false);\
	}

#define ASSERT(cond) assert(cond)


/************************************************/
// STACK helper (push & pop)
/************************************************/
#define STACK_POP(stack, top) { \
	if (stack == NULL) top = NULL; \
	else {	top = stack; 	stack=stack->next; } }
#define STACK_PUSH(stack, entry) {\
	entry->next = stack; stack = entry; }

/************************************************/
// LIST helper (read from head & write to tail)
/************************************************/
#define LIST_GET_HEAD(lhead, ltail, en) {\
	en = lhead; \
	lhead = lhead->next; \
	if (lhead) lhead->prev = NULL; \
	else ltail = NULL; \
	en->next = NULL; }
#define LIST_PUT_TAIL(lhead, ltail, en) {\
	en->next = NULL; \
	en->prev = NULL; \
	if (ltail) { en->prev = ltail; ltail->next = en; ltail = en; } \
	else { lhead = en; ltail = en; }}
#define LIST_INSERT_BEFORE(entry, newentry) { \
	newentry->next = entry; \
	newentry->prev = entry->prev; \
	if (entry->prev) entry->prev->next = newentry; \
	entry->prev = newentry; }
#define LIST_REMOVE(entry) { \
	if (entry->next) entry->next->prev = entry->prev; \
	if (entry->prev) entry->prev->next = entry->next; }
#define LIST_REMOVE_HT(entry, head, tail) { \
	if (entry->next) entry->next->prev = entry->prev; \
	else { assert(entry == tail); tail = entry->prev; } \
	if (entry->prev) entry->prev->next = entry->next; \
	else { assert(entry == head); head = entry->next; } \
}

/************************************************/
// STATS helper
/************************************************/
#define INC_STATS(tid, name, value) \
	if (STATS_ENABLE) \
		stats._stats[tid]->name += value;

#define INC_TMP_STATS(tid, name, value) \
	if (STATS_ENABLE) \
		stats.tmp_stats[tid]->name += value;

#define INC_GLOB_STATS(name, value) \
	if (STATS_ENABLE) \
		stats.name += value;

/************************************************/
// malloc helper
/************************************************/
// In order to avoid false sharing, any unshared read/write array residing on the same 
// cache line should be modified to be read only array with pointers to thread local data block.
// TODO. in order to have per-thread malloc, this needs to be modified !!!

#define ARR_PTR_MULTI(type, name, size, scale) \
	name = new type * [size]; \
	if (g_part_alloc || THREAD_ALLOC) { \
		for (UInt32 i = 0; i < size; i ++) {\
			UInt32 padsize = sizeof(type) * (scale); \
			if (g_mem_pad && padsize % CL_SIZE != 0) \
				padsize += CL_SIZE - padsize % CL_SIZE; \
			name[i] = (type *) mem_allocator.alloc(padsize, i); \
			for (UInt32 j = 0; j < scale; j++) \
				new (&name[i][j]) type(); \
		}\
	} else { \
		for (UInt32 i = 0; i < size; i++) \
			name[i] = new type[scale]; \
	}

#define ARR_PTR(type, name, size) \
	ARR_PTR_MULTI(type, name, size, 1)

#define ARR_PTR_INIT(type, name, size, value) \
	name = new type * [size]; \
	if (g_part_alloc) { \
		for (UInt32 i = 0; i < size; i ++) {\
			int padsize = sizeof(type); \
			if (g_mem_pad && padsize % CL_SIZE != 0) \
				padsize += CL_SIZE - padsize % CL_SIZE; \
			name[i] = (type *) mem_allocator.alloc(padsize, i); \
			new (name[i]) type(); \
		}\
	} else \
		for (UInt32 i = 0; i < size; i++) \
			name[i] = new type; \
	for (UInt32 i = 0; i < size; i++) \
		*name[i] = value; \

enum Data_type {DT_table, DT_page, DT_row };

// TODO currently, only DR_row supported
// data item type. 
class itemid_t {
public:
	itemid_t() { };
	itemid_t(Data_type type, void * loc) {
        this->type = type;
        this->location = loc;
    };
	Data_type type;
	void * location; // points to the table | page | row
	itemid_t * next;
	bool valid;
	void init();
	bool operator==(const itemid_t &other) const;
	bool operator!=(const itemid_t &other) const;
	void operator=(const itemid_t &other);
};

int get_thdid_from_txnid(uint64_t txnid);

// key_to_part() is only for ycsb
uint64_t key_to_part(uint64_t key);
uint64_t get_part_id(void * addr);
// TODO can the following two functions be merged?
uint64_t merge_idx_key(uint64_t key_cnt, uint64_t * keys);
uint64_t merge_idx_key(uint64_t key1, uint64_t key2);
uint64_t merge_idx_key(uint64_t key1, uint64_t key2, uint64_t key3);

extern timespec * res;
inline uint64_t get_server_clock() {
#if defined(__i386__)
    uint64_t ret;
    __asm__ __volatile__("rdtsc" : "=A" (ret));
#elif defined(__x86_64__)
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    uint64_t ret = ( (uint64_t)lo)|( ((uint64_t)hi)<<32 );
	ret = (uint64_t) ((double)ret / CPU_FREQ);
#else 
	timespec * tp = new timespec;
    clock_gettime(CLOCK_REALTIME, tp);
    uint64_t ret = tp->tv_sec * 1000000000 + tp->tv_nsec;
#endif
    return ret;
}

inline uint64_t get_sys_clock() {
#ifndef NOGRAPHITE
	static volatile uint64_t fake_clock = 0;
	if (warmup_finish)
		return CarbonGetTime();   // in ns
	else {
		return ATOM_ADD_FETCH(fake_clock, 100);
	}
#else
  #if TIME_ENABLE
	return get_server_clock();
  #else
	return 0;
  #endif
#endif
}
class myrand {
public:
	void init(uint64_t seed);
	uint64_t next();
private:
	uint64_t seed;
};

inline void set_affinity(uint64_t thd_id) {
	return;
	/*
	// TOOD. the following mapping only works for swarm
	// which has 4-socket, 10 physical core per socket, 
	// 80 threads in total with hyper-threading
	uint64_t a = thd_id % 40;
	uint64_t processor_id = a / 10 + (a % 10) * 4;
	processor_id += (thd_id / 40) * 40;
	
	cpu_set_t  mask;
	CPU_ZERO(&mask);
	CPU_SET(processor_id, &mask);
	sched_setaffinity(0, sizeof(cpu_set_t), &mask);
	*/
}
