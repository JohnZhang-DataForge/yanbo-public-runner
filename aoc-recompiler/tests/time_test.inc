/* SPDX-License-Identifier: GPL-2.0-or-later */
extern uint64_t a64_cntpct_host(void);
static uint64_t time_test_u64(uint64_t address){uint64_t value;memcpy(&value,A64_PTR(address),8);return value;}
static void check_time_service(Hos *os,uint32_t sm) {
    Ctx c;hos_context(os,&c);uint32_t manager=nv_test_service(&c,sm,"time:u");
    uint64_t before=a64_cntpct_host();a64_svc(&c,30);uint64_t after=a64_cntpct_host();
    assert(before<=c.x[0] && c.x[0]<=after); /* CPU and kernel counters share one epoch. */
    ipc_test_request(5,0,0);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(32)==1);
    uint32_t user=applet_test_child(&c,manager,1,0),network=applet_test_child(&c,manager,1,1);
    uint32_t steady=applet_test_child(&c,manager,1,2),zone=applet_test_child(&c,manager,1,3);
    uint32_t local=applet_test_child(&c,manager,1,4);
    time_t wall_before=time(NULL);
    apm_test_domain(user,0,0);c.x[0]=manager;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(40)==0);uint64_t now=time_test_u64(HOS_TLS_BASE+48);
    assert(now+1>=(uint64_t)wall_before && now<=(uint64_t)time(NULL)+1);
    apm_test_domain(local,0,0);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0 && time_test_u64(HOS_TLS_BASE+48)>=now);
    apm_test_domain(network,0,0);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(40)==((102u<<9)|116u));
    apm_test_domain(steady,0,0);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(40)==0);
    uint64_t seconds=time_test_u64(HOS_TLS_BASE+48);uint8_t source[16];memcpy(source,A64_PTR(HOS_TLS_BASE+56),16);
    assert(seconds>=before/19200000ULL && seconds<=a64_cntpct_host()/19200000ULL);
    apm_test_domain(zone,0,0);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0 && !memcmp(A64_PTR(HOS_TLS_BASE+48),"Etc/UTC",8));
    apm_test_domain(zone,101,8);ipc_test_put(48,0);c.x[0]=manager;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(40)==0 && ipc_test_word(48)==0x010107b2); /* 1970-01-01. */
    assert(ipc_test_word(56)==4 && ipc_test_word(60)==0 && !memcmp(A64_PTR(HOS_TLS_BASE+64),"UTC",4));
    apm_test_domain(1,20,0);c.x[0]=manager;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(8)==2);uint32_t memory=ipc_test_word(12);
    uint64_t address=HOS_ALIAS_BASE+0x20000;
    c.x[0]=memory;c.x[1]=address;c.x[2]=4096;c.x[3]=3;a64_svc(&c,19);assert(c.x[0]==0xd801);
    c.x[0]=memory;c.x[1]=address;c.x[2]=4096;c.x[3]=1;a64_svc(&c,19);
    assert(c.x[0]==0 && hos_access(os,address,4096,1) && !hos_access(os,address,4096,2));
    assert(time_test_u64(address+8)==0 && !memcmp(A64_PTR(address+16),source,16));
    assert(!memcmp(A64_PTR(address+0x50),source,16) && memcmp(A64_PTR(address+0x98),source,16));
    int64_t offset;memcpy(&offset,A64_PTR(address+0x40),8);
    assert((uint64_t)(offset+(int64_t)(a64_cntpct_host()/19200000ULL))>=now);
    memory_query(&c,address);assert(memory_info_word(16)==6 && memory_info_word(24)==1);
    c.x[0]=memory;a64_svc(&c,33);assert(c.x[0]==0xe401); /* Not an IPC endpoint. */
    c.x[0]=memory;a64_svc(&c,22);assert(c.x[0]==0 && hos_access(os,address,4096,1));
    apm_test_domain(1,20,0);c.x[0]=manager;a64_svc(&c,33);uint32_t copy=ipc_test_word(12);assert(c.x[0]==0 && copy!=memory);
    c.x[0]=copy;c.x[1]=address;c.x[2]=4096;a64_svc(&c,20);assert(c.x[0]==0 && !hos_access(os,address,1,1));
    c.x[0]=copy;c.x[1]=address;c.x[2]=4096;a64_svc(&c,20);assert(c.x[0]==0xd401);
    apm_test_domain(user,1,8);c.x[0]=manager;assert(hos_run(os,&c,f_08000034)==HOS_STOP_UNIMPLEMENTED);
    c.x[0]=copy;a64_svc(&c,22);assert(c.x[0]==0);
    c.x[0]=manager;a64_svc(&c,22);assert(c.x[0]==0);
    puts("UTC/steady clocks, uninitialized network time, calendar conversion, counter epoch and readonly shared mappings passed");
}
