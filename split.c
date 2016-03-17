/*
    mtr  --  a network diagnostic tool
    Copyright (C) 1997  Matt Kimball

    split.c -- raw output (for inclusion in KDE Network Utilities or others
                         GUI based tools)
    Copyright (C) 1998  Bertrand Leconte <B.Leconte@mail.dotcom.fr>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as 
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "config.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#include "mtr.h"
#include "display.h"
#include "dns.h"
#include "asn.h"

#include "net.h"
#include "split.h"

#ifdef NO_CURSES
#include <sys/time.h>
#include <sys/types.h>
#else
/* Use the curses variant */

#if defined(UNICODE)
#define _XOPEN_SOURCE_EXTENDED
#if defined (HAVE_NCURSESW_NCURSES_H)
#  include <ncursesw/ncurses.h>
#elif defined (HAVE_NCURSESW_CURSES_H)
#  include <ncursesw/curses.h>
#elif defined (HAVE_CURSES_H)
#  include <curses.h>
#else
#  error No ncursesw header file available
#endif
#else

#if defined(HAVE_NCURSES_H)
#  include <ncurses.h>
#elif defined(HAVE_NCURSES_CURSES_H)
#  include <ncurses/curses.h>
#elif defined(HAVE_CURSES_H)
#  include <curses.h>
#else
#  error No curses header file available
#endif

#endif
#endif

#ifdef NO_CURSES
#define SPLIT_PRINT(x)	{ printf x; printf("\n"); }
#else
// like dns.c:restell()
#define SPLIT_PRINT(x)	{ printf x; printf("\r\n"); }
#endif

extern char *Hostname;
extern int WaitTime;
extern int af;

/* There is 256 hops max in the IP header (coded with a byte) */
#define MAX_LINE_COUNT 256
#define MAX_LINE_SIZE  512

char Lines[MAX_LINE_COUNT][MAX_LINE_SIZE];
int  LineCount;


#define DEBUG 0


void split_redraw(void) 
{
  int   max;
  int   at;
  ip_t *addr, *addrs;
  char  newLine[MAX_LINE_SIZE];
  int   i;

#if DEBUG
  SPLIT_PRINT(("split_redraw()"));
#endif

  /* 
   * If there is less lines than last time, we delete them
   * TEST THIS PLEASE
   */
  max = net_max();
  for (i=LineCount; i>max; i--) {
    SPLIT_PRINT(("-%d", i));
    LineCount--;
  }

  /*
   * For each line, we compute the new one and we compare it to the old one
   */
  for(at = 0; at < max; at++) {
    addr = net_addr(at);
    if(addrcmp((void*)addr, (void*)&unspec_addr, af)) {
      char str[256], *name;
      if (!(name = dns_lookup(addr)))
        name = strlongip(addr);
      if (show_ips) {
        snprintf(str, sizeof(str), "%s %s", name, strlongip(addr));
        name = str;
      }
      /* May be we should test name's length */
      snprintf(newLine, sizeof(newLine), "%s %.1f %d %d %.1f %.1f %.1f %.1f %s", name,
               net_loss(at)/1000.0,
               net_returned(at), net_xmit(at),
               net_best(at) /1000.0, net_avg(at)/1000.0,
               net_worst(at)/1000.0,
               net_stdev(at)/1000.0,
               fmt_ipinfo(addr));
    } else {
      sprintf(newLine, "???");
    }

    if (strcmp(newLine, Lines[at]) == 0) {
      /* The same, so do nothing */
#if DEBUG
      SPLIT_PRINT(("SAME LINE"));
#endif
    } else {
      SPLIT_PRINT(("%d %s", at+1, newLine));

      if (strcmp(newLine, "???") != 0) {
        /* Multi path */
        for (i=0; i < MAXPATH; i++ ) {
          addrs = net_addrs(at, i);
          if ( addrcmp( (void *) addrs, (void *) addr, af ) == 0 ) continue;
          if ( addrcmp( (void *) addrs, (void *) &unspec_addr, af ) == 0 ) break;
          char *name;
 
          if (!(name = dns_lookup(addrs)))
            name = strlongip(addrs);
          if (show_ips) {
            SPLIT_PRINT(("- %d %d %s %s %s", at+1, i+1, name, strlongip(addrs), fmt_ipinfo(addrs)));
          } else {
            SPLIT_PRINT(("- %d %d %s %s", at+1, i+1, name, fmt_ipinfo(addrs)));
          }
        }
      }
 
      fflush(stdout);
      strcpy(Lines[at], newLine);
      if (LineCount < (at+1)) {
	LineCount = at+1;
      }
    }
  }
}


void split_open(void)
{
  int i;
#if DEBUG
  printf("split_open()\n");
#endif
  LineCount = -1;
  for (i=0; i<MAX_LINE_COUNT; i++) {
    strcpy(Lines[i], "");
  }
#ifndef NO_CURSES
  FILE *stdout2 = fopen("/dev/tty", "w");
  if (!stdout2)
    fprintf(stderr, "split_open(): fopen() failed\n");
  else {
    SCREEN *screen;
    if ((screen = newterm(NULL, stdout2, stdin))) {
      set_term(screen);
      raw();
      noecho();
    } else
      fprintf(stderr, "split_open(): newterm() failed\n");
  }
#endif
}


void split_close(void)
{
#ifndef NO_CURSES
  endwin();
#endif
#if DEBUG
  printf("split_close()\n");
#endif
}


int split_keyaction(void) 
{
#ifdef NO_CURSES
  fd_set readfds;
  struct timeval tv;
  char c;

  FD_ZERO (&readfds);
  FD_SET (0, &readfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  if (select (1, &readfds, NULL, NULL, &tv) > 0) {
    read (0, &c, 1);
  } else 
    return 0;
#else
  char c = getch();
#endif

#if DEBUG
  SPLIT_PRINT(("split_keyaction()"));
#endif
  if(tolower((int)c) == 'q')
    return ActionQuit;
  if(c==3)
    return ActionQuit;
  if(tolower((int)c) == 'r')
    return ActionReset;

  return 0;
}
