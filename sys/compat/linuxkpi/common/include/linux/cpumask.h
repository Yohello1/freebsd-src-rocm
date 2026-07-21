
#ifndef _LINUX_CPUMASK_H_
#define _LINUX_CPUMASK_H_

typedef	cpuset_t	cpumask_t;

extern cpumask_t cpu_online_mask;

#include <sys/param.h>
#include <sys/systm.h>
#define nr_cpu_ids (mp_maxid + 1)

#include <linux/cpu.h>
/* Define cpu_none_mask as a constant expression */


/* Map Linux's cpumask_t directly to FreeBSD's cpuset_t */
typedef cpuset_t cpumask_t;

/* Declare the underlying global variable */
extern const cpumask_t linux_cpu_none_mask;
#define cpu_none_mask ((const struct cpumask *)&linux_cpu_none_mask)

struct cpumask {
    cpuset_t bits;
};

static inline int cpumask_first(const struct cpumask *cpumask)
{
    return CPU_FFS((const cpuset_t *)cpumask) - 1;
}

static inline const struct cpumask *cpumask_of_node(int node)
{
    /* Use the global mask as a safe default if specific NUMA 
       mapping isn't strictly required for your current testing */
    return (const struct cpumask *)&cpu_online_mask; 
}

#endif
