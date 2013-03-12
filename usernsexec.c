/*
 * Copyright 2008 IBM Corp.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sched.h>

#include "clone.h"
int unshare(int flags);

static const char* procname;

static void usage(const char *name)
{
	printf("usage: %s [-h] [-c] [-mnuUip] [-P <pid-file>]"
			"[command [arg ..]]\n", name);
	printf("\n");
	printf("  -h		this message\n");
	printf("\n");
	printf("  -m <uid-maps> uid maps to use\n");
	printf("\n");
	exit(1);
}

/*
 * Copied following opentty() from Fedora's util-linux rpm
 * I just changed the "FATAL" message below from syslog()
 * to printf
 */
static void opentty(const char * tty) {
	int i, fd, flags;

	fd = open(tty, O_RDWR | O_NONBLOCK);
	if (fd == -1) {
		printf("FATAL: can't reopen tty: %s", strerror(errno));
		sleep(1);
		exit(1);
	}

	flags = fcntl(fd, F_GETFL);
	flags &= ~O_NONBLOCK;
	fcntl(fd, F_SETFL, flags);

	for (i = 0; i < fd; i++)
		close(i);
	for (i = 0; i < 3; i++)
		if (fd != i)
			dup2(fd, i);
	if (fd >= 3)
		close(fd);
}
// Code copy end

static int do_child(void *vargv)
{
	char **argv = (char **)vargv;

	// Assume we want to become root
	if (setgid(0) < 0) {
		perror("setgid");
		return -1;
	}
	if (setuid(0) < 0) {
		perror("setuid");
		return -1;
	}
	if (unshare(CLONE_NEWNS) < 0) {
		perror("unshare CLONE_NEWNS");
		return -1;
	}
	execve(argv[0], argv, __environ);
	perror("execve");
	return -1;
}

/*
 * given a string like "b:0:100000:10", map both uids and gids
 * 0-10 to 100000 to 100010
 */
static void parse_map(char *map)
{
	// not yet implemented
}

struct id_map {
	char which; // b or u or g
	int host_id, ns_id, range;
	struct id_map *next;
};

/*
 * go through /etc/subuids and /etc/subgids to find this user's
 * allowed maps
 */
static void find_default_map(void)
{
	// not yet implemented.
}

struct id_map default_map = {
	.which = 'b',
	.host_id = 100000,
	.ns_id = 0,
	.range = 10000,
};
static struct id_map *active_map = &default_map;

static int run_cmd(char **argv)
{
    int status;
	pid_t pid = fork();

	if (pid < 0)
		return pid;
	if (pid == 0) {
		execvp(argv[0], argv);
		perror("exec failed");
		exit(1);
	}
	if (waitpid(pid, &status, __WALL) < 0) {
        perror("waitpid");
		return -1;
	}

	return WEXITSTATUS(status);
}

