/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * Copyright (C) 2010 Swann Perarnau
 * Author: Swann Perarnau <swann.perarnau@imag.fr>
 */

/* small executable to load/unload and ld_preload a binary */

#include"config.h"
#include<ccontrol.h>
#include<dirent.h>
#include<fcntl.h>
#include<getopt.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<unistd.h>

/* global variables:
 * mem: mem string to pass to module
 * size: string to use for CCONTROL_SIZE
 * colors: colorset string to use for CCONTROL_COLORS
 * ld: should ld_preload be set in forked environment
 */
char *mem = "1M";
char *size = "900K";
char *colors = "1-32";
int ask_ld = 0;
int ask_noload = 0;


static size_t cache_size;
static unsigned long cache_assoc;
static unsigned long numcolors;
/* scan /sys/devices/system and open last index
 * get size, assoc
 * compute LL_NUM_COLORS
 */
#define SYSPATH "/sys/devices/system/cpu/cpu0/cache"
static int scan_sys_cache_info(void)
{
	int r,i,n;
	struct dirent **list;
	FILE *f;
	char buf[80],*s;
	unsigned long pg_sz;
	/* move to directory */
	r = chdir(SYSPATH);
	if(r != 0)
	{
		perror("moving to sysinfo cache directory");
		return -1;
	}
	/* versionsort sort by index number */
	n = scandir(SYSPATH,&list,0,versionsort);
	if(n < 0)
	{
		perror("scanning sys info cache directory");
		return -1;
	}
	r = chdir(list[n-1]->d_name);
	/* clean list */
	for(i = 0; i < n; i++)
		free(list[i]);
	free(list);
	if(r != 0)
	{
		perror("moving to sysinfo index directory");
		return -1;
	}
	/* read size */
	f = fopen("size","r");
	if(f == NULL)
	{
		perror("opening sysinfo file 'size'");
		return -1;
	}
	s = fgets(buf,80,f);
	if(s == NULL)
	{
		perror("reading sysinfo file 'size'");
		return -1;
	}
	fclose(f);
	ccontrol_str2size(&cache_size,buf);
	/* read ways_of_associativity */
	f = fopen("ways_of_associativity","r");
	if(f == NULL)
	{
		perror("opening sysinfo file 'ways_of_associativity'");
		return -1;
	}
	r = fscanf(f,"%lu",&cache_assoc);
	if(r != 1)
	{
		perror("scanning ways_of_associativity");
		fclose(f);
	}
	fclose(f);
	/* compute number of colors */
	pg_sz = sysconf(_SC_PAGESIZE);
	if(pg_sz == -1)
	{
		perror("getting PAGESIZE from sysconf");
		return -1;
	}
	numcolors = cache_size/(pg_sz*cache_assoc);
	return 0;
}


/* commands:
 * load: load the kernel module
 * unload: unload the kernel module
 * exec: load, exec binary and unload
 * info: print cache stats
 */
static int load_module(void)
{
	int status;
	pid_t pid;
	char arg[80];
	snprintf(arg,80,"mem=%s",mem);
	/* we need to fork to execute modprobe */
	pid = fork();
	if(pid == -1)
		return EXIT_FAILURE;

	if(!pid)
	{
		status = execlp("modprobe","modprobe", "ccontrol",arg,(char *)NULL);
		perror("exec modprobe");
		exit(EXIT_FAILURE);
	}
	pid = waitpid(pid,&status,0);
	if(pid == -1)
	{
		perror("waitpid");
		return EXIT_FAILURE;
	}
	status = WIFEXITED(status) && WEXITSTATUS(status);
	return status;
}

static int unload_module(void)
{
	int status;
	pid_t pid;

	/* we need to fork to execute modprobe */
	pid = fork();
	if(pid == -1)
		return EXIT_FAILURE;

	if(!pid)
	{
		status = execlp("modprobe","modprobe","-r","ccontrol",(char *)NULL);
		perror("exec modprobe");
		exit(EXIT_FAILURE);
	}
	pid = waitpid(pid,&status,0);
	if(pid == -1)
	{
		perror("waitpid");
		return EXIT_FAILURE;
	}
	return WIFEXITED(status) && WEXITSTATUS(status);
}

