/* SPDX-License-Identifier: GPL-2.0-or-later
 * Stateful nvdrv transport/ownership. GPU commands require an explicit backend.
 * ABI consulted: libnx services/nv.c. No upstream implementation code copied.
 */
static uint32_t nv_release_session(Hos *os,Session *session) {
    if(session->nv_index<0)return 0;
    NvClient *client=&os->nv_clients[session->nv_index];
    if(client->refs==1) {
        if(client->transfer_index>=0) {
            uint32_t result=transfer_release(os,&os->sessions[client->transfer_index]);
            if(result)return result;
        }
        memset(client,0,sizeof(*client));
    } else client->refs--;
    session->nv_index=-1;return 0;
}
static void nv_clone_session(Ctx *c,Session *source,uint32_t token) {
    Hos *os=owner(c);NvClient *client=&os->nv_clients[source->nv_index];
    uint32_t handle=ipc_allocate_session(os,18);
    if(!handle){c->x[0]=0xd201;return;}
    ipc_session(os,handle)->nv_index=source->nv_index;client->refs++;
    ipc_response(os->memory+c->tpidrro_el0,token,0,0,0,handle,0,0);c->x[0]=0;
}
static void nv_reply(Ctx *c,uint32_t token,uint32_t size,uint32_t value,uint32_t error) {
    uint8_t *out=owner(c)->memory+c->tpidrro_el0;
    ipc_response(out,token,0,size==8?4:size,value,0,0,0);
    if(size==8){ipc_write32(out+4,ipc_read32(out+4)+1);ipc_write32(out+36,error);}
    c->x[0]=0;
}
static int nv_request(Ctx *c,Session *session,uint32_t command,uint32_t token,
                      const uint8_t *data,uint32_t size,int pid,uint32_t copies,const uint32_t *handles,
                      uint64_t buffer,uint64_t length,uint32_t buffers) {
    Hos *os=owner(c);NvClient *client=&os->nv_clients[session->nv_index];
    if(command==3) { /* Initialize: copied process + transfer-memory handles. */
        if(copies!=2 || size<4 || buffers){c->x[0]=0x1c01;return 1;}
        if(!process_handle(os,handles[0])){c->x[0]=R_HANDLE;return 1;}
        Session *transfer=ipc_session(os,handles[1]);
        if(!transfer || transfer->kind!=31){c->x[0]=R_HANDLE;return 1;}
        uint32_t requested=ipc_read32(data);
        if(!requested || requested>transfer->transfer_size || transfer->transfer_permission!=0){c->x[0]=R_MEMORY;return 1;}
        if(client->initialized){ipc_response(os->memory+c->tpidrro_el0,token,0xfa01,0,0,0,0,0);c->x[0]=0;return 1;}
        transfer->transfer_refs++;client->transfer_index=(int)(transfer-os->sessions);client->initialized=1;
        nv_reply(c,token,4,0,0);return 1;
    }
    if(command==8) { /* SetClientPID supplies an applet resource user ID, with kernel PID. */
        if(!pid || size<8 || buffers){c->x[0]=0x1c01;return 1;}
        uint64_t aruid;memcpy(&aruid,data,8);
        if(!client->initialized || aruid!=1){ipc_response(os->memory+c->tpidrro_el0,token,0xfa01,0,0,0,0,0);c->x[0]=0;return 1;}
        client->aruid=aruid;nv_reply(c,token,4,0,0);return 1;
    }
    if(command==13) { /* Firmware memory-margin configuration, u64 input/no output. */
        if(size<8 || buffers || copies){c->x[0]=0x1c01;return 1;}
        if(!client->initialized){ipc_response(os->memory+c->tpidrro_el0,token,0xfa01,0,0,0,0,0);c->x[0]=0;return 1;}
        memcpy(&client->firmware_margin_request,data,8);
        nv_reply(c,token,0,0,0);return 1;
    }
    if(command==0) { /* Open: register a typed device FD; no GPU execution implied. */
        if(!client->initialized){nv_reply(c,token,8,UINT32_MAX,3);return 1;}
        if(buffers!=1 || !length || length>=64){c->x[0]=R_SIZE;return 1;}
        if(!hos_access(os,buffer,length,1)){c->x[0]=R_MEMORY;return 1;}
        char path[64];memcpy(path,os->memory+buffer,(size_t)length);path[length]=0;
        /* A single trailing NUL is permitted; hidden suffixes are not device names. */
        if(memchr(path,0,(size_t)length-1)){nv_reply(c,token,8,UINT32_MAX,4);return 1;}
        static const char *const devices[]={"/dev/nvmap","/dev/nvhost-as-gpu","/dev/nvhost-gpu","/dev/nvhost-ctrl","/dev/nvhost-ctrl-gpu"};
        uint8_t kind=0;for(unsigned i=0;i<5;i++)if(!strcmp(path,devices[i]))kind=(uint8_t)(i+1);
        if(!kind){nv_reply(c,token,8,UINT32_MAX,2);return 1;}
        for(unsigned i=0;i<64;i++)if(!client->fds[i].id && client->next_fd<UINT32_MAX-1) {
            NvFd *fd=&client->fds[i];fd->id=++client->next_fd;fd->kind=kind;
            if(os->trace)fprintf(stderr,"[HOS] nvdrv Open '%s' -> %u\n",path,fd->id);
            nv_reply(c,token,8,fd->id,0);return 1;
        }
        nv_reply(c,token,8,UINT32_MAX,6);return 1;
    }
    if(command==2) { /* Close */
        if(size<4){c->x[0]=0x1c01;return 1;}
        uint32_t id=ipc_read32(data);
        for(unsigned i=0;i<64;i++)if(id && client->fds[i].id==id) {
            memset(&client->fds[i],0,sizeof(client->fds[i]));nv_reply(c,token,4,0,0);return 1;
        }
        nv_reply(c,token,4,4,0);return 1;
    }
    return 0;
}
