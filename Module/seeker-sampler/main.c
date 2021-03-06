/*****************************************************
 * Copyright 2008 Amithash Prasad                    *
 *                                                   *
 * This file is part of Seeker                       *
 *                                                   *
 * Seeker is free software: you can redistribute     *
 * it and/or modify it under the terms of the        *
 * GNU General Public License as published by        *
 * the Free Software Foundation, either version      *
 * 3 of the License, or (at your option) any         *
 * later version.                                    *
 *                                                   *
 * This program is distributed in the hope that      *
 * it will be useful, but WITHOUT ANY WARRANTY;      *
 * without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR       *
 * PURPOSE. See the GNU General Public License       *
 * for more details.                                 *
 *                                                   *
 * You should have received a copy of the GNU        *
 * General Public License along with this program.   *
 * If not, see <http://www.gnu.org/licenses/>.       *
 *****************************************************/

#include <linux/version.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/timer.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cpumask.h>
#include <linux/fs.h>
//#include <asm/hw_irq.h>

#include <seeker.h>
#include <pmu.h>
#include <fpmu.h>

#include "intr.h"
#include "io.h"
#include "probe.h"
#include "sample.h"
#include "alloc.h"
#include "log.h"
#include "exit.h"

#define SEEKER_SAMPLE_MINOR 240

/* Stop compilation early if unknown or more than
 * one architectures are provided.
 */
#if !(defined(ARCH_C2D) || defined(ARCH_K8) || defined(ARCH_K10))
#	error Unknown Architecture
#endif

#if defined(ARCH_C2D) && !(defined(ARCH_K8)) && !(defined(ARCH_K10))
#	define ARCHITECTURE "Intel Core 2 Duo Architecture"
#elif defined(ARCH_K8) && !(defined(ARCH_K10)) && !(defined(ARCH_C2D))
#	define ARCHITECTURE "AMD Opteron (Hammer) Architecture"
#elif defined(ARCH_K10) && !(defined(ARCH_K8)) && !(defined(ARCH_C2D))
#	define ARCHITECTURE "AMD Opteron (Barcelona) Architecture"
#else
#	error "Unknown Architecture"
#endif

/************************* Parameter variables *******************************/

int log_events[MAX_COUNTERS_PER_CPU];
unsigned int log_ev_masks[MAX_COUNTERS_PER_CPU];
int log_num_events = 0;
int sample_freq = -1;
int os_flag = 0;
int pmu_intr = -1;

/************************* Declarations & Prototypes *************************/

static struct file_operations seeker_sample_fops = {
	.owner = THIS_MODULE,
	.open = seeker_sample_open,
	.release = seeker_sample_close,
	.read = seeker_sample_log_read
};

static struct miscdevice seeker_sample_mdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "seeker_samples",
	.fops = &seeker_sample_fops
};

static int mdev_registered = 0;
static int kprobes_registered = 0;
int dev_open = 0;

extern struct timer_list sample_timer;
extern int sample_timer_started;

extern struct kprobe kp_schedule;
#ifdef LOCAL_PMU_VECTOR
extern struct jprobe jp_smp_pmu_interrupt;
#endif
extern struct jprobe jp_release_thread;
extern struct jprobe jp___switch_to;

/*---------------------------------------------------------------------------*
 * Function: seeker_sample_log_init
 * Descreption: registers seeker as a device with a dev name seeker_samples
 * 		This also tells the system what are the read/open/close 
 * 		functions.
 * Input Parameters: None
 * Output Parameters: None
 *---------------------------------------------------------------------------*/
static int seeker_sample_log_init(void)
{
	int ret;

	ret = misc_register(&seeker_sample_mdev);
	if (unlikely(ret != 0)) {
		error("Device register failed with error code %d", ret);
		return -1;
	} else {
		mdev_registered = 1;
	}
	return 0;
}

/*---------------------------------------------------------------------------*
 * Function: seeker_sampler_init
 * Descreption: Initialization function called at module creation time.
 * Input Parameters: None
 * Output Parameters: None
 *---------------------------------------------------------------------------*/
