/* SPDX-License-Identifier: GPL-2.0-or-later */
static uint32_t fs_test_clone(Ctx *c,uint32_t session) {
    ipc_test_request(5,4,0);ipc_test_put(32,1);c->x[0]=session;a64_svc(c,33);
    assert(c->x[0]==0);return ipc_test_word(12);
}
static void fs_test_read(Ctx *c,uint32_t session,uint32_t object,uint64_t offset,uint64_t count,uint64_t buffer,uint32_t capacity) {
    memset(A64_PTR(HOS_TLS_BASE),0,128);ipc_test_put(0,6u|(1u<<24));ipc_test_put(4,15);
    ipc_test_put(8,capacity);ipc_test_put(12,(uint32_t)buffer);ipc_test_put(16,1);
    ipc_test_put(32,1u|(32u<<16));ipc_test_put(36,object);ipc_test_put(48,0x49434653);
    memcpy(A64_PTR(HOS_TLS_BASE+64),&offset,8);memcpy(A64_PTR(HOS_TLS_BASE+72),&count,8);
    c->x[0]=session;a64_svc(c,33);
}
static void check_fs(Hos *os,uint32_t sm) {
    Ctx c;hos_context(os,&c);uint32_t manager=nv_test_service(&c,sm,"fsp-srv");
    ipc_test_request(5,0,0);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(32)==1);
    uint32_t a=fs_test_clone(&c,manager),b=fs_test_clone(&c,manager);assert(a!=b && a!=manager);
    apm_test_domain(1,200,0);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(40)==0xfa01);
    apm_test_domain(1,1,8);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0x1c01);
    memset(A64_PTR(HOS_TLS_BASE),0,128);ipc_test_put(0,6);ipc_test_put(4,0x80000011u);ipc_test_put(8,1);
    ipc_test_put(32,1u|(24u<<16));ipc_test_put(36,1);ipc_test_put(48,0x49434653);ipc_test_put(56,1);
    c.x[0]=a;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(40)==0);
    apm_test_domain(1,1005,0);c.x[0]=b;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(40)==0 && ipc_test_word(48)==0);
    c.x[0]=manager;a64_svc(&c,22);assert(c.x[0]==0);
    uint32_t storage=applet_test_child(&c,a,1,200);assert(storage==2);
    apm_test_domain(storage,4,0);c.x[0]=b;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(40)==0 && time_test_u64(HOS_TLS_BASE+48)==0x100002000ULL);
    uint64_t buffer=HOS_HEAP_BASE+0x18000;
    fs_test_read(&c,b,storage,0,80,buffer,80);assert(c.x[0]==0 && ipc_test_word(40)==0 && time_test_u64(buffer)==80);
    fs_test_read(&c,b,storage,0x100001000ULL,4,buffer,4);assert(c.x[0]==0 && ipc_test_word(40)==0 && !memcmp(A64_PTR(buffer),"TAIL",4));
    fs_test_read(&c,b,storage,0x100001000ULL,4,buffer,3);assert(c.x[0]==0xca01);
    fs_test_read(&c,b,storage,0x100001fffULL,2,buffer,2);assert(c.x[0]==0 && ipc_test_word(40)==((3005u<<9)|2u));
    fs_test_read(&c,b,storage,UINT64_MAX,1,buffer,1);assert(c.x[0]==0 && ipc_test_word(40)==((3005u<<9)|2u));
    c.x[0]=buffer;c.x[1]=4096;c.x[2]=1;a64_svc(&c,2);assert(c.x[0]==0);
    fs_test_read(&c,b,storage,0,80,buffer,80);assert(c.x[0]==0xd401);
    c.x[0]=buffer;c.x[1]=4096;c.x[2]=3;a64_svc(&c,2);assert(c.x[0]==0);
    memset(A64_PTR(HOS_TLS_BASE),0,128);ipc_test_put(0,6);ipc_test_put(4,8);ipc_test_put(16,2);ipc_test_put(20,storage);
    c.x[0]=a;a64_svc(&c,33);assert(c.x[0]==0);
    apm_test_domain(storage,4,0);c.x[0]=b;a64_svc(&c,33);assert(c.x[0]==0xe401);
    storage=applet_test_child(&c,b,1,200);assert(storage==3);
    c.x[0]=a;a64_svc(&c,22);assert(c.x[0]==0);
    for(unsigned i=0;i<70;i++){
        memcpy(A64_PTR(HOS_HEAP_BASE+256),"sm:",4);c.x[1]=HOS_HEAP_BASE+256;a64_svc(&c,31);assert(c.x[0]==0);
        uint32_t tmp=(uint32_t)c.x[1];c.x[0]=tmp;a64_svc(&c,22);assert(c.x[0]==0);
    }
    fs_test_read(&c,b,storage,0x100001000ULL,4,buffer,4);assert(c.x[0]==0 && ipc_test_word(40)==0);
    apm_test_domain(storage,1,16);c.x[0]=b;assert(hos_run(os,&c,f_08000034)==HOS_STOP_UNIMPLEMENTED);
    memset(A64_PTR(HOS_TLS_BASE),0,128);ipc_test_put(0,2);c.x[0]=b;a64_svc(&c,33);assert(c.x[0]==0);
    c.x[0]=b;a64_svc(&c,22);assert(c.x[0]==0);
    puts("FSP PID registration, shared domains/clones, 64-bit RomFS reads, bounds, readonly policy and lifetime passed");
}
