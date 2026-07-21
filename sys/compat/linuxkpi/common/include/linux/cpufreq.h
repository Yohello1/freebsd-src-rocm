#ifndef _LINUX_CPUFREQ_H_
#define _LINUX_CPUFREQ_H_

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/pcpu.h>

/**
 * cpufreq_quick_get_max - Get the maximum frequency of a CPU.
 * @cpu: The CPU core index (unused in this simplified fallback)
 *
 * Returns: CPU frequency in kHz to match Linux behavior.
 */
static inline unsigned int cpufreq_quick_get_max(unsigned int cpu)
{
    int clockrate = 0;
    size_t len = sizeof(clockrate);

    /*
     * Use kernel_sysctlbyname to safely query the sysctl tree from 
     * inside the FreeBSD kernel space.
     */
    if (kernel_sysctlbyname(curthread, "hw.clockrate", &clockrate, &len, 
                            NULL, 0, NULL, 0) == 0) {
        // hw.clockrate is in MHz. Linux expects kHz.
        return (unsigned int)clockrate * 1000;
    }

    // Default fallback (e.g., 2.0 GHz) if the sysctl fails
    return 2000000;
}

#endif /* _LINUX_CPUFREQ_H_ */