static int __init seeker_sampler_init(void)
{
	int i;
	int probe_ret;

#ifdef SEEKER_PLUGIN_PATCH
	if (NUM_FIXED_COUNTERS == 0 && log_num_events <= 3) {
		warn("Please note that the first 3 counters MUST BE retired instructions,real cycles, ref cycles");
		warn("You need to configure at least the first 3 counters to use seeker_scheduler.");
	}
#endif

	printk("---------------------------------------\n");
	printk("Initializing seeker kernel Module for the " ARCHITECTURE "\n");
	if (log_num_events <= 0) {
		printk("Monitoring only fixed counters and "
		       "temperature. PMU NOT CONFIGURED\n");
	}

	if (unlikely(msrs_init() < 0)) {
		error("msrs_init failure\n");
		return -ENODEV;
	}

	if (unlikely(seeker_sample_log_init() < 0)) {
		error("seeker_sample_log_init failure\n");
		seeker_sampler_exit_handler();
		return -ENODEV;
	}

	printk("seeker sampler module loaded logging %d events\n",
	       log_num_events + NUM_FIXED_COUNTERS + NUM_EXTRA_COUNTERS);

	for (i = 0; i < log_num_events; i++) {
		printk("%d:0x%x ", log_events[i], log_ev_masks[i]);
	}
	printk("\n");
	printk("Fixed pmu 1,2,3 and temperature also monitored\n");

	if (pmu_intr == -1) {
		printk("Timer is used as sampling interrupt source\n");
		if (sample_freq <= 0)
			sample_freq = 100;

		sample_freq = HZ / sample_freq;
		printk("seeker sampler sampling every %d jiffies, HZ is %d\n",
		       sample_freq, HZ);
		setup_timer(&sample_timer, do_timer_sample, 0);
		sample_timer_started = 1;
		mod_timer(&sample_timer, jiffies + sample_freq);
	} else {
#ifdef LOCAL_PMU_VECTOR
		if (sample_freq <= 0)
			sample_freq = 1000000;
		if (pmu_intr < NUM_FIXED_COUNTERS) {
			printk("Sampling every %d events of fixed counter %d\n",
			       sample_freq, pmu_intr);
			int_callbacks.enable_interrupts =
			    &fpmu_enable_interrupt;
			int_callbacks.disable_interrupts =
			    &fpmu_disable_interrupt;
			int_callbacks.configure_interrupts =
			    &fpmu_configure_interrupt;
			int_callbacks.clear_ovf_status = &fpmu_clear_ovf_status;
			int_callbacks.is_interrupt = &fpmu_is_interrupt;
		} else if (pmu_intr < (NUM_FIXED_COUNTERS + NUM_COUNTERS)) {
			pmu_intr = pmu_intr - NUM_FIXED_COUNTERS;
			printk("Sampling every %d events of counter %d\n",
			       sample_freq, pmu_intr);
			int_callbacks.enable_interrupts = &pmu_enable_interrupt;
			int_callbacks.disable_interrupts =
			    &pmu_disable_interrupt;
			int_callbacks.configure_interrupts =
			    &pmu_configure_interrupt;
			int_callbacks.clear_ovf_status = &pmu_clear_ovf_status;
			int_callbacks.is_interrupt = &pmu_is_interrupt;
		} else {
			error("pmu_intr=%d is invalid, max supported is %d",
			      pmu_intr, NUM_FIXED_COUNTERS + NUM_COUNTERS);
			return -ENOSYS;
		}

		printk
		    ("Fixed counter %d used as the sampling interrupt source, "
		     "sampling every %d events\n", pmu_intr, sample_freq);

		if ((probe_ret = register_jprobe(&jp_smp_pmu_interrupt)) < 0) {
			error("Could not find %s to probe, returned %d\n",
			      PMU_ISR, probe_ret);
			return -ENODEV;
		}

		if (ON_EACH_CPU((void *)enable_apic_pmu, NULL, 1, 1) < 0) {
			error
			    ("Could not enable local pmu interrupt on all cpu's\n");
		}
#else
		error("An attempt is made to use the pmu_intr "
		      "facility without applying a seeker patch to the kernel. Exiting");
		return -ENOTSUPP;
#endif
	}

	/* Register kprobes */

	if ((unlikely(probe_ret = register_kprobe(&kp_schedule)) < 0)) {
		error("Could not find schededule to probe, returned %d",
		      probe_ret);
		return -ENOSYS;
	}
	if (unlikely((probe_ret = register_jprobe(&jp_release_thread)) < 0)) {
		error("Could not find release_thread to probe, returned %d",
		      probe_ret);
		return -ENOSYS;
	}
	if (unlikely((probe_ret = register_jprobe(&jp___switch_to)) < 0)) {
		error("Could not find __switch_to to probe, returned %d",
		      probe_ret);
		return -ENOSYS;
	}
	kprobes_registered = 1;
	printk("kprobe registered\n");

	return 0;
}

