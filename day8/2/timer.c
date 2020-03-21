#include "bootpack.h"
#define PIT_CTRL    0x0043
#define PIT_CNT0    0x0040
#define TIMER_FLAGS_ALLOC   1
#define TIMER_FLAGS_USING   2

struct TIMERCTL timerctl;

void init_pit(void){
    int i;
    struct TIMER *t;
    io_out8(PIT_CTRL, 0x34);
    io_out8(PIT_CNT0, 0x9c);
    io_out8(PIT_CNT0, 0x2e);
    timerctl.count = 0;
    for(i=0; i<MAX_TIMER; i++){
        timerctl.timers0[i].flags = 0;
    }
    t = timer_alloc();
    t->timeout = 0xffffffff;
    t->flags = TIMER_FLAGS_USING;
    t->next = 0;
    timerctl.t0 = t;
    timerctl.next = 0xffffffff;
    return ;
}

struct TIMER *timer_alloc(void){
    int i;
    for(i=0; i<MAX_TIMER; i++){
        if(timerctl.timers0[i].flags == 0){
            timerctl.timers0[i].flags = TIMER_FLAGS_ALLOC;
            return &timerctl.timers0[i];
        }
    }
    return 0;
}

void timer_free(struct TIMER *timer){
    timer->flags = 0;
    return ;
}

void timer_init(struct TIMER *timer, struct FIFO32 *fifo, int data){
    timer->fifo = fifo;
    timer->data = data;
    return ;
}

void timer_settime(struct TIMER *timer, unsigned int timeout){
    int e;
    struct TIMER *t, *s;
    timer->timeout = timeout+timerctl.count;
    timer->flags = TIMER_FLAGS_USING;
    e = io_load_eflags();
    io_cli();
    t = timerctl.t0;
    if(timer->timeout <= t->timeout){
        /* 插入最前面的情况 */
        timerctl.t0 = timer;
        timer->next = t; /* 下面是设定t*/
        timerctl.next = timer->timeout;
        io_store_eflags(e);
        return ;
    }
    /* 搜索插入位置 */
    for(;;){
        s = t;
        t = t->next;
        if(timer->timeout <= t->timeout){
            /* 插到s和t之间 */
            s->next = timer; /* s的下一个是timer */
            timer->next = t; /* timer的下一个是t */
            io_store_eflags(e);
            return ;
        }
    }
}

void inthandler20(int *esp){
    struct TIMER *timer;
    io_out8(PIC0_OCW2, 0x60);   /* 把IRQ-00信号接收完了的信息通知给PIC */
    timerctl.count++;
    if(timerctl.next>timerctl.count){
        return; /* 还不到下一个时刻，所以结束*/
    }
    timer = timerctl.t0;
    for(;;){    /* 因为timers的定时器都处于运行状态，所以不确认flags */
        if(timer->timeout > timerctl.count){
            break;
        }
        /* 超时 */
        timer->flags = TIMER_FLAGS_ALLOC;
        fifo32_put(timer->fifo, timer->data);
        timer = timer->next;
    }
    timerctl.t0 = timer;
    timerctl.next = timer->timeout;
    return ;
}
