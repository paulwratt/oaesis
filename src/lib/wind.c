/*
** wind.c
**
** Copyright 1996 - 2001 Christer Gustavsson <cg@nocrew.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** Read the file COPYING for more information.
**
*/


#define DEBUGLEVEL 0


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_ALLOC_H
#include <alloc.h>
#endif

#ifdef HAVE_BASEPAGE_H
#include <basepage.h>
#endif

#ifdef HAVE_MINTBIND_H
#include <mintbind.h>
#endif

#ifdef HAVE_OSBIND_H
#include <osbind.h>
#endif

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_SUPPORT_H
#include <support.h>
#endif

#include <unistd.h>

#include "aesbind.h"
#include "appl.h"
#include "debug.h"
#include "evnthndl.h"
#include "lib_comm.h"
#include "lib_global.h"
#include "mintdefs.h"
#include "mesagdef.h"
#include "lib_misc.h"
#include "objc.h"
#include "resource.h"
#include "srv_interface.h"
#include "srv_put.h"
#include "types.h"
#include "wind.h"


#define INTS2LONG(a,b) (((((LONG)a)<<16)&0xffff0000L)|(((LONG)b)&0xffff))

#define IMOVER 0x8000  /* Used with set_win_elem() to make icon window */


/*
** Structures used with windows
*/
typedef struct window_struct {
  WORD     id;
  WORD     status;
  WORD     elements;
  RECT     maxsize;
  RECT     totsize;
  RECT     worksize;
  RECT     lastsize;
  OBJECT * tree;

  WORD hslidepos;     /*position of the horizontal slider*/
  WORD vslidepos;     /*position of the vertical slider*/
  WORD hslidesize;    /*size of the horizontal slider*/
  WORD vslidesize;   /*size of the vertical slider*/
  
  WORD top_colour[20];
  WORD untop_colour[20];
  WORD own_colour;
} WINDOW_STRUCT;

typedef struct window_list {
  WINDOW_STRUCT        ws;
  struct window_list * next;
} WINDOW_LIST;
typedef WINDOW_LIST * WINDOW_LIST_REF;
#define WINDOW_LIST_REF_NIL ((WINDOW_LIST_REF)NULL)

static WORD     elemnumber = -1;
static WORD     tednumber;

static WORD widgetmap[] = {
  0,        /* W_BOX     */
  0,        /* W_TITLE   */
  WCLOSER,  /* W_CLOSER  */
  WMOVER,   /* W_NAME    */
  WFULLER,  /* W_FULLER  */
  WINFO,    /* W_INFO    */
  0,        /* W_DATA    */
  0,        /* W_WORK    */
  WSIZER,   /* W_SIZER   */
  0,        /* W_VBAR    */
  WUP,      /* W_UPARROW */
  WDOWN,    /* W_DNARROW */
  WVSB,     /* W_VSLIDE  */
  WVSLIDER, /* W_VELEV   */
  0,        /* W_HBAR    */
  WLEFT,    /* W_LFARROW */
  WRIGHT,   /* W_RTARROW */
  WHSB,     /* W_HSLIDE  */
  WHSLIDER, /* W_HELEV   */
  WSMALLER  /* W_SMALLER */
};

static
WINDOW_STRUCT *
find_window_struct (WORD apid,
                    WORD id);


/*
** Description
** Set window widget colour
*/
static
void
set_widget_colour(WINDOW_STRUCT * win,
                  WORD            widget,
                  WORD            top,
                  WORD            untop)
{
  U_OB_SPEC * obspec;
  WORD        object = 0;
  WORD        new_colour;

  if(win->tree)
  {
    object = widgetmap[widget];
    
    if(OB_FLAGS(&win->tree[object]) & INDIRECT)
    {
      obspec = ((U_OB_SPEC *)OB_SPEC(&win->tree[object]));
    }
    else
    {
      obspec = &win->tree[object].ob_spec;
    }
    
    if(win->status & WIN_TOPPED)
    {
      new_colour = top;
    }
    else
    {
      new_colour = untop;
    }

    switch(OB_TYPE(&win->tree[object]) & 0xff)
    {
    case G_BOX:
    case G_IBOX:
    case G_BOXCHAR:
      obspec->index = HL_TO_CL((CL_TO_HL(obspec->index) & 0xffff0000) |
                               (new_colour & 0x0000ffff));
      break;
      
    case G_TEXT:
    case G_BOXTEXT:
    case G_FTEXT:
    case G_FBOXTEXT:
      TE_COLOR_PUT(CL_TO_HL(obspec->tedinfo), new_colour);
      break;
      
    case G_IMAGE:
      BI_COLOR_PUT(CL_TO_HL(obspec->bitblk), new_colour);
      break;
      
    case G_BUTTON:
    case G_PROGDEF:
    case G_STRING:
    case G_TITLE:
      return;
      
    default:
      DEBUG1("Unsupported type %d in set_widget_colour, widget %d",
             OB_TYPE(&win->tree[object]), widget);
      return;
    }
  }
}


/*calcworksize calculates the worksize or the total size of
a window. If dir == WC_WORK the worksize will be calculated and 
otherwise the total size will be calculated.*/

static
void
calcworksize (WORD apid,
              WORD elem,
              RECT *orig,
              RECT *new,
              WORD dir)
{
  WORD  bottomsize = 1;
  WORD  headsize = 1;
  WORD  leftsize = 1;
  WORD  rightsize = 1;
  WORD  topsize;
  GLOBAL_COMMON * globals_common = get_global_common ();
        
  DEBUG3 ("wind.c: calcworksize: %d %d %d %d",
          orig->x, orig->y, orig->width, orig->height);
  if((HSLIDE | LFARROW | RTARROW) & elem)
  {
    bottomsize = OB_HEIGHT(&globals_common->windowtad[WLEFT]) + (D3DSIZE << 1);
  }
        
  if((CLOSER | MOVER | FULLER | NAME) & elem)
  {
    topsize = OB_HEIGHT(&globals_common->windowtad[WMOVER]) + (D3DSIZE << 1);
  }
  else if(IMOVER & elem)
  {
    topsize = globals_common->csheight + 2 + D3DSIZE * 2;
  }
  else
  {
    topsize = 0;
  }
        
  if(INFO & elem)
  {
    headsize =
      topsize + OB_HEIGHT(&globals_common->windowtad[WINFO]) + 2 * D3DSIZE;
  }
  else
  {
    if(topsize)
      headsize = topsize;
    else
      headsize = 1;
  }
        
  if((LFARROW | HSLIDE | RTARROW) & elem)
  {
    bottomsize = OB_HEIGHT(&globals_common->windowtad[WLEFT]) + (D3DSIZE << 1);
  }
        
  if(((bottomsize < OB_HEIGHT(&globals_common->windowtad[WLEFT])) &&
      (SIZER & elem)) || ((VSLIDE | UPARROW | DNARROW) & elem))
  {
    rightsize = OB_WIDTH(&globals_common->windowtad[WSIZER]) + (D3DSIZE << 1);
  }

  if(dir == WC_WORK) {
    new->x = orig->x + leftsize;
    new->y = orig->y + headsize;
    new->width = orig->width - leftsize - rightsize;
    new->height = orig->height - headsize - bottomsize;
  }
  else {
    new->x = orig->x - leftsize;
    new->y = orig->y - headsize;
    new->width = orig->width + leftsize + rightsize;
    new->height = orig->height + headsize + bottomsize;
  }

  DEBUG3 ("wind.c: calcworksize: new %d %d %d %d",
          new->x, new->y, new->width, new->height);
}

