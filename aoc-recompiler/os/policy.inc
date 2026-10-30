/* SPDX-License-Identifier: GPL-2.0-or-later
 * Process registration for pctl. Permission/PIN/policy operations are not
 * implemented and are never silently answered as authorized.
 */
static int policy_request(Ctx *c,Session *session,int kind,int domain,uint32_t command,uint32_t token,uint32_t size,int pid) {
    Hos *os=owner(c);
    if(kind==16 && (command==0 || command==1)) {
        if(!pid || size<8){c->x[0]=0x1c01;return 1;}
        return applet_child(c,session,17,domain,token);
    }
    if(kind==17 && command==1) {
        os->policy_initialized=1;
        ipc_response(os->memory+c->tpidrro_el0,token,0,0,0,0,domain,0);c->x[0]=0;return 1;
    }
    return 0;
}
