/* SPDX-License-Identifier: GPL-2.0-or-later */
extern void f_08000188(Ctx *A64_NOESC);
static void transfer_create(Ctx *c,uint64_t address,uint64_t size,uint32_t permission) {
    c->x[0]=0;c->x[1]=address;c->x[2]=size;c->x[3]=permission;a64_svc(c,21);
}
static void check_transfer_memory(Hos *os) {
    Ctx c;hos_context(os,&c);uint64_t address=HOS_HEAP_BASE+0x100000;
    memset(A64_PTR(address),0x39,8192);
    transfer_create(&c,address+1,8192,0);assert(c.x[0]==0xcc01);
    transfer_create(&c,address,0,0);assert(c.x[0]==0xca01);
    transfer_create(&c,address,8192,2);assert(c.x[0]==0xd801);
    transfer_create(&c,HOS_TLS_BASE,4096,0);assert(c.x[0]==0xd401);
    for(uint32_t mode=0;mode<=3;mode++)if(mode!=2) {
        c.x[0]=0;c.x[1]=address;c.x[2]=8192;c.x[3]=mode;
        assert(hos_run(os,&c,f_08000188)==HOS_STOP_RETURNED && c.x[0]==0);
        uint32_t handle=(uint32_t)c.x[1];assert(handle);
        assert(hos_access(os,address,8192,1)==(mode!=0));
        assert(hos_access(os,address,8192,2)==(mode==3));
        memory_query(&c,address);assert(memory_info_word(16)==5 && memory_info_word(20)==1 && memory_info_word(24)==mode);
        transfer_create(&c,address,4096,0);assert(c.x[0]==0xd401);
        c.x[0]=address;c.x[1]=8192;c.x[2]=3;a64_svc(&c,2);assert(c.x[0]==0xd401);
        c.x[1]=0;a64_svc(&c,1);assert(c.x[0]==0xd401 && hos_heap_size(os)==0x200000);
        c.x[0]=handle;a64_svc(&c,33);assert(c.x[0]==0xe401); /* Not an IPC endpoint. */
        c.x[0]=handle;a64_svc(&c,22);assert(c.x[0]==0 && hos_access(os,address,8192,3));
        assert(*(uint32_t *)A64_PTR(address)==0x39393939 && *((uint8_t *)A64_PTR(address)+8191)==0x39);
        memory_query(&c,address);assert(memory_info_word(20)==0 && memory_info_word(24)==3);
        c.x[0]=handle;a64_svc(&c,22);assert(c.x[0]==0xe401);
    }
    uint32_t handles[64];unsigned count=0;
    for(;count<64;count++) {
        transfer_create(&c,address+count*4096,4096,0);
        if(c.x[0]){assert(c.x[0]==0xd201);break;}
        handles[count]=(uint32_t)c.x[1];
    }
    transfer_create(&c,address+count*4096,4096,0);
    assert(c.x[0]==0xd201 && hos_access(os,address+count*4096,4096,3));
    for(unsigned i=0;i<count;i++){c.x[0]=handles[i];a64_svc(&c,22);assert(c.x[0]==0);}
    assert(hos_access(os,address,8192,3));
    puts("Transfer-object permissions, locked pages, heap protection, close restoration and stale handles passed");
}
