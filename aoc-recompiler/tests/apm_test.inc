/* SPDX-License-Identifier: GPL-2.0-or-later */
static void apm_test_domain(uint32_t object,uint32_t command,uint32_t size) {
    memset(A64_PTR(HOS_TLS_BASE),0,128);
    ipc_test_put(0,6);ipc_test_put(4,(48+size+3)/4);
    ipc_test_put(16,1u|((16+size)<<16));ipc_test_put(20,object);
    ipc_test_put(32,0x49434653);ipc_test_put(40,command);
}
static void check_apm(Hos *os,uint32_t sm) {
    Ctx c;hos_context(os,&c);
    ipc_test_request(4,1,0);memcpy((uint8_t *)A64_PTR(HOS_TLS_BASE)+32,"apm\0\0\0\0\0",8);
    c.x[0]=sm;assert(hos_run(os,&c,f_08000034)==HOS_STOP_RETURNED && c.x[0]==0);
    uint32_t manager=ipc_test_word(12);assert(manager);
    ipc_test_request(4,1,0);c.x[0]=manager;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(24)==0 && ipc_test_word(32)==0); /* Normal */
    ipc_test_request(4,0,0);c.x[0]=manager;a64_svc(&c,33);
    uint32_t session=ipc_test_word(12);assert(c.x[0]==0 && session && session!=manager);
    ipc_test_request(4,1,0);ipc_test_put(32,0);c.x[0]=session;
    assert(hos_run(os,&c,f_08000034)==HOS_STOP_UNIMPLEMENTED);
    assert(strstr(hos_stop_message(os),"no guest configuration selected"));
    for(uint32_t mode=0;mode<2;mode++) {
        ipc_test_request(4,0,0);ipc_test_put(32,mode);ipc_test_put(36,0x12340000+mode);
        c.x[0]=session;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(24)==0);
    }
    /* Invalid mode and truncated payload cannot alter previously stored values. */
    ipc_test_request(4,0,0);ipc_test_put(32,2);ipc_test_put(36,0xdeadbeef);
    c.x[0]=session;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(24)==0xf001);
    ipc_test_request(4,1,0);ipc_test_put(32,0xffffffff);c.x[0]=session;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(24)==0xf001);
    ipc_test_request(4,0,0);ipc_test_put(4,7);c.x[0]=session;a64_svc(&c,33);assert(c.x[0]==0x1c01);
    for(uint32_t mode=0;mode<2;mode++) {
        ipc_test_request(4,1,0);ipc_test_put(32,mode);c.x[0]=session;a64_svc(&c,33);
        assert(c.x[0]==0 && ipc_test_word(24)==0 && ipc_test_word(32)==0x12340000+mode);
    }
    /* Manager closure must not invalidate the independently returned session. */
    c.x[0]=manager;a64_svc(&c,22);assert(c.x[0]==0);
    ipc_test_request(4,1,0);ipc_test_put(32,1);c.x[0]=session;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(32)==0x12340001);
    c.x[0]=session;a64_svc(&c,22);assert(c.x[0]==0);
    /* A second manager shares process performance state, including domain objects. */
    ipc_test_request(4,1,0);memcpy((uint8_t *)A64_PTR(HOS_TLS_BASE)+32,"apm\0\0\0\0\0",8);
    c.x[0]=sm;a64_svc(&c,33);manager=ipc_test_word(12);assert(c.x[0]==0);
    ipc_test_request(5,0,0);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(32)==1);
    apm_test_domain(1,0,0);c.x[0]=manager;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(16)==1 && ipc_test_word(32)==0x4f434653);
    uint32_t object=ipc_test_word(48);assert(object==2);
    apm_test_domain(object,1,4);ipc_test_put(48,0);c.x[0]=manager;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(40)==0 && ipc_test_word(48)==0x12340000);
    apm_test_domain(object,0,8);ipc_test_put(48,1);ipc_test_put(52,0x5678);
    c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(40)==0);
    apm_test_domain(object,1,4);ipc_test_put(48,1);c.x[0]=manager;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(48)==0x5678);
    apm_test_domain(object,0,8);ipc_test_put(48,1);ipc_test_put(52,0);
    c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(40)==0);
    apm_test_domain(object,1,4);ipc_test_put(48,1);c.x[0]=manager;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(40)==0 && ipc_test_word(48)==0); /* Zero is still a selected value. */
    c.x[0]=manager;a64_svc(&c,22);assert(c.x[0]==0);
    c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0xe401);
    puts("APM mode, configuration isolation/readback, domains and session ownership passed");
}
