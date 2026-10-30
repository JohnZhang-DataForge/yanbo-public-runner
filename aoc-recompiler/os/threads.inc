/* SPDX-License-Identifier: GPL-2.0-or-later
 * Cooperative guest threads: Windows fibers / POSIX ucontext. All guest and
 * kernel execution stays on one host thread. No preemption or CPU parallelism.
 */
static HOST_TLS Hos *fiber_os;
static GuestThread *kernel_thread(Hos *os,uint64_t handle) {
    if(handle==HOS_CURRENT_THREAD)return &os->threads[os->current_thread];
    if(handle==HOS_MAIN_HANDLE && os->thread_open)return &os->threads[0];
    Session *s=ipc_session(os,handle);
    if(!s || s->kind!=30 || s->event_index<=0 || s->event_index>=64)return NULL;
    GuestThread *t=&os->threads[s->event_index];return t->handle==handle && t->state!=T_FREE?t:NULL;
}
static void cancel_thread_wait(Hos *os,GuestThread *thread) {
    for(int i=0;i<thread->wait_count;i++)if(thread->waits[i].kind==1) {
        GuestThread *target=&os->threads[thread->waits[i].index];if(target->wait_refs)target->wait_refs--;
    }
    if(thread->lock_ref){GuestThread *owner=&os->threads[thread->lock_owner_slot];if(owner->wait_refs)owner->wait_refs--;thread->lock_ref=0;}
    thread->wait_count=0;thread->wait_kind=0;
}
static void thread_release_host(GuestThread *t) {
#if defined(_WIN32)
    if(t->fiber)DeleteFiber(t->fiber);
#else
    free(t->host_stack);
#endif
    Run *r=(Run *)t->saved_run;
    while(r){Run *parent=r->parent;free(r);r=parent;}
    t->saved_run=NULL;
}
static void reap_guest_threads(Hos *os) {
    for(int i=1;i<64;i++) {
        GuestThread *t=&os->threads[i];
        if(i!=os->current_thread && (t->state==T_EXITED || t->state==T_CREATED) && !t->wait_refs && !ipc_session(os,t->handle)) {
            uint64_t tls=t->storage.tpidrro_el0;
            thread_release_host(t);
            if(tls && !release_stack_view(os,tls,PAGE))stop(os->threads[os->current_thread].ctx,HOS_STOP_TRAP,"host TLS release failed");
            memset(t,0,sizeof(*t));
        }
    }
}
static void threads_destroy(Hos *os) {
    for(int i=1;i<64;i++)if(os->threads[i].state!=T_FREE)thread_release_host(&os->threads[i]);
#if defined(_WIN32)
    if(os->main_fiber_owned)ConvertFiberToThread();
#endif
    if(fiber_os==os)fiber_os=NULL;
}
static uint64_t thread_deadline(int64_t ns) {
    if(ns<0)return UINT64_MAX;
    uint64_t delta=(uint64_t)ns/1000000000ULL*19200000ULL+(uint64_t)ns%1000000000ULL*19200000ULL/1000000000ULL;
    uint64_t now=ticks();if(!delta)delta=1;return delta>UINT64_MAX-now?UINT64_MAX:now+delta;
}
static void thread_poll_waits(Hos *os) {
    uint64_t now=ticks();
    for(int i=0;i<64;i++) {
        GuestThread *t=&os->threads[i];if(t->state!=T_WAIT)continue;
        if(t->wait_kind==1)for(int j=0;j<t->wait_count;j++) {
            WaitObject w=t->waits[j];int ready=w.kind==0?os->applet_events[w.index]!=0:os->threads[w.index].state==T_EXITED;
            if(ready){t->state=T_READY;t->wait_result=0;t->wait_index=j;break;}
        }
        if(t->state==T_WAIT && t->deadline!=UINT64_MAX && now>=t->deadline){t->state=T_READY;t->wait_result=0xea01;}
    }
}
static void thread_switch(Hos *os,int next) {
    int from=os->current_thread;if(from==next){os->threads[from].state=T_RUNNING;return;}
    GuestThread *a=&os->threads[from],*b=&os->threads[next];
    a->saved_run=active;active=(Run *)b->saved_run;os->current_thread=next;b->state=T_RUNNING;fiber_os=os;
#if defined(_WIN32)
    SwitchToFiber(b->fiber);
#else
    if(swapcontext(&a->fiber,&b->fiber))abort();
#endif
    active=(Run *)a->saved_run;
}
static void thread_priorities(Hos *os,int priority[64]) {
    for(int i=0;i<64;i++)priority[i]=os->threads[i].priority;
    for(int pass=0;pass<64;pass++) {
        int changed=0;
        for(int i=0;i<64;i++) {
            GuestThread *w=&os->threads[i];
            if(w->state==T_WAIT && w->wait_kind==3 && w->lock_ref && priority[w->lock_owner_slot]>priority[i]){priority[w->lock_owner_slot]=priority[i];changed=1;}
        }
        if(!changed)break;
    }
}
static void guest_schedule(Ctx *c) {
    Hos *os=owner(c);
    for(;;) {
        thread_poll_waits(os);int next=-1;
        int priority[64];thread_priorities(os,priority);
        for(int n=1;n<=64;n++) {
            int i=(os->current_thread+n)%64;GuestThread *t=&os->threads[i];
            if(t->state==T_READY && (next<0 || priority[i]<priority[next]))next=i;
        }
        if(next>=0) {
            thread_switch(os,next);reap_guest_threads(os);
            if(os->background_fault){int reason=os->background_fault;os->background_fault=0;c->pc=os->background_pc;stop(c,reason,os->background_message);}
            return;
        }
        uint64_t earliest=UINT64_MAX;
        for(int i=0;i<64;i++)if(os->threads[i].state==T_WAIT && os->threads[i].deadline<earliest)earliest=os->threads[i].deadline;
        if(earliest!=UINT64_MAX) {
            uint64_t now=ticks(),remaining=earliest>now?earliest-now:0;
            uint64_t ns=remaining/19200000ULL*1000000000ULL+(remaining%19200000ULL)*1000000000ULL/19200000ULL;
#if defined(_WIN32)
            Sleep((DWORD)(ns/1000000ULL+(ns%1000000ULL!=0)));
#else
            struct timespec duration={(time_t)(ns/1000000000ULL),(long)(ns%1000000000ULL)};
            while(nanosleep(&duration,&duration) && errno==EINTR){}
#endif
            continue;
        }
        const char *why="blocking event wait: no runnable guest thread or asynchronous host event source";
        if(os->current_thread!=0) {
            if(!os->background_fault){os->background_fault=HOS_STOP_UNIMPLEMENTED;os->background_pc=c->pc;snprintf(os->background_message,sizeof(os->background_message),"%s",why);}
            os->threads[0].state=T_READY;continue;
        }
        stop(c,HOS_STOP_UNIMPLEMENTED,why);return;
    }
}
static void guest_thread_entry(GuestThread *t) {
    Hos *os=t->os;int result=hos_run(os,&t->storage,t->entry);
    if(result!=HOS_STOP_RETURNED && result!=HOS_STOP_THREAD_EXIT) {
        os->background_fault=result;os->background_pc=t->storage.pc;
        snprintf(os->background_message,sizeof(os->background_message),"thread %#x: %.155s",t->handle,os->message);
    }
    t->state=T_EXITED;guest_schedule(&t->storage);abort();
}
#if defined(_WIN32)
static VOID WINAPI guest_fiber_entry(void *argument){guest_thread_entry((GuestThread *)argument);}
#else
static void guest_fiber_entry(int slot){guest_thread_entry(&fiber_os->threads[slot]);}
#endif
static int prepare_main_fiber(Hos *os) {
#if defined(_WIN32)
    if(!os->threads[0].fiber) {
        if(IsThreadAFiber())os->threads[0].fiber=GetCurrentFiber();
        else {os->threads[0].fiber=ConvertThreadToFiberEx(NULL,FIBER_FLAG_FLOAT_SWITCH);os->main_fiber_owned=os->threads[0].fiber!=NULL;}
        if(!os->threads[0].fiber)return 0;
    }
#else
    (void)os;
#endif
    return 1;
}
static void create_guest_thread(Ctx *c) {
    Hos *os=owner(c);int32_t priority=(int32_t)c->x[4],core=(int32_t)c->x[5];
    if(priority<0 || priority>63){c->x[0]=0xe001;return;}
    if(core!=0 && core!=-2){c->x[0]=0xe201;return;}
    if((c->x[3]&15) || c->x[3]<16 || !hos_access(os,c->x[3]-16,16,3)){c->x[0]=R_MEMORY;return;}
    A64Fn entry=a64_lookup(c->x[1]);
    if(!entry){char text[128];snprintf(text,sizeof(text),"thread entry has no function: PC %#llx",(unsigned long long)c->x[1]);stop(c,HOS_STOP_TRAP,text);return;}
    reap_guest_threads(os);int slot=0;for(int i=1;i<64;i++)if(os->threads[i].state==T_FREE){slot=i;break;}
    if(!slot){c->x[0]=0x10801;return;}
    uint32_t handle=ipc_new_session(os,30);if(!handle){c->x[0]=0xd201;return;}
    GuestThread *t=&os->threads[slot];memset(t,0,sizeof(*t));t->os=os;t->slot=slot;t->handle=handle;t->entry=entry;t->priority=priority;t->id=++os->next_thread_id;
    uint64_t tls=HOS_TLS_BASE+(uint64_t)slot*PAGE;
    if(!hos_map(os,tls,PAGE,12,3)){ipc_session(os,handle)->handle=0;c->x[0]=0xce01;return;}
    hos_context(os,&t->storage);t->storage.x[0]=c->x[2];t->storage.x[1]=0;t->storage.x[31]=c->x[3];t->storage.pc=c->x[1];t->storage.tpidrro_el0=tls;t->storage.fpcr=c->fpcr;t->storage.fpsr=c->fpsr;t->ctx=&t->storage;
    memcpy(os->memory+tls+0x110,&handle,4);
#if defined(_WIN32)
    t->fiber=CreateFiberEx(65536,8*1024*1024,FIBER_FLAG_FLOAT_SWITCH,guest_fiber_entry,t);
    int ready=t->fiber!=NULL;
#else
    t->host_stack=malloc(8*1024*1024);int ready=t->host_stack && !getcontext(&t->fiber);
    if(ready){t->fiber.uc_stack.ss_sp=t->host_stack;t->fiber.uc_stack.ss_size=8*1024*1024;t->fiber.uc_stack.ss_flags=0;t->fiber.uc_link=NULL;makecontext(&t->fiber,(void (*)(void))guest_fiber_entry,1,slot);}
#endif
    if(!ready){thread_release_host(t);release_stack_view(os,tls,PAGE);ipc_session(os,handle)->handle=0;memset(t,0,sizeof(*t));c->x[0]=0xce01;return;}
    ipc_session(os,handle)->event_index=slot;t->state=T_CREATED;c->x[0]=0;c->x[1]=handle;
}
static void start_guest_thread(Ctx *c) {
    Hos *os=owner(c);GuestThread *t=kernel_thread(os,c->x[0]);
    if(!t){c->x[0]=R_HANDLE;return;}if(t->state!=T_CREATED){c->x[0]=0xfa01;return;}
    if(!prepare_main_fiber(os)){c->x[0]=0xce01;return;}
    t->state=T_READY;c->x[0]=0;os->threads[os->current_thread].state=T_READY;guest_schedule(c);
}
static void sleep_guest_thread(Ctx *c) {
    Hos *os=owner(c);GuestThread *t=&os->threads[os->current_thread];int64_t ns=(int64_t)c->x[0];
    if(ns<=0)t->state=T_READY;
    else{t->wait_kind=2;t->deadline=thread_deadline(ns);t->state=T_WAIT;}
    guest_schedule(c);t->wait_kind=0;
}
static void wait_guest_objects(Ctx *c) {
    Hos *os=owner(c);GuestThread *t=&os->threads[os->current_thread];int32_t count=(int32_t)c->x[2];
    if(count<0 || count>64){c->x[0]=0xee01;return;}
    if(count && !hos_access(os,c->x[1],(uint64_t)count*4,1)){c->x[0]=R_MEMORY;return;}
    WaitObject objects[64];
    for(int i=0;i<count;i++) {
        uint32_t h;memcpy(&h,os->memory+c->x[1]+4*i,4);Session *s=ipc_session(os,h);GuestThread *target=kernel_thread(os,h);
        if(h==HOS_CURRENT_THREAD || h==HOS_CURRENT_PROCESS){c->x[0]=R_HANDLE;return;}
        if(s && s->kind==20)objects[i]=(WaitObject){0,s->event_index};
        else if(target)objects[i]=(WaitObject){1,target->slot};else{c->x[0]=R_HANDLE;return;}
    }
    for(int i=0;i<count;i++) {
        WaitObject w=objects[i];if(w.kind==0?os->applet_events[w.index]!=0:os->threads[w.index].state==T_EXITED){c->x[0]=0;c->x[1]=(uint32_t)i;return;}
    }
    if(!c->x[3]){c->x[0]=0xea01;return;}
    if(t->cancel_pending){t->cancel_pending=0;c->x[0]=0xec01;return;}
    cancel_thread_wait(os,t);memcpy(t->waits,objects,(size_t)count*sizeof(WaitObject));t->wait_count=count;
    for(int i=0;i<count;i++)if(objects[i].kind==1)os->threads[objects[i].index].wait_refs++;
    t->wait_kind=1;t->deadline=thread_deadline((int64_t)c->x[3]);t->wait_result=0;t->wait_index=0;t->state=T_WAIT;
    guest_schedule(c);c->x[0]=t->wait_result;if(!t->wait_result)c->x[1]=(uint32_t)t->wait_index;
    cancel_thread_wait(os,t);reap_guest_threads(os);
}
