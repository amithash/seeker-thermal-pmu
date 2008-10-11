#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>


#include "state.h"
#include "../scpufreq.h"
#include "../seeker.h"

#include "hint.h"
#include "estimate.h"
#include "compute.h"
#include "stats.h"

int max_state_possible[NR_CPUS] = {0};
int max_state_in_system = -1;
int cur_cpu_states[NR_CPUS] = {0};

int get_total_states(void)
{
	return max_state_in_system;
}

int freq_delta(int delta)
{
	int **cpu_state = NULL;
	int cpus = num_online_cpus();
	int i;
	warn("total online cpus = %d",cpus);	

	choose_layout(delta);	

	get_state_of_cpu(cpu_state);
	for(i=0;i<cpus;i++)
		(*cpu_state)[i] = cur_cpu_states[i];
	put_state_of_cpu();
	return 0;
}

int init_cpu_states(unsigned int how)
{
	int cpus = num_online_cpus();
	int i;
	for(i=0;i<cpus;i++){
		max_state_possible[i] = get_max_states(i);
		if(max_state_in_system < max_state_possible[i])
			max_state_in_system = max_state_possible[i];
	}
	
	switch(how){
		case ALL_HIGH:
			for(i=0;i<cpus;i++){
				cur_cpu_states[i] = max_state_possible[i];
				set_freq(i,cur_cpu_states[i]);
			}
			break;
		case ALL_LOW:
			for(i=0;i<cpus;i++){
				cur_cpu_states[i] = 0;
				set_freq(i,cur_cpu_states[i]);
			}
			break;
		case BALANCE:
			for(i=0;i<(cpus>>1);i++){
				cur_cpu_states[i] = 0;
				set_freq(i,cur_cpu_states[i]);
			}
			for(;i<cpus;i++){
				cur_cpu_states[i] = max_state_possible[i];
				set_freq(i,cur_cpu_states[i]);
			}
			break;
		default:
			for(i=0;i<cpus;i++){
				cur_cpu_states[i] = max_state_possible[i];
				set_freq(i,cur_cpu_states[i]);
			}
			break;
	}
	return 0;
}

