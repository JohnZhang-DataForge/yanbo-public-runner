/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "hos.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern void f_08000000(Ctx *A64_NOESC);
extern void f_08000010(Ctx *A64_NOESC);
extern void f_0800001c(Ctx *A64_NOESC);
extern void f_08000024(Ctx *A64_NOESC);
static void set_heap(Ctx *c,uint64_t size){c->x[1]=size;a64_svc(c,1);}
#include "ipc_test.inc"
#include "memory_test.inc"
#include "transfer_test.inc"
#include "threads_test.inc"
#include "sync_test.inc"
int main(int argc,char **argv) {
    assert(argc==2);
    Hos *os=hos_create();assert(os);
    assert(hos_load_image(os,argv[1]));
    Ctx c;hos_context(os,&c);
    assert(c.x[31]==HOS_STACK_BASE+HOS_STACK_SIZE && c.tpidrro_el0==HOS_TLS_BASE);
    assert(c.x[1]==HOS_MAIN_HANDLE && !hos_access(os,0,4,1));
    c.x[1]=0x200000;
    assert(hos_run(os,&c,f_08000000)==HOS_STOP_THREAD_EXIT);
    assert(c.x[0]==0 && hos_heap_size(os)==0x200000 && c.pc==0x08000004);
    assert(hos_access(os,HOS_HEAP_BASE,0x200000,3));
    check_guest_threads(os);
    check_guest_sync(os);
    check_stack_mapping(os);
    check_transfer_memory(os);
    check_ipc(os);
    /* Generated SVC 0x1d must clear an empty-queue flag and preserve void ABI. */
    for(int count=-1;count<=2;count++) {
        *(uint32_t *)A64_PTR(HOS_HEAP_BASE)=1;
        c.x[0]=HOS_HEAP_BASE+3;c.x[1]=(uint64_t)(int64_t)count;c.x[2]=0x1234;
        assert(hos_run(os,&c,f_0800001c)==HOS_STOP_RETURNED);
        assert(*(uint32_t *)A64_PTR(HOS_HEAP_BASE)==0);
        assert(c.x[0]==HOS_HEAP_BASE+3 && c.x[1]==(uint64_t)(int64_t)count && c.x[2]==0x1234);
    }
    c.x[0]=0;a64_svc(&c,29);assert(c.x[0]==0); /* Invalid user write is ignored. */
    memset(A64_PTR(HOS_HEAP_BASE),0x5a,0x200000);
    set_heap(&c,1);assert(c.x[0]==0xca01 && hos_heap_size(os)==0x200000);
    set_heap(&c,HOS_HEAP_LIMIT+0x200000);assert(c.x[0]==0xd001);
    c.x[0]=HOS_TLS_BASE;c.x[2]=HOS_HEAP_BASE;a64_svc(&c,6);assert(c.x[0]==0 && c.x[1]==0);
    uint64_t base,size;uint32_t state,perm;
    memcpy(&base,A64_PTR(HOS_TLS_BASE),8);memcpy(&size,A64_PTR(HOS_TLS_BASE+8),8);
    memcpy(&state,A64_PTR(HOS_TLS_BASE+16),4);memcpy(&perm,A64_PTR(HOS_TLS_BASE+24),4);
    assert(base==HOS_HEAP_BASE && size==0x200000 && state==5 && perm==3);
    c.x[0]=0;c.x[2]=HOS_HEAP_BASE;a64_svc(&c,6);assert(c.x[0]==0xd401);
    c.x[0]=HOS_HEAP_BASE;c.x[1]=4096;c.x[2]=1;a64_svc(&c,2);assert(c.x[0]==0);
    assert(hos_access(os,HOS_HEAP_BASE,4,1) && !hos_access(os,HOS_HEAP_BASE,4,2));
    c.x[0]=HOS_HEAP_BASE;a64_svc(&c,29);
    assert(c.x[0]==HOS_HEAP_BASE && *(uint32_t *)A64_PTR(HOS_HEAP_BASE)==0x5a5a5a5a);
    c.x[0]=HOS_HEAP_BASE;c.x[1]=4096;c.x[2]=3;a64_svc(&c,2);assert(c.x[0]==0);
    set_heap(&c,0);assert(c.x[0]==0 && !hos_access(os,HOS_HEAP_BASE,1,1));
    set_heap(&c,0x200000);assert(c.x[0]==0 && *(uint32_t *)A64_PTR(HOS_HEAP_BASE)==0);
    c.x[1]=4;c.x[2]=HOS_CURRENT_PROCESS;c.x[3]=0;a64_svc(&c,41);
    assert(c.x[0]==0 && c.x[1]==HOS_HEAP_BASE);
    assert(HOS_HEAP_BASE+HOS_HEAP_LIMIT<=HOS_ALIAS_BASE && HOS_ALIAS_BASE+HOS_ALIAS_SIZE<=HOS_STACK_BASE);
    c.x[1]=2;c.x[2]=HOS_CURRENT_PROCESS;c.x[3]=0;a64_svc(&c,41);
    assert(c.x[0]==0 && c.x[1]==HOS_ALIAS_BASE);
    c.x[1]=3;c.x[2]=HOS_CURRENT_PROCESS;a64_svc(&c,41);
    assert(c.x[0]==0 && c.x[1]==HOS_ALIAS_SIZE);
    assert(HOS_STACK_BASE>=HOS_STACK_REGION_BASE && HOS_STACK_BASE+HOS_STACK_SIZE<HOS_STACK_REGION_BASE+HOS_STACK_REGION_SIZE);
    c.x[1]=14;c.x[2]=HOS_CURRENT_PROCESS;a64_svc(&c,41);
    assert(c.x[0]==0 && c.x[1]==HOS_STACK_REGION_BASE);
    c.x[1]=15;c.x[2]=HOS_CURRENT_PROCESS;a64_svc(&c,41);
    assert(c.x[0]==0 && c.x[1]==HOS_STACK_REGION_SIZE);
    c.x[0]=HOS_TLS_BASE;c.x[2]=HOS_STACK_BASE+HOS_STACK_SIZE;a64_svc(&c,6);assert(c.x[0]==0);
    memcpy(&state,A64_PTR(HOS_TLS_BASE+16),4);memcpy(&perm,A64_PTR(HOS_TLS_BASE+24),4);
    assert(state==0 && perm==0 && !hos_access(os,HOS_STACK_BASE+HOS_STACK_SIZE,4096,1));
    c.x[1]=4;c.x[2]=123;c.x[3]=0;a64_svc(&c,41);assert(c.x[0]==0xe401);
    c.x[1]=HOS_MAIN_HANDLE;a64_svc(&c,37);assert(c.x[0]==0 && c.x[1]==1);
    c.x[0]=HOS_MAIN_HANDLE;a64_svc(&c,22);assert(c.x[0]==0);
    c.x[1]=HOS_MAIN_HANDLE;a64_svc(&c,37);assert(c.x[0]==0xe401);
    c.x[1]=HOS_CURRENT_THREAD;a64_svc(&c,37);assert(c.x[0]==0);
    a64_svc(&c,30);uint64_t first=c.x[0];a64_svc(&c,30);assert(c.x[0]>=first);
    hos_context(os,&c);c.x[0]=0;c.x[1]=HOS_TLS_BASE;c.x[2]=4;
    *(uint32_t *)A64_PTR(HOS_TLS_BASE)=0xf001;
    assert(hos_run(os,&c,f_08000024)==HOS_STOP_BREAK);
    assert(strstr(hos_stop_message(os),"result=0xf001") && strstr(hos_stop_message(os),"reason=0"));
    hos_context(os,&c);c.x[0]=0x1234;
    assert(hos_run(os,&c,f_08000010)==HOS_STOP_UNIMPLEMENTED);
    assert(c.x[0]==0x1234 && c.halted && hos_last_svc(os)==0xff && c.pc==0x08000010);
    assert(strstr(hos_stop_message(os),"unimplemented SVC"));
    hos_destroy(os);
    puts("HOS memory, TLS, heap, permissions, handles, SVC ABI and exit unwinding passed");
    return 0;
}
