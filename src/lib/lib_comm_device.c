/*
** lib_comm_device.c
**
** Copyright 2000 Christer Gustavsson <cg@nocrew.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** Read the file COPYING for more information.
*/

#define DEBUGLEVEL 0

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "debug.h"
#include "mintdefs.h"
#include "oconfig.h"
#include "srv_put.h"
#include "srv_sockets.h"

typedef struct pid_sockfd_pair
{
  int                      pid;
  int                      sockfd;
} PID_SOCKFD_PAIR;


static PID_SOCKFD_PAIR * pid_sockfd_pair_table;

void
init_comm_device(void)
{
  int i;

  pid_sockfd_pair_table =
    (PID_SOCKFD_PAIR *)Mxalloc(sizeof(PID_SOCKFD_PAIR) * MAX_NUM_APPS,
			       GLOBALMEM);

  for(i = 0; i < MAX_NUM_APPS; i++)
  {
    pid_sockfd_pair_table[i].pid = -1;
  }
}


LONG
Client_open (void)
{
  int pid;
  int i;
  int empty = -1;

  DEBUG1("In Client_open");

  pid = getpid();

  for(i = 0;
      (i < MAX_NUM_APPS) && (pid_sockfd_pair_table[i].pid != pid);
      i++)
  {
    if((empty == -1) && (pid_sockfd_pair_table[i].pid == -1))
    {
      empty = i;
    }
  }

  if((i == MAX_NUM_APPS) && (empty != -1))
  {
    pid_sockfd_pair_table[empty].pid = pid;
    pid_sockfd_pair_table[empty].sockfd = Fopen("u:/dev/oaes_ker", O_RDWR);
  }

  return 0;
}

/*
** Description
** Put a message to the server and wait for a reply
*/
LONG
Client_send_recv (void * in,
                  int    bytes_in,
                  void * out,
                  int    max_bytes_out)
{
  int               i;
  int               numbytes;
  int               sockfd;
  int               pid;

  pid = getpid();
  numbytes = -1;

  for(i = 0;
      (i < MAX_NUM_APPS) && (pid_sockfd_pair_table[i].pid != pid);
      i++)
  {
    ;
  }

  if(i < MAX_NUM_APPS)
  {
    sockfd = pid_sockfd_pair_table[i].sockfd;

    if ((numbytes = Fwrite(sockfd, bytes_in, in)) < 0)
    {
      DEBUG1("Couldn't write to oaes_ker");
      exit(-1);
    }
    
    DEBUG3("srv_put_sockets.c: sent numbytes = %d", numbytes);
    
    if ((numbytes = Fread(sockfd, max_bytes_out, out)) < 0)
    {
      DEBUG1("Couldn't read from oaes_ker");
      exit(-1);
    }
  }

  return numbytes;
}