/*
** Description
** Change window slider position and size
**
** 1999-03-28 CG
*/
static
WORD
Wind_set_slider (WORD        apid,
                 WORD        id,
                 WORD        redraw,
                 WORD        which,
                 WORD        position,
                 WORD        size) {	
  WINDOW_STRUCT * win = find_window_struct (apid, id);
  WORD redraw2 = 0;
  GLOBAL_COMMON * globals = get_global_common ();
  
  DEBUG3 ("wind.c: Wind_set_slider: position %d size %d", position, size);

  if(which & VSLIDE) {
    WORD newheight,newy;
    
    if (position != -1) {
      if(position > 1000) {
        win->vslidepos = 1000;
      } else if (position < 1) {
        win->vslidepos = 1;
      } else {
        win->vslidepos = position;
      }
    }
		
    if (size != -1) {
      if (size > 1000) {
        win->vslidesize = 1000;
      } else if (size < 1) {
        win->vslidesize = 1;
      } else {
        win->vslidesize = size;
      }
    }

    newy = (WORD)(((LONG)win->vslidepos *
                   (LONG)(OB_HEIGHT(&win->tree[WVSB]) -
                          OB_HEIGHT(&win->tree[WVSLIDER]))) / 1000L);
    newheight = (WORD)(((LONG)win->vslidesize *
                        (LONG)OB_HEIGHT(&win->tree[WVSB])) / 1000L);
    
    if((OB_Y(&win->tree[WVSLIDER]) != newy) ||
       (OB_HEIGHT(&win->tree[WVSLIDER]) != newheight))
    {
      OB_Y_PUT(&win->tree[WVSLIDER], newy);
      OB_HEIGHT_PUT(&win->tree[WVSLIDER], newheight);
      
      redraw2 = 1;
    }
  }
  
  if(which & HSLIDE) {
    WORD newx,newwidth;
    
    if(position != -1) {
      if(position > 1000) {
        win->hslidepos = 1000;
      } else if (position < 1) {
        win->hslidepos = 1;
      } else {
        win->hslidepos = position;
      }
    }
    
    if(size != -1) {
      if(size > 1000) {
        win->hslidesize = 1000;
      } else if(size < 1) {
        win->hslidesize = 1;
      } else {
        win->hslidesize = size;
      }
    }
    
    newx = (WORD)(((LONG)win->hslidepos *
                   (LONG)(OB_WIDTH(&win->tree[WHSB]) -
                          OB_WIDTH(&win->tree[WHSLIDER]))) / 1000L);
    newwidth = (WORD)(((LONG)win->hslidesize *
                       (LONG)OB_WIDTH(&win->tree[WHSB])) / 1000L);
    
    if((OB_X(&win->tree[WHSLIDER]) != newx) ||
       (OB_WIDTH(&win->tree[WHSLIDER]) != newwidth))
    {
      OB_X_PUT(&win->tree[WHSLIDER], newx);
      OB_WIDTH_PUT(&win->tree[WHSLIDER], newwidth);
      
      redraw2 = 1;
    }
  }

  if(redraw && redraw2 && (win->status & WIN_OPEN)) {
    DEBUG3 ("screen : %d %d %d %d",
            globals->screen.x,
            globals->screen.y,
            globals->screen.width,
            globals->screen.height);
    if(which & VSLIDE) { 
      Wind_redraw_elements (apid, id, &globals->screen, WVSB);
    }
    
    if(which & HSLIDE) {
      Wind_redraw_elements (apid, id, &globals->screen, WHSB);
    }
  }
  
  return 1;
}


#define ADD_OB_X(ob, val)      OB_X_PUT(ob, OB_X(ob) + val)
#define ADD_OB_Y(ob, val)      OB_Y_PUT(ob, OB_Y(ob) + val)
#define ADD_OB_WIDTH(ob, val)  OB_WIDTH_PUT(ob, OB_WIDTH(ob) + val)
#define ADD_OB_HEIGHT(ob, val) OB_HEIGHT_PUT(ob, OB_HEIGHT(ob) + val)

/*
** Description
** Wind_set_size sets the size and position of window <win> to <size>
*/
static
WORD
Wind_set_size (int    apid,
               int    id,
               RECT * size)
{
  WORD            dx,dy,dw,dh;
  WINDOW_STRUCT * ws = find_window_struct (apid, id);

  if(ws != NULL)
  {
    ws->lastsize = ws->totsize;
    
    dx = size->x - ws->lastsize.x;
    dy = size->y - ws->lastsize.y;
    dw = size->width - ws->lastsize.width;
    dh = size->height - ws->lastsize.height;

    ws->totsize = *size;
    
    ws->worksize.x += dx;
    ws->worksize.y += dy;
    ws->worksize.width += dw;
    ws->worksize.height += dh;
    
    if(ws->tree)
    {
      OB_X_PUT(&ws->tree[0], ws->totsize.x);
      OB_Y_PUT(&ws->tree[0], ws->totsize.y);
      OB_WIDTH_PUT(&ws->tree[0], ws->totsize.width);
      OB_HEIGHT_PUT(&ws->tree[0], ws->totsize.height);
      
      ADD_OB_WIDTH(&ws->tree[WMOVER], dw);
      
      ADD_OB_X(&ws->tree[WFULLER], dw);
      
      ADD_OB_X(&ws->tree[WSMALLER], dw);
      
      ADD_OB_X(&ws->tree[WDOWN], dw);
      ADD_OB_Y(&ws->tree[WDOWN], dh);
      
      ADD_OB_X(&ws->tree[WSIZER], dw);
      ADD_OB_Y(&ws->tree[WSIZER], dh);
      
      ADD_OB_X(&ws->tree[WRIGHT], dw);
      ADD_OB_Y(&ws->tree[WRIGHT], dh);
      
      ADD_OB_Y(&ws->tree[WLEFT], dh);
      
      ADD_OB_X(&ws->tree[WVSB], dw);
      ADD_OB_HEIGHT(&ws->tree[WVSB], dh);
      
      ADD_OB_Y(&ws->tree[WHSB], dh);
      ADD_OB_WIDTH(&ws->tree[WHSB], dw);
      
      ADD_OB_WIDTH(&ws->tree[WINFO], dw);
      
      ADD_OB_X(&ws->tree[WUP], dw);
      
      ADD_OB_WIDTH(&ws->tree[TFILLOUT], dw);
      
      ADD_OB_HEIGHT(&ws->tree[RFILLOUT], dh);
      ADD_OB_X(&ws->tree[RFILLOUT], dw);
      
      ADD_OB_WIDTH(&ws->tree[BFILLOUT], dw);
      ADD_OB_Y(&ws->tree[BFILLOUT], dy);
      
      ADD_OB_X(&ws->tree[SFILLOUT], dw);
      ADD_OB_Y(&ws->tree[SFILLOUT], dh);
      
      OB_WIDTH_PUT(&ws->tree[WAPP], OB_WIDTH(&ws->tree[WMOVER]));
      
      Wind_set_slider (apid,
                       id,
                       0,
                       HSLIDE | VSLIDE,
                       -1,
                       -1);
    }
  }

  /* OK */
  return 1;
}


