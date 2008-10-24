
#ifndef __STATE_H_
#define __STATE_H_

#define ALL_HIGH 1
#define BALANCE 2
#define ALL_LOW 3

struct state_desc{
	short state;
	cpumask_t cpumask;
	short cpus;
	unsigned int demand;
};

void hint_inc(int state);
void hint_dec(int state);
int freq_delta(int delta);
int init_cpu_states(unsigned int how);

#endif