/* SPDX-License-Identifier: GPL-2.0-or-later
 * MapMemory/UnmapMemory for the single-owner CPU bootstrap profile.
 * Source access is revoked while the stack view owns the bytes. Copy-back on
 * exact unmap preserves guest CPU behavior without exposing simultaneous views.
 * This is not a shared physical-memory/DMA alias implementation.
 */
static int release_stack_view(Hos *os,uint64_t address,uint64_t size) {
#if defined(_WIN32)
    if(!VirtualFree(os->memory+address,(SIZE_T)size,MEM_DECOMMIT))return 0;
#else
    memset(os->memory+address,0,(size_t)size);
    if(!host_protect(os,address,size,0))return 0;
    (void)madvise(os->memory+address,(size_t)size,MADV_DONTNEED);
#endif
    memset(os->pages+address/PAGE,0,(size_t)(size/PAGE)*sizeof(Page));return 1;
}
static uint32_t stack_alias(Ctx *c,int unmap) {
    Hos *os=owner(c);uint64_t dst=c->x[0],src=c->x[1],size=c->x[2];
    if((dst|src)&(PAGE-1))return R_ADDRESS;
    if(!size || size&(PAGE-1))return R_SIZE;
    if(!range(src,size) || !range(dst,size))return R_MEMORY;
    if(dst<HOS_STACK_REGION_BASE || size>HOS_STACK_REGION_SIZE ||
       dst-HOS_STACK_REGION_BASE>HOS_STACK_REGION_SIZE-size)return 0xdc01;
    if(src<dst+size && dst<src+size)return R_MEMORY;
    Alias *slot=NULL;
    if(unmap) {
        for(unsigned i=0;i<64;i++)if(os->aliases[i].size && os->aliases[i].src==src && os->aliases[i].dst==dst && os->aliases[i].size==size){slot=&os->aliases[i];break;}
        if(!slot)return R_MEMORY; /* Exact mappings only in this profile. */
        for(uint64_t i=src/PAGE;i<(src+size)/PAGE;i++)if(os->pages[i].attribute!=1 || os->pages[i].permission)return R_MEMORY;
        for(uint64_t i=dst/PAGE;i<(dst+size)/PAGE;i++)if(os->pages[i].state!=11 || os->pages[i].permission!=3 || os->pages[i].attribute)return R_MEMORY;
        if(!host_protect(os,src,size,3))return 0xce01;
        memcpy(os->memory+src,os->memory+dst,(size_t)size);
        if(!release_stack_view(os,dst,size)) {
            memcpy(os->memory+dst,os->memory+src,(size_t)size);
            if(!host_protect(os,src,size,0))stop(c,HOS_STOP_TRAP,"host stack unmap rollback failed");
            return 0xce01;
        }
        for(uint64_t i=src/PAGE;i<(src+size)/PAGE;i++){os->pages[i].permission=3;os->pages[i].attribute=0;}
        memset(slot,0,sizeof(*slot));return 0;
    }
    for(unsigned i=0;i<64;i++)if(!os->aliases[i].size){slot=&os->aliases[i];break;}
    if(!slot)return 0xce01;
    if(!hos_access(os,src,size,3))return R_MEMORY;
    uint8_t state=os->pages[src/PAGE].state;
    if(state!=3 && state!=5)return R_MEMORY; /* Coarse RW image or normal heap. */
    for(uint64_t i=src/PAGE;i<(src+size)/PAGE;i++)if(os->pages[i].state!=state || os->pages[i].attribute)return R_MEMORY;
    for(uint64_t i=dst/PAGE;i<(dst+size)/PAGE;i++)if(os->pages[i].state)return R_MEMORY;
    if(!hos_map(os,dst,size,11,3))return 0xce01;
    memcpy(os->memory+dst,os->memory+src,(size_t)size);
    if(!host_protect(os,src,size,0)) {
        if(!release_stack_view(os,dst,size))stop(c,HOS_STOP_TRAP,"host stack map rollback failed");
        return 0xce01;
    }
    for(uint64_t i=src/PAGE;i<(src+size)/PAGE;i++){os->pages[i].permission=0;os->pages[i].attribute=1;}
    *slot=(Alias){src,dst,size};return 0;
}