/*---------------------------------------------------------------------------*
 * Function: seeker_samper_exit_handler
 * Descreption: This is the exit handler. Called by the module exit function
 * 		and any error check - failures.
 * Input Parameters: None
 * Output Parameters: None
 *---------------------------------------------------------------------------*/
void seeker_sampler_exit_handler(void)
{
	if (dev_open) {
		if (mdev_registered) {
			misc_deregister(&seeker_sample_mdev);
			mdev_registered = 0;
		}
		seeker_sample_close(NULL, NULL);
	}

	if (sample_timer_started) {
		del_timer_sync(&sample_timer);
		sample_timer_started = 0;
	}

	if (kprobes_registered) {
		unregister_kprobe(&kp_schedule);
		unregister_jprobe(&jp_release_thread);
		unregister_jprobe(&jp___switch_to);
#ifdef LOCAL_PMU_VECTOR
		if (pmu_intr >= 0) {
			unregister_jprobe(&jp_smp_pmu_interrupt);
		}
#endif
		printk("kprobe unregistered\n");
		kprobes_registered = 0;
	}

	/* Just incase something happens when the device and interrupts are enabled.... */
#ifdef LOCAL_PMU_VECTOR
	if (dev_open == 1) {
		/* Disable interrupts if they were enabled in the first palce. */
		if (pmu_intr >= 0) {
			/* Disable interrupts on all cpus */
			if (unlikely
			    (ON_EACH_CPU
			     ((void *)configure_disable_interrupts, NULL, 1,
			      1) < 0)) {
				error
				    ("Oops... Could not disable interrupts on all cpu's");
			}
		}
	}
#endif
}

/*---------------------------------------------------------------------------*
 * Function: seeker_sampler_exit
 * Descreption: Called when the module is unloaded. It in turn calls the exit 
 * 		handler.
 * Input Parameters: None
 * Output Parameters: None
 *---------------------------------------------------------------------------*/
static void __exit seeker_sampler_exit(void)
{
	seeker_sampler_exit_handler();
}

/*---------------------------------------------------------------------------*
 * Descreption: Module Parameters.
 *---------------------------------------------------------------------------*/

module_param_named(sample_freq, sample_freq, int, 0444);
MODULE_PARM_DESC(sample_freq, "The sampling frequency, either "
		 "samples per second or sample per x events");

module_param(os_flag, int, 0444);
MODULE_PARM_DESC(os_flag, "0->Sample in user space only, "
		 "1->Sample in user and kernel space");

module_param(pmu_intr, int, 0444);
MODULE_PARM_DESC(pmu_intr, "pmu_intr=x where x is the fixed counter "
		 "which will be the interrupt source");

module_param_array(log_events, int, &log_num_events, 0444);
MODULE_PARM_DESC(log_events, "The event numbers. Refer *_EVENTS.pdf");

module_param_array(log_ev_masks, int, &log_num_events, 0444);
MODULE_PARM_DESC(log_ev_masks, "The masks for the corrosponding "
		 "event. Refer *_EVENTS.pdf");

module_init(seeker_sampler_init);
module_exit(seeker_sampler_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amithash Prasad (amithash.prasad@colorado.edu)");
MODULE_DESCRIPTION("seeker-sampler samples the hardware performance "
		   "counters at regular intervals specified by the user");

/* EOF */