/*winalloc creates/fetches a free window structure/id*/

static
WINDOW_STRUCT *
winalloc (WORD apid) {
  WINDOW_LIST * wl;
  GLOBAL_APPL * globals = get_globals (apid);

  /*    
        if(win_free) {
        wl = win_free;
        win_free = win_free->next;
        }
        else {
        */
  wl = (WINDOW_LIST *)malloc(sizeof(WINDOW_LIST));

  /*
    };
    */
        
  wl->next = globals->windows;
  globals->windows = wl;
        
  return &wl->ws;
}


/*
** Description
** Allocate a resource for a new window
*/
static
OBJECT *
allocate_window_elements (void) {
  WORD    i = 0,tnr = 0;
  OBJECT  *t;
  TEDINFO *ti;
  LONG    size;
  GLOBAL_COMMON * globals = get_global_common ();

  while(elemnumber == -1) {
    switch(OB_TYPE(&globals->windowtad[i]))
    {
    case        G_TEXT          :
    case        G_BOXTEXT       :
    case        G_FTEXT         :
    case        G_FBOXTEXT      :
      tnr++;
    }
                
    if(OB_FLAGS(&globals->windowtad[i]) & LASTOB)
    {
      elemnumber = i + 1;
      tednumber = tnr;
    }
                
    i++;
  }

  DEBUG3("wind.c: allocate_window_elements: elemnumber = %d",
         elemnumber);
        
  size = sizeof(OBJECT) * elemnumber + sizeof(TEDINFO) * tednumber;
        
  t = (OBJECT *)malloc(size);
        
  if(t != NULL) {
    ti = (TEDINFO *)&t[elemnumber];
                
    memcpy(t,globals->windowtad,sizeof(OBJECT) * elemnumber);
                
    for(i = 0; i < elemnumber; i++) {
      switch(OB_TYPE(&globals->windowtad[i]))
      {
      case      G_TEXT          :
      case      G_BOXTEXT       :
      case      G_FTEXT         :
      case      G_FBOXTEXT      :
        OB_SPEC_PUT(&t[i], ti);
        memcpy(ti, (void *)OB_SPEC(&globals->windowtad[i]), sizeof(TEDINFO));
        ti++;
      }
    }
  }

  return t;
}


/*
** Description
** Pack window elements
*/
static
void
packelem (OBJECT * tree,
          WORD     object,
          WORD     left,
          WORD     right,
          WORD     top,
          WORD     bottom)
{
  if((left != -1) && (right != -1))
  {
    if(left == 0)
    {
      OB_X_PUT(&tree[object], D3DSIZE);
    }
    else
    {
      OB_X_PUT(&tree[object],
               OB_X(&tree[left]) + OB_WIDTH(&tree[left]) + D3DSIZE * 2);
    }
                
    if(right == 0)
    {
      OB_WIDTH_PUT(&tree[object],
                   OB_WIDTH(&tree[0]) - OB_X(&tree[object]) - D3DSIZE);
    }
    else
    {
      OB_WIDTH_PUT(&tree[object],
                   OB_X(&tree[right]) - OB_X(&tree[object]) - D3DSIZE * 2);
    }
  }
  else if(left != -1)
  {
    if(left == 0)
    {
      OB_X_PUT(&tree[object], D3DSIZE);
    }
    else
    {
      OB_X_PUT(&tree[object],
               OB_X(&tree[left]) + OB_WIDTH(&tree[left]) + D3DSIZE * 2);
    }
  }
  else if(right != -1)
  {
    if(right == 0)
    {
      OB_X_PUT(&tree[object],
               OB_WIDTH(&tree[0]) - OB_WIDTH(&tree[object]) - D3DSIZE);
    }
    else
    {
      OB_X_PUT(&tree[object],
               OB_X(&tree[right]) - OB_WIDTH(&tree[object]) - D3DSIZE * 2);
    }
  }
        
        
  if((top != -1) && (bottom != -1))
  {
    if(top == 0)
    {
      OB_Y_PUT(&tree[object], D3DSIZE);
    }
    else
    {
      OB_Y_PUT(&tree[object],
               OB_Y(&tree[top]) + OB_HEIGHT(&tree[top]) + D3DSIZE * 2);
    }
                
    if(bottom == 0)
    {
      OB_HEIGHT_PUT(&tree[object],
                    OB_HEIGHT(&tree[0]) - OB_Y(&tree[object]) - D3DSIZE);
    }
    else
    {
      OB_HEIGHT_PUT(&tree[object],
                    OB_Y(&tree[bottom]) - OB_Y(&tree[object]) - D3DSIZE * 2);
    }
  }
  else if(top != -1)
  {
    if(top == 0)
    {
      OB_Y_PUT(&tree[object], D3DSIZE);
    }
    else
    {
      OB_Y_PUT(&tree[object],
               OB_Y(&tree[top]) + OB_HEIGHT(&tree[top]) + D3DSIZE * 2);
    }
  }
  else if(bottom != -1)
  {
    if(bottom == 0)
    {
      OB_Y_PUT(&tree[object],
               OB_HEIGHT(&tree[0]) - OB_HEIGHT(&tree[object]) - D3DSIZE);
    }
    else
    {
      OB_Y_PUT(&tree[object],
               OB_Y(&tree[bottom]) - OB_HEIGHT(&tree[object]) - D3DSIZE * 2);
    }
  }
}


