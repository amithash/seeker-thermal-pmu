#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/timer.h>

#include <seeker.h>

#include "quanta.h"
#include "state.h"

u32 interval_jiffies;
extern int change_interval;
struct timer_list state_change_timer;
extern int delta;

void destroy_timer(void)
{
	del_timer_sync(&state_change_timer);
}

int create_timer(void)
{
	interval_jiffies = change_interval * HZ;
	debug("Interval set to every %d jiffies",interval_jiffies);
	init_timer(&state_change_timer);
	state_change_timer.function = &state_change;
	state_change(0);
	return 0;
}
void state_change(unsigned long param)
{
	debug("State change now.");
	freq_delta(delta);
	mod_timer(&state_change_timer, jiffies + interval_jiffies);
}
