/* SPDX-License-Identifier: GPL-2.0-or-later
 * User mutex/condition-variable queues on the cooperative scheduler.
 * No host execution switches between releasing a mutex and enqueueing a wait.
 */
#define MUTEX_WAIT_MASK 0x40000000u
static int sync_word(Hos *os,uint64_t address,uint32_t *value) {
    if((address&3) || !hos_access(os,address,4,3))return 0;
    memcpy(value,os->memory+address,4);return 1;
}
static void sync_write(Hos *os,uint64_t address,uint32_t value) {memcpy(os->memory+address,&value,4);}
static void sync_owner(Hos *os,GuestThread *waiter,int slot) {
    if(waiter->lock_ref && os->threads[waiter->lock_owner_slot].wait_refs)os->threads[waiter->lock_owner_slot].wait_refs--;
    waiter->lock_ref=1;waiter->lock_owner_slot=slot;os->threads[slot].wait_refs++;
}
static GuestThread *sync_best(Hos *os,int kind,uint64_t address,int owner) {
    GuestThread *best=NULL;int priority[64];thread_priorities(os,priority);
    for(int i=0;i<64;i++) {
        GuestThread *t=&os->threads[i];if(t->state!=T_WAIT || t->wait_kind!=kind)continue;
        if((kind==4?t->sync_key:t->sync_address)!=address)continue;
        if(owner>=0 && (!t->lock_ref || t->lock_owner_slot!=owner))continue;
        if(!best || priority[t->slot]<priority[best->slot] || (priority[t->slot]==priority[best->slot] && t->wait_order<best->wait_order))best=t;
    }
    return best;
}
static void sync_yield(Ctx *c) {Hos *os=owner(c);os->threads[os->current_thread].state=T_READY;guest_schedule(c);}
static uint32_t sync_handoff(Hos *os,uint64_t address) {
    uint32_t old;if(!sync_word(os,address,&old))return R_MEMORY;
    GuestThread *next=sync_best(os,3,address,os->current_thread);
    uint32_t tag=next?next->sync_tag:0;
    if(next) {
        next->state=T_READY;next->wait_result=0;
        for(int i=0;i<64;i++) {
            GuestThread *w=&os->threads[i];
            if(w->state==T_WAIT && w->wait_kind==3 && w->sync_address==address && w->lock_ref && w->lock_owner_slot==os->current_thread){sync_owner(os,w,next->slot);tag|=MUTEX_WAIT_MASK;}
        }
    }
    sync_write(os,address,tag);return 0;
}
static uint32_t sync_lock(Ctx *c) {
    Hos *os=owner(c);uint64_t address=c->x[1];uint32_t value;
    if(address&3)return R_ADDRESS;
    if(!sync_word(os,address,&value))return R_MEMORY;
    uint32_t owner_handle=(uint32_t)c->x[0];
    if(value!=(owner_handle|MUTEX_WAIT_MASK))return 0;
    GuestThread *lock_owner=kernel_thread(os,owner_handle),*self=&os->threads[os->current_thread];
    if(!lock_owner || owner_handle==HOS_CURRENT_THREAD || owner_handle==HOS_CURRENT_PROCESS)return R_HANDLE;
    cancel_thread_wait(os,self);self->sync_address=address;self->sync_tag=(uint32_t)c->x[2];
    self->wait_kind=3;self->wait_order=++os->next_wait_order;self->deadline=UINT64_MAX;self->wait_result=0;
    sync_owner(os,self,lock_owner->slot);self->state=T_WAIT;guest_schedule(c);
    uint32_t result=self->wait_result;cancel_thread_wait(os,self);reap_guest_threads(os);return result;
}
static uint32_t sync_unlock(Ctx *c,uint64_t address) {
    if(address&3)return R_ADDRESS;
    uint32_t result=sync_handoff(owner(c),address);
    if(!result)sync_yield(c);
    return result;
}
static uint32_t sync_wait_key(Ctx *c) {
    Hos *os=owner(c);uint64_t address=c->x[0],key=c->x[1]&~3ULL;int64_t timeout=(int64_t)c->x[3];
    uint32_t value;if(address&3)return R_ADDRESS;
    if(!sync_word(os,address,&value))return R_MEMORY;
    uint32_t result=sync_handoff(os,address);if(result)return result;
    if(hos_access(os,key,4,2))sync_write(os,key,1);
    if(!timeout){sync_yield(c);return 0xea01;}
    GuestThread *self=&os->threads[os->current_thread];cancel_thread_wait(os,self);
    self->sync_address=address;self->sync_key=key;self->sync_tag=(uint32_t)c->x[2];
    self->wait_kind=4;self->deadline=thread_deadline(timeout);self->wait_order=++os->next_wait_order;self->wait_result=0;
    self->state=T_WAIT;guest_schedule(c);result=self->wait_result;cancel_thread_wait(os,self);reap_guest_threads(os);return result;
}
static void sync_signal_key(Ctx *c) {
    Hos *os=owner(c);uint64_t key=c->x[0]&~3ULL;int32_t count=(int32_t)c->x[1];int signaled=0;
    while(count<=0 || signaled<count) {
        GuestThread *w=sync_best(os,4,key,-1);if(!w)break;
        uint32_t value;w->deadline=UINT64_MAX;
        if(!sync_word(os,w->sync_address,&value)){w->wait_result=R_MEMORY;w->state=T_READY;}
        else if(!value){sync_write(os,w->sync_address,w->sync_tag);w->wait_result=0;w->state=T_READY;}
        else {
            GuestThread *lock_owner=kernel_thread(os,value&~MUTEX_WAIT_MASK);
            if(!lock_owner){w->wait_result=0xfa01;w->state=T_READY;}
            else{sync_write(os,w->sync_address,value|MUTEX_WAIT_MASK);w->wait_kind=3;w->wait_order=++os->next_wait_order;sync_owner(os,w,lock_owner->slot);}
        }
        signaled++;
    }
    if(!sync_best(os,4,key,-1) && hos_access(os,key,4,2))sync_write(os,key,0);
    if(signaled)sync_yield(c);
}
