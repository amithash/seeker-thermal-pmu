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

#ifndef __ASSIGNCPU_H_
#define __ASSIGNCPU_H_

void put_mask_from_stats(struct task_struct *ts);

/* Keep the threshold at 1M Instructions
 * This removes artifcats from IPC and 
 * removes IPC Computation for small tasks
 */
#define INST_THRESHOLD 10000000

/* Macro to access 'seeker' added members in
 * task_struct (TS). This is used, to avoid
 * the ugly #define SEEKER_PLUGIN_PATCH
 * everywhere, just so that I can test it on
 * an unpatched kernel for compilation errors! 
 */
#ifdef SEEKER_PLUGIN_PATCH
#define TS_MEMBER(ts,member)	ts->member
#else
#define TS_MEMBER(ts,member)	(ts)->flags
#endif

#endif
