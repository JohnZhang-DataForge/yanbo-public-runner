/* SPDX-License-Identifier: GPL-2.0-or-later */
static void startup_pid_request(uint32_t object,uint32_t command,uint32_t size) {
    memset(A64_PTR(HOS_TLS_BASE),0,128);ipc_test_put(0,6);ipc_test_put(4,((48+size+3)/4)|0x80000000u);ipc_test_put(8,1);
    ipc_test_put(32,1u|((16+size)<<16));ipc_test_put(36,object);ipc_test_put(48,0x49434653);ipc_test_put(56,command);
}
static uint32_t startup_service(Ctx *c,uint32_t sm,const char name[8]) {
    ipc_test_request(4,1,0);memcpy((uint8_t *)A64_PTR(HOS_TLS_BASE)+32,name,8);c->x[0]=sm;a64_svc(c,33);assert(c->x[0]==0);
    uint32_t h=ipc_test_word(12);ipc_test_request(5,0,0);c->x[0]=h;a64_svc(c,33);assert(c->x[0]==0);return h;
}
static void check_startup_services(Hos *os,uint32_t sm) {
    Ctx c;hos_context(os,&c);
    uint32_t addon=startup_service(&c,sm,"aoc:u\0\0\0");
    apm_test_domain(1,2,8);c.x[0]=addon;a64_svc(&c,33);assert(c.x[0]==0x1c01);
    startup_pid_request(1,2,8);c.x[0]=addon;
    assert(hos_run(os,&c,f_08000034)==HOS_STOP_RETURNED && c.x[0]==0 && ipc_test_word(40)==0 && ipc_test_word(48)==0);
    startup_pid_request(1,7,16);c.x[0]=addon;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(40)!=0); /* No nonexistent DLC is marked prepared. */
    apm_test_domain(1,8,0);c.x[0]=addon;a64_svc(&c,33);
    uint32_t event=ipc_test_word(12);assert(c.x[0]==0 && ipc_test_word(8)==2);
    c.x[0]=addon;a64_svc(&c,22);assert(c.x[0]==0);
    applet_test_poll(&c,event,0,0);assert(c.x[0]==0xea01);
    c.x[0]=event;a64_svc(&c,22);assert(c.x[0]==0);
    uint32_t policy=startup_service(&c,sm,"pctl\0\0\0\0");
    apm_test_domain(1,1,8);c.x[0]=policy;a64_svc(&c,33);assert(c.x[0]==0x1c01);
    startup_pid_request(1,1,8);c.x[0]=policy;a64_svc(&c,33);
    uint32_t object=ipc_test_word(48);assert(c.x[0]==0 && ipc_test_word(16)==1 && object==2);
    apm_test_domain(object,1,0);c.x[0]=policy;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(40)==0);
    apm_test_domain(object,1006,0);c.x[0]=policy;
    assert(hos_run(os,&c,f_08000034)==HOS_STOP_UNIMPLEMENTED); /* No fabricated policy authorization. */
    c.x[0]=policy;a64_svc(&c,22);assert(c.x[0]==0);
    const char aliases[3][8]={"pctl:s","pctl:r","pctl:a"};
    for(unsigned i=0;i<3;i++){uint32_t alias=startup_service(&c,sm,aliases[i]);c.x[0]=alias;a64_svc(&c,22);assert(c.x[0]==0);}
    puts("Base-game add-on registry, change-event lifetime and policy registration passed");
}
