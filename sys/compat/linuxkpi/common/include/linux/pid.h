#ifndef _LINUXKPI_LINUX_PID_H_
#define _LINUXKPI_LINUX_PID_H_

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sx.h>
#include <sys/mutex.h>

struct pid;
struct task_struct;

enum pid_type {
	PIDTYPE_PID,
	PIDTYPE_PGID,
	PIDTYPE_SID,
	PIDTYPE_MAX
};

#define pid_task(pid, type) ({                  \
        struct task_struct *__ts;               \
        CTASSERT((type) == PIDTYPE_PID);        \
        __ts = linux_pid_task(pid);             \
        __ts;                                   \
})

#define pid_nr(n) (n)
#define pid_vnr(n) (n)
#define from_kuid_munged(a, uid) (uid)


static inline struct pid *
find_get_pid(pid_t nr)
{
	struct proc *p;
	struct pid *pid;

	sx_slock(&allproc_lock);
	p = pfind(nr);
	if (p != NULL) {
		PHOLD(p);
		PROC_UNLOCK(p);
		pid = (struct pid *)p;
	} else {
		pid = NULL;
	}
	sx_sunlock(&allproc_lock);

	return (pid);
}

static inline void
put_pid(struct pid *pid)
{
	struct proc *p = (struct proc *)pid;
	if (p != NULL)
		PRELE(p);
}

#define get_pid_task(_pid, _type) ({ \
	struct proc *_p = (struct proc *)(_pid); \
	struct task_struct *_task = NULL; \
	if (_p != NULL) { \
		_task = (struct task_struct *)_p; \
		get_task_struct(_task); \
	} \
	_task; \
})

#define get_task_pid(_task, _type) ({ \
	struct proc *_p = (struct proc *)(_task); \
	if (_p != NULL) { \
		PHOLD(_p); \
	} \
	(struct pid *)_p; \
})

struct task_struct;
extern struct task_struct *linux_pid_task(pid_t);
extern struct task_struct *linux_get_pid_task(pid_t);


#endif /* _LINUXKPI_LINUX_PID_H_ */
