/* SPDX-License-Identifier: GPL-2.0-or-later
 * Single-thread bootstrap HLE for translated code. Not a Horizon kernel.
 * Register ABI reference: switchbrew.org/wiki/SVC and switchbrew/libnx svc.h.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hos.h"
#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <sched.h>
#endif
#define SPACE (1ULL<<32)
#define PAGE 4096ULL
#define R_SIZE 0xca01u
#define R_ADDRESS 0xcc01u
#define R_MEMORY 0xd401u
#define R_PERMISSION 0xd801u
#define R_HANDLE 0xe401u
#define R_ENUM 0xf001u
#define R_RESOURCE 0x10801u
#if defined(_MSC_VER)
#define HOST_TLS __declspec(thread)
#else
#define HOST_TLS _Thread_local
#endif

typedef struct { uint8_t state,permission; } Page;
typedef struct {uint32_t handle;int initialized,kind,domain;uint32_t next_object;uint8_t objects[16];int event_index;} Session;
struct Hos {
    Session sessions[64];
    uint32_t next_session,log_destination;
    uint32_t performance_config[2];
    uint8_t performance_configured[2];
    uint32_t applet_messages[16],applet_head,applet_count;
    uint8_t applet_events[3],applet_self_flags[24];
    int applet_foreground,applet_running,applet_exit_locked;
    uint8_t *memory;
    Page *pages;
    uint64_t heap,image_size;
    uint32_t last_svc;
    int stop,thread_open,process_open,priority,trace;
    char message[192];
};
typedef struct Run { jmp_buf target; struct Run *parent; Hos *os; } Run;
static HOST_TLS Run *active;
static Hos *owner(Ctx *c) { return (Hos *)c->thread; }
static int range(uint64_t a,uint64_t n) { return a<SPACE && n<=SPACE-a; }
static int aligned(uint64_t a,uint64_t n) { return range(a,n) && !(a&(PAGE-1)) && n && !(n&(PAGE-1)); }
static int host_protect(Hos *os,uint64_t a,uint64_t n,uint32_t p) {
#if defined(_WIN32)
    DWORD old;
    DWORD permission=(p&2)?PAGE_READWRITE:(p&1)?PAGE_READONLY:PAGE_NOACCESS;
    return VirtualProtect(os->memory+a,(SIZE_T)n,permission,&old)!=0;
#else
    int permission=((p&1)?PROT_READ:0)|((p&2)?PROT_WRITE:0);
    return mprotect(os->memory+a,(size_t)n,permission)==0;
#endif
}
int hos_access(Hos *os,uint64_t a,uint64_t n,uint32_t p) {
    if (!os || !range(a,n)) return 0;
    if (!n) return 1;
    for (uint64_t i=a/PAGE;i<=(a+n-1)/PAGE;i++)
        if (!os->pages[i].state || (os->pages[i].permission&p)!=p) return 0;
    return 1;
}
int hos_map(Hos *os,uint64_t a,uint64_t n,uint32_t state,uint32_t p) {
    if (!os || !aligned(a,n) || !state || state>255 || (p!=0 && p!=1 && p!=3 && p!=5)) return 0;
    for (uint64_t i=a/PAGE;i<(a+n)/PAGE;i++) if(os->pages[i].state) return 0;
#if defined(_WIN32)
    if (!VirtualAlloc(os->memory+a,(SIZE_T)n,MEM_COMMIT,PAGE_READWRITE)) return 0;
#else
    if (!host_protect(os,a,n,3)) return 0;
#endif
    if (!host_protect(os,a,n,p)) return 0;
    for (uint64_t i=a/PAGE;i<(a+n)/PAGE;i++) {
        os->pages[i].state=(uint8_t)state;os->pages[i].permission=(uint8_t)p;
    }
    return 1;
}
Hos *hos_create(void) {
    Hos *os=(Hos *)calloc(1,sizeof(*os));
    if(!os)return NULL;
    os->pages=(Page *)calloc(SPACE/PAGE,sizeof(Page));
    if(!os->pages){free(os);return NULL;}
#if defined(_WIN32)
    os->memory=(uint8_t *)VirtualAlloc((void *)A64_HOST_BASE,(SIZE_T)SPACE,MEM_RESERVE,PAGE_NOACCESS);
#else
    void *p=mmap((void *)A64_HOST_BASE,(size_t)SPACE,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    os->memory=p==MAP_FAILED?NULL:(uint8_t *)p;
#endif
    if(os->memory!=(uint8_t *)A64_HOST_BASE){hos_destroy(os);return NULL;}
    os->trace=getenv("HOS_TRACE") && strcmp(getenv("HOS_TRACE"),"0")!=0;
    os->next_session=0x20000;os->log_destination=1;
    os->priority=44;os->thread_open=os->process_open=1;os->last_svc=~0u;
    if(!hos_map(os,HOS_STACK_BASE,HOS_STACK_SIZE,11,3) || !hos_map(os,HOS_TLS_BASE,PAGE,5,3)) {
        hos_destroy(os);return NULL;
    }
    return os;
}
void hos_destroy(Hos *os) {
    if(!os)return;
    if(os->memory) {
#if defined(_WIN32)
        VirtualFree(os->memory,0,MEM_RELEASE);
#else
        munmap(os->memory,(size_t)SPACE);
#endif
    }
    free(os->pages);free(os);
}
int hos_load_image(Hos *os,const char *path) {
    FILE *f=fopen(path,"rb");
    if(!f)return 0;
    if(fseek(f,0,SEEK_END)){fclose(f);return 0;}
    long length=ftell(f);
    if(length<=0 || (uint64_t)length>HOS_HEAP_BASE-HOS_IMAGE_BASE){fclose(f);return 0;}
    rewind(f);
    uint64_t rounded=((uint64_t)length+PAGE-1)&~(PAGE-1);
    if(!hos_map(os,HOS_IMAGE_BASE,rounded,3,3)){fclose(f);return 0;}
    int ok=fread(os->memory+HOS_IMAGE_BASE,1,(size_t)length,f)==(size_t)length;
    fclose(f);if(ok)os->image_size=rounded;return ok;
}
void hos_context(Hos *os,Ctx *c) {
    memset(c,0,sizeof(*c));c->thread=os;
    c->x[1]=HOS_MAIN_HANDLE;c->x[31]=HOS_STACK_BASE+HOS_STACK_SIZE;
    c->tpidrro_el0=HOS_TLS_BASE;
}
static void stop(Ctx *c,int reason,const char *message) {
    Hos *os=owner(c);c->halted=1;
    os->stop=reason;snprintf(os->message,sizeof(os->message),"%s",message);
    if(active && active->os==os)longjmp(active->target,1);
}
void hos_applet_signal_gpu_error(Hos *os) { if(os)os->applet_events[2]=1; }
int hos_applet_post_message(Hos *os,uint32_t message) {
    if(!os || os->applet_count==16)return 0;
    os->applet_messages[(os->applet_head+os->applet_count)%16]=message;
    os->applet_count++;os->applet_events[0]=1;return 1;
}
int hos_run(Hos *os,Ctx *c,A64Fn entry) {
    Run frame;frame.parent=active;frame.os=os;active=&frame;
    c->halted=0;os->stop=HOS_STOP_RETURNED;os->message[0]=0;
    if(setjmp(frame.target)==0)entry(c);
    active=frame.parent;return os->stop;
}
uint64_t hos_heap_size(Hos *os){return os->heap;}
uint32_t hos_last_svc(Hos *os){return os->last_svc;}
const char *hos_stop_message(Hos *os){return os->message;}
static uint32_t resize_heap(Hos *os,uint64_t size) {
    if(size&0x1fffff || size>SPACE)return R_SIZE;
    if(size>HOS_HEAP_LIMIT)return 0xd001;
    if(size>os->heap) {
        if(!hos_map(os,HOS_HEAP_BASE+os->heap,size-os->heap,5,3))return R_RESOURCE;
    } else if(size<os->heap) {
        uint64_t a=HOS_HEAP_BASE+size,n=os->heap-size;
#if defined(_WIN32)
        if(!VirtualFree(os->memory+a,(SIZE_T)n,MEM_DECOMMIT))return R_RESOURCE;
#else
        if(madvise(os->memory+a,(size_t)n,MADV_DONTNEED) || !host_protect(os,a,n,0))return R_RESOURCE;
#endif
        memset(os->pages+a/PAGE,0,(size_t)(n/PAGE)*sizeof(Page));
    }
    os->heap=size;return 0;
}
static int thread_handle(Hos *os,uint64_t h){return h==HOS_CURRENT_THREAD || (os->thread_open && h==HOS_MAIN_HANDLE);}
static int process_handle(Hos *os,uint64_t h){return h==HOS_CURRENT_PROCESS || (os->process_open && h==HOS_PROCESS_HANDLE);}
static uint64_t ticks(void) {
#if defined(_WIN32)
    LARGE_INTEGER now,freq;QueryPerformanceCounter(&now);QueryPerformanceFrequency(&freq);
    return (uint64_t)(now.QuadPart/freq.QuadPart)*19200000ULL+(uint64_t)(now.QuadPart%freq.QuadPart)*19200000ULL/(uint64_t)freq.QuadPart;
#else
    struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);
    return (uint64_t)now.tv_sec*19200000ULL+(uint64_t)now.tv_nsec*19200000ULL/1000000000ULL;
#endif
}
#include "ipc.inc"

void a64_svc(Ctx *c,uint32_t id) {
    Hos *os=owner(c);os->last_svc=id;
    if(os->trace)fprintf(stderr,"[HOS] SVC %#x pc=%#llx x0=%#llx x1=%#llx x2=%#llx x3=%#llx\n",id,
        (unsigned long long)c->pc,(unsigned long long)c->x[0],(unsigned long long)c->x[1],
        (unsigned long long)c->x[2],(unsigned long long)c->x[3]);
    switch(id) {
    case 1: { uint32_t result=resize_heap(os,c->x[1]);c->x[0]=result;if(!result)c->x[1]=HOS_HEAP_BASE;return; }
    case 2: {
        uint64_t a=c->x[0],n=c->x[1];uint32_t p=(uint32_t)c->x[2];
        if(a&(PAGE-1)){c->x[0]=R_ADDRESS;return;}
        if(!n || n&(PAGE-1)){c->x[0]=R_SIZE;return;}
        if(p!=0 && p!=1 && p!=3){c->x[0]=R_PERMISSION;return;}
        if(!hos_access(os,a,n,0)){c->x[0]=R_MEMORY;return;}
        for(uint64_t i=a/PAGE;i<(a+n)/PAGE;i++)if(os->pages[i].state!=5){c->x[0]=R_MEMORY;return;}
        if(!host_protect(os,a,n,p)){c->x[0]=R_RESOURCE;return;}
        for(uint64_t i=a/PAGE;i<(a+n)/PAGE;i++)os->pages[i].permission=(uint8_t)p;
        c->x[0]=0;return;
    }
    case 6: {
        uint64_t out=c->x[0],a=c->x[2];
        if(!hos_access(os,out,0x28,2) || !range(a,1)){c->x[0]=R_MEMORY;return;}
        uint64_t page=a/PAGE,lo=page,hi=page+1;Page value=os->pages[page];
        while(lo && os->pages[lo-1].state==value.state && os->pages[lo-1].permission==value.permission)lo--;
        while(hi<SPACE/PAGE && os->pages[hi].state==value.state && os->pages[hi].permission==value.permission)hi++;
        uint8_t info[0x28]={0};uint64_t base=lo*PAGE,size=(hi-lo)*PAGE;uint32_t state=value.state,perm=value.permission;
        memcpy(info,&base,8);memcpy(info+8,&size,8);memcpy(info+16,&state,4);memcpy(info+24,&perm,4);
        memcpy(os->memory+out,info,sizeof(info));c->x[0]=c->x[1]=0;return;
    }
    case 7: stop(c,HOS_STOP_PROCESS_EXIT,"guest ExitProcess");return;
    case 10: stop(c,HOS_STOP_THREAD_EXIT,"guest ExitThread");return;
    case 11: {
        int64_t ns=(int64_t)c->x[0];
#if defined(_WIN32)
        if(ns<=0)SwitchToThread();else Sleep((DWORD)((uint64_t)ns/1000000ULL+((uint64_t)ns%1000000ULL!=0)));
#else
        if(ns<=0)sched_yield();else {struct timespec duration={ns/1000000000LL,ns%1000000000LL};while(nanosleep(&duration,&duration)&&errno==EINTR){}}
#endif
        return;
    }
    case 12: c->x[0]=thread_handle(os,c->x[1])?0:R_HANDLE;c->x[1]=(uint64_t)os->priority;return;
    case 13: if(!thread_handle(os,c->x[0]))c->x[0]=R_HANDLE;else if(c->x[1]>63)c->x[0]=R_ENUM;else{os->priority=(int)c->x[1];c->x[0]=0;}return;
    case 16: c->x[0]=0;return; /* Single main-thread profile, core 0. */
    case 18: case 23: {
        Session *event=ipc_session(os,c->x[0]);
        if(!event || event->kind!=20){c->x[0]=R_HANDLE;return;}
        if(id==23 && !os->applet_events[event->event_index]){c->x[0]=0xfa01;return;}
        os->applet_events[event->event_index]=0;c->x[0]=0;return;
    }
    case 24: {
        int32_t count=(int32_t)c->x[2];
        if(count<0 || count>64){c->x[0]=0xee01;return;}
        if(count && !hos_access(os,c->x[1],(uint64_t)count*4,1)){c->x[0]=R_MEMORY;return;}
        Session *events[64];
        for(int32_t i=0;i<count;i++) {
            uint32_t handle;memcpy(&handle,os->memory+c->x[1]+4*i,4);
            events[i]=ipc_session(os,handle);
            if(!events[i] || events[i]->kind!=20){c->x[0]=R_HANDLE;return;}
        }
        for(int32_t i=0;i<count;i++)if(os->applet_events[events[i]->event_index]){c->x[0]=0;c->x[1]=(uint32_t)i;return;}
        if(!c->x[3]){c->x[0]=0xea01;return;}
        stop(c,HOS_STOP_UNIMPLEMENTED,"blocking event wait requires a scheduler/host event source");return;
    }
    case 22:
        {Session *session=ipc_session(os,c->x[0]);if(session){session->handle=0;c->x[0]=0;return;}}
        if(c->x[0]==HOS_MAIN_HANDLE && os->thread_open){os->thread_open=0;c->x[0]=0;}
        else if(c->x[0]==HOS_PROCESS_HANDLE && os->process_open){os->process_open=0;c->x[0]=0;}
        else c->x[0]=R_HANDLE;
        return;
    case 29: { /* SignalProcessWideKey: void ABI, key rounded down to u32. */
        /* This profile has one runnable thread and cannot register waiters:
         * CreateThread and WaitProcessWideKeyAtomic still stop explicitly.
         * With an empty wait queue, signaling clears the guest waiter flag.
         * A failed user write is ignored, as in the kernel's void signal path.
         * Revisit this branch when a scheduler/wait queue is introduced.
         */
        uint64_t key=c->x[0]&~3ULL;
        if(hos_access(os,key,sizeof(uint32_t),2)) {
            uint32_t no_waiters=0;memcpy(os->memory+key,&no_waiters,sizeof(no_waiters));
        }
        return;
    }
    case 30: c->x[0]=ticks();return;
    case 31: ipc_connect(c);return;
    case 33: ipc_send(c);return;
    case 36: if(process_handle(os,c->x[1])){c->x[0]=0;c->x[1]=1;}else c->x[0]=R_HANDLE;return;
    case 37: if(thread_handle(os,c->x[1])){c->x[0]=0;c->x[1]=1;}else c->x[0]=R_HANDLE;return;
    case 38: {
        char text[192];uint32_t result=0;
        int has_result=c->x[2]==sizeof(result) && hos_access(os,c->x[1],sizeof(result),1);
        if(has_result)memcpy(&result,os->memory+c->x[1],sizeof(result));
        snprintf(text,sizeof(text),"guest svcBreak reason=%#llx buffer=%#llx size=%llu lr=%#llx%s%#x",
            (unsigned long long)c->x[0],(unsigned long long)c->x[1],(unsigned long long)c->x[2],
            (unsigned long long)c->x[30],has_result?" result=":" unread-result=",result);
        stop(c,HOS_STOP_BREAK,text);return;
    }
    case 39: {
        uint64_t a=c->x[0],n=c->x[1];
        if(n>4096 || !hos_access(os,a,n,1)){c->x[0]=R_MEMORY;return;}
        fwrite(os->memory+a,1,(size_t)n,stderr);c->x[0]=0;return;
    }
    case 41: {
        uint64_t type=c->x[1],value=0;
        if(!process_handle(os,c->x[2])){c->x[0]=R_HANDLE;return;}
        if(c->x[3]){c->x[0]=R_ENUM;return;}
        switch(type) {
        case 0:value=1;break;case 1:value=~0ULL;break;
        case 2:value=HOS_ALIAS_BASE;break;case 3:value=HOS_ALIAS_SIZE;break;
        case 4:value=HOS_HEAP_BASE;break;case 5:value=HOS_HEAP_LIMIT;break;
        case 6:case 21:value=HOS_HEAP_LIMIT+HOS_STACK_SIZE+os->image_size+PAGE;break;
        case 7:case 22:value=os->heap+HOS_STACK_SIZE+os->image_size+PAGE;break;
        case 8:value=0;break;
        case 12:value=HOS_IMAGE_BASE;break;case 13:value=SPACE-HOS_IMAGE_BASE;break;
        case 14:value=HOS_STACK_BASE;break;case 15:value=HOS_STACK_SIZE;break;
        case 23:value=1;break;
        default:c->x[0]=R_ENUM;return;
        }
        c->x[0]=0;c->x[1]=value;return;
    }
    default: {
        char text[128];snprintf(text,sizeof(text),"unimplemented SVC %#x at guest PC %#llx",id,(unsigned long long)c->pc);
        stop(c,HOS_STOP_UNIMPLEMENTED,text);return;
    }
    }
}
void a64_on_trap(Ctx *c,uint64_t pc,uint32_t instruction,const char *why) {
    char text[192];snprintf(text,sizeof(text),"%s: PC %#llx instruction %08x",why,(unsigned long long)pc,instruction);
    c->pc=pc;stop(c,HOS_STOP_TRAP,text);
}