static int exec_command(char **argv)
{
	int status;
	pid_t pid;
	if(ask_noload)
		goto fork_command;

	status = load_module();
	if(status)
		return status;

fork_command:
	/* we need to fork to execute modprobe */
	pid = fork();
	if(pid == -1)
		return EXIT_FAILURE;

	if(!pid)
	{
		if(ask_ld)
		{
			setenv("LD_PRELOAD",CCONTROL_LIB_PATH,1);
			setenv(CCONTROL_ENV_SIZE,size,1);
			setenv(CCONTROL_ENV_COLORS,colors,1);
		}
		status = execvp(argv[1],&argv[1]);
		perror("exec command");
		exit(EXIT_FAILURE);
	}
	pid = waitpid(pid,&status,0);
	if(pid == -1)
	{
		perror("waitpid");
		return EXIT_FAILURE;
	}
	if(WIFEXITED(status))
		printf("command exited with code: %d\n",WEXITSTATUS(status));
	else
		printf("command exited abnormaly\n");

	status = 0;
	if(!ask_noload)
		status = unload_module();

	return status;
}

static int cmd_info(void)
{
	int status;
	status = scan_sys_cache_info();
	if(status != 0)
	{
		fprintf(stderr,"command info failed\n");
		return status;
	}
	printf("Cache stats:\n");
	printf("LLC size:             %zu\n",cache_size);
	printf("LLC associativity:    %lu\n",cache_assoc);
	printf("LLC number of colors: %lu\n",numcolors);
	return status;
}

/* command line helpers */
static const char *version_string = PACKAGE_STRING;
int ask_help = 0;
int ask_version = 0;

void print_help()
{
	printf("Usage: ccontrol [options] <cmd> <args>\n\n");
	printf("Available options:\n");
	printf("--help,-h               : print this help message\n");
	printf("--version,-h            : print program version\n");
	printf("--mem,-m <string>       : mem argument of the module\n");
	printf("--size,-s <string>      : CCONTROL_SIZE value\n");
	printf("--colors,-c <string>    : CCONTROL_COLORS value\n");
	printf("--ld-preload,-l         : set LD_PRELOAD before exec\n");
	printf("--no-load,-n            : don't load module before exec\n");
	printf("Available commands:\n");
	printf("load                    : load kernel module\n");
	printf("unload                  : unload kernel module\n");
	printf("exec <args>             : execute args\n");
	printf("info                    : print cache information\n");
}

/* command line arguments */
static struct option long_options[] = {
	{ "help", no_argument, &ask_help, 1},
	{ "version", no_argument, &ask_version, 1},
	{ "ld-preload", no_argument, &ask_ld, 1},
	{ "no-load", no_argument, &ask_noload, 1},
	{ "mem", required_argument, NULL, 'm' },
	{ "colors", required_argument, NULL, 'c' },
	{ "size", required_argument, NULL, 's' },
	{ 0, 0 , 0, 0},
};

static const char* short_opts ="hVlnm:c:s:";

int main(int argc, char *argv[])
{
	int c;
	int option_index = 0;
	int status = 0;
	// parse options
	while(1)
	{
		c = getopt_long(argc, argv, short_opts,long_options, &option_index);
		if(c == -1)
			break;

		switch(c)
		{
			case 0:
				break;
			case 'm':
				mem = optarg;
				break;
			case 'c':
				colors = optarg;
				break;
			case 'l':
				ask_ld = 1;
				break;
			case 'h':
				ask_help = 1;
				break;
			case 'V':
				ask_version =1;
				break;
			case 'n':
				ask_noload = 1;
				break;
			case 's':
				size = optarg;
				break;
			default:
				fprintf(stderr,
					"ccontrol bug: someone forgot how to write a switch\n");
				exit(EXIT_FAILURE);
			case '?':
				fprintf(stderr,"ccontrol bug: getopt failed miserably\n");
				exit(EXIT_FAILURE);
		}
	}
	// forget the parsed part of argv
	argc -= optind;
	argv = &(argv[optind]);

	if(ask_version)
	{
		printf("ccontrol: version %s\n",version_string);
		exit(EXIT_SUCCESS);
	}

	if(ask_help || argc == 0)
	{
		print_help();
		exit(EXIT_SUCCESS);
	}
	if(!strcmp(argv[0],"info"))
	{
		status = cmd_info();
		goto end;
	}
	if(!strcmp(argv[0],"load"))
	{
		status = load_module();
		goto end;
	}
	else if(!strcmp(argv[0],"unload"))
	{
		status = unload_module();
		goto end;
	}
	else if (!strcmp(argv[0],"exec"))
	{
		status = exec_command(argv);
		goto end;
	}
	status = EXIT_FAILURE;
	fprintf(stderr,"error: command not found\n");
end:
	return status;
}
