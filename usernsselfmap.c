#include <stdio.h>
#include <sched.h>
#include <linux/sched.h>
#include <stdlib.h>
#include <errno.h>

int writemaps(pid_t pid)
{
	FILE *fout;
	char path[1024];
	int origuid = getuid();
	int origgid = getgid();
	int ret;

	printf("starting from uid %d gid %d\n", origuid, origgid);
	snprintf(path, 1024, "/proc/%d/uid_map", pid);
	fout = fopen(path, "w");
	ret = fprintf(fout, "0 %d 1\n", origuid);
	if (ret < 0) {
		perror("writing uidmap\n");
		return -1;
	}
	ret = fclose(fout);
	if (ret < 0) {
		perror("closing uidmap\n");
		return -1;
	}

	snprintf(path, 1024, "/proc/%d/gid_map", pid);
	fout = fopen(path, "w");
	ret = fprintf(fout, "0 %d 1\n", origgid);
	if (ret < 0) {
		perror("writing gidmap\n");
		return -1;
	}
	ret = fclose(fout);
	if (ret < 0) {
		perror("closing gidmap\n");
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	char *args[] = { "/bin/bash", NULL };
	int ret, fromchildpipe[2], tochildpipe[2];
	pid_t pid;

	ret = pipe(fromchildpipe);
	if (ret < 0)
		exit(1);
	ret = pipe(tochildpipe);
	if (ret < 0)
		exit(1);

	pid = fork();
	if (pid < 0)
		exit(1);
	int x = 0;

	if (pid > 0) {
		int status;

		close(fromchildpipe[1]);
		close(tochildpipe[0]);
		read(fromchildpipe[0], &x, sizeof(x));
		if (x == 1)
			exit(1);
		close(fromchildpipe[0]);
		ret = writemaps(pid);
		if (ret < 0) {
			printf("Error writing maps for %d\n", pid);
			x = 1;
		}
		write(tochildpipe[1], &x, sizeof(x));
		close(tochildpipe[1]);
		waitpid(pid, &status, __WALL);
		exit(x);
	}
	close(fromchildpipe[0]);
	close(tochildpipe[1]);
	ret = unshare(CLONE_NEWUSER);
	if (ret < 0) {
		perror("unshare");
		x = 1;
		write(fromchildpipe[1], &x, sizeof(x));
		exit(1);
	}
	write(fromchildpipe[1], &x, sizeof(x));
	read(tochildpipe[0], &x, sizeof(x));
	if (x == 1) {
		printf("error in parent writing uid maps\n");
		exit(1);
	}
	close(fromchildpipe[1]);
	close(tochildpipe[0]);
	ret = setgid(0);
	if (ret < 0)
		perror("setgid");
	ret = setuid(0);
		perror("setuid");
	printf("execing bash (I am  now %d %d)\n", getuid(), getgid());
	execv(args[0], args);
}
