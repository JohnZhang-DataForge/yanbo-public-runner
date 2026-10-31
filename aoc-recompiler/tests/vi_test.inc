/* SPDX-License-Identifier: GPL-2.0-or-later */
static uint32_t vi_test_child(Ctx *c,uint32_t parent,uint32_t command) {
    ipc_test_request(4,command,0);c->x[0]=parent;a64_svc(c,33);
    assert(c->x[0]==0 && ipc_test_word(24)==0);return ipc_test_word(12);
}
static void vi_test_open(Ctx *c,uint32_t app,const char *name) {
    ipc_test_request(4,1010,0);ipc_test_put(4,24);memset(A64_PTR(HOS_TLS_BASE+32),0,64);
    memcpy(A64_PTR(HOS_TLS_BASE+32),name,strlen(name));c->x[0]=app;a64_svc(c,33);
}
static void vi_test_display(Ctx *c,uint32_t service,uint32_t cmd,uint64_t id) {
    ipc_test_request(4,cmd,0);memcpy(A64_PTR(HOS_TLS_BASE+32),&id,8);c->x[0]=service;a64_svc(c,33);
}
static void check_vi(Hos *os,uint32_t sm) {
    Ctx c;hos_context(os,&c);uint32_t root=nv_test_service(&c,sm,"vi:m");
    ipc_test_request(4,2,0);ipc_test_put(32,9);c.x[0]=root;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(24)==0xf001);
    ipc_test_request(4,2,0);ipc_test_put(4,6);c.x[0]=root;a64_svc(&c,33);assert(c.x[0]==0x1c01);
    ipc_test_request(4,2,0);ipc_test_put(32,1);c.x[0]=root;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(24)==0);
    uint32_t app=ipc_test_word(12);assert(app && app!=root);
    c.x[0]=root;a64_svc(&c,22);assert(c.x[0]==0);
    uint32_t binder=vi_test_child(&c,app,100),system=vi_test_child(&c,app,101);
    uint32_t manager=vi_test_child(&c,app,102),indirect=vi_test_child(&c,app,103);
    vi_test_display(&c,app,1102,1);assert(c.x[0]==0 && ipc_test_word(24)==0xe401);
    vi_test_open(&c,app,"Unknown");assert(c.x[0]==0 && ipc_test_word(24)==0xf001);
    vi_test_open(&c,app,"Default");assert(c.x[0]==0 && ipc_test_word(24)==0 && time_test_u64(HOS_TLS_BASE+32)==1);
    vi_test_open(&c,app,"Default");assert(c.x[0]==0 && ipc_test_word(24)==0);
    vi_test_display(&c,app,1102,1);assert(c.x[0]==0 && ipc_test_word(24)==0);
    assert(time_test_u64(HOS_TLS_BASE+32)==1280 && time_test_u64(HOS_TLS_BASE+40)==720);
    vi_test_display(&c,system,1203,1);assert(c.x[0]==0 && ipc_test_word(32)==1280 && ipc_test_word(36)==720);
    vi_test_display(&c,system,1202,1);assert(c.x[0]==0 && time_test_u64(HOS_TLS_BASE+32)==255);
    vi_test_display(&c,app,1020,1);assert(c.x[0]==0 && ipc_test_word(24)==0);
    vi_test_display(&c,app,1102,1);assert(c.x[0]==0 && ipc_test_word(24)==0);
    vi_test_display(&c,app,1020,1);assert(c.x[0]==0 && ipc_test_word(24)==0);
    vi_test_display(&c,app,1020,1);assert(c.x[0]==0 && ipc_test_word(24)==0xe401);
    ipc_test_request(4,0,0);c.x[0]=binder;assert(hos_run(os,&c,f_08000034)==HOS_STOP_UNIMPLEMENTED);
    const uint32_t clients[]={binder,system,manager,indirect,app};
    for(unsigned i=0;i<5;i++){c.x[0]=clients[i];a64_svc(&c,22);assert(c.x[0]==0);}
    root=nv_test_service(&c,sm,"vi:u");ipc_test_request(4,0,0);ipc_test_put(32,0);c.x[0]=root;a64_svc(&c,33);app=ipc_test_word(12);assert(c.x[0]==0);
    ipc_test_request(4,102,0);c.x[0]=app;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(24)==0xfa01);
    c.x[0]=app;a64_svc(&c,22);c.x[0]=root;a64_svc(&c,22);
    root=nv_test_service(&c,sm,"vi:s");ipc_test_request(4,1,0);ipc_test_put(32,1);c.x[0]=root;a64_svc(&c,33);app=ipc_test_word(12);assert(c.x[0]==0);
    ipc_test_request(4,102,0);c.x[0]=app;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(24)==0xfa01);
    c.x[0]=app;a64_svc(&c,22);c.x[0]=root;a64_svc(&c,22);
    root=nv_test_service(&c,sm,"vi:m");ipc_test_request(5,0,0);c.x[0]=root;a64_svc(&c,33);assert(c.x[0]==0);
    apm_test_domain(1,2,4);ipc_test_put(48,1);c.x[0]=root;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(40)==0);app=ipc_test_word(48);
    apm_test_domain(app,1010,64);memcpy(A64_PTR(HOS_TLS_BASE+48),"Default",8);c.x[0]=root;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(40)==0 && time_test_u64(HOS_TLS_BASE+48)==1);
    apm_test_domain(app,1020,8);ipc_test_put(48,1);c.x[0]=root;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(40)==0);
    assert(*(uint32_t *)A64_PTR(HOS_TLS_BASE+0x110)==HOS_MAIN_HANDLE); /* 256-byte IPC leaves kernel TLS intact. */
    c.x[0]=root;a64_svc(&c,22);assert(c.x[0]==0);
    puts("VI typed clients, access levels, display references, resolution and domain framing passed");
}
