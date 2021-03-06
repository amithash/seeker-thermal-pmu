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
#ifndef _PMU_H_
#define _PMU_H_

void pmu_init_msrs(void *info);
void counter_clear(u32 counter);
void counter_read(void);
u64 get_counter_data(u32 counter, u32 cpu_id);
void counter_disable(int counter);
int counter_enable(u32 event_num, u32 ev_mask, u32 os);
int pmu_configure_interrupt(int ctr, u32 low, u32 high);
int pmu_enable_interrupt(int ctr);
int pmu_disable_interrupt(int ctr);
int pmu_clear_ovf_status(int ctr);
int pmu_is_interrupt(int ctr);

#endif