/*
** Description
** Set the elements to use for a window (in the resource tree).
*/
static
void
set_win_elem (OBJECT * tree,
              WORD     elem)
{
  WORD bottomsize = 0;
  WORD rightsize = 0;
  WORD left = 0,right = 0,top = 0,bottom = 0;

  DEBUG3("wind.c: set_win_elem: tree = %p", tree);
  if((HSLIDE | LFARROW | RTARROW) & elem)
  {
    bottomsize = OB_HEIGHT(&tree[WLEFT]) + (D3DSIZE << 1);
  }
        
  DEBUG3("wind.c: set_win_elem: 2");
  if((LFARROW | HSLIDE | RTARROW) & elem)
  {
    bottomsize = OB_HEIGHT(&tree[WLEFT]) + (D3DSIZE << 1);
  }
        
  DEBUG3("wind.c: set_win_elem: 3");
  if(((bottomsize == 0) && (SIZER & elem))
     || ((VSLIDE | UPARROW | DNARROW) & elem))
  {
    DEBUG3("wind.c: set_win_elem: 3.1: tree[WSIZER]@%p",&tree[WSIZER]);
    DEBUG3("wind.c: tree[WSIZER].OB_WIDTH = %d", OB_WIDTH(&tree[WSIZER]));
    rightsize = OB_WIDTH(&tree[WSIZER]) + (D3DSIZE << 1);
  }
        
  DEBUG3("wind.c: set_win_elem: 4");
  if(CLOSER & elem)
  {
    OB_FLAGS_CLEAR(&tree[WCLOSER], HIDETREE);        
                
    packelem(tree,WCLOSER,0,-1,0,-1);
    left = WCLOSER;
  }     
  else
  {
    OB_FLAGS_SET(&tree[WCLOSER], HIDETREE);
  }
        
  DEBUG3("wind.c: set_win_elem: 5");
  if(FULLER & elem)
  {
    OB_FLAGS_CLEAR(&tree[WFULLER], HIDETREE);        
                
    packelem(tree,WFULLER,-1,0,0,-1);
    right = WFULLER;
  }     
  else
  {
    OB_FLAGS_SET(&tree[WFULLER], HIDETREE);
  }
                
  DEBUG3("wind.c: set_win_elem: 6");
  if(SMALLER & elem)
  {
    OB_FLAGS_CLEAR(&tree[WSMALLER], HIDETREE);       
                
    packelem(tree,WSMALLER,-1,right,0,-1);
    right = WSMALLER;
  }     
  else
  {
    OB_FLAGS_SET(&tree[WSMALLER], HIDETREE);
  }
                
  DEBUG3("wind.c: set_win_elem: 7");
  if(MOVER & elem)
  {
    OB_FLAGS_CLEAR(&tree[WMOVER], HIDETREE);
    OB_FLAGS_SET(&tree[TFILLOUT], HIDETREE);
                
    OB_HEIGHT_PUT(&tree[WMOVER], OB_HEIGHT(&tree[WCLOSER]));
    TE_FONT_PUT(OB_SPEC(&tree[WMOVER]), IBM);

    packelem(tree, WMOVER, left, right, 0, -1);
    top = WMOVER;
  }
  else
  {
    OB_FLAGS_SET(&tree[WMOVER], HIDETREE);

    if((left != 0) || (right != 0))
    {
      OB_FLAGS_CLEAR(&tree[TFILLOUT], HIDETREE);

      packelem(tree,TFILLOUT,left,right,0,-1);
      top = TFILLOUT;
    }
    else
    {
      OB_FLAGS_SET(&tree[TFILLOUT], HIDETREE);
    }
  }
        
  DEBUG3("wind.c: set_win_elem: 8");
  if(INFO & elem)
  {
    OB_FLAGS_CLEAR(&tree[WINFO], HIDETREE);

    packelem(tree,WINFO,0,0,top,-1);
    top = WINFO;                
  }
  else
  {
    OB_FLAGS_SET(&tree[WINFO], HIDETREE);
  }

  right = 0;
  left = 0;

  DEBUG3("wind.c: set_win_elem: 9");
  if(elem & UPARROW)
  {
    OB_FLAGS_CLEAR(&tree[WUP], HIDETREE);
                
    packelem(tree,WUP,-1,0,top,-1);
    top = WUP;
  }
  else
  {
    OB_FLAGS_SET(&tree[WUP], HIDETREE);
  }

  DEBUG3("wind.c: set_win_elem: 10");
  if(SIZER & elem)
  {
    OB_FLAGS_CLEAR(&tree[WSIZER], HIDETREE);
    OB_FLAGS_SET(&tree[SFILLOUT], HIDETREE);
                
    packelem(tree,WSIZER,-1,0,-1,0);
    bottom = right = WSIZER;
  }     
  else
  {
    OB_FLAGS_SET(&tree[WSIZER], HIDETREE);
                
    if((bottomsize > 0) && (rightsize > 0))
    {
      OB_FLAGS_CLEAR(&tree[SFILLOUT], HIDETREE);
                        
      packelem(tree,SFILLOUT,-1,0,-1,0);
      bottom = right = SFILLOUT;
    }
    else
    {
      OB_FLAGS_SET(&tree[SFILLOUT], HIDETREE);
    }
  }
        
  DEBUG3("wind.c: set_win_elem: 11");
  if(elem & DNARROW)
  {
    OB_FLAGS_CLEAR(&tree[WDOWN], HIDETREE);

    packelem(tree,WDOWN,-1,0,-1,bottom);
    bottom = WDOWN;             
  }
  else
  {
    OB_FLAGS_SET(&tree[WDOWN], HIDETREE);
  }
        
  DEBUG3("wind.c: set_win_elem: 12");
  if(elem & VSLIDE)
  {
    OB_FLAGS_CLEAR(&tree[WVSB], HIDETREE);

    packelem(tree,WVSB,-1,0,top,bottom);                
  }
  else
  {
    OB_FLAGS_SET(&tree[WVSB], HIDETREE);
  }
        
  DEBUG3("wind.c: set_win_elem: 13");
  if(!(VSLIDE & elem) && (rightsize > 0))
  {
    OB_FLAGS_CLEAR(&tree[RFILLOUT], HIDETREE);

    packelem(tree,RFILLOUT,-1,0,top,bottom);            
  }
  else
  {
    OB_FLAGS_SET(&tree[RFILLOUT], HIDETREE);
  }
        
  DEBUG3("wind.c: set_win_elem: 14");
  if(LFARROW & elem)
  {
    OB_FLAGS_CLEAR(&tree[WLEFT], HIDETREE);
                
    packelem(tree,WLEFT,0,-1,-1,0);
    left = WLEFT;
  }
  else
  {
    OB_FLAGS_SET(&tree[WLEFT], HIDETREE);
  }
        
  DEBUG3("wind.c: set_win_elem: 15");
  if(RTARROW & elem)
  {
    OB_FLAGS_CLEAR(&tree[WRIGHT], HIDETREE);
                
    packelem(tree,WRIGHT,-1,right,-1,0);
    right = WRIGHT;
  }
  else
  {
    OB_FLAGS_SET(&tree[WRIGHT], HIDETREE);
  }
        
  DEBUG3("wind.c: set_win_elem: 16");
  if(elem & HSLIDE)
  {
    OB_FLAGS_CLEAR(&tree[WHSB], HIDETREE);
                
    packelem(tree,WHSB,left,right,-1,0);
  }
  else
  {
    OB_FLAGS_SET(&tree[WHSB], HIDETREE);
  }
        
  DEBUG3("wind.c: set_win_elem: 17");
  if(!(HSLIDE & elem) && (bottomsize > 0))
  {
    OB_FLAGS_CLEAR(&tree[BFILLOUT], HIDETREE);
                
    packelem(tree,BFILLOUT,left,right,-1,0);
  }
  else
  {
    OB_FLAGS_SET(&tree[BFILLOUT], HIDETREE);
  }
        
  DEBUG3("wind.c: set_win_elem: 18");
  if(IMOVER & elem)
  {
    GLOBAL_COMMON * globals_common = get_global_common();

    OB_FLAGS_CLEAR(&tree[WMOVER], HIDETREE);
    OB_HEIGHT_PUT(&tree[WMOVER], globals_common->csheight + 2);
    TE_FONT_PUT(OB_SPEC(&tree[WMOVER]), SMALL);
    packelem(tree,WMOVER,0,0,0,-1);

    OB_FLAGS_SET(&tree[WAPP], HIDETREE);
  }
  else
  {
    OB_FLAGS_CLEAR(&tree[WAPP], HIDETREE);
  }
  DEBUG3("wind.c: set_win_elem: 19");
}




