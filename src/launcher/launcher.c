/*
** launcher.c
**
** Copyright 1998 Christer Gustavsson <cg@nocrew.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** Read the file COPYING for more information.
*/

#include <stdio.h>

#include <aesbind.h>
#include <vdibind.h>

#define WORD short
#define LONG long

#define HI_WORD(l) ((WORD)((LONG)l >> 16))
#define LO_WORD(l) ((WORD)((LONG)l & 0xffff))

static int  vid;
static WORD num_colors;
static char title[] = "Lines";

static
WORD
max (WORD a,
     WORD b) {
  if(a > b) 
    return a;
  else
    return b;
}

static
WORD
min (WORD a,
     WORD b) {
  if(a < b) 
    return a;
  else
    return b;
}

#define NUM_LINES       10

void
updatewait (int wid) {
  WORD  quit_count = 0;
  WORD  ant_klick;
  WORD  buffert[16];
  WORD  happ;
  WORD  knapplage;
  WORD  lastline = 0;
  WORD  num_lines = 1;
  WORD  tangent,tanglage;
  WORD  winx,winy,winw,winh;
  WORD  x,y,w,h;
  VRECT lines[NUM_LINES];
  
  WORD  sx1 = 5,sy1 = 10,sx2 = 15,sy2 = 5;

  wind_get (wid, WF_WORKXYWH, &winx, &winy, &winw, &winh);

  /*
  fprintf (stderr, "wind_get (WF_WORKXYWH...): x=%d y=%d w=%d h=%d\n",
           winx, winy, winw, winh);
           */

  lines[0].v_x1 = winx;
  lines[0].v_y1 = winy;
  lines[0].v_x2 = winx + 100;
  lines[0].v_y2 = winy + 100;

  do {
    happ = evnt_multi(MU_KEYBD | MU_MESAG | MU_TIMER | MU_BUTTON,
                      0,0,0,0,0,0,0,0,0,0,0,0,0,
                      buffert,0,&x,&y,&knapplage,&tanglage,
                      &tangent,&ant_klick);

    if (happ & MU_MESAG) {
      fprintf (stderr,
               "lines.prg: evnt_multi returned MU_MESAG, buffert[0] = 0x%x (%d)\n",
               buffert[0], buffert[0]);
    }

    if (happ & MU_BUTTON) {
      fprintf (stderr,
               "lines.prg: evnt_multi returned MU_BUTTON x = %d y = %d buttons = %d\n",
               x, y, knapplage);
      /*quit_count++;*/

      if (quit_count == 10) {
        break;
      }
    }

    if ((happ & MU_MESAG) && (buffert[0] == WM_REDRAW)) {
      WORD      x,y,w,h;

      fprintf (stderr, "lines.prg: Got WM_REDRAW\n");

      wind_update(BEG_UPDATE);
      
      wind_get (wid, WF_FIRSTXYWH, &x, &y, &w, &h);

      /*
      fprintf (stderr, "wind_get (WF_FIRSTXYWH...): x=%d y=%d w=%d h=%d\n",
               x, y, w, h);
               */

      while((w > 0) && (h > 0))
      {
        WORD    xn,yn,wn,hn;
        
        xn = max(x,buffert[4]);
        wn = min(x + w, buffert[4] + buffert[6]) - xn;
        yn = max(y,buffert[5]);
        hn = min(y + h, buffert[5] + buffert[7]) - yn;
   
        if((wn > 0) && (hn > 0))
        {
          WORD  i;
          int   xyxy[4];
          
          xyxy[0] = xn;
          xyxy[1] = yn;
          xyxy[2] = xn+wn-1;
          xyxy[3] = yn+hn-1;

          vs_clip(vid,1,xyxy);

          /*
          graf_mouse(M_OFF,NULL);
          */

          vr_recfl(vid,xyxy);
          
          for(i = 0; i < num_lines; i++) {
            vsl_color(vid,i % (num_colors - 1));
            
            v_pline(vid,2,&lines[i]);
          };

          /*
          graf_mouse(M_ON,NULL);
          */

          vs_clip(vid,0,xyxy);
        }
        
        wind_get(wid,WF_NEXTXYWH,&x,&y,&w,&h);
      }                         
      wind_update(END_UPDATE);
    } else if((happ & MU_MESAG) && (buffert[0] == WM_TOPPED)) {
      wind_set (wid, WF_TOP, 0, 0, 0, 0);
    } else if((happ & MU_MESAG) && (buffert[0] == WM_CLOSED)) {
      fprintf (stderr, "lines.prg: WM_CLOSED received\n");

      wind_close (wid);
      
      break;
    } else if((happ & MU_MESAG) && (buffert[0] == WM_SIZED)) {          
      WORD      i;
      WORD      newx,newy,neww,newh;
      
      wind_set(wid,WF_CURRXYWH,buffert[4],buffert[5]
               ,buffert[6],buffert[7]);
      
      wind_get(wid,WF_WORKXYWH,&newx,&newy,&neww,&newh);
      
      for(i = 0; i < num_lines; i++) {
        if(lines[i].v_x1 >= (newx + neww)) {
          lines[i].v_x1 = newx + neww - 1;
        };
        
        if(lines[i].v_y1 >= (newy + newh)) {
          lines[i].v_y1 = newy + newh - 1;
        };
        
        if(lines[i].v_x2 >= (newx + neww)) {
          lines[i].v_x2 = newx + neww - 1;
        };
        
        if(lines[i].v_y2 >= (newy + newh)) {
          lines[i].v_y2 = newy + newh - 1;
        };
      };
      
      winx = newx;
      winy = newy;
      winw = neww;
      winh = newh;
    } else if((happ & MU_MESAG) && (buffert[0] == WM_MOVED)) {
      WORD      i;
      WORD      newx,newy,neww,newh;
      
      wind_set(wid,WF_CURRXYWH,buffert[4],buffert[5]
               ,buffert[6],buffert[7]);
      
      wind_get(wid,WF_WORKXYWH,&newx,&newy,&neww,&newh);
      
      for(i = 0; i < num_lines; i++) {
        lines[i].v_x1 += newx - winx;
        lines[i].v_y1 += newy - winy;
        lines[i].v_x2 += newx - winx;
        lines[i].v_y2 += newy - winy;
      }
      
      winx = newx;
      winy = newy;
      winw = neww;
      winh = newh;
    } else if((happ & MU_KEYBD) && ((tangent & 0xff) == 'q')) {
      break;
    } else if(happ & MU_TIMER) {
      VRECT     delete;

      /*
      fprintf (stderr, "lines.prg: evnt_multi returned MU_TIMER\n");
      */

      delete = lines[(lastline + 1) % NUM_LINES];
      lines[(lastline + 1) % NUM_LINES] = lines[lastline];
      lastline = (lastline + 1) % NUM_LINES;
      
      lines[lastline].v_x1 += sx1;
      if((lines[lastline].v_x1 >= (winx + winw)) ||
         (lines[lastline].v_x1 < winx)) {
        sx1 = -sx1;
        
        lines[lastline].v_x1 += sx1;
      };
      
      lines[lastline].v_y1 += sy1;
      if((lines[lastline].v_y1 >= (winy + winh)) ||
         (lines[lastline].v_y1 < winy)) {
        sy1 = -sy1;
        
        lines[lastline].v_y1 += sy1;
      };
      
      lines[lastline].v_x2 += sx2;
      if((lines[lastline].v_x2 >= (winx + winw)) ||
         (lines[lastline].v_x2 < winx)) {
        sx2 = -sx2;
        
        lines[lastline].v_x2 += sx2;
      };
      
      lines[lastline].v_y2 += sy2;
      if((lines[lastline].v_y2 >= (winy + winh)) ||
         (lines[lastline].v_y2 < winy)) {
        sy2 = -sy2;
        
        lines[lastline].v_y2 += sy2;
      };

      wind_update(BEG_UPDATE);
      
      wind_get(wid,WF_FIRSTXYWH,&x,&y,&w,&h);

      /*
      fprintf (stderr, "wind_get (WF_FIRSTXYWH...): x=%d y=%d w=%d h=%d\n",
               x, y, w, h);
               */

      while((w > 0) && (h > 0)) {
        int     xyxy[4];
        
        xyxy[0] = x;
        xyxy[1] = y;
        xyxy[2] = x + w - 1;
        xyxy[3] = y + h - 1;

        vs_clip(vid,1,xyxy);

        /*
        graf_mouse(M_OFF,NULL);
        */

        vsl_color(vid,lastline % (num_colors - 1));
        v_pline(vid,2,&lines[lastline]);
        
        if(num_lines == NUM_LINES) {
          vsl_color(vid,BLACK);
          v_pline(vid,2,&delete);
        }

        /*
        graf_mouse(M_ON,NULL);
        */

        vs_clip(vid,0,xyxy);

        wind_get(wid,WF_NEXTXYWH,&x,&y,&w,&h);
      }
      
      wind_update(END_UPDATE);

      if(num_lines < NUM_LINES) {
        num_lines++;
      };
    };
  }while(1);
}

