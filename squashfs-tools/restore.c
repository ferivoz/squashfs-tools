/*
 * Create a squashfs filesystem.  This is a highly compressed read only
 * filesystem.
 *
 * Copyright (c) 2013, 2014
 * Phillip Lougher <phillip@squashfs.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * restore.c
 */

#include <pthread.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "squashfs_fs.h"
#include "mksquashfs.h"
#include "error.h"
#include "progressbar.h"
#include "info.h"

#define FALSE 0
#define TRUE 1

pthread_t restore_thread, main_thread;
int interrupted = 0;

extern void restorefs();
extern pthread_t *thread;
extern int processors;


void *restore_thrd(void *arg)
{
	sigset_t sigmask, old_mask;
	int i, sig;

	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGINT);
	sigaddset(&sigmask, SIGTERM);
	sigaddset(&sigmask, SIGUSR1);
	pthread_sigmask(SIG_BLOCK, &sigmask, &old_mask);

	while(1) {
		sigwait(&sigmask, &sig);

		if((sig == SIGINT || sig == SIGTERM) && !interrupted) {
			ERROR("Interrupting will restore original "
				"filesystem!\n");
                	ERROR("Interrupt again to quit\n");
			interrupted = TRUE;
			continue;
		}

		/* kill main thread/worker threads and restore */
		set_progressbar_state(FALSE);
		disable_info();
		pthread_cancel(main_thread);
		pthread_join(main_thread, NULL);

		for(i = 0; i < 2 + processors * 2; i++)
			pthread_cancel(thread[i]);
		for(i = 0; i < 2 + processors * 2; i++)
			pthread_join(thread[i], NULL);

		TRACE("All threads cancelled\n");

		restorefs();
	}
}


pthread_t *init_restore_thread(pthread_t thread)
{
	main_thread = thread;
	pthread_create(&restore_thread, NULL, restore_thrd, NULL);
	return &restore_thread;
}