/* SPDX-License-Identifier: GPL-2.0-or-later */
static void sync_test_start(Hos *os,Ctx *c,uint32_t h) {c->x[0]=h;assert(hos_run(os,c,f_08000090)==HOS_STOP_RETURNED && c->x[0]==0);}
static void sync_test_close(Ctx *c,uint32_t h) {c->x[0]=h;a64_svc(c,22);assert(c->x[0]==0);}
static void sync_test_signal(Ctx *c,uint64_t key,int count) {c->x[0]=key;c->x[1]=(uint64_t)(int64_t)count;a64_svc(c,29);assert(c->x[0]==key);}
static uint32_t sync_test_word(uint64_t address) {uint32_t v;memcpy(&v,A64_PTR(address),4);return v;}
static void sync_test_put(uint64_t address,uint32_t value) {memcpy(A64_PTR(address),&value,4);}
static uint32_t sync_test_cv_thread(Hos *os,Ctx *c,uint64_t data,int64_t timeout) {
    uint32_t handle=test_thread_create(os,c,0x080000fc,data);
    sync_test_put(data,handle);sync_test_put(data+16,handle);memcpy(A64_PTR(data+40),&timeout,8);
    sync_test_start(os,c,handle);return handle;
}
static void check_guest_sync(Hos *os) {
    Ctx c;hos_context(os,&c);uint64_t data=HOS_HEAP_BASE+0x12000;
    memset(A64_PTR(data),0,64);
    c.x[0]=HOS_MAIN_HANDLE;c.x[1]=data+1;c.x[2]=HOS_MAIN_HANDLE;a64_svc(&c,26);assert(c.x[0]==0xcc01);
    c.x[0]=0xdead;c.x[1]=data;sync_test_put(data,0x4000dead);a64_svc(&c,26);assert(c.x[0]==0xe401);
    sync_test_put(data,0);c.x[0]=0xdead;c.x[1]=data;a64_svc(&c,26);assert(c.x[0]==0); /* Word changed before entering kernel. */
    memset(A64_PTR(data),0,64);uint32_t mutex_thread=test_thread_create(os,&c,0x080000c4,data);
    sync_test_put(data,HOS_MAIN_HANDLE|0x40000000);sync_test_put(data+12,HOS_MAIN_HANDLE);sync_test_put(data+16,mutex_thread);
    sync_test_start(os,&c,mutex_thread);assert(sync_test_word(data+4)==0);
    c.x[0]=data;a64_svc(&c,27);assert(c.x[0]==0);
    assert(test_thread_join(os,&c,mutex_thread,-1)==HOS_STOP_RETURNED && c.x[0]==0);
    assert(sync_test_word(data+4)==1 && sync_test_word(data+8)==0 && sync_test_word(data+32)==mutex_thread && sync_test_word(data)==0);
    sync_test_close(&c,mutex_thread);
    /* A blocked high-priority waiter donates priority to its runnable owner. */
    memset(A64_PTR(data),0,256);uint32_t high=test_thread_create(os,&c,0x080000c4,data);
    c.x[0]=high;c.x[1]=20;a64_svc(&c,13);assert(c.x[0]==0);
    sync_test_put(data,HOS_MAIN_HANDLE|0x40000000);sync_test_put(data+12,HOS_MAIN_HANDLE);sync_test_put(data+16,high);
    sync_test_start(os,&c,high);
    uint32_t medium=test_thread_create(os,&c,0x0800013c,data+128);
    c.x[0]=medium;c.x[1]=30;a64_svc(&c,13);assert(c.x[0]==0);
    sync_test_start(os,&c,medium);assert(sync_test_word(data+128)==0);
    c.x[0]=data;a64_svc(&c,27);assert(c.x[0]==0);
    assert(test_thread_join(os,&c,high,-1)==HOS_STOP_RETURNED && c.x[0]==0);
    assert(test_thread_join(os,&c,medium,-1)==HOS_STOP_RETURNED && c.x[0]==0 && sync_test_word(data+128)==3);
    sync_test_close(&c,high);sync_test_close(&c,medium);
    /* Signaling while a mutex is held moves a CV waiter to its mutex queue. */
    memset(A64_PTR(data),0,64);uint32_t cv=sync_test_cv_thread(os,&c,data,-1);
    assert(sync_test_word(data)==0 && sync_test_word(data+24)==1 && sync_test_word(data+4)==0);
    sync_test_put(data,HOS_MAIN_HANDLE);sync_test_signal(&c,data+24,1);
    assert(sync_test_word(data)==(HOS_MAIN_HANDLE|0x40000000) && sync_test_word(data+24)==0 && sync_test_word(data+4)==0);
    c.x[0]=data;a64_svc(&c,27);assert(c.x[0]==0);
    assert(test_thread_join(os,&c,cv,-1)==HOS_STOP_RETURNED && c.x[0]==0);
    assert(sync_test_word(data+8)==0 && sync_test_word(data+32)==cv);sync_test_close(&c,cv);
    /* Count-limited signals and wake-all preserve the remaining key flag. */
    memset(A64_PTR(data),0,64);uint32_t first=sync_test_cv_thread(os,&c,data,-1);
    uint32_t second=sync_test_cv_thread(os,&c,data,-1);
    sync_test_signal(&c,data+24,1);
    assert(test_thread_join(os,&c,first,-1)==HOS_STOP_RETURNED && c.x[0]==0);
    assert(test_thread_join(os,&c,second,0)==HOS_STOP_RETURNED && c.x[0]==0xea01 && sync_test_word(data+24)==1);
    sync_test_signal(&c,data+24,0);
    assert(test_thread_join(os,&c,second,-1)==HOS_STOP_RETURNED && c.x[0]==0 && sync_test_word(data+24)==0);
    assert(sync_test_word(data+32)==second);sync_test_close(&c,first);sync_test_close(&c,second);
    /* Timeout leaves the mutex unlocked; the caller must relock it. */
    memset(A64_PTR(data),0,64);uint32_t timed=sync_test_cv_thread(os,&c,data,1000000);
    assert(test_thread_join(os,&c,timed,-1)==HOS_STOP_RETURNED && c.x[0]==0);
    assert(sync_test_word(data+8)==0xea01 && sync_test_word(data)==0);sync_test_close(&c,timed);
    sync_test_signal(&c,data+27,-1);assert(sync_test_word(data+24)==0);
    sync_test_put(data,HOS_MAIN_HANDLE);c.x[0]=data;c.x[1]=data+27;c.x[2]=HOS_MAIN_HANDLE;c.x[3]=0;a64_svc(&c,28);
    assert(c.x[0]==0xea01 && sync_test_word(data)==0 && sync_test_word(data+24)==1);
    sync_test_signal(&c,data+24,0);assert(sync_test_word(data+24)==0);
    puts("Mutex handoff, CV release/relock, wake counts, FIFO, timeout and waiter flags passed");
}
