// tom foolery once again

#define __SWAIT_QUEUE_HEAD_INITIALIZER(name) {                          \
        .lock           = __RAW_SPIN_LOCK_UNLOCKED(name.lock),          \
        .task_list      = LIST_HEAD_INIT((name).task_list),             \
}