/*
** Description
** Wind_set_size sets the size and position of window <win> to <size>
*/
static
WORD
Wind_set_iconify(int    apid,
                 int    id,
                 RECT * size)
{
  WINDOW_STRUCT * ws;

  ws = find_window_struct (apid, id);

  if(ws != NULL)
  {
    if(ws->tree != NULL)
    {
      set_win_elem(ws->tree, IMOVER);
      calcworksize(apid, IMOVER, &ws->totsize, &ws->worksize, WC_WORK);
      return Wind_set_size(apid, id, size);
    }
  }

  return 0; /* Not ok */
}


/*
** Description
** Wind_set_size sets the size and position of window <win> to <size>
*/
static
WORD
Wind_set_uniconify(int    apid,
                   int    id,
                   RECT * size)
{
  WINDOW_STRUCT * ws;

  ws = find_window_struct (apid, id);

  if(ws != NULL)
  {
    if(ws->tree != NULL)
    {
      set_win_elem(ws->tree, ws->elements);
      calcworksize(apid, ws->elements, &ws->totsize, &ws->worksize, WC_WORK);
      return Wind_set_size(apid, id, size);
    }
  }

  return 0; /* Not ok */
}


/*
** Description
** Find the window struct of window <id>. The window must belong to
** application <apid>
**
** 1998-10-11 CG
** 1999-01-10 CG
*/
static
WINDOW_STRUCT *
find_window_struct (WORD apid,
                    WORD id)
{
  GLOBAL_APPL * globals = get_globals (apid);
  WINDOW_LIST * wl = globals->windows;

  while (wl != NULL) {
    if (wl->ws.id == id) {
      return &wl->ws;
    }

    wl = wl->next;
  }

  return NULL;
}


/*
** Description
** Draw window elements of window <id> with a clipping rectangle <clip>.
** If <id> is REDRAW_ALL all windows of the application will be redrawn.
**
** 1998-10-11 CG
** 1999-01-01 CG
** 1999-01-09 CG
*/
void
Wind_redraw_elements (WORD   apid,
                      WORD   id,
                      RECT * clip,
                      WORD   start) {
  GLOBAL_APPL * globals = get_globals (apid);
  WINDOW_LIST * wl = globals->windows;
  
  /* Handle desktop and menu separately */
  if ((id == DESKTOP_WINDOW) || (id == MENU_BAR_WINDOW)) {
    OBJECT * tree =
      (id == DESKTOP_WINDOW) ? globals->desktop_background : globals->menu;

    if (tree != NULL) {
      RECT r;
     
      Wind_do_get (apid,
                   id,
                   WF_FIRSTXYWH,
                   &r.x,
                   &r.y,
                   &r.width,
                   &r.height,
                   FALSE);
      
      /* Loop while there are more rectangles to redraw */
      while (!((r.width == 0) && (r.height == 0))) {
        RECT  r2;
        
        if(Misc_intersect(&r, clip, &r2)) {
          Objc_do_draw (globals->vid,
                        tree,
                        start,
                        3,
                        &r2);
        }
        
        if (Wind_do_get (apid,
                         id,
                         WF_NEXTXYWH,
                         &r.x,
                         &r.y,
                         &r.width,
                         &r.height,
                         FALSE) == 0) {
          /* An error occurred in Wind_do_get */
          break;
        }
      }
    }
  } else { /* Window handle != 0 */
    while (wl) {
      if (wl->ws.id == id) {
        if(wl->ws.tree != NULL) {
          RECT r;
          
          Wind_do_get (apid,
                       wl->ws.id,
                       WF_FIRSTXYWH,
                       &r.x,
                       &r.y,
                       &r.width,
                       &r.height,
                       FALSE);
          
          /* Loop while there are more rectangles to redraw */
          while (!((r.width == 0) && (r.height == 0))) {
            RECT  r2;
            
            if(Misc_intersect(&r, clip, &r2)) {
              Objc_do_draw (globals->vid, wl->ws.tree, start, 3, &r2);
            }
            
            if (Wind_do_get (apid,
                             wl->ws.id,
                             WF_NEXTXYWH,
                             &r.x,
                             &r.y,
                             &r.width,
                             &r.height,
                           FALSE) == 0) {
              /* An error occurred in Wind_do_get */
              break;
            }
          }
        }
        
        /* Get out of the loop if only one window was supposed to be redrawn */
        if (!(id == REDRAW_ALL)) {
          break;
        }
      }
      
      /* Continue with the next window */
      wl = wl->next;
    }
  }
}


