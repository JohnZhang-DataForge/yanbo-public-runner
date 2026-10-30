/* SPDX-License-Identifier: GPL-2.0-or-later */
extern void f_08000044(Ctx *A64_NOESC);
extern void f_0800004c(Ctx *A64_NOESC);
static uint32_t memory_info_word(unsigned offset) {uint32_t v;memcpy(&v,(uint8_t *)A64_PTR(HOS_TLS_BASE)+offset,4);return v;}
static void memory_query(Ctx *c,uint64_t address){c->x[0]=HOS_TLS_BASE;c->x[2]=address;a64_svc(c,6);assert(c->x[0]==0);}
static void memory_call(Ctx *c,uint32_t svc,uint64_t dst,uint64_t src,uint64_t size){c->x[0]=dst;c->x[1]=src;c->x[2]=size;a64_svc(c,svc);}
static void check_stack_mapping(Hos *os) {
    Ctx c;hos_context(os,&c);uint64_t src=HOS_HEAP_BASE+0x4000,dst=HOS_STACK_BASE+HOS_STACK_SIZE+0x20000;
    memset(A64_PTR(src),0x5a,8192);
    memory_call(&c,4,dst+1,src,8192);assert(c.x[0]==0xcc01 && !hos_access(os,dst,1,1));
    memory_call(&c,4,dst,src,0);assert(c.x[0]==0xca01);
    memory_call(&c,4,HOS_ALIAS_BASE,src,8192);assert(c.x[0]==0xdc01);
    memory_call(&c,4,HOS_STACK_BASE,src,8192);assert(c.x[0]==0xd401);
    memory_call(&c,4,dst,0,8192);assert(c.x[0]==0xd401);
    c.x[0]=src;c.x[1]=8192;c.x[2]=1;a64_svc(&c,2);assert(c.x[0]==0);
    memory_call(&c,4,dst,src,8192);assert(c.x[0]==0xd401 && !hos_access(os,dst,1,1));
    c.x[0]=src;c.x[1]=8192;c.x[2]=3;a64_svc(&c,2);assert(c.x[0]==0);
    c.x[0]=dst;c.x[1]=src;c.x[2]=8192;
    assert(hos_run(os,&c,f_08000044)==HOS_STOP_RETURNED && c.x[0]==0);
    assert(!hos_access(os,src,8192,1) && hos_access(os,dst,8192,3));
    assert(*(uint32_t *)A64_PTR(dst)==0x5a5a5a5a && *((uint8_t *)A64_PTR(dst)+8191)==0x5a);
    *(uint32_t *)A64_PTR(dst)=0x12345678;*((uint8_t *)A64_PTR(dst)+8191)=0x9a;
    memory_query(&c,src);assert(memory_info_word(16)==5 && memory_info_word(20)==1 && memory_info_word(24)==0);
    memory_query(&c,dst);assert(memory_info_word(16)==11 && memory_info_word(20)==0 && memory_info_word(24)==3);
    c.x[0]=src;c.x[1]=8192;c.x[2]=3;a64_svc(&c,2);assert(c.x[0]==0xd401 && !hos_access(os,src,1,1));
    c.x[1]=0;a64_svc(&c,1);assert(c.x[0]==0xd401 && hos_heap_size(os)==0x200000);
    memory_call(&c,4,dst+0x10000,src,8192);assert(c.x[0]==0xd401 && !hos_access(os,dst+0x10000,1,1));
    memory_call(&c,5,dst,src,4096);assert(c.x[0]==0xd401 && !hos_access(os,src,1,1));
    memory_call(&c,5,dst,src+4096,8192);assert(c.x[0]==0xd401);
    c.x[0]=dst;c.x[1]=src;c.x[2]=8192;
    assert(hos_run(os,&c,f_0800004c)==HOS_STOP_RETURNED && c.x[0]==0);
    assert(hos_access(os,src,8192,3) && !hos_access(os,dst,1,1));
    assert(*(uint32_t *)A64_PTR(src)==0x12345678 && *((uint8_t *)A64_PTR(src)+8191)==0x9a);
    memory_query(&c,src);assert(memory_info_word(20)==0 && memory_info_word(24)==3);
    memory_query(&c,dst);assert(memory_info_word(16)==0 && memory_info_word(24)==0);
    memory_call(&c,5,dst,src,8192);assert(c.x[0]==0xd401);
    for(unsigned i=0;i<64;i++){memory_call(&c,4,dst+4096*i,src+4096*i,4096);assert(c.x[0]==0);}
    memory_call(&c,4,dst+4096*64,src+4096*64,4096);assert(c.x[0]==0xce01);
    for(unsigned i=0;i<64;i++){memory_call(&c,5,dst+4096*i,src+4096*i,4096);assert(c.x[0]==0);}
    puts("Stack ownership transfer, protections, query attributes, copy-back, rejection and mapping limits passed");
}
