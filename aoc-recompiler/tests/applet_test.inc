/* SPDX-License-Identifier: GPL-2.0-or-later */
extern void f_0800003c(Ctx *A64_NOESC);
static void applet_test_open_proxy(uint32_t process) {
    memset(A64_PTR(HOS_TLS_BASE),0,128);ipc_test_put(0,6);ipc_test_put(4,14u|0x80000000u);
    ipc_test_put(8,3);ipc_test_put(20,process); /* PID and one copied process handle. */
    ipc_test_put(32,1u|(24u<<16));ipc_test_put(36,1);ipc_test_put(48,0x49434653);
}
static uint32_t applet_test_child(Ctx *c,uint32_t manager,uint32_t object,uint32_t command) {
    apm_test_domain(object,command,0);c->x[0]=manager;a64_svc(c,33);
    assert(c->x[0]==0 && ipc_test_word(16)==1 && ipc_test_word(40)==0);
    return ipc_test_word(48);
}
static void applet_test_poll(Ctx *c,uint32_t a,uint32_t b,int64_t timeout) {
    uint32_t handles[2]={a,b};memcpy(A64_PTR(HOS_HEAP_BASE+2048),handles,sizeof(handles));
    c->x[0]=0;c->x[1]=HOS_HEAP_BASE+2048;c->x[2]=b?2:1;c->x[3]=(uint64_t)timeout;a64_svc(c,24);
}
static void check_applet(Hos *os,uint32_t sm) {
    Ctx c;hos_context(os,&c);
    ipc_test_request(4,1,0);memcpy((uint8_t *)A64_PTR(HOS_TLS_BASE)+32,"appletOE",8);
    c.x[0]=sm;a64_svc(&c,33);assert(c.x[0]==0);uint32_t manager=ipc_test_word(12);
    /* Missing PID is rejected even with a valid copied process handle. */
    ipc_test_request(4,0,0);ipc_test_put(4,10u|0x80000000u);ipc_test_put(8,2);ipc_test_put(12,HOS_CURRENT_PROCESS);
    c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0x1c01);
    ipc_test_request(5,0,0);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0);
    applet_test_open_proxy(123);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0xe401);
    applet_test_open_proxy(HOS_CURRENT_PROCESS);c.x[0]=manager;
    assert(hos_run(os,&c,f_08000034)==HOS_STOP_RETURNED && c.x[0]==0);
    uint32_t proxy=ipc_test_word(48);assert(proxy==2);
    uint32_t common=applet_test_child(&c,manager,proxy,0);
    uint32_t self=applet_test_child(&c,manager,proxy,1);
    uint32_t window=applet_test_child(&c,manager,proxy,2);
    uint32_t functions=applet_test_child(&c,manager,proxy,20);
    apm_test_domain(common,1,0);c.x[0]=manager;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(40)==0 && ipc_test_word(48)==15); /* Initial foreground focus transition. */
    applet_test_open_proxy(HOS_CURRENT_PROCESS);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(48)!=proxy);
    apm_test_domain(common,1,0);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(40)==0x680); /* No duplicate launch message. */
    applet_test_child(&c,manager,proxy,11);
    uint32_t audio=applet_test_child(&c,manager,proxy,3);
    applet_test_child(&c,manager,proxy,4);
    applet_test_child(&c,manager,proxy,1000);
    apm_test_domain(audio,99,0);c.x[0]=manager;
    assert(hos_run(os,&c,f_08000034)==HOS_STOP_UNIMPLEMENTED); /* Object creation is not an implemented audio backend. */
    const uint32_t commands[3]={5,6,9},expected[3]={0,0,1};
    for(unsigned i=0;i<3;i++){
        apm_test_domain(common,commands[i],0);c.x[0]=manager;a64_svc(&c,33);
        assert(c.x[0]==0 && ipc_test_word(40)==0 && ipc_test_word(48)==expected[i]);
    }
    assert(hos_applet_cpu_boost_mode(os)==0);
    apm_test_domain(common,66,4);ipc_test_put(48,1);c.x[0]=manager;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(40)==0 && hos_applet_cpu_boost_mode(os)==1);
    apm_test_domain(common,66,4);ipc_test_put(48,2);c.x[0]=manager;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(40)==0xf001 && hos_applet_cpu_boost_mode(os)==1);
    apm_test_domain(common,66,0);c.x[0]=manager;a64_svc(&c,33);
    assert(c.x[0]==0x1c01 && hos_applet_cpu_boost_mode(os)==1);
    apm_test_domain(common,67,0);c.x[0]=manager;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(40)==0 && hos_applet_cpu_boost_mode(os)==0);
    apm_test_domain(window,1,0);c.x[0]=manager;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(48)==1 && ipc_test_word(52)==0); /* Full u64 response. */
    apm_test_domain(window,10,0);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0);
    apm_test_domain(functions,40,0);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(48)==1);
    apm_test_domain(self,10,4);ipc_test_put(48,2);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0);
    apm_test_domain(self,10,4);ipc_test_put(48,3);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0xf001);
    for(unsigned cmd=11;cmd<=12;cmd++){
        apm_test_domain(self,cmd,1);ipc_test_put(48,1);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0);
    }
    apm_test_domain(self,13,3);ipc_test_put(48,0x010001);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0);
    apm_test_domain(self,13,3);ipc_test_put(48,2);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0xf001);
    apm_test_domain(common,0,0);c.x[0]=manager;a64_svc(&c,33);
    uint32_t event1=ipc_test_word(12);assert(c.x[0]==0 && ipc_test_word(8)==2 && ipc_test_word(16)==0);
    apm_test_domain(common,0,0);c.x[0]=manager;a64_svc(&c,33);uint32_t event2=ipc_test_word(12);
    assert(c.x[0]==0 && event2!=event1);
    c.x[0]=0;c.x[1]=0;c.x[2]=0;c.x[3]=0;a64_svc(&c,24);assert(c.x[0]==0xea01);
    c.x[2]=~0ULL;a64_svc(&c,24);assert(c.x[0]==0xee01);
    apm_test_domain(functions,130,0);c.x[0]=manager;a64_svc(&c,33);
    uint32_t gpu_event=ipc_test_word(12);assert(c.x[0]==0 && ipc_test_word(8)==2);
    applet_test_poll(&c,gpu_event,0,0);assert(c.x[0]==0xea01);
    applet_test_poll(&c,event1,0,0);assert(c.x[0]==0xea01);
    c.x[0]=event1;a64_svc(&c,23);assert(c.x[0]==0xfa01);
    c.x[0]=event1;a64_svc(&c,33);assert(c.x[0]==0xe401); /* Kernel events are not IPC endpoints. */
    applet_test_poll(&c,event1,0,0);c.x[0]=0;c.x[3]=~0ULL;
    assert(hos_run(os,&c,f_0800003c)==HOS_STOP_UNIMPLEMENTED);
    assert(strstr(hos_stop_message(os),"blocking event wait"));
    assert(hos_applet_post_message(os,0x11223344) && hos_applet_post_message(os,0x55));
    applet_test_poll(&c,event1,event2,~0LL);assert(c.x[0]==0 && c.x[1]==0);
    applet_test_poll(&c,gpu_event,event2,0);assert(c.x[0]==0 && c.x[1]==1);
    applet_test_poll(&c,event1,0xffffffff,0);assert(c.x[0]==0xe401); /* Validate every handle first. */
    c.x[0]=event1;a64_svc(&c,18);assert(c.x[0]==0);
    applet_test_poll(&c,event2,0,0);assert(c.x[0]==0xea01); /* Copies share the same event. */
    assert(hos_applet_post_message(os,0x66));
    uint32_t messages[3]={0x11223344,0x55,0x66};
    for(unsigned i=0;i<3;i++){
        apm_test_domain(common,1,0);c.x[0]=manager;a64_svc(&c,33);
        assert(c.x[0]==0 && ipc_test_word(40)==0 && ipc_test_word(48)==messages[i]);
    }
    applet_test_poll(&c,event2,0,0);assert(c.x[0]==0xea01);
    apm_test_domain(common,1,0);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(40)==0x680);
    for(unsigned i=0;i<16;i++)assert(hos_applet_post_message(os,i));
    assert(!hos_applet_post_message(os,999));
    for(unsigned i=0;i<16;i++){
        apm_test_domain(common,1,0);c.x[0]=manager;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(48)==i);
    }
    hos_applet_signal_gpu_error(os);
    applet_test_poll(&c,gpu_event,0,0);assert(c.x[0]==0 && c.x[1]==0);
    c.x[0]=gpu_event;a64_svc(&c,23);assert(c.x[0]==0);
    applet_test_poll(&c,gpu_event,0,0);assert(c.x[0]==0xea01);
    c.x[0]=event1;a64_svc(&c,22);assert(c.x[0]==0);
    applet_test_poll(&c,event1,0,0);assert(c.x[0]==0xe401);
    c.x[0]=manager;a64_svc(&c,22);assert(c.x[0]==0);
    applet_test_poll(&c,event2,0,0);assert(c.x[0]==0xea01); /* Independent copied-handle lifetime. */
    c.x[0]=event2;a64_svc(&c,22);assert(c.x[0]==0);
    c.x[0]=gpu_event;a64_svc(&c,22);assert(c.x[0]==0);
    puts("Applet proxy validation, state queries, message queue, copied events and polling passed");
}
