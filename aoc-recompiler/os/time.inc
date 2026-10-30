/* SPDX-License-Identifier: GPL-2.0-or-later
 * Read-only host UTC/monotonic clock profile. No network time synchronization.
 * ABI/layouts consulted: libnx services/time.c and services/time.h.
 */
static const uint8_t time_source_id[16]={'a','o','c','-','h','o','s','-','c','l','o','c','k','-','0','1'};
static int time_initialize(Hos *os) {
    if(os->clock_ready)return 1;
    time_t now=time(NULL);if(now==(time_t)-1)return 0;
    int64_t steady=(int64_t)(ticks()/19200000ULL);os->clock_offset=(int64_t)now-steady;
    uint8_t context[32]={0};memcpy(context+8,time_source_id,16); /* Steady base_time=0. */
    memcpy(os->clock_page+8,context,24);memcpy(os->clock_page+32,context,24);
    memcpy(context,&os->clock_offset,8);memcpy(context+8,&steady,8);memcpy(context+16,time_source_id,16);
    memcpy(os->clock_page+0x40,context,32);memcpy(os->clock_page+0x60,context,32);
    /* Network context remains uninitialized (different/zero source ID).
     * Automatic network-clock correction is disabled. Sequence counters are 0. */
    os->clock_ready=1;return 1;
}
static void time_reply(Ctx *c,uint32_t token,int domain,uint32_t result,const void *data,uint32_t size) {
    uint8_t *out=owner(c)->memory+c->tpidrro_el0;
    ipc_response(out,token,result,0,0,0,domain,0);
    if(size){ipc_write32(out+4,ipc_read32(out+4)+(size+3)/4);memcpy(out+(domain?48:32),data,size);}
    c->x[0]=0;
}
static int time_request(Ctx *c,Session *session,int kind,int domain,uint32_t command,uint32_t token,
                        const uint8_t *data,uint32_t size) {
    Hos *os=owner(c);
    if(!time_initialize(os)){stop(c,HOS_STOP_TRAP,"host UTC clock unavailable");return 1;}
    if(kind==19) {
        if(command<=4)return applet_child(c,session,21+(int)command,domain,token);
        if(command==20) {
            uint32_t handle=ipc_new_session(os,32);
            if(!handle){c->x[0]=0xd201;return 1;}
            ipc_response(os->memory+c->tpidrro_el0,token,0,0,0,handle,domain,0);
            ipc_write32(os->memory+c->tpidrro_el0+8,2);c->x[0]=0;return 1;
        }
        if(command==100){uint8_t disabled=0;time_reply(c,token,domain,0,&disabled,1);return 1;}
    }
    if(kind==21 || kind==22 || kind==25) { /* User, network and local system clocks. */
        if(command==0 || command==2) {
            if(kind==22){time_reply(c,token,domain,(102u<<9)|116u,NULL,0);return 1;}
            int64_t now=os->clock_offset+(int64_t)(ticks()/19200000ULL);
            if(command==0)time_reply(c,token,domain,0,&now,8);
            else time_reply(c,token,domain,0,os->clock_page+0x40,32);
            return 1;
        }
    }
    if(kind==23) { /* StandardSteadyClock */
        if(command==0) {
            uint8_t point[24];uint64_t steady=ticks()/19200000ULL;
            memcpy(point,&steady,8);memcpy(point+8,time_source_id,16);
            time_reply(c,token,domain,0,point,sizeof(point));return 1;
        }
        if(command==200){uint64_t offset=0;time_reply(c,token,domain,0,&offset,8);return 1;}
    }
    if(kind==24) { /* TimeZoneService: UTC only, no host timezone mutation. */
        if(command==0){char name[36]={0};memcpy(name,"Etc/UTC",7);time_reply(c,token,domain,0,name,sizeof(name));return 1;}
        if(command==2){uint32_t count=1;time_reply(c,token,domain,0,&count,4);return 1;}
        if(command==101) {
            if(size<8){c->x[0]=0x1c01;return 1;}
            uint64_t raw;memcpy(&raw,data,8);time_t timestamp=(time_t)raw;
            if(raw>INT64_MAX || (uint64_t)timestamp!=raw){time_reply(c,token,domain,R_ENUM,NULL,0);return 1;}
            struct tm *utc=gmtime(&timestamp);
            if(!utc || utc->tm_year< -1899 || utc->tm_year>30867){time_reply(c,token,domain,R_ENUM,NULL,0);return 1;}
            uint8_t result[32]={0};int16_t year=(int16_t)(utc->tm_year+1900);
            memcpy(result,&year,2);result[2]=(uint8_t)(utc->tm_mon+1);result[3]=(uint8_t)utc->tm_mday;
            result[4]=(uint8_t)utc->tm_hour;result[5]=(uint8_t)utc->tm_min;result[6]=(uint8_t)utc->tm_sec;
            ipc_write32(result+8,(uint32_t)utc->tm_wday);ipc_write32(result+12,(uint32_t)utc->tm_yday);
            memcpy(result+16,"UTC",3);time_reply(c,token,domain,0,result,sizeof(result));return 1;
        }
    }
    return 0;
}
static uint32_t time_map_shared(Ctx *c,int unmap) {
    Hos *os=owner(c);Session *object=ipc_session(os,c->x[0]);uint64_t address=c->x[1],size=c->x[2];
    if(address&(PAGE-1))return R_ADDRESS;
    if(size!=PAGE)return R_SIZE;
    if(!object || object->kind!=32)return R_HANDLE;
    if(address<HOS_ALIAS_BASE || address>HOS_ALIAS_BASE+HOS_ALIAS_SIZE-PAGE)return R_MEMORY;
    if(!unmap && c->x[3]!=1)return R_PERMISSION;
    if(unmap) {
        for(unsigned i=0;i<16;i++)if(os->clock_mappings[i]==address) {
#if defined(_WIN32)
            if(!VirtualFree(os->memory+address,PAGE,MEM_DECOMMIT))return R_RESOURCE;
#else
            if(!host_protect(os,address,PAGE,0))return R_RESOURCE;
            (void)madvise(os->memory+address,PAGE,MADV_DONTNEED);
#endif
            os->pages[address/PAGE]=(Page){0};os->clock_mappings[i]=0;return 0;
        }
        return R_MEMORY;
    }
    for(unsigned i=0;i<16;i++)if(!os->clock_mappings[i]) {
        if(os->pages[address/PAGE].state)return R_MEMORY;
        if(!hos_map(os,address,PAGE,6,3))return R_RESOURCE;
        memcpy(os->memory+address,os->clock_page,PAGE);
        if(!host_protect(os,address,PAGE,1)) {
            if(!release_stack_view(os,address,PAGE))stop(c,HOS_STOP_TRAP,"clock shared mapping rollback failed");
            return R_RESOURCE;
        }
        os->pages[address/PAGE].permission=1;os->clock_mappings[i]=address;return 0;
    }
    return R_RESOURCE;
}