/*
** Exported
*/
WORD
Wind_do_create (WORD   apid,
                WORD   elements,
                RECT * maxsize,
                WORD   status) {
  C_WIND_CREATE   par;
  R_WIND_CREATE   ret;
  WINDOW_STRUCT * ws;
  GLOBAL_APPL   * globals = get_globals (apid);

  PUT_C_ALL(WIND_CREATE,&par);

  par.elements = elements;
  par.maxsize = *maxsize;
  par.status = status;

  CLIENT_SEND_RECV(&par,
                   sizeof (C_WIND_CREATE),
                   &ret,
                   sizeof (R_WIND_CREATE));
  
  ws = winalloc (apid);

  ws->id = ret.common.retval;

  ws->status = status;
        
  ws->maxsize = *maxsize;

  ws->vslidepos = 1;
  ws->vslidesize = 1000;
  ws->hslidepos = 1;
  ws->hslidesize = 1000;

  if((ws->status & WIN_DIALOG) || (ws->status & WIN_MENU))
  {
    ws->tree = 0L;
    ws->totsize = ws->maxsize;
    ws->worksize = ws->totsize;
  }
  else
  {
    WORD    i;

    ws->tree = allocate_window_elements ();

    ws->elements = elements;
    set_win_elem (ws->tree, ws->elements);

    TE_PTEXT_PUT(OB_SPEC(&ws->tree[WAPP]), globals->application_name);

    DEBUG3("application_name = %s", globals->application_name);
    /* FIXME : What was this used for?
    if(globals.wind_appl == 0) {
      TE_PTEXT(OB_SPEC(&ws->tree[WAPP]))[0] = 0;
    }
    */

    ws->totsize.x      = OB_X(&ws->tree[0]);
    ws->totsize.y      = OB_Y(&ws->tree[0]);
    ws->totsize.width  = OB_WIDTH(&ws->tree[0]);
    ws->totsize.height = OB_HEIGHT(&ws->tree[0]);
    
    calcworksize (apid, ws->elements, &ws->totsize, &ws->worksize, WC_WORK);

    /* Set total size in server */
    Wind_do_set(apid,
                ws->id,
                WF_CURRXYWH,
                ws->totsize.x,
                ws->totsize.y,
                ws->totsize.width,
                ws->totsize.height);

    /* Set work size in server */
    Wind_do_set(apid,
                ws->id,
                WF_WORKXYWH,
                ws->worksize.x,
                ws->worksize.y,
                ws->worksize.width,
                ws->worksize.height);

    /* Set widget colours */
    for(i = 0; i <= W_SMALLER; i++)
    {
      WORD elem = i;

      Wind_do_get(apid,
                  0,
                  WF_DCOLOR,
                  &elem,
                  (WORD *)&ws->top_colour[i],
                  (WORD *)&ws->untop_colour[i],
                  &elem,
                  0);

      set_widget_colour(ws, i, ws->top_colour[i], ws->untop_colour[i]);
    }

    ws->own_colour = 0;
  }

  return ret.common.retval;
}


/* wind_create 0x0064 */

void
Wind_create (AES_PB *apb)
{       
  CHECK_APID(apb->global->apid);

  apb->int_out[0] = Wind_do_create(apb->global->apid,
                                   apb->int_in[0],
                                   (RECT *)&apb->int_in[1],
                                   0);
}


/*wind_open 0x0065*/

/*
** Exported
*/
WORD
Wind_do_open (WORD   apid,
              WORD   id,
              RECT * size)
{
  C_WIND_OPEN     par;
  R_WIND_OPEN     ret;
  WINDOW_STRUCT * win = find_window_struct (apid, id);

  PUT_C_ALL(WIND_OPEN, &par);

  par.id = id;
  par.size = *size;

  CLIENT_SEND_RECV(&par,
                   sizeof (C_WIND_OPEN),
                   &ret,
                   sizeof (R_WIND_OPEN));

  /* Set the size of the window elements */
  Wind_set_size (apid, id, size);

  win->status |= WIN_OPEN;

  return ret.common.retval;
}


void
Wind_open(AES_PB *apb)
{
  CHECK_APID(apb->global->apid);

  apb->int_out[0] = Wind_do_open (apb->global->apid,
                                  apb->int_in[0],
                                  (RECT *)&apb->int_in[1]);
}


/*
** Exported
*/
WORD
Wind_do_close (WORD apid,
               WORD wid) {
  C_WIND_CLOSE par;
  R_WIND_CLOSE ret;
  
  PUT_C_ALL(WIND_CLOSE, &par);

  par.id = wid;
        
  CLIENT_SEND_RECV(&par,
                   sizeof (C_WIND_CLOSE),
                   &ret,
                   sizeof (R_WIND_CLOSE));

  return ret.common.retval;
}


/*
** Exported
*/
void
Wind_close (AES_PB *apb)
{
  CHECK_APID(apb->global->apid);

  apb->int_out[0] = Wind_do_close (apb->global->apid, apb->int_in[0]);
}


/****************************************************************************
 * Wind_do_delete                                                           *
 *  Implementation of wind_delete().                                        *
 ****************************************************************************/
WORD             /* 0 if error or 1 if ok.                                  */
Wind_do_delete(  /*                                                         */
WORD apid,
WORD wid)        /* Identification number of window to close.               */
/****************************************************************************/
{
  C_WIND_DELETE par;
  R_WIND_DELETE ret;

  PUT_C_ALL(WIND_DELETE, &par);

  par.id = wid;
        
  CLIENT_SEND_RECV(&par,
                   sizeof (C_WIND_DELETE),
                   &ret,
                   sizeof (R_WIND_DELETE));

  return ret.common.retval;
}


/*wind_delete 0x0067*/
void Wind_delete(AES_PB *apb)
{
  CHECK_APID(apb->global->apid);

  apb->int_out[0] = Wind_do_delete(apb->global->apid,
                                   apb->int_in[0]);
}

/*wind_get 0x0068*/


