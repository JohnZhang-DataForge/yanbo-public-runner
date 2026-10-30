/* SPDX-License-Identifier: GPL-2.0-or-later */
static uint32_t nv_test_service(Ctx *c,uint32_t sm,const char *name) {
    ipc_test_request(4,1,0);memset((uint8_t *)A64_PTR(HOS_TLS_BASE)+32,0,8);
    memcpy((uint8_t *)A64_PTR(HOS_TLS_BASE)+32,name,strlen(name));c->x[0]=sm;a64_svc(c,33);
    assert(c->x[0]==0 && ipc_test_word(24)==0);return ipc_test_word(12);
}
static void nv_test_initialize(Ctx *c,uint32_t driver,uint32_t process,uint32_t transfer,uint32_t size) {
    memset(A64_PTR(HOS_TLS_BASE),0,128);ipc_test_put(0,4);ipc_test_put(4,8u|0x80000000u);
    ipc_test_put(8,4);ipc_test_put(12,process);ipc_test_put(16,transfer);
    ipc_test_put(32,0x49434653);ipc_test_put(40,3);ipc_test_put(48,size);
    c->x[0]=driver;a64_svc(c,33);
}
static void nv_test_open(Ctx *c,uint32_t driver,const char *path) {
    uint64_t address=HOS_HEAP_BASE+0x1800;size_t size=strlen(path);
    memcpy(A64_PTR(address),path,size);
    memset(A64_PTR(HOS_TLS_BASE),0,128);ipc_test_put(0,4u|(1u<<20));ipc_test_put(4,7);
    ipc_test_put(8,(uint32_t)size);ipc_test_put(12,(uint32_t)address);
    ipc_test_put(32,0x49434653);c->x[0]=driver;a64_svc(c,33);
}
static void check_nvdrv(Hos *os,uint32_t sm) {
    Ctx c;hos_context(os,&c);uint64_t address=HOS_HEAP_BASE+0x100000;
    memset(A64_PTR(address),0x6b,8192);
    uint32_t driver=nv_test_service(&c,sm,"nvdrv");
    nv_test_open(&c,driver,"/dev/nvmap");assert(c.x[0]==0 && ipc_test_word(32)==UINT32_MAX && ipc_test_word(36)==3);
    transfer_create(&c,address,8192,0);assert(c.x[0]==0);uint32_t transfer=(uint32_t)c.x[1];
    nv_test_initialize(&c,driver,123,transfer,8192);assert(c.x[0]==0xe401);
    nv_test_initialize(&c,driver,HOS_CURRENT_PROCESS,sm,8192);assert(c.x[0]==0xe401);
    nv_test_initialize(&c,driver,HOS_CURRENT_PROCESS,transfer,0);assert(c.x[0]==0xd401);
    nv_test_initialize(&c,driver,HOS_CURRENT_PROCESS,transfer,12288);assert(c.x[0]==0xd401);
    nv_test_initialize(&c,driver,HOS_CURRENT_PROCESS,transfer,8192);assert(c.x[0]==0 && ipc_test_word(24)==0);
    nv_test_initialize(&c,driver,HOS_CURRENT_PROCESS,transfer,8192);assert(c.x[0]==0 && ipc_test_word(24)==0xfa01);
    /* The service copied the transfer object, not the caller's handle value. */
    c.x[0]=transfer;a64_svc(&c,22);assert(c.x[0]==0 && !hos_access(os,address,8192,1));
    c.x[0]=transfer;a64_svc(&c,22);assert(c.x[0]==0xe401);
    for(unsigned i=0;i<70;i++){
        uint64_t name=HOS_HEAP_BASE+256;memcpy(A64_PTR(name),"sm:",4);c.x[1]=name;a64_svc(&c,31);
        assert(c.x[0]==0);uint32_t temporary=(uint32_t)c.x[1];
        c.x[0]=temporary;a64_svc(&c,22);assert(c.x[0]==0);
    }
    assert(!hos_access(os,address,8192,1)); /* Closed transfer slot cannot be reused while pinned. */
    ipc_test_request(4,8,0);ipc_test_put(32,1);c.x[0]=driver;a64_svc(&c,33);assert(c.x[0]==0x1c01);
    ipc_test_request(4,8,1);ipc_test_put(48,1);c.x[0]=driver;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(24)==0);
    ipc_test_request(4,13,0);ipc_test_put(32,1);c.x[0]=driver;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(24)==0);
    ipc_test_request(4,13,0);ipc_test_put(4,6);c.x[0]=driver;a64_svc(&c,33);assert(c.x[0]==0x1c01);
    ipc_test_request(5,4,0);ipc_test_put(32,1);c.x[0]=driver;a64_svc(&c,33);
    assert(c.x[0]==0 && ipc_test_word(24)==0);uint32_t clone=ipc_test_word(12);assert(clone!=driver);
    nv_test_open(&c,driver,"/dev/absent");assert(c.x[0]==0 && ipc_test_word(36)==2);
    nv_test_open(&c,driver,"/dev/nvmap");assert(c.x[0]==0 && ipc_test_word(36)==0);uint32_t fd=ipc_test_word(32);
    ipc_test_request(4,2,0);ipc_test_put(32,fd);c.x[0]=clone;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(32)==0);
    ipc_test_request(4,2,0);ipc_test_put(32,fd);c.x[0]=driver;a64_svc(&c,33);assert(c.x[0]==0 && ipc_test_word(32)==4);
    nv_test_open(&c,clone,"/dev/nvmap");assert(c.x[0]==0 && ipc_test_word(36)==0 && ipc_test_word(32)!=fd);
    ipc_test_request(4,1,0);c.x[0]=clone;
    assert(hos_run(os,&c,f_08000034)==HOS_STOP_UNIMPLEMENTED); /* Opening a device doesn't imply GPU execution. */
    c.x[0]=driver;a64_svc(&c,22);assert(c.x[0]==0 && !hos_access(os,address,8192,1));
    c.x[0]=clone;a64_svc(&c,22);assert(c.x[0]==0 && hos_access(os,address,8192,3));
    assert(*(uint32_t *)A64_PTR(address)==0x6b6b6b6b);
    /* Peer-close releases the driver's ref, but the caller's own handle pins memory. */
    driver=nv_test_service(&c,sm,"nvdrv:a");
    transfer_create(&c,address,8192,0);assert(c.x[0]==0);transfer=(uint32_t)c.x[1];
    nv_test_initialize(&c,driver,HOS_CURRENT_PROCESS,transfer,8192);assert(c.x[0]==0 && ipc_test_word(24)==0);
    memset(A64_PTR(HOS_TLS_BASE),0,128);ipc_test_put(0,2);c.x[0]=driver;a64_svc(&c,33);
    assert(c.x[0]==0 && !hos_access(os,address,8192,1));
    c.x[0]=driver;a64_svc(&c,22);assert(c.x[0]==0);
    c.x[0]=transfer;a64_svc(&c,22);assert(c.x[0]==0 && hos_access(os,address,8192,3));
    const char *aliases[]={"nvdrv:s","nvdrv:t"};
    for(unsigned i=0;i<2;i++){driver=nv_test_service(&c,sm,aliases[i]);c.x[0]=driver;a64_svc(&c,22);assert(c.x[0]==0);}
    puts("nvdrv initialization, copied transfer ownership, shared-session clones, device lifetime and peer close passed");
}
