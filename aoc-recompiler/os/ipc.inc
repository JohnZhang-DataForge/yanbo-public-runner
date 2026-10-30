/* SPDX-License-Identifier: GPL-2.0-or-later
 * Bounded HIPC/CMIF transport for the single-process bootstrap profile.
 * Consulted libnx hipc.h, cmif.h and services/sm.c; no upstream code imported.
 * Included after Hos memory/stop helpers in hos.c.
 */
static uint32_t ipc_read32(const uint8_t *p) { uint32_t v;memcpy(&v,p,4);return v; }
static void ipc_write32(uint8_t *p,uint32_t v) { memcpy(p,&v,4); }
static Session *ipc_session(Hos *os,uint64_t handle) {
    for(unsigned i=0;i<64;i++)if(os->sessions[i].handle && os->sessions[i].handle==handle)return &os->sessions[i];
    return NULL;
}
static uint32_t ipc_new_session(Hos *os,int kind) {
    if(os->next_session==UINT32_MAX)return 0;
    for(unsigned i=0;i<64;i++)if(!os->sessions[i].handle) {
        Session *s=&os->sessions[i];memset(s,0,sizeof(*s));s->kind=kind;s->handle=++os->next_session;return s->handle;
    }
    return 0;
}
static void ipc_connect(Ctx *c) {
    Hos *os=owner(c);char name[12];unsigned i;
    for(i=0;i<sizeof(name);i++) {
        if(c->x[1]>SPACE-1-i || !hos_access(os,c->x[1]+i,1,1)){c->x[0]=R_MEMORY;return;}
        name[i]=(char)os->memory[c->x[1]+i];if(!name[i])break;
    }
    if(i==sizeof(name)){c->x[0]=0xe601;return;} /* InvalidPointer: unterminated port name. */
    if(strcmp(name,"sm:")){c->x[0]=0xf201;return;} /* Kernel NotFound. */
    uint32_t handle=ipc_new_session(os,0);
    if(!handle){c->x[0]=0xd201;return;} /* OutOfHandles. */
    c->x[0]=0;c->x[1]=handle;
}
static uint32_t ipc_new_object(Session *session,int kind) {
    if(session->next_object>=15)return 0;
    uint32_t id=++session->next_object;session->objects[id]=(uint8_t)(kind+1);return id;
}
static void ipc_response(uint8_t *message,uint32_t token,uint32_t result,uint32_t payload_size,uint32_t payload,uint32_t moved_handle,int domain,uint32_t object) {
    memset(message,0,128);
    ipc_write32(message+4,((32+(domain?16:0)+payload_size+(object?4:0)+3)/4)|(moved_handle?0x80000000u:0));
    if(moved_handle){ipc_write32(message+8,1u<<5);ipc_write32(message+12,moved_handle);}
    uint32_t start=16;
    if(domain){ipc_write32(message+start,object?1:0);start+=16;}
    ipc_write32(message+start,0x4f434653); /* SFCO */
    ipc_write32(message+start+8,result);ipc_write32(message+start+12,token);
    if(payload_size)memcpy(message+start+16,&payload,payload_size);
    if(object)ipc_write32(message+start+16+payload_size,object);
}
static void ipc_send(Ctx *c) {
    Hos *os=owner(c);Session *session=ipc_session(os,c->x[0]);
    if(!session){c->x[0]=R_HANDLE;return;}
    uint64_t address=c->tpidrro_el0;
    if(!hos_access(os,address,128,3)){c->x[0]=R_MEMORY;return;}
    uint8_t request[128];memcpy(request,os->memory+address,sizeof(request));
    uint32_t h0=ipc_read32(request),h1=ipc_read32(request+4),type=h0&0xffff;
    uint32_t words=h1&0x3ff,raw=8,special=0;
    uint32_t nx=(h0>>16)&15,na=(h0>>20)&15;
    if(nx>1 || na>1 || h0>>24 || (h1&0x7ffffc00u)) {
        stop(c,HOS_STOP_UNIMPLEMENTED,"IPC buffer descriptors/receive lists are not implemented");return;
    }
    if(h1>>31) {
        special=ipc_read32(request+8);raw=12;
        if(special&~1u){stop(c,HOS_STOP_UNIMPLEMENTED,"IPC input handles are not implemented");return;}
        if(special&1)raw+=8; /* PID is supplied by the host process, not this placeholder. */
    }
    uint64_t buffer=0,length=0;
    uint32_t descriptors=raw;
    raw+=nx*8+na*12;
    if(raw>128){c->x[0]=R_SIZE;return;}
    if(words>(128-raw)/4){c->x[0]=R_SIZE;return;}
    uint32_t end=raw+words*4,start=(raw+15)&~15u;
    if(type==2 && !words && !special) {
        session->handle=0;memset(os->memory+address,0,128);c->x[0]=0;return;
    }
    if(type!=4 && type!=6 && type!=5 && type!=7) {
        stop(c,HOS_STOP_UNIMPLEMENTED,"IPC message type is not implemented");return;
    }
    int domain=session->domain && (type==4 || type==6),kind=session->kind;
    if(domain) {
        if(start+16>end){c->x[0]=0x1c01;return;}
        uint32_t dh=ipc_read32(request+start),object=ipc_read32(request+start+4),size=dh>>16;
        if(!object || object>15 || !session->objects[object]){c->x[0]=R_HANDLE;return;}
        if((dh&0xff)==2 && !(dh&0xff00)) {
            session->objects[object]=0;memset(os->memory+address,0,128);c->x[0]=0;return;
        }
        if((dh&0xffff)!=1 || size<16 || size>end-start-16){c->x[0]=0x1c01;return;}
        kind=session->objects[object]-1;start+=16;end=start+size;
    }
    if(start+16>end || ipc_read32(request+start)!=0x49434653 || ipc_read32(request+start+4)>1) {
        c->x[0]=0x1c01;return; /* InvalidArgument: malformed CMIF. */
    }
    uint32_t command=ipc_read32(request+start+8),token=ipc_read32(request+start+12);
    if(os->trace)fprintf(stderr,"[HOS] IPC kind=%d type=%u cmd=%u pid=%u bytes=%u\n",kind,type,command,special&1,end-start-16);
    if((nx || na) && !(kind==2 && (type==4 || type==6) && command==0)) {
        stop(c,HOS_STOP_UNIMPLEMENTED,"IPC buffer descriptors are supported only for logger packets");return;
    }
    if(nx) {
        uint32_t x=ipc_read32(request+descriptors);
        length=x>>16;buffer=ipc_read32(request+descriptors+4);
        if(length && (x&0xffc0u)){c->x[0]=R_MEMORY;return;}
    }
    if(na) {
        uint32_t off=descriptors+nx*8,a=ipc_read32(request+off+8);
        uint64_t size=ipc_read32(request+off)|((uint64_t)((a>>24)&15)<<32);
        if(size) {
            if(length || (a&0xf0fffffcu)){c->x[0]=0x1c01;return;}
            buffer=ipc_read32(request+off+4);length=size;
        }
    }
    if((type==5 || type==7) && command==0) {
        if(session->domain){c->x[0]=0xfa01;return;}
        session->domain=1;session->next_object=1;session->objects[1]=(uint8_t)(session->kind+1);
        ipc_response(os->memory+address,token,0,4,1,0,0,0);c->x[0]=0;return;
    }
    if((type==5 || type==7) && command==3) { /* QueryPointerBufferSize: none supported. */
        ipc_response(os->memory+address,token,0,2,0,0,0,0);c->x[0]=0;return;
    }
    if((type==4 || type==6) && kind==1 && command==0) { /* lm OpenLogger */
        if(!(special&1) || end-start-16<8){c->x[0]=0x1c01;return;}
        uint32_t object=domain?ipc_new_object(session,2):0;
        uint32_t handle=domain?0:ipc_new_session(os,2);
        if(!object && !handle){c->x[0]=0xd201;return;}
        ipc_response(os->memory+address,token,0,0,0,handle,domain,object);c->x[0]=0;return;
    }
    if((type==4 || type==6) && kind==2) {
        if(command==1 && end-start-16>=4) {
            os->log_destination=ipc_read32(request+start+16);
            ipc_response(os->memory+address,token,0,0,0,0,domain,0);c->x[0]=0;return;
        }
        if(command==0 && (nx || na)) {
            if(length>4096){c->x[0]=R_SIZE;return;}
            if(!hos_access(os,buffer,length,1)){c->x[0]=R_MEMORY;return;}
            if(os->log_destination && length) {
                fprintf(stderr,"[guest lm packet %llu bytes]",(unsigned long long)length);
                for(uint64_t i=0;i<length;i++)fprintf(stderr,"%02x",os->memory[buffer+i]);
                fputc('\n',stderr);
            }
            ipc_response(os->memory+address,token,0,0,0,0,domain,0);c->x[0]=0;return;
        }
    }
    if((type==4 || type==6) && kind==0) {
        if(command==0) { /* SM RegisterClient. */
            if(!(special&1) || end-start-16<8){c->x[0]=0x1c01;return;}
            session->initialized=1;
            ipc_response(os->memory+address,token,0,0,0,0,domain,0);c->x[0]=0;return;
        }
        if(command==1 && session->initialized && end-start-16>=8) {
            char name[9];memcpy(name,request+start+16,8);name[8]=0;
            if(!memcmp(name,"lm\0\0\0\0\0\0",8)) {
                uint32_t object=domain?ipc_new_object(session,1):0;
                uint32_t handle=domain?0:ipc_new_session(os,1);
                if(!object && !handle){c->x[0]=0xd201;return;}
                ipc_response(os->memory+address,token,0,0,0,handle,domain,object);c->x[0]=0;return;
            }
            /* Unsupported services do not receive successful dummy sessions. */
            for(unsigned i=0;i<8;i++)if(name[i] && ((unsigned char)name[i]<32 || (unsigned char)name[i]>126))name[i]='?';
            char text[160];snprintf(text,sizeof(text),"SM GetService '%s': service backend is not implemented",name);
            stop(c,HOS_STOP_UNIMPLEMENTED,text);return;
        }
    }
    char text[128];snprintf(text,sizeof(text),"IPC kind=%d: unsupported type %u command %u (initialized=%d)",kind,type,command,session->initialized);
    stop(c,HOS_STOP_UNIMPLEMENTED,text);
}