/*
** Description
** Library part of wind_get
*/
WORD
Wind_do_get (WORD   apid,
             WORD   handle,
             WORD   mode,
             WORD * parm1,
             WORD * parm2,
             WORD * parm3,
             WORD * parm4,
             WORD   in_workarea)
{
  C_WIND_GET par;
  R_WIND_GET ret;
  RECT       workarea;

  /*
  ** We can handle some calls without calling the server.
  */
  switch (mode)
  {
  case WF_WORKXYWH:
  case WF_CURRXYWH:
  case WF_PREVXYWH:
  case WF_FULLXYWH:
  {
    WINDOW_STRUCT * ws;
    
    ws = find_window_struct (apid, handle);
    /*
    ** Return area if we have it. If not just do a server call the normal
    ** way.
    */
    if (ws != NULL)
    {
      RECT * size;

      switch (mode)
      {
      case WF_WORKXYWH :
        size = &ws->worksize;
        break;

      case WF_CURRXYWH :
        size = &ws->totsize;
        break;

      case WF_PREVXYWH :
        size = &ws->lastsize;
        break;

      case WF_FULLXYWH :
        size = &ws->maxsize;
        break;

      default:
        /* This should be impossible */
        DEBUG1("Wind_do_get: Unhandled size mode %d", mode);
        return 0;
      }

      *parm1 = size->x;
      *parm2 = size->y;
      *parm3 = size->width;
      *parm4 = size->height;

      /* OK */
      return 1;
    }
  }
  break;

  case WF_TOP:
  case WF_DCOLOR:
  case WF_OWNER:
  case WF_BOTTOM:
  case WF_ICONIFY:
  case WF_UNICONIFY:
    /* These calls are only handled in the server */
    break;

  case WF_FIRSTXYWH:
  case WF_NEXTXYWH:
  case WF_FTOOLBAR:
  case WF_NTOOLBAR:
    /* These calls are handled locally after server call(s) */
    break;

  case WF_HSLIDE:
  case WF_VSLIDE:
  case WF_NEWDESK:
  case WF_HSLSIZE:
  case WF_VSLSIZE:
  case WF_SCREEN:
  case WF_COLOR:
  case WF_BEVENT:
  case WF_TOOLBAR:
    /* These calls should be implemented locally */
    DEBUG1("Wind_do_get: Implement mode %d locally!", mode);
    break;

  case WF_NAME:
  case WF_INFO:
  case WF_KIND:
  case WF_RESVD:
  case WF_UNICONIFYXYWH:
  case WF_WINX:
  case WF_WINXCFG:
  default:
    DEBUG1("Unhandled mode in Wind_do_get %d. Implement?\n", mode);
  }

  if(in_workarea && ((mode == WF_FIRSTXYWH) || (mode == WF_NEXTXYWH)))
  {
    /* Get work area of window for later use */
    if (Wind_do_get (apid,
                     handle,
                     WF_WORKXYWH,
                     &workarea.x,
                     &workarea.y,
                     &workarea.width,
                     &workarea.height,
                     TRUE) == 0)
    {
      /* An error occurred */
      return 0;
    }
  }

  PUT_C_ALL(WIND_GET, &par);

  par.handle = handle;
  par.mode = mode;
  par.parm1 = *parm1;
        
  /* Loop until we get the data we need (needed for WF_{FIRST,NEXT}XYWH */
  while(TRUE)
  {
    CLIENT_SEND_RECV(&par,
                     sizeof (C_WIND_GET),
                     &ret,
                     sizeof (R_WIND_GET));
    
    if(in_workarea && ((mode == WF_FIRSTXYWH) || (mode == WF_NEXTXYWH)))
    {
      RECT r;

      /* If the rectangle is empty anyhow just exit the loop */
      if((ret.parm3 == 0) && (ret.parm4 == 0))
      {
        break;
      }
      
      if(Misc_intersect ((RECT *)&ret.parm1, &workarea, &r) > 0)
      {
        /* Rectangles do intersect. Now return the intersecting area */
        *(RECT *)&ret.parm1 = r;
        break;
      }

      par.mode = WF_NEXTXYWH;
    }
    else
    {
      /*
      ** Get out of the loop. It is only needed for WF_{FIRST,NEXT}XYWH 
      ** when getting rectangles within workarea
      */
      break;
    }
  } /* while (TRUE) */
    
  *parm1 = ret.parm1;
  *parm2 = ret.parm2;
  *parm3 = ret.parm3;
  *parm4 = ret.parm4;
        
  return ret.common.retval;
}


/*
** Exported
*/
void
Wind_get (AES_PB *apb)
{
  CHECK_APID(apb->global->apid);

  apb->int_out[0] = Wind_do_get (apb->global->apid,
                                 apb->int_in[0],
                                 apb->int_in[1],
                                 &apb->int_out[1],
                                 &apb->int_out[2],
                                 &apb->int_out[3],
                                 &apb->int_out[4],
                                 TRUE);
}


/*
** Description
** Set the name or the info string of a window
*/
static
inline
WORD
Wind_set_name_or_info (WORD   apid,
                       WORD   id,
                       WORD   mode,
                       BYTE * str)
{
  WORD object = (mode == WF_NAME) ? WMOVER : WINFO;
  WINDOW_STRUCT * win = find_window_struct (apid, id);

  if(win != NULL)
  {
    if(win->tree != NULL)
    {
      TE_PTEXT_PUT(OB_SPEC(&win->tree[object]),
                   (BYTE *)malloc(strlen (str)));
      
      strcpy(TE_PTEXT(OB_SPEC(&win->tree[object])), str);
      
      Wind_redraw_elements (apid, id, &win->totsize, widgetmap [object]);

      return 1;
    }
  }

  return 0;
}
      

/*
** Description
** Set desktop background
*/
static
WORD
Wind_set_desktop_background (WORD     apid,
                             OBJECT * tree)
{
  GLOBAL_APPL * globals = get_globals (apid);
  WORD          x, y, w, h;

  globals->desktop_background = tree;

  /* Adjust the size of the resource to the size of the desktop */
  if (tree != NULL)
  {
    Wind_do_get (apid,
                 0,
                 WF_WORKXYWH,
                 &x,
                 &y,
                 &w,
                 &h,
                 FALSE);

    OB_X_PUT(tree, x);
    OB_Y_PUT(tree, y);
    OB_WIDTH_PUT(tree, w);
    OB_HEIGHT_PUT(tree, h);
  }

  /* Return OK */
  return 1;
}


/*
** Exported
*/
WORD
Wind_do_set(WORD apid,
            WORD handle,
            WORD mode,
            WORD parm1,
            WORD parm2,
            WORD parm3,
            WORD parm4)
{
  C_WIND_SET par;
  R_WIND_SET ret;
  
  switch (mode)
  {
  case WF_NAME :
  case WF_INFO :
    ret.common.retval = 1;
    break;

  case WF_KIND:
  case WF_WORKXYWH:
  case WF_CURRXYWH:
  case WF_PREVXYWH:
  case WF_FULLXYWH:
  case WF_HSLIDE:
  case WF_VSLIDE:
  case WF_TOP:
  case WF_FIRSTXYWH:
  case WF_NEXTXYWH:
  case WF_RESVD:
  case WF_NEWDESK:
  case WF_HSLSIZE:
  case WF_VSLSIZE:
  case WF_SCREEN:
  case WF_COLOR:
  case WF_DCOLOR:
  case WF_OWNER:
  case WF_BEVENT:
  case WF_BOTTOM:
  case WF_ICONIFY:
  case WF_UNICONIFY:
  case WF_UNICONIFYXYWH:
  case WF_TOOLBAR:
  case WF_FTOOLBAR:
  case WF_NTOOLBAR:
  case WF_WINX:
  case WF_WINXCFG:
    PUT_C_ALL(WIND_SET, &par);
    
    par.handle = handle;
    par.mode = mode;
    
    par.parm1 = parm1;
    par.parm2 = parm2;
    par.parm3 = parm3;
    par.parm4 = parm4;
    
    CLIENT_SEND_RECV(&par,
                     sizeof (C_WIND_SET),
                     &ret,
                     sizeof (R_WIND_SET));
    break;

  default:
    DEBUG1("oaesis: Unhandled mode %d in Wind_do_set\n", mode);
  }

  /* FIXME
  ** Check the retval if the server operation went ok
  */
  switch (mode)
  {
  case WF_NAME :
  case WF_INFO :
    return Wind_set_name_or_info (apid,
                                  handle,
                                  mode,
                                  (BYTE *)INTS2LONG (parm1,
                                                     parm2));

  case WF_CURRXYWH :
    return Wind_set_size (apid, handle, (RECT *)&par.parm1);
    
  case WF_NEWDESK :
    return Wind_set_desktop_background (apid,
                                        (OBJECT *)INTS2LONG (parm1, parm2));

  case WF_HSLIDE :
    return Wind_set_slider (apid,
                            handle,
                            1,
                            HSLIDE,
                            parm1,
                            -1);

  case WF_VSLIDE :
    return Wind_set_slider (apid,
                            handle,
                            1,
                            VSLIDE,
                            parm1,
                            -1);

  case WF_HSLSIZE :
    return Wind_set_slider (apid,
                            handle,
                            1,
                            HSLIDE,
                            -1,
                            parm1);

  case WF_VSLSIZE :
    return Wind_set_slider (apid,
                            handle,
                            1,
                            VSLIDE,
                            -1,
                            parm1);

  case WF_DCOLOR:
    /* These are calls that should not be handled here */
    break;

  case WF_ICONIFY:
    return Wind_set_iconify(apid, handle, (RECT *)&par.parm1);

  case WF_UNICONIFY:
    return Wind_set_uniconify(apid, handle, (RECT *)&par.parm1);

  case WF_KIND:
  case WF_WORKXYWH:
  case WF_PREVXYWH:
  case WF_FULLXYWH:
  case WF_TOP:
  case WF_FIRSTXYWH:
  case WF_NEXTXYWH:
  case WF_RESVD:
  case WF_SCREEN:
  case WF_COLOR:
  case WF_OWNER:
  case WF_BEVENT:
  case WF_BOTTOM:
  case WF_UNICONIFYXYWH:
  case WF_TOOLBAR:
  case WF_FTOOLBAR:
  case WF_NTOOLBAR:
  case WF_WINX:
  case WF_WINXCFG:
  default:
    DEBUG1("oaesis: Unhandled mode %d in Wind_do_set\n", mode);
  }

  return ret.common.retval;
}


