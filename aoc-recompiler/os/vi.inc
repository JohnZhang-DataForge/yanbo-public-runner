/* SPDX-License-Identifier: GPL-2.0-or-later
 * Logical handheld display configuration and typed VI clients. No rendering.
 * ABI consulted: libnx services/vi.c and vi.h; original implementation.
 */
static void vi_reply(Ctx *c,uint32_t token,int domain,uint32_t result,const void *data,uint32_t size) {
    uint8_t *out=owner(c)->memory+c->tpidrro_el0;
    ipc_response(out,token,result,0,0,0,domain,0);
    if(size){ipc_write32(out+4,ipc_read32(out+4)+(size+3)/4);memcpy(out+(domain?48:32),data,size);}
    c->x[0]=0;
}
static int vi_child(Ctx *c,Session *session,int kind,int domain,uint32_t token) {
    Hos *os=owner(c);int level=session->vi_level;
    uint32_t object=domain?ipc_new_object(session,kind):0;
    uint32_t handle=domain?0:ipc_new_session(os,kind);
    if(!object && !handle){c->x[0]=0xd201;return 1;}
    if(handle)ipc_session(os,handle)->vi_level=level;
    ipc_response(os->memory+c->tpidrro_el0,token,0,0,0,handle,domain,object);c->x[0]=0;return 1;
}
static int vi_request(Ctx *c,Session *session,int kind,int domain,uint32_t command,uint32_t token,
                      const uint8_t *data,uint32_t size) {
    Hos *os=owner(c);int level=session->vi_level;
    if(kind==40 || kind==46 || kind==47) {
        if(command==(uint32_t)level) {
            if(size<4){c->x[0]=0x1c01;return 1;}
            uint32_t policy=ipc_read32(data);
            if(policy!=(level?1u:0u)){vi_reply(c,token,domain,R_ENUM,NULL,0);return 1;}
            return vi_child(c,session,41,domain,token);
        }
    }
    if(kind==41) { /* IApplicationDisplayService */
        if(command==100)return vi_child(c,session,44,domain,token);
        if(command==101 || command==102 || command==103) {
            int required=command==102?2:1;
            if(level<required){vi_reply(c,token,domain,0xfa01,NULL,0);return 1;}
            return vi_child(c,session,command==101?42:command==102?43:45,domain,token);
        }
        if(command==1010) { /* OpenDisplay */
            if(size<64){c->x[0]=0x1c01;return 1;}
            if(memcmp(data,"Default\0",8)){vi_reply(c,token,domain,R_ENUM,NULL,0);return 1;}
            if(os->display_refs==UINT32_MAX){vi_reply(c,token,domain,R_RESOURCE,NULL,0);return 1;}
            os->display_refs++;uint64_t id=1;vi_reply(c,token,domain,0,&id,8);return 1;
        }
        if(command==1020 || command==1102) {
            if(size<8){c->x[0]=0x1c01;return 1;}
            uint64_t id;memcpy(&id,data,8);
            if(id!=1 || !os->display_refs){vi_reply(c,token,domain,R_HANDLE,NULL,0);return 1;}
            if(command==1020){os->display_refs--;vi_reply(c,token,domain,0,NULL,0);return 1;}
            int64_t resolution[2]={1280,720};vi_reply(c,token,domain,0,resolution,16);return 1;
        }
    }
    if(kind==42 && (command==1200 || command==1202 || command==1203)) {
        if(size<8){c->x[0]=0x1c01;return 1;}
        uint64_t id;memcpy(&id,data,8);
        if(id!=1 || !os->display_refs){vi_reply(c,token,domain,R_HANDLE,NULL,0);return 1;}
        if(command==1203){int32_t resolution[2]={1280,720};vi_reply(c,token,domain,0,resolution,8);return 1;}
        int64_t z=command==1200?0:255;vi_reply(c,token,domain,0,&z,8);return 1;
    }
    return 0;
}