void testwin(void)
{
  WORD  xoff,yoff,woff,hoff;
  
  WORD  wid;
  
  wid = wind_create(NAME|MOVER|FULLER|SIZER|CLOSER,0,0,640,480);

  wind_set (wid, WF_NAME, (long)title >> 16, (long)title & 0xffff, 0, 0);
  
  /*
  wind_get(0,WF_WORKXYWH,&xoff,&yoff,&woff,&hoff);
  */

  xoff = 0;
  yoff = 0;
  woff = 640;
  hoff = 480;
  wind_open(wid,xoff,yoff,woff,hoff);

  updatewait(wid);

  /*
  wind_delete(wid);
  */
}


/*
** Description
** The launcher takes care of program and accessory launching for oaesis
**
** 1998-12-28 CG
** 1999-01-01 CG
*/
int
main ()
{
  int   work_in[] = {1,1,1,1,1,1
                     ,1,1,1,1,2,0};
  
  int   work_out[57];
  static OBJECT deskbg [1];
  char path[] = "/usr/dum";
  char file[] = "djuro";
  short exit_button;
  
  WORD  wc, hc, wb, hb;

  fprintf (stderr, "lines.prg: calling appl_init\n");
  appl_init();
  fprintf (stderr, "lines.prg: returned from appl_init\n");

  /* Setup something to use as desktop background */
  deskbg[0].ob_next = OO_LAST;
  deskbg[0].ob_head = OO_LAST;
  deskbg[0].ob_tail = OO_LAST;
  deskbg[0].ob_type = G_BOX;
  deskbg[0].ob_flags = LASTOB;
  deskbg[0].ob_state = 0;
  deskbg[0].ob_spec.obspec.character = 0;
  deskbg[0].ob_spec.obspec.framesize = 0;
  deskbg[0].ob_spec.obspec.framecol = 0;
  deskbg[0].ob_spec.obspec.textcol = 0;
  deskbg[0].ob_spec.obspec.textmode = 0;
  deskbg[0].ob_spec.obspec.interiorcol = 10;
  deskbg[0].ob_spec.obspec.fillpattern = 7;
  deskbg[0].ob_x = 0;
  deskbg[0].ob_y = 0;
  deskbg[0].ob_width = 1024;
  deskbg[0].ob_height = 768;
  
  wind_set (0,
            WF_NEWDESK,
            HI_WORD(deskbg),
            LO_WORD(deskbg),
            0,
            0);

  vid = graf_handle (&wc, &hc, &wb, &hb);
  v_opnvwk(work_in,&vid,work_out);
  num_colors = work_out[39];
  num_colors = 256;

  vsf_interior(vid,1);

  #if 0
  graf_mouse(ARROW,0L);
  #endif

  fsel_exinput (path, file, &exit_button, "Hulabopp");

  testwin();

  v_clsvwk(vid);

  appl_exit();
        
  return 0;
}