static int map_child_uids(int pid, struct id_map *map)
{
	char **uidargs = NULL, **gidargs = NULL;
	int i, nuargs = 2, ngargs = 2;
	struct id_map *m;

	uidargs = malloc(3 * sizeof(*uidargs));
	gidargs = malloc(3 * sizeof(*gidargs));
	if (uidargs == NULL || gidargs == NULL)
		return -1;
	uidargs[0] = malloc(10);
	gidargs[0] = malloc(10);
	uidargs[1] = malloc(21);
	gidargs[1] = malloc(21);
	uidargs[2] = NULL;
	gidargs[2] = NULL;
	if (!uidargs[0] || !uidargs[1] || !gidargs[0] || !gidargs[1])
		return -1;
	sprintf(uidargs[0], "newuidmap");
	sprintf(gidargs[0], "newgidmap");
	sprintf(uidargs[1], "%d", pid);
	sprintf(gidargs[1], "%d", pid);
	for (m=map; m; m = m->next) {
		if (m->which == 'b' || m->which == 'u') {
			nuargs += 3;
			uidargs = realloc(uidargs, (nuargs+1) * sizeof(*uidargs));
			if (!uidargs)
				return -1;
			uidargs[nuargs - 3] = malloc(21);
			uidargs[nuargs - 2] = malloc(21);
			uidargs[nuargs - 1] = malloc(21);
			if (!uidargs[nuargs-3] || !uidargs[nuargs-2] || !uidargs[nuargs-1])
				return -1;
			sprintf(uidargs[nuargs - 3], "%d", m->ns_id);
			sprintf(uidargs[nuargs - 2], "%d", m->host_id);
			sprintf(uidargs[nuargs - 1], "%d", m->range);
			uidargs[nuargs] = NULL;
		}
		if (m->which == 'b' || m->which == 'g') {
			ngargs += 3;
			gidargs = realloc(gidargs, (ngargs+1) * sizeof(*gidargs));
			if (!gidargs)
				return -1;
			gidargs[ngargs - 3] = malloc(21);
			gidargs[ngargs - 2] = malloc(21);
			gidargs[ngargs - 1] = malloc(21);
			if (!gidargs[ngargs-3] || !gidargs[ngargs-2] || !gidargs[ngargs-1])
				return -1;
			sprintf(gidargs[ngargs - 3], "%d", m->ns_id);
			sprintf(gidargs[ngargs - 2], "%d", m->host_id);
			sprintf(gidargs[ngargs - 1], "%d", m->range);
			gidargs[ngargs] = NULL;
		}
	}

	// exec newuidmap
	if (run_cmd(uidargs) != 0) {
		fprintf(stderr, "Error mapping uids\n");
		return -2;
	}
	// exec newgidmap
	if (run_cmd(gidargs) != 0) {
		fprintf(stderr, "Error mapping gids\n");
		return -2;
	}

	for (i=0; i<nuargs; i++)
		free(uidargs[i]);
	for (i=0; i<ngargs; i++)
		free(gidargs[i]);
	free(uidargs);
	free(gidargs);

    return 0;
}

int main(int argc, char *argv[])
{	
	int c;
	unsigned long flags = CLONE_NEWUSER | CLONE_NEWNS;
	char ttyname[256];
	int status;
	int ret;
	int pid;
	char *default_args[] = {"/bin/sh", NULL};
	int pipe1[2],  // child tells parent it has unshared
	    pipe2[2];  // parent tells child it is mapped and may proceed

	procname = basename(argv[0]);

	memset(ttyname, '\0', sizeof(ttyname));
	readlink("/proc/self/fd/0", ttyname, sizeof(ttyname));

	while ((c = getopt(argc, argv, "m:h")) != EOF) {
		switch (c) {
			case 'm': parse_map(optarg); break;
			case 'h':
			default:
				  usage(procname);
		}
	};

	if (active_map == &default_map) {
		find_default_map();
	}

	argv = &argv[optind];
	argc = argc - optind;	
	if (argc < 1) {
		argv = default_args;
		argc = 1;
	}

	if (pipe(pipe1) < 0 || pipe(pipe2) < 0) {
		perror("pipe");
		exit(1);
	}
	if ((pid = fork()) == 0) {
		// Child.

		close(pipe1[0]);
		close(pipe2[1]);
		opentty(ttyname);

		ret = unshare(flags);
		if (ret < 0) {
			perror("unshare");
			return 1;
		}
		ret = 1;
		if (write(pipe1[1], &ret, 1) < 1) {
			perror("write pipe");
			exit(1);
		}
		if (read(pipe2[0], &ret, 1) < 1) {
			perror("read pipe");
			exit(1);
		}
		if (ret != 1) {
			fprintf(stderr, "parent had an error, child exiting\n");
			exit(1);
		}

		close(pipe1[1]);
		close(pipe2[0]);
		return do_child((void*)argv);
	}

	close(pipe1[1]);
	close(pipe2[0]);
	if (read(pipe1[0], &ret, 1) < 1) {
		perror("read pipe");
		exit(1);
	}

	ret = 1;
	if (map_child_uids(pid, active_map)) {
		fprintf(stderr, "error mapping child\n");
		ret = 0;
	}
	write(pipe2[1], &ret, 1);

	if ((ret = waitpid(pid, &status, __WALL)) < 0) {
		printf("waitpid() returns %d, errno %d\n", ret, errno);
		exit(ret);
	}

	exit(WEXITSTATUS(status));
}
