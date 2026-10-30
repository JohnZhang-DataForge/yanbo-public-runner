/* SPDX-License-Identifier: GPL-2.0-or-later */
extern void f_08000088(Ctx *A64_NOESC);
extern void f_08000090(Ctx *A64_NOESC);
static uint32_t test_thread_create(Hos *os,Ctx *c,uint64_t entry,uint64_t argument) {
    static unsigned test_stack_slot;
    c->x[1]=entry;c->x[2]=argument;c->x[3]=HOS_STACK_BASE+0x20000+((test_stack_slot++)%64)*0x10000;c->x[4]=44;c->x[5]=(uint64_t)-2;
    assert(hos_run(os,c,f_08000088)==HOS_STOP_RETURNED && c->x[0]==0);return (uint32_t)c->x[1];
}
static int test_thread_join(Hos *os,Ctx *c,uint32_t handle,int64_t timeout) {
    memcpy(A64_PTR(HOS_HEAP_BASE+0x11000),&handle,4);
    c->x[1]=HOS_HEAP_BASE+0x11000;c->x[2]=1;c->x[3]=(uint64_t)timeout;
    return hos_run(os,c,f_0800003c);
}
static void check_guest_threads(Hos *os) {
    Ctx c;hos_context(os,&c);uint64_t data=HOS_HEAP_BASE+0x10000;memset(A64_PTR(data),0,32);
    uint32_t handle=test_thread_create(os,&c,0x08000054,data);
    c.x[1]=handle;a64_svc(&c,37);assert(c.x[0]==0 && c.x[1]>1);uint64_t id=c.x[1];
    c.x[1]=handle;a64_svc(&c,12);assert(c.x[0]==0 && c.x[1]==44);
    c.x[2]=handle;a64_svc(&c,14);assert(c.x[0]==0 && c.x[1]==0 && c.x[2]==1);
    c.x[0]=handle;c.x[1]=0;c.x[2]=1;a64_svc(&c,15);assert(c.x[0]==0);
    c.x[0]=handle;c.x[1]=1;c.x[2]=2;a64_svc(&c,15);assert(c.x[0]==0xe201);
    c.x[19]=0xabcdef;c.x[0]=handle;
    assert(hos_run(os,&c,f_08000090)==HOS_STOP_RETURNED && c.x[0]==0);
    assert(*(uint32_t *)A64_PTR(data)==1 && c.x[19]==0xabcdef);
    uint64_t tls;memcpy(&tls,(uint8_t *)A64_PTR(data)+8,8);
    assert(tls!=HOS_TLS_BASE && hos_access(os,tls,4096,3));
    assert(*(uint32_t *)A64_PTR(tls+0x110)==handle);
    c.x[0]=handle;a64_svc(&c,9);assert(c.x[0]==0xfa01);
    assert(test_thread_join(os,&c,handle,-1)==HOS_STOP_RETURNED && c.x[0]==0 && c.x[1]==0);
    assert(*(uint32_t *)A64_PTR(data)==2 && c.x[19]==0xabcdef);
    c.x[1]=HOS_CURRENT_THREAD;a64_svc(&c,37);assert(c.x[0]==0 && c.x[1]==1);
    c.x[0]=handle;a64_svc(&c,22);assert(c.x[0]==0 && !hos_access(os,tls,1,1));
    c.x[1]=handle;a64_svc(&c,37);assert(c.x[0]==0xe401);
    uint32_t second=test_thread_create(os,&c,0x08000054,data);assert(second!=handle);
    c.x[1]=second;a64_svc(&c,37);assert(c.x[0]==0 && c.x[1]>id);
    c.x[0]=HOS_CURRENT_THREAD;a64_svc(&c,25);assert(c.x[0]==0);
    assert(test_thread_join(os,&c,second,0)==HOS_STOP_RETURNED && c.x[0]==0xea01);
    assert(test_thread_join(os,&c,second,1000000)==HOS_STOP_RETURNED && c.x[0]==0xec01);
    assert(test_thread_join(os,&c,second,1000000)==HOS_STOP_RETURNED && c.x[0]==0xea01);
    c.x[0]=second;a64_svc(&c,22);assert(c.x[0]==0); /* Never-started thread is reclaimed. */
    /* A waiter holds the thread object, even when that thread closes its handle. */
    memset(A64_PTR(data),0,32);uint32_t closes_self=test_thread_create(os,&c,0x08000098,data);
    memcpy((uint8_t *)A64_PTR(data)+16,&closes_self,4);c.x[0]=closes_self;
    assert(hos_run(os,&c,f_08000090)==HOS_STOP_RETURNED && *(uint32_t *)A64_PTR(data)==1);
    assert(test_thread_join(os,&c,closes_self,-1)==HOS_STOP_RETURNED && c.x[0]==0);
    assert(*(uint32_t *)A64_PTR(data)==2);
    c.x[0]=closes_self;a64_svc(&c,22);assert(c.x[0]==0xe401);
    /* Cancel a currently blocked object wait without terminating its thread. */
    memset(A64_PTR(data),0,32);uint32_t main_handle=HOS_MAIN_HANDLE;memcpy(A64_PTR(data),&main_handle,4);
    uint32_t canceled=test_thread_create(os,&c,0x08000168,data);c.x[0]=canceled;
    assert(hos_run(os,&c,f_08000090)==HOS_STOP_RETURNED);
    c.x[0]=canceled;a64_svc(&c,25);assert(c.x[0]==0);
    assert(test_thread_join(os,&c,canceled,-1)==HOS_STOP_RETURNED && c.x[0]==0);
    assert(*(uint32_t *)((uint8_t *)A64_PTR(data)+8)==0xec01);
    c.x[0]=canceled;a64_svc(&c,22);assert(c.x[0]==0);
    /* Background faults must propagate to the main run instead of disappearing. */
    uint32_t fault=test_thread_create(os,&c,0x08000080,data);c.x[0]=fault;
    assert(hos_run(os,&c,f_08000090)==HOS_STOP_UNIMPLEMENTED);
    assert(strstr(hos_stop_message(os),"thread") && strstr(hos_stop_message(os),"unimplemented SVC"));
    c.x[0]=fault;a64_svc(&c,22);assert(c.x[0]==0);
    puts("Guest fibers, TLS, yielding, join, timeouts, lifetime and background fault propagation passed");
}
