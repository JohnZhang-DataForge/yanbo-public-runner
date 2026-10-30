/* SPDX-License-Identifier: GPL-2.0-or-later
 * Transfer-object ownership with retained nvdrv copies. Remote maps are unsupported.
 * Consulted Atmosphere transfer-memory SVC/object and page-table contracts.
 */
static void create_transfer_memory(Ctx *c) {
    Hos *os=owner(c);uint64_t address=c->x[1],size=c->x[2];uint32_t permission=(uint32_t)c->x[3];
    if(address&(PAGE-1)){c->x[0]=R_ADDRESS;return;}
    if(!size || size&(PAGE-1)){c->x[0]=R_SIZE;return;}
    if(!range(address,size)){c->x[0]=R_MEMORY;return;}
    if(permission!=0 && permission!=1 && permission!=3){c->x[0]=R_PERMISSION;return;}
    for(uint64_t i=address/PAGE;i<(address+size)/PAGE;i++) {
        Page p=os->pages[i];
        if(p.state!=5 || p.permission!=3 || p.attribute){c->x[0]=R_MEMORY;return;}
    }
    uint32_t handle=ipc_new_session(os,31);
    if(!handle){c->x[0]=0xd201;return;}
    Session *object=ipc_session(os,handle);
    if(!host_protect(os,address,size,permission)){object->handle=0;c->x[0]=R_RESOURCE;return;}
    object->transfer_refs=1;object->transfer_address=address;object->transfer_size=size;object->transfer_permission=permission;
    for(uint64_t i=address/PAGE;i<(address+size)/PAGE;i++) {
        os->pages[i].permission=(uint8_t)permission;os->pages[i].attribute=1;
    }
    c->x[0]=0;c->x[1]=handle;
}
static uint32_t transfer_release(Hos *os,Session *object) {
    if(object->transfer_refs>1){object->transfer_refs--;return 0;}
    if(object->transfer_refs!=1)return R_HANDLE;
    uint64_t address=object->transfer_address,size=object->transfer_size;
    if(!host_protect(os,address,size,3))return R_RESOURCE;
    for(uint64_t i=address/PAGE;i<(address+size)/PAGE;i++) {
        os->pages[i].permission=3;os->pages[i].attribute=0;
    }
    object->transfer_refs=0;return 0;
}

static uint32_t close_transfer_memory(Hos *os,Session *object) {
    uint32_t result=transfer_release(os,object);
    if(!result)object->handle=0;
    return result;
}
