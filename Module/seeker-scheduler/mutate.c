#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/cpumask.h>

#include <seeker.h>

#include "state.h"
#include "scpufreq.h"
#include "debug.h"

#define ABS(i) ((int)(i) >= 0 ? (i) : (-1*((int)(i))))
#define div(a,b) ((b) != 0 ? ((((a) + (b) - 1))/(b))  : 0)

extern int total_online_cpus;
extern int max_allowed_states[NR_CPUS];
extern int cur_cpu_state[NR_CPUS];
extern unsigned int max_state_in_system;
int state_matrix[NR_CPUS][MAX_STATES];
extern struct state_desc states[MAX_STATES];

u64 interval_count;

inline int procs(int hints,int total, int proc, int total_load);

inline int procs(int hints,int total, int proc, int total_load)
{
	if(hints == 0)
		return 0;
	if(hints == total)
		return total_load;
	return div((hints * total_load),total);
}

void update_state_matrix(int *cpu_state, int delta)
{
	int i,j,l;
	for(i=0;i<total_online_cpus;i++){
		l=0;
		for(j=cpu_state[i];j<max_state_in_system;j++){
			if(l>delta)
				state_matrix[i][j] = 0;
			else
				state_matrix[i][j] = (max_state_in_system-l)*(max_state_in_system-l);
			l++;
		}
		l=1;
		for(j=cpu_state[i]-1;j>=0;j--){
			if(l>delta)
				state_matrix[i][j] = 0;
			else
				state_matrix[i][j] = (max_state_in_system-l)*(max_state_in_system-l);
			l++;
		}
	}
}

void choose_layout(int delta)
{
	int total = 0;
	int demand[MAX_STATES];
	int load = 0;
	struct debug_block *p = NULL;
	unsigned int i,j;
	unsigned int winner=0;
	unsigned int winner_val = 0;
	unsigned int winner_best_proc = 0;
	unsigned int winner_best_proc_value = 0;
	unsigned int winner_best_low_proc_value = 0;
	unsigned int best_proc = 0;
	unsigned int best_proc_value = 0;
	unsigned int best_low_proc_value = 0;
	int sum;
	short poison[NR_CPUS] = {1};
	int new_cpu_state[NR_CPUS];

	interval_count++;

	/* Create a state matrix such that, the cell which
	 * indicates a processors current state, gets the highest
	 * value = iax_state_in_system^2, and parabolically decreases on either side.
	 */
	for(i=0;i<total_online_cpus;i++){
		new_cpu_state[i] = cur_cpu_state[i];
		load += weighted_cpuload(i) >= SCHED_LOAD_SCALE ? 1 : 0;
	}

	update_state_matrix(new_cpu_state,delta);
	

	/* Total Hint */
	
	for(j=0;j<max_state_in_system;j++){
		total += states[j].demand;
	}

	/* Num of cpus required for this state 
	 * SUM(demand[]) could be < cpus. 
	 * Make sure to bring down their states. */
	for(j=0;j<max_state_in_system;j++){
		demand[j] = procs(states[j].demand,total,total_online_cpus,load);
		debug("required cpus for state %d = %d",j,demand[j]);
	}

	/* Now for each delta to spend, hold an auction */
	while(delta > 0){
		winner = 0;
		winner_val = 0;
		winner_best_proc = 0;
		winner_best_proc_value = 0;
		winner_best_low_proc_value = 0;

		/* There is an optimization here, so do not get confused.
		 * Technically, each column in the state matrix is supposed
		 * to be multiplied by the demand. But that is done here,
		 * as the demand decreases once won.
		 */

		/* For each state, */
		for(j=0;j<max_state_in_system;j++){
			sum = 0;
			best_proc = 0;
			best_proc_value = 0;
			best_low_proc_value = -1;

			/* Sum the cost over all rows */
			for(i=0;i<total_online_cpus;i++){
				if(state_matrix[i][j] * poison[i] > best_proc_value){
					best_proc_value = state_matrix[i][j] * poison[i];
					best_proc = i;
				} else if(state_matrix[i][j] < best_low_proc_value){
					best_low_proc_value = state_matrix[i][j];
				}
				sum += state_matrix[i][j];
			}

			sum = sum * (demand[j]+1);

			/* Find the max sum and the sate, and its best proc 
			 * If there is contention for that, choose the one
			 * with the best proc, if there is contention for both,
			 * choose the one with the best lowest proc,
			 * if there is contention for that too, then first come
			 * first serve */
			if(sum < winner_val)
				continue;
			if(sum > winner_val) 
				goto assign;
			if(best_proc_value < winner_best_proc_value)
				continue;
			if(best_proc_value > winner_best_proc_value)
				goto assign;

			if(best_low_proc_value < winner_best_low_proc_value)
				continue;
assign:
			winner = j;
			winner_val = sum;
			winner_best_proc= best_proc;
			winner_best_proc_value = best_proc_value;
			winner_best_low_proc_value = best_low_proc_value;

		}
		/* A winning val of 0 indicated a failed auction.
		 * all contenstents are broke. Go home loosers.*/
		if(winner_val <= 0)
			break;

		/* Now the winning state, reduces its demand */
		demand[winner]--;
	
		/* The best processor is best_proc */
		/* Poison the choosen processor */
		poison[winner_best_proc] = 0;

		/* Subtract that from the delta */
		delta -= abs(cur_cpu_state[winner_best_proc] - winner);

		/* Assign the new cpus state to be the winner */
		new_cpu_state[winner_best_proc] = winner;

		if(delta == 0)
			break;

		/* If the new state of the CPU is different,
		 * change the state matrix to reflect it */
		if(cur_cpu_state[winner_best_proc] != winner){
			update_state_matrix(new_cpu_state,delta);
		}

		/* Continue the auction if delta > 0 */
	}	
	p = get_debug();
	if(p){
		p->entry.type = DEBUG_MUT;
		p->entry.u.mut.interval = interval_count;
		p->entry.u.mut.count = max_state_in_system;
	}

	for(j=0;j<max_state_in_system;j++){
		states[j].cpus = 0;
		cpus_clear(states[j].cpumask);
		if(p)
			p->entry.u.mut.hint[j] = states[j].demand;
	}


	for(i=0;i<total_online_cpus;i++){
		/* XXX This violates DELTA. But this 
		 * also makes sure that unused processors 
		 * are in the lowest cpu state */
		if(poison[i] == 1)
			new_cpu_state[i] = 0;
		
		if(new_cpu_state[i] != cur_cpu_state[i]){
			cur_cpu_state[i] = new_cpu_state[i];
			set_freq(i,new_cpu_state[i]);
		}
		states[cur_cpu_state[i]].cpus++;
		cpu_set(i,states[cur_cpu_state[i]].cpumask);
		if(p)
			p->entry.u.mut.cpustates[i] = cur_cpu_state[i];
	}
	put_debug(p);
}


