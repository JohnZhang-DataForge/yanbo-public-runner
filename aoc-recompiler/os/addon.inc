/* SPDX-License-Identifier: GPL-2.0-or-later
 * Base-game content profile: the installed add-on registry is empty.
 * No DLC installation, purchasing, entitlement or content preparation is faked.
 */
static int addon_request(Ctx *c,int domain,uint32_t command,uint32_t token,uint32_t size,int pid) {
    Hos *os=owner(c);uint8_t *out=os->memory+c->tpidrro_el0;
    if(command==8)return applet_event(c,3,domain,token); /* List-changed notification. */
    if(command==2) { /* CountAddOnContent(client PID). */
        if(!pid || size<8){c->x[0]=0x1c01;return 1;}
        ipc_response(out,token,0,4,0,0,domain,0);c->x[0]=0;return 1;
    }
    if(command==7) { /* Nothing can be prepared in an empty installed registry. */
        if(!pid || size<16){c->x[0]=0x1c01;return 1;}
        ipc_response(out,token,0xf201,0,0,0,domain,0);c->x[0]=0;return 1;
    }
    return 0;
}