/*
** Exported
*/
void
Wind_set (AES_PB *apb)
{
  CHECK_APID(apb->global->apid);

  apb->int_out[0] = Wind_do_set (apb->global->apid,
                                  apb->int_in[0],
                                  apb->int_in[1],
                                  apb->int_in[2],
                                  apb->int_in[3],
                                  apb->int_in[4],
                                  apb->int_in[5]);
}


/*
** Exported
*/
WORD
Wind_do_find (WORD apid,
              WORD x,
              WORD y) {
  C_WIND_FIND par;
  R_WIND_FIND ret;

  PUT_C_ALL(WIND_FIND, &par);

  par.x = x;
  par.y = y;
        
  CLIENT_SEND_RECV(&par,
                   sizeof (C_WIND_FIND),
                   &ret,
                   sizeof (R_WIND_FIND));

  return ret.common.retval;
}


/*
** Exported
*/
void
Wind_find (AES_PB *apb)
{
  CHECK_APID(apb->global->apid);

  apb->int_out[0] = Wind_do_find (apb->global->apid,
                                  apb->int_in[0],
                                  apb->int_in[1]);
}


/*wind_update 0x006b*/

/*
** Exported
**
** 1998-10-04 CG
*/
WORD
Wind_do_update (WORD apid,
                WORD mode)
{
  C_WIND_UPDATE par;
  R_WIND_UPDATE ret;

  PUT_C_ALL(WIND_UPDATE, &par);

  par.mode = mode;

  CLIENT_SEND_RECV(&par,
                   sizeof (C_WIND_UPDATE),
                   &ret,
                   sizeof (R_WIND_UPDATE));

  return ret.common.retval;
}


void
Wind_update(AES_PB *apb)
{
  CHECK_APID(apb->global->apid);

  apb->int_out[0] = Wind_do_update (apb->global->apid,
                                    apb->int_in[0]);
}


/*wind_calc 0x006c*/
void
Wind_calc (AES_PB *apb)
{
  CHECK_APID(apb->global->apid);

  calcworksize (apb->global->apid,
                apb->int_in[1],
                (RECT *)&apb->int_in[2],
                (RECT *)&apb->int_out[1],
                apb->int_in[0]);
  
  apb->int_out[0] = 1;
}


/*
** Description
** Implementation of wind_new ()
*/
static
inline
WORD
Wind_do_new (WORD apid) {
  C_WIND_NEW par;
  R_WIND_NEW ret;

  PUT_C_ALL(WIND_NEW, &par);

  CLIENT_SEND_RECV(&par,
                   sizeof (C_WIND_NEW),
                   &ret,
                   sizeof (R_WIND_NEW));

  /* FIXME: Remove structures on lib side */

  return ret.common.retval;
}


/*
** Description
** 0x006d wind_new ()
*/
void
Wind_new (AES_PB * apb)
{
  CHECK_APID(apb->global->apid);

  apb->int_out[0] = Wind_do_new(apb->global->apid);
}


/*
** Exported
**
** 1998-12-20 CG
** 1999-01-10 CG
*/
OBJECT *
Wind_get_rsrc (WORD apid,
               WORD id) {
  GLOBAL_APPL *   globals = get_globals (apid);
  WINDOW_LIST_REF wl;

  wl = globals->windows;

  /* Find the window structure for our window */
  while (wl != WINDOW_LIST_REF_NIL) {
    if (wl->ws.id == id) {
      return wl->ws.tree;
    }

    wl = wl->next;
  }

  return NULL;
}


/*
** Exported
*/
WORD
Wind_change (WORD apid,
             WORD id,
             WORD object,
             WORD newstate)
{
  WINDOW_STRUCT * win;

  win = find_window_struct(apid, id);
        
  if(win)
  {
    if (newstate != OB_STATE(&win->tree[widgetmap[object]]))
    {
      GLOBAL_COMMON * globals = get_global_common();
      OB_STATE_PUT(&win->tree[widgetmap[object]], newstate);
      Wind_redraw_elements(apid, id, &globals->screen, widgetmap[object]);
    }
    
    return 1;
  }
  
  return 0;
}


/*
** Description
** Inform a window that it is new top window or no longer top window
*/
void
Wind_newtop(int apid,
            int id,
            int status)
{
  if((status == WIND_NEWTOP) || (status == WIND_UNTOPPED))
  {
    WINDOW_STRUCT * win;

    win = find_window_struct(apid, id);

    if(win != NULL)
    {
      int i;

      if(status == WIND_NEWTOP)
      {
        win->status |= WIN_TOPPED;
      }
      else
      {
        win->status &= ~WIN_TOPPED;
      }

      for(i = 0; i <= W_SMALLER; i++)
      {
        set_widget_colour(win, i, win->top_colour[i], win->untop_colour[i]);
      }
    }
    else
    {
      DEBUG1("Wind_newtop: window struct for apid %d id %d not found",
             apid, id);
    }
  }
  else
  {
    DEBUG0("Wind_newtop: illegal status: %d\n", status);
  }
}
