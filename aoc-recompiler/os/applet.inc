/* SPDX-License-Identifier: GPL-2.0-or-later
 * One foreground application in the headless startup profile. No library
 * applet execution or display backend is implied by these state interfaces.
 */
static void ipc_response_u64(uint8_t *message,uint32_t token,uint64_t value,int domain) {
    ipc_response(message,token,0,4,(uint32_t)value,0,domain,0);
    ipc_write32(message+4,ipc_read32(message+4)+1);
    ipc_write32(message+(domain?52:36),(uint32_t)(value>>32));
}
static int applet_child(Ctx *c,Session *session,int kind,int domain,uint32_t token) {
    Hos *os=owner(c);uint32_t object=domain?ipc_new_object(session,kind):0;
    uint32_t handle=domain?0:ipc_new_session(os,kind);
    if(!object && !handle){c->x[0]=0xd201;return 1;}
    ipc_response(os->memory+c->tpidrro_el0,token,0,0,0,handle,domain,object);c->x[0]=0;return 1;
}
static int applet_event(Ctx *c,int index,int domain,uint32_t token) {
    Hos *os=owner(c);uint32_t handle=ipc_new_session(os,20);
    if(!handle){c->x[0]=0xd201;return 1;}
    ipc_session(os,handle)->event_index=index;
    ipc_response(os->memory+c->tpidrro_el0,token,0,0,0,handle,domain,0);
    ipc_write32(os->memory+c->tpidrro_el0+8,2); /* One copy handle, not a moved object. */
    c->x[0]=0;return 1;
}
static int applet_request(Ctx *c,Session *session,int kind,int domain,uint32_t command,uint32_t token,
                          const uint8_t *data,uint32_t size,int pid,int copies,uint32_t process) {
    Hos *os=owner(c);uint8_t *out=os->memory+c->tpidrro_el0;
    if(kind==5 && command==0) { /* appletOE OpenApplicationProxy */
        if(!pid || copies!=1 || size<8){c->x[0]=0x1c01;return 1;}
        if(!process_handle(os,process)){c->x[0]=R_HANDLE;return 1;}
        return applet_child(c,session,6,domain,token);
    }
    if(kind==6) { /* IApplicationProxy */
        /* Library/audio/display/debug factories provide typed object identities.
         * Their operation methods still stop until their backends are implemented. */
        int child=command==0?7:command==1?8:command==2?9:command==20?10:
                  command==11?11:command==3?12:command==4?13:command==1000?14:-1;
        if(child>=0)return applet_child(c,session,child,domain,token);
    }
    if(kind==7) { /* ICommonStateGetter */
        if(command==0)return applet_event(c,0,domain,token);
        if(command==2) {
            uint32_t result=0,value=0;
            if(!os->applet_count)result=0x680; /* AM NoMessages. */
            else {
                value=os->applet_messages[os->applet_head];os->applet_head=(os->applet_head+1)%16;
                os->applet_count--;if(!os->applet_count)os->applet_events[0]=0;
            }
            ipc_response(out,token,result,4,value,0,domain,0);c->x[0]=0;return 1;
        }
        if(command==5 || command==6 || command==9) {
            uint32_t value=command==9?1:0; /* Handheld, Normal performance, InFocus. */
            ipc_response(out,token,0,command==6?4:1,value,0,domain,0);c->x[0]=0;return 1;
        }
    }
    if(kind==8) { /* ISelfController */
        if(command==1 || command==2) {
            os->applet_exit_locked=command==1;
            ipc_response(out,token,0,0,0,0,domain,0);c->x[0]=0;return 1;
        }
        if(command==10 || command==11 || command==12 || command==13 || command==14 || command==16) {
            uint32_t required=command==10?4:command==13?3:1;
            if(size<required){c->x[0]=0x1c01;return 1;}
            uint32_t value=command==10?ipc_read32(data):data[0];
            if(command==13) {
                if(data[0]>1 || data[1]>1 || data[2]>1){c->x[0]=R_ENUM;return 1;}
                value=data[0]|(data[1]<<1)|(data[2]<<2);
            } else if(value>(command==10?2u:1u)){c->x[0]=R_ENUM;return 1;}
            os->applet_self_flags[command]=(uint8_t)value;
            ipc_response(out,token,0,0,0,0,domain,0);c->x[0]=0;return 1;
        }
        if(command==90){ipc_response_u64(out,token,0,domain);c->x[0]=0;return 1;} /* Never suspended. */
        if(command==91)return applet_event(c,1,domain,token);
    }
    if(kind==9) { /* IWindowController */
        if(command==1){ipc_response_u64(out,token,1,domain);c->x[0]=0;return 1;}
        if(command==10) {
            os->applet_foreground=1;ipc_response(out,token,0,0,0,0,domain,0);c->x[0]=0;return 1;
        }
    }
    if(kind==10 && command==130)return applet_event(c,2,domain,token); /* GPU-error notification. */
    if(kind==10 && command==40) { /* IApplicationFunctions NotifyRunning */
        os->applet_running=1;ipc_response(out,token,0,1,1,0,domain,0);c->x[0]=0;return 1;
    }
    return 0;
}
