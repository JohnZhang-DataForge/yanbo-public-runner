/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef AOC_HOS_H
#define AOC_HOS_H
#include "a64.h"
#include <stdint.h>
#define HOS_IMAGE_BASE 0x08000000ULL
#define HOS_HEAP_BASE 0x20000000ULL
#define HOS_HEAP_LIMIT 0x40000000ULL
#define HOS_ALIAS_BASE 0x60000000ULL
#define HOS_ALIAS_SIZE 0x10000000ULL
#define HOS_STACK_BASE 0x70000000ULL
#define HOS_STACK_SIZE 0x00800000ULL
#define HOS_TLS_BASE 0x01000000ULL
#define HOS_MAIN_HANDLE 0x10000u
#define HOS_PROCESS_HANDLE 0x10001u
#define HOS_CURRENT_THREAD 0xffff8000u
#define HOS_CURRENT_PROCESS 0xffff8001u
#define HOS_STOP_RETURNED 0
#define HOS_STOP_PROCESS_EXIT 1
#define HOS_STOP_THREAD_EXIT 2
#define HOS_STOP_UNIMPLEMENTED 3
#define HOS_STOP_TRAP 4
#define HOS_STOP_BREAK 5

typedef struct Hos Hos;
Hos *hos_create(void);
void hos_destroy(Hos *os);
/* Serialized host-thread injection; this single-thread profile is not thread-safe. */
int hos_applet_post_message(Hos *os,uint32_t message);
void hos_applet_signal_gpu_error(Hos *os);
int hos_load_image(Hos *os,const char *path);
int hos_map(Hos *os,uint64_t address,uint64_t size,uint32_t state,uint32_t permission);
void hos_context(Hos *os,Ctx *context);
int hos_run(Hos *os,Ctx *context,A64Fn entry);
int hos_access(Hos *os,uint64_t address,uint64_t size,uint32_t permission);
uint64_t hos_heap_size(Hos *os);
uint32_t hos_last_svc(Hos *os);
const char *hos_stop_message(Hos *os);
#endif
