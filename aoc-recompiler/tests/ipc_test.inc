/* SPDX-License-Identifier: GPL-2.0-or-later */
extern void f_0800002c(Ctx *A64_NOESC);
extern void f_08000034(Ctx *A64_NOESC);
static uint32_t ipc_test_word(unsigned offset) {
    uint32_t value;memcpy(&value,(uint8_t *)A64_PTR(HOS_TLS_BASE)+offset,4);return value;
}
static void ipc_test_put(unsigned offset,uint32_t value) {
    memcpy((uint8_t *)A64_PTR(HOS_TLS_BASE)+offset,&value,4);
}
static void ipc_test_request(uint32_t type,uint32_t cmd,int pid) {
    memset(A64_PTR(HOS_TLS_BASE),0,128);
    ipc_test_put(0,type);ipc_test_put(4,10u|(pid?0x80000000u:0));
    unsigned start=pid?32:16;
    if(pid)ipc_test_put(8,1);
    ipc_test_put(start,0x49434653);ipc_test_put(start+8,cmd);ipc_test_put(start+12,0x12345678);
}
#include "nvdrv_test.inc"
#include "apm_test.inc"
#include "applet_test.inc"
#include "time_test.inc"
#include "startup_services_test.inc"
static void check_ipc(Hos *os) {
    Ctx c;hos_context(os,&c);uint64_t name=HOS_HEAP_BASE+256;
    memcpy(A64_PTR(name),"sm:",4);c.x[1]=name;
    assert(hos_run(os,&c,f_0800002c)==HOS_STOP_RETURNED && c.x[0]==0);
    uint32_t handle=(uint32_t)c.x[1];assert(handle>0x20000);
    ipc_test_request(4,0,0);c.x[0]=handle;a64_svc(&c,33);assert(c.x[0]==0x1c01);
    ipc_test_request(4,1,0);c.x[0]=handle;
    assert(hos_run(os,&c,f_08000034)==HOS_STOP_UNIMPLEMENTED);
    assert(strstr(hos_stop_message(os),"initialized=0"));
    ipc_test_request(4,0,1);c.x[0]=handle;
    assert(hos_run(os,&c,f_08000034)==HOS_STOP_RETURNED && c.x[0]==0);
    assert(ipc_test_word(16)==0x4f434653 && ipc_test_word(24)==0 && ipc_test_word(28)==0x12345678);
    assert((ipc_test_word(4)&0x3ff)==8);
    ipc_test_request(5,3,0);c.x[0]=handle;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(16)==0x4f434653 && ipc_test_word(32)==0);
    ipc_test_request(4,1,0);memcpy((uint8_t *)A64_PTR(HOS_TLS_BASE)+32,"fsp-srv",8);c.x[0]=handle;
    assert(hos_run(os,&c,f_08000034)==HOS_STOP_UNIMPLEMENTED);
    assert(strstr(hos_stop_message(os),"fsp-srv") && strstr(hos_stop_message(os),"service backend"));
    check_nvdrv(os,handle);
    check_time_service(os,handle);
    check_apm(os,handle);
    check_applet(os,handle);
    check_startup_services(os,handle);
    /* A real lm service returns a logger endpoint; requests dispatch by type. */
    ipc_test_request(4,1,0);memcpy((uint8_t *)A64_PTR(HOS_TLS_BASE)+32,"lm\0\0\0\0\0\0",8);c.x[0]=handle;a64_svc(&c,33);
    assert(c.x[0]==0 && (ipc_test_word(4)>>31)==1 && ipc_test_word(8)==32);
    uint32_t lm=ipc_test_word(12);
    ipc_test_request(4,0,1);c.x[0]=lm;a64_svc(&c,33);assert(c.x[0]==0);
    uint32_t logger=ipc_test_word(12);assert(logger!=lm && logger!=handle);
    uint64_t packet=HOS_HEAP_BASE+512;memcpy(A64_PTR(packet),"abc",3);
    ipc_test_request(4,0,0);ipc_test_put(0,4u|(1u<<16));ipc_test_put(8,3u<<16);ipc_test_put(12,(uint32_t)packet);
    c.x[0]=logger;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(24)==0);
    ipc_test_request(4,0,0);ipc_test_put(0,4u|(1u<<16));ipc_test_put(8,3u<<16);ipc_test_put(12,0);
    c.x[0]=logger;a64_svc(&c,33);assert(c.x[0]==0xd401);
    /* Send-buffer descriptor: raw data begins at 20, CMIF at 32. */
    memset(A64_PTR(HOS_TLS_BASE),0,128);ipc_test_put(0,4u|(1u<<20));ipc_test_put(4,8);
    ipc_test_put(8,4097);ipc_test_put(12,(uint32_t)packet);ipc_test_put(32,0x49434653);
    c.x[0]=logger;a64_svc(&c,33);assert(c.x[0]==0xca01);
    ipc_test_put(8,3);c.x[0]=logger;a64_svc(&c,33);assert(c.x[0]==0);
    ipc_test_request(4,1,0);ipc_test_put(32,0);c.x[0]=logger;a64_svc(&c,33);assert(c.x[0]==0);
    c.x[0]=logger;a64_svc(&c,22);assert(c.x[0]==0);
    /* Domain conversion, returned object IDs and domain dispatch/close. */
    ipc_test_request(5,0,0);c.x[0]=lm;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(32)==1);
    memset(A64_PTR(HOS_TLS_BASE),0,128);ipc_test_put(0,6);ipc_test_put(4,14u|0x80000000u);ipc_test_put(8,1);
    ipc_test_put(32,1u|(24u<<16));ipc_test_put(36,1);ipc_test_put(48,0x49434653);
    c.x[0]=lm;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(16)==1 && ipc_test_word(32)==0x4f434653);
    uint32_t object=ipc_test_word(48);assert(object==2);
    memset(A64_PTR(HOS_TLS_BASE),0,128);ipc_test_put(0,4);ipc_test_put(4,13);
    ipc_test_put(16,1u|(20u<<16));ipc_test_put(20,object);ipc_test_put(32,0x49434653);ipc_test_put(40,1);ipc_test_put(48,0);
    c.x[0]=lm;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(40)==0);
    memset(A64_PTR(HOS_TLS_BASE),0,128);ipc_test_put(0,4);ipc_test_put(4,8);ipc_test_put(16,2);ipc_test_put(20,object);
    c.x[0]=lm;a64_svc(&c,33);assert(c.x[0]==0);
    ipc_test_put(0,4);ipc_test_put(4,8);ipc_test_put(16,2);ipc_test_put(20,object);
    c.x[0]=lm;a64_svc(&c,33);assert(c.x[0]==0xe401); /* Closed domain object. */
    c.x[0]=lm;a64_svc(&c,22);assert(c.x[0]==0);
    ipc_test_request(4,0,1);ipc_test_put(4,0x800003ff);c.x[0]=handle;a64_svc(&c,33);
    assert(c.x[0]==0xca01); /* Declared raw payload must fit the TLS message. */
    ipc_test_request(4,0,0);ipc_test_put(4,0);c.x[0]=handle;a64_svc(&c,33);
    assert(c.x[0]==0x1c01); /* SFCI outside declared raw data is not accepted. */
    ipc_test_request(4,0,1);ipc_test_put(0,4u|(1u<<20));c.x[0]=handle;
    assert(hos_run(os,&c,f_08000034)==HOS_STOP_UNIMPLEMENTED);
    assert(strstr(hos_stop_message(os),"descriptors"));
    c.x[0]=handle;c.tpidrro_el0=0;a64_svc(&c,33);assert(c.x[0]==0xd401);c.tpidrro_el0=HOS_TLS_BASE;
    c.x[0]=handle;a64_svc(&c,22);assert(c.x[0]==0);
    c.x[0]=handle;a64_svc(&c,33);assert(c.x[0]==0xe401);
    c.x[0]=handle;a64_svc(&c,22);assert(c.x[0]==0xe401);
    /* Reuse storage, never stale public handle values. */
    c.x[1]=name;a64_svc(&c,31);uint32_t second=(uint32_t)c.x[1];assert(c.x[0]==0 && second!=handle);
    memset(A64_PTR(HOS_TLS_BASE),0,128);ipc_test_put(0,2);c.x[0]=second;a64_svc(&c,33);assert(c.x[0]==0);
    c.x[0]=second;a64_svc(&c,33);assert(c.x[0]==0xf601);
    c.x[0]=second;a64_svc(&c,22);assert(c.x[0]==0);
    c.x[0]=second;a64_svc(&c,33);assert(c.x[0]==0xe401);
    memcpy(A64_PTR(name),"absent",7);c.x[1]=name;a64_svc(&c,31);assert(c.x[0]==0xf201);
    c.x[1]=0;a64_svc(&c,31);assert(c.x[0]==0xd401);
    memset(A64_PTR(name),'x',12);c.x[1]=name;a64_svc(&c,31);assert(c.x[0]==0xe601);
    memcpy(A64_PTR(name),"sm:",4);
    uint32_t handles[64];
    for(unsigned i=0;i<64;i++){c.x[1]=name;a64_svc(&c,31);assert(c.x[0]==0);handles[i]=(uint32_t)c.x[1];}
    c.x[1]=name;a64_svc(&c,31);assert(c.x[0]==0xd201);
    for(unsigned i=0;i<64;i++){c.x[0]=handles[i];a64_svc(&c,22);assert(c.x[0]==0);}
    puts("IPC registration, framing, session lifetime, bounds and unsupported service diagnostics passed");
}
