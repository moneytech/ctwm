/* 
 *  [ ctwm ]
 *
 *  Copyright 1992 Claude Lecommandeur.
 *            
 * Permission to use, copy, modify  and distribute this software  [ctwm] and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above  copyright notice appear  in all copies and that both that
 * copyright notice and this permission notice appear in supporting documen-
 * tation, and that the name of  Claude Lecommandeur not be used in adverti-
 * sing or  publicity  pertaining to  distribution of  the software  without
 * specific, written prior permission. Claude Lecommandeur make no represen-
 * tations  about the suitability  of this software  for any purpose.  It is
 * provided "as is" without express or implied warranty.
 *
 * Claude Lecommandeur DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL  IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS.  IN NO
 * EVENT SHALL  Claude Lecommandeur  BE LIABLE FOR ANY SPECIAL,  INDIRECT OR
 * CONSEQUENTIAL  DAMAGES OR ANY  DAMAGES WHATSOEVER  RESULTING FROM LOSS OF
 * USE, DATA  OR PROFITS,  WHETHER IN AN ACTION  OF CONTRACT,  NEGLIGENCE OR
 * OTHER  TORTIOUS ACTION,  ARISING OUT OF OR IN  CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Claude Lecommandeur [ lecom@sic.epfl.ch ][ April 1992 ]
 */
#include <stdio.h>
#if defined(sony_news) || defined __QNX__
#  include <ctype.h>
#endif
#include "twm.h"
#include "util.h"
#include "parse.h"
#include "screen.h"
#include "icons.h"
#include "resize.h"
#include "add_window.h"
#include "events.h"
#include "gram.h"
#include "clicktofocus.h"
#ifdef VMS
#include <ctype.h>
#include <string.h>
#include <decw$include/Xos.h>
#include <decw$include/Xatom.h>
#include <X11Xmu/CharSet.h>
#include <decw$include/Xresource.h>
#else
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/Xmu/CharSet.h>
#include <X11/Xresource.h>
#endif
#ifdef macII
int strcmp(); /* missing from string.h in AUX 2.0 */
#endif
#ifdef BUGGY_HP700_SERVER
static void fakeRaiseLower ();
#endif

#ifdef GNOME
#  include "gnomewindefs.h" /* include GNOME hints definitions */
   extern Atom _XA_WIN_WORKSPACE;
   extern Atom _XA_WIN_STATE;
#endif /* GNOME */

extern int twmrc_error_prefix (); /* in ctwm.c */
extern int match ();		/* in list.c */
extern char *captivename;

/***********************************************************************
 *
 *  Procedure:
 *	CreateWorkSpaceManager - create teh workspace manager window
 *		for this screen.
 *
 *  Returned Value:
 *	none
 *
 *  Inputs:
 *	none
 *
 ***********************************************************************
 */
#define WSPCWINDOW    0
#define OCCUPYWINDOW  1
#define OCCUPYBUTTON  2

static void Vanish			();
static void DisplayWin			();
static void CreateWorkSpaceManagerWindow ();
static void CreateOccupyWindow		();
static unsigned int GetMaskFromResource	();
unsigned int GetMaskFromProperty	();
static   int GetPropertyFromMask	();
static void PaintWorkSpaceManagerBorder	();
static void PaintButton			();
static void WMapRemoveFromList		();
static void WMapAddToList		();
static void ResizeWorkSpaceManager	();
static void ResizeOccupyWindow		();
static WorkSpace *GetWorkspace  	();
static void WMapRedrawWindow		();

Atom _XA_WM_OCCUPATION;
Atom _XA_WM_CURRENTWORKSPACE;
Atom _XA_WM_WORKSPACESLIST;
Atom _XA_WM_CTWMSLIST;
Atom _OL_WIN_ATTR;

int       fullOccupation    = 0;
int       useBackgroundInfo = False;
XContext  MapWListContext = (XContext) 0;
static Cursor handCursor  = (Cursor) 0;
static Bool DontRedirect ();

extern Bool MaybeAnimate;
extern FILE *tracefile;

void InitWorkSpaceManager () {
    Scr->workSpaceMgr.count	    = 0;
    Scr->workSpaceMgr.workSpaceList = NULL;
    Scr->workSpaceMgr.initialstate  = BUTTONSSTATE;
    Scr->workSpaceMgr.geometry      = NULL;
    Scr->workSpaceMgr.buttonStyle   = STYLE_NORMAL;
    Scr->workSpaceMgr.windowcp.back = Scr->White;
    Scr->workSpaceMgr.windowcp.fore = Scr->Black;
    Scr->workSpaceMgr.windowcpgiven = False;

    Scr->workSpaceMgr.occupyWindow = (OccupyWindow*) malloc (sizeof (OccupyWindow));
    Scr->workSpaceMgr.occupyWindow->name      = "Occupy Window";
    Scr->workSpaceMgr.occupyWindow->icon_name = "Occupy Window Icon";
    Scr->workSpaceMgr.occupyWindow->geometry  = NULL;
    Scr->workSpaceMgr.occupyWindow->columns   = 0;
    Scr->workSpaceMgr.occupyWindow->twm_win   = (TwmWindow*) 0;

    XrmInitialize ();
    if (MapWListContext == (XContext) 0) MapWListContext = XUniqueContext ();
}

ConfigureWorkSpaceManager () {
    virtualScreen *vs;
    for (vs = Scr->vScreenList; vs != NULL; vs = vs->next) {
        WorkSpaceWindow *wsw = (WorkSpaceWindow*) malloc (sizeof (WorkSpaceWindow));
	wsw->name	     = "WorkSpaceManager";
	wsw->icon_name       = "WorkSpaceManager Icon";
	wsw->twm_win	     = (TwmWindow*) 0;
	wsw->state	     = Scr->workSpaceMgr.initialstate; /*  = BUTTONSSTATE */
	wsw->curImage        = None;
	wsw->curColors.back  = Scr->Black;
	wsw->curColors.fore  = Scr->White;
	wsw->curPaint        = False;
	wsw->defImage        = None;
	wsw->defColors.back  = Scr->White;
	wsw->defColors.fore  = Scr->Black;
	wsw->vspace          = Scr->WMgrVertButtonIndent;
	wsw->hspace          = Scr->WMgrHorizButtonIndent;
	vs->wsw = wsw;
    }
    Scr->workSpaceMgr.occupyWindow->vspace = Scr->WMgrVertButtonIndent;
    Scr->workSpaceMgr.occupyWindow->hspace = Scr->WMgrHorizButtonIndent;
}

void CreateWorkSpaceManager () {
    WorkSpace       *wlist;
    char wrkSpcList [512];
    virtualScreen    *vs;
    WorkSpace        *ws;
    int len, junk;

    if (! Scr->workSpaceManagerActive) return;

#ifdef I18N
    Scr->workSpaceMgr.windowFont.basename =
      "-adobe-courier-medium-r-normal--10-100-75-75-m-60-iso8859-1";
#else
    Scr->workSpaceMgr.windowFont.name = "-adobe-times-*-r-*--10-*-*-*-*-*-*-*";
#endif    
    Scr->workSpaceMgr.buttonFont = Scr->IconManagerFont;
    Scr->workSpaceMgr.cp	 = Scr->IconManagerC;
    if (!Scr->BeNiceToColormap) GetShadeColors (&Scr->workSpaceMgr.cp);

    _XA_WM_OCCUPATION       = XInternAtom (dpy, "WM_OCCUPATION",        False);
    _XA_WM_CURRENTWORKSPACE = XInternAtom (dpy, "WM_CURRENTWORKSPACE",  False);
#ifdef GNOME
    _XA_WM_WORKSPACESLIST   = XInternAtom (dpy, "_WIN_WORKSPACE_NAMES", False);
#else /* GNOME */
    _XA_WM_WORKSPACESLIST   = XInternAtom (dpy, "WM_WORKSPACESLIST", False);
#endif /* GNOME */
    _OL_WIN_ATTR            = XInternAtom (dpy, "_OL_WIN_ATTR",         False);

    NewFontCursor (&handCursor, "top_left_arrow");

    ws = Scr->workSpaceMgr.workSpaceList;
    for (vs = Scr->vScreenList; vs != NULL; vs = vs->next) {
      WorkSpaceWindow *wsw = vs->wsw;
      wsw->currentwspc = ws;
      CreateWorkSpaceManagerWindow (vs);
      ws = ws->next;
    }
    CreateOccupyWindow ();

    for (vs = Scr->vScreenList; vs != NULL; vs = vs->next) {
      WorkSpaceWindow *wsw = vs->wsw;
      WorkSpace *ws = wsw->currentwspc;
      MapSubwindow *msw = wsw->mswl [ws->number];
      if (wsw->curImage == None) {
	if (wsw->curPaint) {
	  XSetWindowBackground (dpy, msw->w, wsw->curColors.back);
	}
      } else {
	XSetWindowBackgroundPixmap (dpy, msw->w, wsw->curImage->pixmap);
      }
      XSetWindowBorder (dpy, msw->w, wsw->curBorderColor);
      XClearWindow (dpy, msw->w);

      if (useBackgroundInfo && ! Scr->DontPaintRootWindow) {
	if (ws->image == None)
	    XSetWindowBackground       (dpy, vs->window, ws->backcp.back);
	else
	    XSetWindowBackgroundPixmap (dpy, vs->window, ws->image->pixmap);
	XClearWindow (dpy, vs->window);
      }
    }
    len = GetPropertyFromMask (0xFFFFFFFF, wrkSpcList, &junk);
    XChangeProperty (dpy, Scr->Root, _XA_WM_WORKSPACESLIST, XA_STRING, 8, 
		     PropModeReplace, (unsigned char *) wrkSpcList, len);
}

void GotoWorkSpaceByName (vs, wname)
virtualScreen *vs;
char *wname;
{
    WorkSpace *ws;
    if (! Scr->workSpaceManagerActive) return;
    if (!vs) return;
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
      if (strcmp (ws->label, wname) == 0) break;
    }
    if (ws == NULL) {
      for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	if (strcmp (ws->name, wname) == 0) break;
      }
    }
    if (ws == NULL) return;
    GotoWorkSpace (vs, ws);
}

/* 6/19/1999 nhd for GNOME compliance */
void GotoWorkSpaceByNumber(vs, workspacenum)
virtualScreen *vs;
int workspacenum;
{
    WorkSpace *ws;
    if(! Scr->workSpaceManagerActive) return;
    if (!vs) return;
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
        if (ws->number == workspacenum) break;
    }
    if (ws == NULL) return;
    GotoWorkSpace (vs, ws);
}
 /* */
 
void GotoPrevWorkSpace (vs)
virtualScreen *vs;
{
    WorkSpace *ws1, *ws2;

    if (! Scr->workSpaceManagerActive) return;
    if (!vs) return;
    ws1 = Scr->workSpaceMgr.workSpaceList;
    if (ws1 == NULL) return;
    ws2 = ws1->next;

    while ((ws2 != vs->wsw->currentwspc) && (ws2 != NULL)) {
	ws1 = ws2;
	ws2 = ws2->next;
    }
    GotoWorkSpace (vs, ws1);
}

void GotoNextWorkSpace (vs)
virtualScreen *vs;
{
    WorkSpace *ws;
    if (! Scr->workSpaceManagerActive) return;
    if (!vs) return;

    ws = vs->wsw->currentwspc;
    ws = (ws->next != NULL) ? ws->next : Scr->workSpaceMgr.workSpaceList;
    GotoWorkSpace (vs, ws);
}

void GotoRightWorkSpace (vs)
virtualScreen *vs;
{
    WorkSpace *ws;
    int number, columns, count, i;

    if (! Scr->workSpaceManagerActive) return;
    if (!vs) return;

    ws      = vs->wsw->currentwspc;
    number  = ws->number;
    columns = Scr->workSpaceMgr.columns;
    count   = Scr->workSpaceMgr.count;
    number += ((number + 1) % columns) ? 1 : (1 - columns);
    if (number >= count) number = (number / columns) * columns;
    ws = Scr->workSpaceMgr.workSpaceList;
    for (i = 0; i < number; i++) ws = ws->next;
    GotoWorkSpace (vs, ws);
}

void GotoLeftWorkSpace (vs)
virtualScreen *vs;
{
    WorkSpace *ws;
    int number, columns, count, i;

    if (! Scr->workSpaceManagerActive) return;
    if (!vs) return;

    ws      = vs->wsw->currentwspc;
    number  = ws->number;
    columns = Scr->workSpaceMgr.columns;
    count   = Scr->workSpaceMgr.count;
    number += (number % columns) ? -1 : (columns - 1);
    if (number >= count) number = count - 1;
    ws = Scr->workSpaceMgr.workSpaceList;
    for (i = 0; i < number; i++) ws = ws->next;
    GotoWorkSpace (vs, ws);
}

void GotoUpWorkSpace (vs)
virtualScreen *vs;
{
    WorkSpace *ws;
    int number, lines, columns, count, i;

    if (! Scr->workSpaceManagerActive) return;
    if (!vs) return;

    ws      = vs->wsw->currentwspc;
    number  = ws->number;
    lines   = Scr->workSpaceMgr.lines;
    columns = Scr->workSpaceMgr.columns;
    count   = Scr->workSpaceMgr.count;
    number += (number >= columns) ? - columns : ((lines - 1) * columns);
    while (number >= count) number -= columns;
    ws = Scr->workSpaceMgr.workSpaceList;
    for (i = 0; i < number; i++) ws = ws->next;
    GotoWorkSpace (vs, ws);
}

void GotoDownWorkSpace (vs)
virtualScreen *vs;
{
    WorkSpace *ws;
    int number, lines, columns, count, i;

    if (! Scr->workSpaceManagerActive) return;
    if (!vs) return;

    ws      = vs->wsw->currentwspc;
    number  = ws->number;
    lines   = Scr->workSpaceMgr.lines;
    columns = Scr->workSpaceMgr.columns;
    count   = Scr->workSpaceMgr.count;
    number += (number < ((lines - 1) * columns)) ? columns : ((1 - lines) * columns);
    if (number >= count) number = number % columns;
    ws = Scr->workSpaceMgr.workSpaceList;
    for (i = 0; i < number; i++) ws = ws->next;
    GotoWorkSpace (vs, ws);
}

ShowBackground (vs)
virtualScreen *vs;
{
  static int state = 0;
  TwmWindow *twmWin;

  if (state) {
    for (twmWin = &(Scr->TwmRoot); twmWin != NULL; twmWin = twmWin->next) {
      if (twmWin->savevs == vs) {
	DisplayWin (vs, twmWin);
      }
      twmWin->savevs = NULL;
    }
    state = 0;
  } else {
    for (twmWin = &(Scr->TwmRoot); twmWin != NULL; twmWin = twmWin->next) {
      if (twmWin->vs == vs) {
	twmWin->savevs = twmWin->vs;
	Vanish (vs, twmWin);
      }
    }
    state = 1;
  }
}
void GotoWorkSpace (vs, ws)
virtualScreen *vs;
WorkSpace *ws;
{
    TwmWindow	         *twmWin;
    WorkSpace            *oldws, *newws;
    WList	         *wl, *wl1;
    WinList	         winl;
    XSetWindowAttributes attr;
    XWindowAttributes    winattrs;
    unsigned long	 eventMask;
    IconMgr		 *iconmgr;
    Window		 cachew;
    unsigned long	 valuemask;
    TwmWindow		 *focuswindow;
    TwmWindow		 *last_twmWin;
    virtualScreen        *tmpvs;

    if (! Scr->workSpaceManagerActive) return;
    for (tmpvs = Scr->vScreenList; tmpvs != NULL; tmpvs = tmpvs->next) {
      if (ws == tmpvs->wsw->currentwspc) {
	XBell (dpy, 0);
	return;
      }
    }
    oldws = vs->wsw->currentwspc;
    newws = ws;
    if (oldws == newws) return;

    valuemask = (CWBackingStore | CWSaveUnder);
    attr.backing_store = NotUseful;
    attr.save_under    = False;
    cachew = XCreateWindow (dpy, Scr->Root, vs->x, vs->y,
			(unsigned int) vs->w, (unsigned int) vs->h, (unsigned int) 0,
			CopyFromParent, (unsigned int) CopyFromParent,
			(Visual *) CopyFromParent, valuemask,
			&attr);
    XMapWindow (dpy, cachew);

    if (useBackgroundInfo && ! Scr->DontPaintRootWindow) {
	if (newws->image == None)
	    XSetWindowBackground       (dpy, vs->window, newws->backcp.back);
	else
	    XSetWindowBackgroundPixmap (dpy, vs->window, newws->image->pixmap);
	XClearWindow (dpy, vs->window);
    }
    focuswindow = (TwmWindow*)0;
    for (twmWin = &(Scr->TwmRoot); twmWin != NULL; twmWin = twmWin->next) {
	if (twmWin->vs == vs) {
	  if (!OCCUPY (twmWin, newws)) {
	    virtualScreen *tvs;
	    Vanish (vs, twmWin);
	    for (tvs = Scr->vScreenList; tvs != NULL; tvs = tvs->next) {
	      if (tvs == vs) continue;
	      if (OCCUPY (twmWin, tvs->wsw->currentwspc)) {
		DisplayWin (tvs, twmWin);
		break;
	      }
	    }
	  } else
	    if (twmWin->hasfocusvisible) {
	      focuswindow = twmWin;
	      SetFocusVisualAttributes (focuswindow, False);
	    }
	}
    }
    /* Move to the end of the twmWin list */
    for (twmWin = &(Scr->TwmRoot); twmWin != NULL; twmWin = twmWin->next) {
	last_twmWin = twmWin;
    }
    /* Iconise in reverse order */
    for (twmWin = last_twmWin; twmWin != NULL; twmWin = twmWin->prev) {
	if (OCCUPY (twmWin, newws) && !twmWin->vs) DisplayWin (vs, twmWin);
    }
/*
   Reorganize window lists
*/
    for (twmWin = &(Scr->TwmRoot); twmWin != NULL; twmWin = twmWin->next) {
	wl = twmWin->list;
	if (wl == NULL) continue;
	if (OCCUPY (wl->iconmgr->twm_win, newws)) continue;
	wl1 = wl;
	wl  = wl->nextv;
	while (wl != NULL) {
	    if (OCCUPY (wl->iconmgr->twm_win, newws)) break;
	    wl1 = wl;
	    wl  = wl->nextv;
	}
	if (wl != NULL) {
	    wl1->nextv = wl->nextv;
	    wl->nextv  = twmWin->list;
	    twmWin->list = wl;
	}
    }
    wl = (WList*)0;
    for (iconmgr = newws->iconmgr; iconmgr; iconmgr = iconmgr->next) {
	if (iconmgr->first) {
	    wl = iconmgr->first;
	    break;
	}
    }	
    CurrentIconManagerEntry (wl);
    if (focuswindow) {
	SetFocusVisualAttributes (focuswindow, True);
    }
    vs->wsw->currentwspc = newws;
    if (Scr->ReverseCurrentWorkspace && vs->wsw->state == MAPSTATE) {
        MapSubwindow *msw = vs->wsw->mswl [oldws->number];
	for (winl = msw->wl; winl != NULL; winl = winl->next) {
	    WMapRedrawName (vs, winl);
	}
	msw = vs->wsw->mswl [newws->number];
	for (winl = msw->wl; winl != NULL; winl = winl->next) {
	    WMapRedrawName (vs, winl);
	}
    } else
    if (vs->wsw->state == BUTTONSSTATE) {
        ButtonSubwindow *bsw = vs->wsw->bswl [oldws->number];
	PaintButton (WSPCWINDOW, vs, bsw->w, oldws->label, oldws->cp, off);
	bsw = vs->wsw->bswl [newws->number];
	PaintButton (WSPCWINDOW, vs, bsw->w, newws->label, newws->cp,  on);
    }
    oldws->iconmgr = Scr->iconmgr;
    Scr->iconmgr = newws->iconmgr;

    Window oldw = vs->wsw->mswl [oldws->number]->w;
    Window neww = vs->wsw->mswl [newws->number]->w;
    if (useBackgroundInfo) {
	if (oldws->image == None)
	    XSetWindowBackground       (dpy, oldw, oldws->backcp.back);
	else
	    XSetWindowBackgroundPixmap (dpy, oldw, oldws->image->pixmap);
    }
    else {
	if (vs->wsw->defImage == None)
	    XSetWindowBackground       (dpy, oldw, vs->wsw->defColors.back);
	else
	    XSetWindowBackgroundPixmap (dpy, oldw, vs->wsw->defImage->pixmap);
    }
    attr.border_pixel = vs->wsw->defBorderColor;
    XChangeWindowAttributes (dpy, oldw, CWBorderPixel, &attr);

    if (vs->wsw->curImage == None) {
	if (vs->wsw->curPaint) XSetWindowBackground (dpy, neww, vs->wsw->curColors.back);
    }
    else {
	XSetWindowBackgroundPixmap (dpy, neww, vs->wsw->curImage->pixmap);
    }
    attr.border_pixel = vs->wsw->curBorderColor;
    XChangeWindowAttributes (dpy, neww, CWBorderPixel, &attr);

    XClearWindow (dpy, oldw);
    XClearWindow (dpy, neww);

    XGetWindowAttributes(dpy, Scr->Root, &winattrs);
    eventMask = winattrs.your_event_mask;
    XSelectInput(dpy, Scr->Root, eventMask & ~PropertyChangeMask);

    XChangeProperty (dpy, Scr->Root, _XA_WM_CURRENTWORKSPACE, XA_STRING, 8, 
		     PropModeReplace, (unsigned char *) newws->name, strlen (newws->name));
#ifdef GNOME
/* nhd 6/19/1999 for GNOME compliance */
    XChangeProperty (dpy, Scr->Root, _XA_WIN_WORKSPACE, XA_CARDINAL, 32,
		    PropModeReplace, (unsigned char *) &(newws->number), 1);
#endif /* GNOME */
    XSelectInput(dpy, Scr->Root, eventMask);

    XDestroyWindow (dpy, cachew);
    if (Scr->ChangeWorkspaceFunction.func != 0) {
	char *action;
	XEvent event;

	action = Scr->ChangeWorkspaceFunction.item ?
		Scr->ChangeWorkspaceFunction.item->action : NULL;
	ExecuteFunction (Scr->ChangeWorkspaceFunction.func, action,
			   (Window) 0, (TwmWindow*) 0, &event, C_ROOT, FALSE);
    }
    XSync (dpy, 0);
    if (Scr->ClickToFocus || Scr->SloppyFocus) set_last_window (newws);
    MaybeAnimate = True;
}

char *GetCurrentWorkSpaceName (virtualScreen *vs)
{
    if (! Scr->workSpaceManagerActive) return (NULL);
    if (!vs) vs = Scr->vScreenList;
    return vs->wsw->currentwspc->name;
}

void AddWorkSpace (name, background, foreground, backback, backfore, backpix)
char *name, *background, *foreground, *backback, *backfore, *backpix;
{
    WorkSpace *ws;
    int	      wsnum;
    Image     *image;

    wsnum = Scr->workSpaceMgr.count;
    if (wsnum == MAXWORKSPACE) return;

    fullOccupation |= (1 << wsnum);
    ws = (WorkSpace*) malloc (sizeof (WorkSpace));
    ws->FirstWindowRegion = NULL;
#ifdef VMS
    {
       char *ftemp;
       ftemp = (char *) malloc((strlen(name)+1)*sizeof(char));
       wlist->name = strcpy (ftemp,name);
       ftemp = (char *) malloc((strlen(name)+1)*sizeof(char));
       wlist->label = strcpy (ftemp,name);
    }
#else
    ws->name  = (char*) strdup (name);
    ws->label = (char*) strdup (name);
#endif
    ws->clientlist = NULL;

    if (background == NULL)
	ws->cp.back = Scr->IconManagerC.back;
    else
	GetColor (Scr->Monochrome, &(ws->cp.back), background);

    if (foreground == NULL)
	ws->cp.fore = Scr->IconManagerC.fore;
    else
	GetColor (Scr->Monochrome, &(ws->cp.fore), foreground);

#ifdef COLOR_BLIND_USER
    ws->cp.shadc = Scr->White;
    ws->cp.shadd = Scr->Black;
#else
    if (!Scr->BeNiceToColormap) GetShadeColors (&ws->cp);
#endif

    if (backback == NULL)
	GetColor (Scr->Monochrome, &(ws->backcp.back), "Black");
    else {
	GetColor (Scr->Monochrome, &(ws->backcp.back), backback);
	useBackgroundInfo = True;
    }

    if (backfore == NULL)
	GetColor (Scr->Monochrome, &(ws->backcp.fore), "White");
    else {
	GetColor (Scr->Monochrome, &(ws->backcp.fore), backfore);
	useBackgroundInfo = True;
    }
    if ((image = GetImage (backpix, ws->backcp)) != None) {
	ws->image = image;
	useBackgroundInfo = True;
    }
    else {
	ws->image = None;
    }
    ws->next   = NULL;
    ws->number = wsnum;
    Scr->workSpaceMgr.count++;

    if (Scr->workSpaceMgr.workSpaceList == NULL) {
	Scr->workSpaceMgr.workSpaceList = ws;
    }
    else {
	WorkSpace *wstmp = Scr->workSpaceMgr.workSpaceList;
	while (wstmp->next != NULL) { wstmp = wstmp->next; }
	wstmp->next = ws;
    }
    Scr->workSpaceManagerActive = 1;
}

static XrmOptionDescRec table [] = {
    {"-xrm",		NULL,		XrmoptionResArg, (XPointer) NULL},
};

void SetupOccupation (twm_win, occupation_hint)
TwmWindow *twm_win;
int occupation_hint; /* <== [ Matthew McNeill Feb 1997 ] == */
{
    TwmWindow		*t;
    unsigned char	*prop;
    unsigned long	nitems, bytesafter;
    Atom		actual_type;
    int			actual_format;
    int			state;
    Window		icon;
    char		**cliargv = NULL;
    int			cliargc;
    Bool		status;
    char		*str_type;
    XrmValue		value;
    char		wrkSpcList [512];
    int			len;
    WorkSpace    	*ws;
    XWindowAttributes winattrs;
    unsigned long     eventMask;
    XrmDatabase       db = NULL;
    virtualScreen     *vs;
    int gwkspc = 0; /* for GNOME - which workspace we occupy */

    if (! Scr->workSpaceManagerActive) {
	twm_win->occupation = 1;
	return;
    }
    if (twm_win->wspmgr) return;

    /*twm_win->occupation = twm_win->iswinbox ? fullOccupation : 0;*/
    twm_win->occupation = 0;

    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	if (LookInList (ws->clientlist, twm_win->full_name, &twm_win->class)) {
            twm_win->occupation |= 1 << ws->number;
	}
    }

    if (LookInList (Scr->OccupyAll, twm_win->full_name, &twm_win->class)) {
        twm_win->occupation = fullOccupation;
    }

    if (XGetCommand (dpy, twm_win->w, &cliargv, &cliargc)) {
	XrmParseCommand (&db, table, 1, "ctwm", &cliargc, cliargv);
	status = XrmGetResource (db, "ctwm.workspace", "Ctwm.Workspace", &str_type, &value);
	if ((status == True) && (value.size != 0)) {
	    strncpy (wrkSpcList, value.addr, value.size);
	    twm_win->occupation = GetMaskFromResource (twm_win, wrkSpcList);
	}
	XrmDestroyDatabase (db);
	XFreeStringList (cliargv);
    }

    if (RestartPreviousState) {
	if (XGetWindowProperty (dpy, twm_win->w, _XA_WM_OCCUPATION, 0L, 2500, False,
				XA_STRING, &actual_type, &actual_format, &nitems,
				&bytesafter, &prop) == Success) {
	    if (nitems != 0) {
		twm_win->occupation = GetMaskFromProperty (prop, nitems);
		XFree ((char *) prop);
	    }
	}
    }

    if (twm_win->iconmgr) return; /* someone tried to modify occupation of icon managers */

    if (! Scr->TransientHasOccupation) {
	if (twm_win->transient) {
	    for (t = Scr->TwmRoot.next; t != NULL; t = t->next) {
		if (twm_win->transientfor == t->w) break;
	    }
	    if (t != NULL) twm_win->occupation = t->occupation;
	}
	else
	if (twm_win->group != twm_win->w) {
	    for (t = Scr->TwmRoot.next; t != NULL; t = t->next) {
		if (t->w == twm_win->group) break;
	    }
	    if (t != NULL) twm_win->occupation = t->occupation;
	}
    }

    /*============[ Matthew McNeill Feb 1997 ]========================*
     * added in functionality of specific occupation state. The value 
     * should be a valid occupation bit-field or 0 for the default action
     */

    if (occupation_hint != 0)
      twm_win->occupation = occupation_hint;

    /*================================================================*/

    if ((twm_win->occupation & fullOccupation) == 0) {
      vs = Scr->currentvs;
      if (vs && vs->wsw->currentwspc) 
	twm_win->occupation = 1 << vs->wsw->currentwspc->number;
      else {
	twm_win->occupation = 1;
      }
    }
    twm_win->vs = NULL;
    for (vs = Scr->vScreenList; vs != NULL; vs = vs->next) {
      if (OCCUPY (twm_win, vs->wsw->currentwspc)) {
	twm_win->vs = vs;
	break;
      }
    }
    len = GetPropertyFromMask (twm_win->occupation, wrkSpcList, &gwkspc);

    if (!XGetWindowAttributes(dpy, twm_win->w, &winattrs)) return;
    eventMask = winattrs.your_event_mask;
    XSelectInput(dpy, twm_win->w, eventMask & ~PropertyChangeMask);

    XChangeProperty (dpy, twm_win->w, _XA_WM_OCCUPATION, XA_STRING, 8, 
		     PropModeReplace, (unsigned char *) wrkSpcList, len);
#ifdef GNOME
    XChangeProperty (dpy, twm_win->w, _XA_WIN_WORKSPACE, XA_CARDINAL, 32,
		     PropModeReplace, (unsigned char *)(&gwkspc), 1);
    if (XGetWindowProperty (dpy, twm_win->w, _XA_WIN_STATE, 0L, 32, False,
			    XA_CARDINAL, &actual_type, &actual_format, &nitems,
			    &bytesafter, &prop) 
	!= Success || nitems == 0) gwkspc = 0;
    else gwkspc = (int)*prop;
    if (twm_win->occupation == fullOccupation)
      gwkspc |= WIN_STATE_STICKY;
    else
      gwkspc &= ~WIN_STATE_STICKY;
    XChangeProperty (dpy, twm_win->w, _XA_WIN_STATE, XA_CARDINAL, 32, 
		     PropModeReplace, (unsigned char *)&gwkspc, 1); 
#endif /* GNOME */
    XSelectInput (dpy, twm_win->w, eventMask);

/* kludge */
    state = NormalState;
    if (!(RestartPreviousState && GetWMState (twm_win->w, &state, &icon) &&
	 (state == NormalState || state == IconicState || state == InactiveState))) {
	if (twm_win->wmhints && (twm_win->wmhints->flags & StateHint))
	    state = twm_win->wmhints->initial_state;
    }
    if (visible (twm_win)) {
	if (state == InactiveState) SetMapStateProp (twm_win,   NormalState);
    } else {
	if (state ==   NormalState) SetMapStateProp (twm_win, InactiveState);
    }
}

Bool RedirectToCaptive (window)
Window window;
{
    unsigned char	*prop;
    unsigned long	nitems, bytesafter;
    Atom		actual_type;
    int			actual_format;
    int			state;
    char		**cliargv = NULL;
    int			cliargc;
    Bool		status;
    char		*str_type;
    XrmValue		value;
    int			len;
    int			ret;
    Atom		_XA_WM_CTWMSLIST, _XA_WM_CTWM_ROOT;
    char		*atomname;
    Window		newroot;
    XWindowAttributes	wa;
    XrmDatabase         db = NULL;

    if (DontRedirect (window)) return (False);
    if (!XGetCommand (dpy, window, &cliargv, &cliargc)) return (False);
    XrmParseCommand (&db, table, 1, "ctwm", &cliargc, cliargv);
    if (db == NULL) {
        if (cliargv) XFreeStringList (cliargv);
	return False;
    }
    ret = False;
    status = XrmGetResource (db, "ctwm.redirect", "Ctwm.Redirect", &str_type, &value);
    if ((status == True) && (value.size != 0)) {
	char cctwm [64];
	strncpy (cctwm, value.addr, value.size);
	atomname = (char*) malloc (strlen ("WM_CTWM_ROOT_") + strlen (cctwm) + 1);
	sprintf (atomname, "WM_CTWM_ROOT_%s", cctwm);
	_XA_WM_CTWM_ROOT = XInternAtom (dpy, atomname, False);
	
	if (XGetWindowProperty (dpy, Scr->Root, _XA_WM_CTWM_ROOT,
		0L, 1L, False, AnyPropertyType, &actual_type, &actual_format,
		&nitems, &bytesafter, &prop) == Success) {
	    if (actual_type == XA_WINDOW && actual_format == 32 &&
			nitems == 1 /*&& bytesafter == 0*/) {
		newroot = *((Window*) prop);
		if (XGetWindowAttributes (dpy, newroot, &wa)) {
		    XReparentWindow (dpy, window, newroot, 0, 0);
		    XMapWindow (dpy, window);
		    ret = True;
		}
	    }
	}
    }
    status = XrmGetResource (db, "ctwm.rootWindow", "Ctwm.RootWindow", &str_type, &value);
    if ((status == True) && (value.size != 0)) {
	char rootw [32];
	strncpy (rootw, value.addr, value.size);
	if (sscanf (rootw, "%x", &newroot) == 1) {
	    if (XGetWindowAttributes (dpy, newroot, &wa)) {
		XReparentWindow (dpy, window, newroot, 0, 0);
		XMapWindow (dpy, window);
		ret = True;
	    }
	}
    }
    XrmDestroyDatabase (db);
    XFreeStringList (cliargv);
    return (ret);
}

static TwmWindow *occupyWin = (TwmWindow*) 0;

void Occupy (twm_win)
TwmWindow *twm_win;
{
    int		 x, y, junkX, junkY;
    unsigned int junkB, junkD;
    unsigned int width, height;
    int	       	 xoffset, yoffset;
    Window     	 junkW, w;
    unsigned int junkK;
    WorkSpace	 *ws;

    if (! Scr->workSpaceManagerActive) return;
    if (occupyWin != (TwmWindow*) 0) return;
    if (twm_win->iconmgr) return;
    if (! Scr->TransientHasOccupation) {
	if (twm_win->transient) return;
	if ((twm_win->group != (Window) 0) && (twm_win->group != twm_win->w)) return;
    }
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	Window obuttonw = Scr->workSpaceMgr.occupyWindow->obuttonw [ws->number];
	if (OCCUPY (twm_win, ws)) {
	    PaintButton (OCCUPYWINDOW, NULL, obuttonw, ws->label, ws->cp, on);
	    Scr->workSpaceMgr.occupyWindow->tmpOccupation |= (1 << ws->number);
	}
	else {
	    PaintButton (OCCUPYWINDOW, NULL, obuttonw, ws->label, ws->cp, off);
	    Scr->workSpaceMgr.occupyWindow->tmpOccupation &= ~(1 << ws->number);
	}
    }
    w = Scr->workSpaceMgr.occupyWindow->w;
    XGetGeometry  (dpy, w, &junkW, &x, &y, &width, &height, &junkB, &junkD);
    XQueryPointer (dpy, Scr->Root, &junkW, &junkW, &junkX, &junkY, &x, &y, &junkK);
    x -= (width  / 2);
    y -= (height / 2);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    xoffset = width  + 2 * Scr->BorderWidth;
    yoffset = height + 2 * Scr->BorderWidth + Scr->TitleHeight;
    if ((x + xoffset) > Scr->rootw) x = Scr->rootw - xoffset;
    if ((y + yoffset) > Scr->rooth) y = Scr->rooth - yoffset;

    Scr->workSpaceMgr.occupyWindow->twm_win->occupation = twm_win->occupation;
    if (Scr->Root != Scr->CaptiveRoot)
      XReparentWindow  (dpy, Scr->workSpaceMgr.occupyWindow->twm_win->frame, Scr->Root, x, y);
    else
      XMoveWindow  (dpy, Scr->workSpaceMgr.occupyWindow->twm_win->frame, x, y);

    SetMapStateProp (Scr->workSpaceMgr.occupyWindow->twm_win, NormalState);
    XMapWindow      (dpy, Scr->workSpaceMgr.occupyWindow->w);
    XMapRaised      (dpy, Scr->workSpaceMgr.occupyWindow->twm_win->frame);
    Scr->workSpaceMgr.occupyWindow->twm_win->mapped = TRUE;
    Scr->workSpaceMgr.occupyWindow->x = x;
    Scr->workSpaceMgr.occupyWindow->y = y;
    occupyWin = twm_win;
}

void OccupyHandleButtonEvent (event)
XEvent *event;
{
    WorkSpace	 *ws;
    OccupyWindow *occupyW;
    Window	 buttonW, obuttonw;

    if (! Scr->workSpaceManagerActive) return;
    if (occupyWin == (TwmWindow*) 0) return;

    buttonW = event->xbutton.window;
    if (buttonW == 0) return; /* icon */

    XGrabPointer (dpy, Scr->Root, True,
		  ButtonPressMask | ButtonReleaseMask,
		  GrabModeAsync, GrabModeAsync,
		  Scr->Root, None, CurrentTime);

    occupyW = Scr->workSpaceMgr.occupyWindow;
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	if (occupyW->obuttonw [ws->number] == buttonW) break;
    }
    if (ws != NULL) {
	if ((occupyW->tmpOccupation & (1 << ws->number)) == 0) {
	    PaintButton (OCCUPYWINDOW, NULL, occupyW->obuttonw [ws->number],
			 ws->label, ws->cp, on);
	    occupyW->tmpOccupation |= (1 << ws->number);
	}
	else {
	    PaintButton (OCCUPYWINDOW, NULL, occupyW->obuttonw [ws->number],
			 ws->label, ws->cp, off);
	    occupyW->tmpOccupation &= ~(1 << ws->number);
	}
    }
    else
    if (buttonW == occupyW->OK) {
	if (occupyW->tmpOccupation == 0) return;
	ChangeOccupation (occupyWin, occupyW->tmpOccupation);
	XUnmapWindow (dpy, occupyW->twm_win->frame);
	occupyW->twm_win->mapped = FALSE;
	occupyW->twm_win->occupation = 0;
	occupyWin = (TwmWindow*) 0;
	XSync (dpy, 0);
    }
    else
    if (buttonW == occupyW->cancel) {
	XUnmapWindow (dpy, occupyW->twm_win->frame);
	occupyW->twm_win->mapped = FALSE;
	occupyW->twm_win->occupation = 0;
	occupyWin = (TwmWindow*) 0;
	XSync (dpy, 0);
    }
    else
    if (buttonW == occupyW->allworkspc) {
	for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	    PaintButton (OCCUPYWINDOW, NULL, occupyW->obuttonw [ws->number],
			 ws->label, ws->cp, on);
	}
	occupyW->tmpOccupation = fullOccupation;
    }
    if (ButtonPressed == -1) XUngrabPointer (dpy, CurrentTime);
}

void OccupyAll (twm_win)
TwmWindow *twm_win;
{
    IconMgr *save;

    if (! Scr->workSpaceManagerActive) return;
    if (twm_win->iconmgr)   return;
    if ((! Scr->TransientHasOccupation) && twm_win->transient) return;
    save = Scr->iconmgr;
    Scr->iconmgr = Scr->workSpaceMgr.workSpaceList->iconmgr;
    ChangeOccupation (twm_win, fullOccupation);
    Scr->iconmgr = save;
}

void AddToWorkSpace (wname, twm_win)
char *wname;
TwmWindow *twm_win;
{
    WorkSpace *ws;
    int newoccupation;

    if (! Scr->workSpaceManagerActive) return;
    if (twm_win->iconmgr)   return;
    if ((! Scr->TransientHasOccupation) && twm_win->transient) return;
    ws = GetWorkspace (wname);
    if (!ws) return;

    if (twm_win->occupation & (1 << ws->number)) return;
    newoccupation = twm_win->occupation | (1 << ws->number);
    ChangeOccupation (twm_win, newoccupation);
}

void RemoveFromWorkSpace (wname, twm_win)
char *wname;
TwmWindow *twm_win;
{
    WorkSpace *ws;
    int newoccupation;

    if (! Scr->workSpaceManagerActive) return;
    if (twm_win->iconmgr)   return;
    if ((! Scr->TransientHasOccupation) && twm_win->transient) return;
    ws = GetWorkspace (wname);
    if (!ws) return;

    newoccupation = twm_win->occupation & ~(1 << ws->number);
    if (!newoccupation) return;
    ChangeOccupation (twm_win, newoccupation);
}

void ToggleOccupation (wname, twm_win)
char *wname;
TwmWindow *twm_win;
{
    WorkSpace *ws;
    int newoccupation;

    if (! Scr->workSpaceManagerActive) return;
    if (twm_win->iconmgr)   return;
    if ((! Scr->TransientHasOccupation) && twm_win->transient) return;
    ws = GetWorkspace (wname);
    if (!ws) return;

    if (twm_win->occupation & (1 << ws->number)) {
	newoccupation = twm_win->occupation & ~(1 << ws->number);
    } else {
	newoccupation = twm_win->occupation | (1 << ws->number);
    }
    if (!newoccupation) return;
    ChangeOccupation (twm_win, newoccupation);
}

static WorkSpace *GetWorkspace (wname)
char *wname;
{
    WorkSpace *ws;

    if (!wname) return (NULL);
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	if (strcmp (ws->label, wname) == 0) break;
    }
    if (ws == NULL) {
	for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	    if (strcmp (ws->name, wname) == 0) break;
	}
    }
    return (ws);
}

void AllocateOthersIconManagers () {
    IconMgr   *p, *ip, *oldp, *oldv;
    WorkSpace *ws;

    if (! Scr->workSpaceManagerActive) return;

    oldp = Scr->iconmgr;
    for (ws = Scr->workSpaceMgr.workSpaceList->next; ws != NULL; ws = ws->next) {
	ws->iconmgr  = (IconMgr *) malloc (sizeof (IconMgr));
	*ws->iconmgr = *oldp;
	oldv = ws->iconmgr;
	oldp->nextv = ws->iconmgr;
	oldv->nextv = NULL;

	for (ip = oldp->next; ip != NULL; ip = ip->next) {
	    p  = (IconMgr *) malloc (sizeof (IconMgr));
	    *p = *ip;
	    ip->nextv  = p;
	    p->next    = NULL;
	    p->prev    = oldv;
	    p->nextv   = NULL;
	    oldv->next = p;
	    oldv = p;
        }
	for (ip = ws->iconmgr; ip != NULL; ip = ip->next) {
	    ip->lasti = p;
        }
	oldp = ws->iconmgr;
    }
    Scr->workSpaceMgr.workSpaceList->iconmgr = Scr->iconmgr;
}

static void Vanish (vs, tmp_win)
virtualScreen *vs;
TwmWindow *tmp_win;
{
    TwmWindow	      *t;
    XWindowAttributes winattrs;
    unsigned long     eventMask;

    if (vs && tmp_win->vs && (tmp_win->vs != vs)) return;
    if (tmp_win->UnmapByMovingFarAway) {
        XMoveWindow (dpy, tmp_win->frame, Scr->rootw + 1, Scr->rooth + 1);
    } else
    if (tmp_win->mapped) {
	XGetWindowAttributes(dpy, tmp_win->w, &winattrs);
	eventMask = winattrs.your_event_mask;
	XSelectInput (dpy, tmp_win->w, eventMask & ~StructureNotifyMask);
	XUnmapWindow (dpy, tmp_win->w);
	XUnmapWindow (dpy, tmp_win->frame);
	XSelectInput (dpy, tmp_win->w, eventMask);
	if (!tmp_win->DontSetInactive) SetMapStateProp (tmp_win, InactiveState);
    } else
    if (tmp_win->icon_on && tmp_win->icon && tmp_win->icon->w) {
	XUnmapWindow (dpy, tmp_win->icon->w);
	IconDown (tmp_win);
    }
    tmp_win->oldvs = tmp_win->vs;
    tmp_win->vs = NULL;
}

static void DisplayWin (vs, tmp_win)
virtualScreen *vs;
TwmWindow *tmp_win;
{
    XWindowAttributes	winattrs;
    unsigned long	eventMask;

    if (vs && tmp_win->vs) return;
    tmp_win->vs = vs;

    if (!tmp_win->mapped) {
      if (tmp_win->isicon) {
	if (tmp_win->icon_on) {
	  if (tmp_win->icon && tmp_win->icon->w) {
	    if (vs) XReparentWindow (dpy, tmp_win->icon->w, vs->window, 0, 0);
	    IconUp (tmp_win);
	    XMapWindow (dpy, tmp_win->icon->w);
	    return;
	  }
	}
      }
      return;
    }
    if (tmp_win->UnmapByMovingFarAway) {
        if (vs) XReparentWindow (dpy, tmp_win->frame, vs->window,
				 tmp_win->frame_x, tmp_win->frame_y);
	else XMoveWindow (dpy, tmp_win->frame, tmp_win->frame_x, tmp_win->frame_y);
    } else {
	if (!tmp_win->squeezed) {
	    XGetWindowAttributes(dpy, tmp_win->w, &winattrs);
	    eventMask = winattrs.your_event_mask;
	    XSelectInput (dpy, tmp_win->w, eventMask & ~StructureNotifyMask);
	    XMapWindow   (dpy, tmp_win->w);
	    XSelectInput (dpy, tmp_win->w, eventMask);
	}
	if (vs != tmp_win->oldvs)
	  XReparentWindow (dpy, tmp_win->frame, vs->window, tmp_win->frame_x, tmp_win->frame_y);
	XMapWindow (dpy, tmp_win->frame);
	SetMapStateProp (tmp_win, NormalState);
    }
}

void ChangeOccupation (tmp_win, newoccupation)
TwmWindow *tmp_win;
int newoccupation;
{
    TwmWindow *t;
    virtualScreen *vs;
    WorkSpace *ws;
    int	      oldoccupation;
    char      namelist [512];
    int	      len;
    int	      final_x, final_y;
    XWindowAttributes winattrs;
    unsigned long     eventMask;
    int	gwkspc = 0; /* for gnome - the workspace of this window */
#ifdef GNOME
    unsigned char *prop;
    unsigned long bytesafter, numitems;
    Atom actual_type;
    int actual_format; 	
#endif /* GNOME */

    if ((newoccupation == 0) || /* in case the property has been broken by another client */
	(newoccupation == tmp_win->occupation)) {
	len = GetPropertyFromMask (tmp_win->occupation, namelist, &gwkspc);
	XGetWindowAttributes(dpy, tmp_win->w, &winattrs);
	eventMask = winattrs.your_event_mask;
	XSelectInput(dpy, tmp_win->w, eventMask & ~PropertyChangeMask);

	XChangeProperty (dpy, tmp_win->w, _XA_WM_OCCUPATION, XA_STRING, 8, 
			 PropModeReplace, (unsigned char *) namelist, len);
#ifdef GNOME
 	XChangeProperty (dpy, tmp_win->w, _XA_WIN_WORKSPACE, XA_CARDINAL, 32,
			 PropModeReplace, (unsigned char *)(&gwkspc), 1);
 	if (XGetWindowProperty(dpy, tmp_win->w, _XA_WIN_STATE, 0L, 32, False,
			      XA_CARDINAL, &actual_type, &actual_format, &numitems,
			      &bytesafter, &prop) 
	   != Success || numitems == 0) gwkspc = 0;
 	else gwkspc = (int)*prop;
 	if (tmp_win->occupation == fullOccupation)
	  gwkspc |= WIN_STATE_STICKY;
 	else
	  gwkspc &= ~WIN_STATE_STICKY;
 	XChangeProperty (dpy, tmp_win->w, _XA_WIN_STATE, XA_CARDINAL, 32, 
			 PropModeReplace, (unsigned char *)&gwkspc, 1); 
#endif /* GNOME */
	XSelectInput (dpy, tmp_win->w, eventMask);
	return;
    }
    oldoccupation = tmp_win->occupation;
    tmp_win->occupation = newoccupation & ~oldoccupation;
    AddIconManager (tmp_win);
    tmp_win->occupation = newoccupation;
    RemoveIconManager (tmp_win);

    if (tmp_win->vs && !OCCUPY (tmp_win, tmp_win->vs->wsw->currentwspc)) {
	Vanish (tmp_win->vs, tmp_win);
    }
    if (!tmp_win->vs) {
      for (vs = Scr->vScreenList; vs != NULL; vs = vs->next) {
	if (OCCUPY (tmp_win, vs->wsw->currentwspc)) {
	  DisplayWin (vs, tmp_win);
	  break;
	}
      }
    }
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	if (oldoccupation & (1 << ws->number)) {
	    if (!(newoccupation & (1 << ws->number))) {
		RemoveWindowFromRegion (tmp_win);
		if (PlaceWindowInRegion (tmp_win, &final_x, &final_y))
		    XMoveWindow (dpy, tmp_win->frame, final_x, final_y);
	    }
	    break;
	}
    }
    len = GetPropertyFromMask (newoccupation, namelist, &gwkspc);
    XGetWindowAttributes(dpy, tmp_win->w, &winattrs);
    eventMask = winattrs.your_event_mask;
    XSelectInput(dpy, tmp_win->w, eventMask & ~PropertyChangeMask);

    XChangeProperty (dpy, tmp_win->w, _XA_WM_OCCUPATION, XA_STRING, 8, 
		     PropModeReplace, (unsigned char *) namelist, len);

#ifdef GNOME
    /* Tell GNOME where this window lives */
    XChangeProperty (dpy, tmp_win->w, _XA_WIN_WORKSPACE, XA_CARDINAL, 32,
		     PropModeReplace, (unsigned char *)(&gwkspc), 1); 
    if (XGetWindowProperty (dpy, tmp_win->w, _XA_WIN_STATE, 0L, 32, False,
			    XA_CARDINAL, &actual_type, &actual_format, &numitems,
			    &bytesafter, &prop) 
	!= Success || numitems == 0) gwkspc = 0;
    else gwkspc = (int)*prop;
    if (tmp_win->occupation == fullOccupation)
      gwkspc |= WIN_STATE_STICKY;
    else
      gwkspc &= ~WIN_STATE_STICKY;
    XChangeProperty (dpy, tmp_win->w, _XA_WIN_STATE, XA_CARDINAL, 32, 
		     PropModeReplace, (unsigned char *)&gwkspc, 1); 
#endif /* GNOME */
    XSelectInput(dpy, tmp_win->w, eventMask);

    if (Scr->workSpaceMgr.noshowoccupyall) {
	if (newoccupation == fullOccupation) {
	    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
		if ((oldoccupation & (1 << ws->number)) != 0) {
		    WMapRemoveFromList (tmp_win, ws);
		}
	    }
	    goto trans;
	}
	else
	if (oldoccupation == fullOccupation) {
	    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
		if ((newoccupation & (1 << ws->number)) != 0) {
		    WMapAddToList (tmp_win, ws);
		}
	    }
	    goto trans;
	}
    }
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	if (((oldoccupation & (1 << ws->number)) != 0) &&
	    ((newoccupation & (1 << ws->number)) == 0)) {
	    WMapRemoveFromList (tmp_win, ws);
	}
	else
	if (((newoccupation & (1 << ws->number)) != 0) &&
	    ((oldoccupation & (1 << ws->number)) == 0)) {
	    WMapAddToList (tmp_win, ws);
	}
    }
trans:
    if (! Scr->TransientHasOccupation) {
	for (t = Scr->TwmRoot.next; t != NULL; t = t->next) {
	    if ((t->transient && t->transientfor == tmp_win->w) ||
		((tmp_win->group == tmp_win->w) && (tmp_win->group == t->group) &&
		(tmp_win->group != t->w))) {
		ChangeOccupation (t, newoccupation);
	    }
	}
    }
}

void WmgrRedoOccupation (win)
TwmWindow *win;
{
    WorkSpace *ws;
    int       newoccupation;

    if (LookInList (Scr->OccupyAll, win->full_name, &win->class)) {
	newoccupation = fullOccupation;
    }
    else {
	newoccupation = 0;
	for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	    if (LookInList (ws->clientlist, win->full_name, &win->class)) {
		newoccupation |= 1 << ws->number;
	    }
	}
    }
    if (newoccupation != 0) ChangeOccupation (win, newoccupation);
}

void WMgrRemoveFromCurrentWosksace (vs, win)
virtualScreen *vs;
TwmWindow *win;
{
    WorkSpace *ws;
    int	      newoccupation;

    ws = vs->wsw->currentwspc;
    if (! OCCUPY (win, ws)) return;

    newoccupation = win->occupation & ~(1 << ws->number);
    if (newoccupation == 0) return;

    ChangeOccupation (win, newoccupation);
}

void WMgrAddToCurrentWosksaceAndWarp (vs, winname)
virtualScreen *vs;
char *winname;
{
    TwmWindow *tw;
    int       newoccupation;

    for (tw = Scr->TwmRoot.next; tw != NULL; tw = tw->next) {
	if (match (winname, tw->full_name)) break;
    }
    if (!tw) {
	for (tw = Scr->TwmRoot.next; tw != NULL; tw = tw->next) {
	    if (match (winname, tw->class.res_name)) break;
	}
	if (!tw) {
	    for (tw = Scr->TwmRoot.next; tw != NULL; tw = tw->next) {
		if (match (winname, tw->class.res_class)) break;
	    }
	}
    }
    if (!tw) {
	XBell (dpy, 0);
	return;
    }
    if ((! Scr->WarpUnmapped) && (! tw->mapped)) {
	XBell (dpy, 0);
	return;
    }
    if (! OCCUPY (tw, vs->wsw->currentwspc)) {
	newoccupation = tw->occupation | (1 << vs->wsw->currentwspc->number);
	ChangeOccupation (tw, newoccupation);
    }

    if (! tw->mapped) DeIconify (tw);
    if (! Scr->NoRaiseWarp) RaiseWindow (tw);
    WarpToWindow (tw);
}

static void CreateWorkSpaceManagerWindow (vs)
virtualScreen *vs;
{
    int		  mask;
    int		  lines, vspace, hspace, count, columns;
    int		  width, height, bwidth, bheight, wwidth, wheight;
    char	  *name, *icon_name, *geometry;
    int		  i, j;
    ColorPair	  cp;
    MyFont	  font;
    WorkSpace     *ws;
    int		  x, y, strWid, wid;
    unsigned long border;
    TwmWindow	  *tmp_win;
    XSetWindowAttributes	attr;
    XWindowAttributes		wattr;
    unsigned long		attrmask;
    XSizeHints	  sizehints;
    XWMHints	  wmhints;
    int		  gravity;
#ifdef I18N
	XRectangle inc_rect;
	XRectangle logical_rect;
#endif

    name      = vs->wsw->name;
    icon_name = vs->wsw->icon_name;
    geometry  = Scr->workSpaceMgr.geometry;
    columns   = Scr->workSpaceMgr.columns;
    vspace    = vs->wsw->vspace;
    hspace    = vs->wsw->hspace;
    font      = Scr->workSpaceMgr.buttonFont;
    cp        = Scr->workSpaceMgr.cp;
    border    = vs->wsw->defBorderColor;

    count = 0;
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) count++;
    Scr->workSpaceMgr.count = count;

    if (columns == 0) {
	lines   = 2;
	columns = ((count - 1) / lines) + 1;
    }
    else {
	lines   = ((count - 1) / columns) + 1;
    }
    Scr->workSpaceMgr.lines   = lines;
    Scr->workSpaceMgr.columns = columns;

    strWid = 0;
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
#ifdef I18N
	XmbTextExtents(font.font_set, ws->label, strlen (ws->label),
		       &inc_rect, &logical_rect);
	wid = logical_rect.width;
#else
	wid = XTextWidth (font.font, ws->label, strlen (ws->label));
	if (wid > strWid) strWid = wid;
#endif
    }
    if (geometry != NULL) {
	mask = XParseGeometry (geometry, &x, &y, &width, &height);
	bwidth  = (mask & WidthValue)  ? ((width - (columns * hspace)) / columns) : strWid + 10;
	bheight = (mask & HeightValue) ? ((height - (lines  * vspace)) / lines) : 22;
	width   = columns * (bwidth  + hspace);
	height  = lines   * (bheight + vspace);

	if (! (mask & YValue)) {
            y = 0;
	    mask |= YNegative;
	}
	if (mask & XValue) {
	    if (mask & XNegative) {
		x += vs->w - width;
		gravity = (mask & YNegative) ? SouthEastGravity : NorthEastGravity;
	    }
	    else {
		gravity = (mask & YNegative) ? SouthWestGravity : NorthWestGravity;
	    }
	}
	else {
	    x = (vs->w - width) / 2;
	    gravity = (mask & YValue) ? ((mask & YNegative) ?
			SouthGravity : NorthGravity) : SouthGravity;
	}
	if (mask & YNegative) y += vs->h - height;
    }
    else {
	bwidth  = strWid + 2 * Scr->WMgrButtonShadowDepth + 6;
	bheight = 22;
	width   = columns * (bwidth  + hspace);
	height  = lines   * (bheight + vspace);
	x       = (vs->w - width) / 2;
	y       = vs->h - height;
	gravity = NorthWestGravity;
    }
    wwidth  = width  / columns;
    wheight = height / lines;

    vs->wsw->width   = width;
    vs->wsw->height  = height;
    vs->wsw->bwidth  = bwidth;
    vs->wsw->bheight = bheight;
    vs->wsw->wwidth  = wwidth;
    vs->wsw->wheight = wheight;
    vs->wsw->bswl    = (ButtonSubwindow**)
      malloc (Scr->workSpaceMgr.count * sizeof (ButtonSubwindow*));
    vs->wsw->mswl    = (MapSubwindow**)
      malloc (Scr->workSpaceMgr.count * sizeof (MapSubwindow*));

    vs->wsw->w = XCreateSimpleWindow (dpy, Scr->Root, x, y, width, height, 0,
				      Scr->Black, cp.back);
    i = 0; j = 0;
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
        Window mapsw, butsw;

	vs->wsw->bswl [ws->number] = (ButtonSubwindow*) malloc (sizeof (ButtonSubwindow));
	vs->wsw->mswl [ws->number] = (MapSubwindow*)    malloc (sizeof (MapSubwindow));

        butsw = vs->wsw->bswl [ws->number]->w =
	  XCreateSimpleWindow (dpy, vs->wsw->w,
			       i * (bwidth  + hspace) + (hspace / 2),
			       j * (bheight + vspace) + (vspace / 2),
			       bwidth, bheight, 0, Scr->Black, ws->cp.back);

	vs->wsw->mswl [ws->number]->x = i * wwidth;
	vs->wsw->mswl [ws->number]->y = j * wheight;
	mapsw = vs->wsw->mswl [ws->number]->w = XCreateSimpleWindow (dpy, vs->wsw->w,
				       i * wwidth, j * wheight,
				       wwidth - 2, wheight - 2, 1, border, ws->cp.back);

	if (vs->wsw->state == BUTTONSSTATE)
	    XMapWindow (dpy, butsw);
	else
	    XMapWindow (dpy, mapsw);

	vs->wsw->mswl [ws->number]->wl = NULL;
	if (useBackgroundInfo) {
	    if (ws->image == None) XSetWindowBackground       (dpy, mapsw, ws->backcp.back);
	    else                   XSetWindowBackgroundPixmap (dpy, mapsw, ws->image->pixmap);
	}
	else {
	    if (vs->wsw->defImage == None)
		XSetWindowBackground       (dpy, mapsw, vs->wsw->defColors.back);
	    else
		XSetWindowBackgroundPixmap (dpy, mapsw, vs->wsw->defImage->pixmap);
	}
	XClearWindow (dpy, butsw);
	i++;
	if (i == columns) {i = 0; j++;};
    }

    sizehints.flags       = USPosition | PBaseSize | PMinSize | PResizeInc | PWinGravity;
    sizehints.x           = x;
    sizehints.y           = y;
    sizehints.base_width  = columns * hspace;
    sizehints.base_height = lines   * vspace;
    sizehints.width_inc   = columns;
    sizehints.height_inc  = lines;
    sizehints.min_width   = columns  * (hspace + 2);
    sizehints.min_height  = lines    * (vspace + 2);
    sizehints.win_gravity = gravity;

    XSetStandardProperties (dpy, vs->wsw->w,
			name, icon_name, None, NULL, 0, NULL);
    XSetWMSizeHints (dpy, vs->wsw->w, &sizehints, XA_WM_NORMAL_HINTS);

    wmhints.flags	  = InputHint | StateHint;
    wmhints.input         = True;
    wmhints.initial_state = NormalState;
    XSetWMHints (dpy, vs->wsw->w, &wmhints);
    XSaveContext (dpy, vs->wsw->w, VirtScreenContext, (XPointer) vs);
    tmp_win = AddWindow (vs->wsw->w, 3, Scr->iconmgr);
    if (! tmp_win) {
	fprintf (stderr, "cannot create workspace manager window, exiting...\n");
	exit (1);
    }
    tmp_win->occupation = fullOccupation;
    tmp_win->vs = vs;

    attrmask = 0;
    attr.cursor = Scr->ButtonCursor;
    attrmask |= CWCursor;
    attr.win_gravity = gravity;
    attrmask |= CWWinGravity;
    XChangeWindowAttributes (dpy, vs->wsw->w, attrmask, &attr);

    XGetWindowAttributes (dpy, vs->wsw->w, &wattr);
    attrmask = wattr.your_event_mask | KeyPressMask | KeyReleaseMask | ExposureMask;
    XSelectInput (dpy, vs->wsw->w, attrmask);

    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
        Window buttonw = vs->wsw->bswl [ws->number]->w;
        Window mapsubw = vs->wsw->mswl [ws->number]->w;
	XSelectInput (dpy, buttonw, ButtonPressMask | ButtonReleaseMask | ExposureMask);
	XSaveContext (dpy, buttonw, TwmContext,    (XPointer) tmp_win);
	XSaveContext (dpy, buttonw, ScreenContext, (XPointer) Scr);

	XSelectInput (dpy, mapsubw, ButtonPressMask | ButtonReleaseMask);
	XSaveContext (dpy, mapsubw, TwmContext,    (XPointer) tmp_win);
	XSaveContext (dpy, mapsubw, ScreenContext, (XPointer) Scr);
    }
    SetMapStateProp (tmp_win, WithdrawnState);
    vs->wsw->twm_win = tmp_win;

    ws = Scr->workSpaceMgr.workSpaceList;
    if (useBackgroundInfo && ! Scr->DontPaintRootWindow) {
	if (ws->image == None)
	    XSetWindowBackground (dpy, Scr->Root, ws->backcp.back);
	else
	    XSetWindowBackgroundPixmap (dpy, Scr->Root, ws->image->pixmap);
	XClearWindow (dpy, Scr->Root);
    }
    PaintWorkSpaceManager (vs);
}

void WMgrHandleExposeEvent (vs, event)
virtualScreen *vs;
XEvent *event;
{
    WorkSpace *ws;
    Window buttonw;

    if (vs->wsw->state == BUTTONSSTATE) {
	for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	    buttonw = vs->wsw->bswl [ws->number]->w;
	    if (event->xexpose.window == buttonw) break;
	}
	if (ws == NULL) {
	    PaintWorkSpaceManagerBorder (vs);
	}
	else
	if (ws == vs->wsw->currentwspc)
	    PaintButton (WSPCWINDOW, vs, buttonw, ws->label, ws->cp, on);
	else
	    PaintButton (WSPCWINDOW, vs, buttonw, ws->label, ws->cp, off);
    }
    else {
	WinList	  wl;

        if (XFindContext (dpy, event->xexpose.window, MapWListContext,
		(XPointer *) &wl) == XCNOENT) return;
	if (wl && wl->twm_win && wl->twm_win->mapped) {
	    WMapRedrawName (vs, wl);
	}
    }
}

void PaintWorkSpaceManager (vs)
virtualScreen *vs;
{
    WorkSpace *ws;

    PaintWorkSpaceManagerBorder (vs);
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
        Window buttonw = vs->wsw->bswl [ws->number]->w;
	if (ws == vs->wsw->currentwspc)
	    PaintButton (WSPCWINDOW, vs, buttonw, ws->label, ws->cp, on);
	else
	    PaintButton (WSPCWINDOW, vs, buttonw, ws->label, ws->cp, off);
    }
}

static void PaintWorkSpaceManagerBorder (vs)
virtualScreen *vs;
{
    int width, height;

    width  = vs->wsw->width;
    height = vs->wsw->height;
    Draw3DBorder (vs->wsw->w, 0, 0, width, height, 2, Scr->workSpaceMgr.cp, off, True, False);
}

ColorPair occupyButtoncp;

char *ok_string		= "OK",
     *cancel_string	= "Cancel",
     *everywhere_string	= "All";

static void CreateOccupyWindow () {
    int		  width, height, lines, columns, x, y;
    int		  bwidth, bheight, owidth, oheight, hspace, vspace;
    int		  min_bwidth, min_width;
    int		  i, j;
    Window	  w, OK, cancel, allworkspc;
    char	  *name, *icon_name;
    ColorPair	  cp;
    TwmWindow	  *tmp_win;
    WorkSpace     *ws;
    XSizeHints	  sizehints;
    XWMHints      wmhints;
    MyFont	  font;
    XSetWindowAttributes attr;
    XWindowAttributes	 wattr;
    unsigned long attrmask;
    OccupyWindow  *occwin;
    virtualScreen *vs;
#ifdef I18N
    XRectangle inc_rect;
    XRectangle logical_rect;
#endif

    occwin = Scr->workSpaceMgr.occupyWindow;
    occwin->font     = Scr->IconManagerFont;
    occwin->cp       = Scr->IconManagerC;
#ifdef COLOR_BLIND_USER
    occwin->cp.shadc = Scr->White;
    occwin->cp.shadd = Scr->Black;
#else
    if (!Scr->BeNiceToColormap) GetShadeColors (&occwin->cp);
#endif
    vs        = Scr->vScreenList;
    name      = occwin->name;
    icon_name = occwin->icon_name;
    lines     = Scr->workSpaceMgr.lines;
    columns   = Scr->workSpaceMgr.columns;
    bwidth    = vs->wsw->bwidth;
    bheight   = vs->wsw->bheight;
    oheight   = vs->wsw->bheight;
    vspace    = occwin->vspace;
    hspace    = occwin->hspace;
    cp        = occwin->cp;
    height    = ((bheight + vspace) * lines) + oheight + (2 * vspace);
    font      = occwin->font;
#ifdef I18N
    XmbTextExtents(font.font_set, ok_string, strlen(ok_string),
		       &inc_rect, &logical_rect);
    min_bwidth = logical_rect.width;
    XmbTextExtents(font.font_set, cancel_string, strlen (cancel_string),
		   &inc_rect, &logical_rect);
    i = logical_rect.width;
    if (i > min_bwidth) min_bwidth = i;
    XmbTextExtents(font.font_set,everywhere_string, strlen (everywhere_string),
		   &inc_rect, &logical_rect);
    i = logical_rect.width;
    if (i > min_bwidth) min_bwidth = i;
#else
    min_bwidth = XTextWidth (font.font, ok_string, strlen (ok_string));
    i = XTextWidth (font.font, cancel_string, strlen (cancel_string));
    if (i > min_bwidth) min_bwidth = i;
    i = XTextWidth (font.font, everywhere_string, strlen (everywhere_string));
    if (i > min_bwidth) min_bwidth = i;
#endif    
    min_bwidth = (min_bwidth + hspace); /* normal width calculation */
    width = columns * (bwidth  + hspace); 
    min_width = 3 * (min_bwidth + hspace); /* width by text width */

    if (columns < 3) {
	owidth = min_bwidth + 2 * Scr->WMgrButtonShadowDepth + 2;
	if (width < min_width) width = min_width;
	bwidth = (width - columns * hspace) / columns;
    }
    else {
	owidth = min_bwidth + 2 * Scr->WMgrButtonShadowDepth + 2;
	width  = columns * (bwidth  + hspace);
    }
    occwin->lines   = lines;
    occwin->columns = columns;
    occwin->width   = width;
    occwin->height  = height;
    occwin->bwidth  = bwidth;
    occwin->bheight = bheight;
    occwin->owidth  = owidth;
    occwin->oheight = oheight;

    w = occwin->w = XCreateSimpleWindow (dpy, Scr->Root, 0, 0, width, height,
					 1, Scr->Black, cp.back);
    occwin->obuttonw = (Window*) malloc (Scr->workSpaceMgr.count * sizeof (Window));
    i = 0; j = 0;
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	occwin->obuttonw [j * columns + i] =
	  XCreateSimpleWindow(dpy, w,
			      i * (bwidth  + hspace) + (hspace / 2),
			      j * (bheight + vspace) + (vspace / 2), bwidth, bheight, 0,
			      Scr->Black, ws->cp.back);
	XMapWindow (dpy, occwin->obuttonw [j * columns + i]);
	i++;
	if (i == columns) {i = 0; j++;}
    }
    GetColor (Scr->Monochrome, &(occupyButtoncp.back), "gray50");
    occupyButtoncp.fore = Scr->White;
    if (!Scr->BeNiceToColormap) GetShadeColors (&occupyButtoncp);

    hspace = (width - 3 * owidth) / 3;
    x  = hspace / 2;
    y  = ((bheight + vspace) * lines) + ((3 * vspace) / 2);
    OK = XCreateSimpleWindow (dpy, w, x, y, owidth, oheight, 0,
			Scr->Black, occupyButtoncp.back);
    XMapWindow (dpy, OK);
    x += owidth + vspace;
    cancel = XCreateSimpleWindow (dpy, w, x, y, owidth, oheight, 0,
			Scr->Black, occupyButtoncp.back);
    XMapWindow (dpy, cancel);
    x += owidth + vspace;
    allworkspc = XCreateSimpleWindow (dpy, w, x, y, owidth, oheight, 0,
			Scr->Black, occupyButtoncp.back);
    XMapWindow (dpy, allworkspc);

    occwin->OK         = OK;
    occwin->cancel     = cancel;
    occwin->allworkspc = allworkspc;

    sizehints.flags       = PBaseSize | PMinSize | PResizeInc;
    sizehints.base_width  = columns;
    sizehints.base_height = lines;
    sizehints.width_inc   = columns;
    sizehints.height_inc  = lines;
    sizehints.min_width   = 2 * columns;
    sizehints.min_height  = 2 * lines;
    XSetStandardProperties (dpy, w, name, icon_name, None, NULL, 0, &sizehints);

    wmhints.flags	  = InputHint | StateHint;
    wmhints.input         = True;
    wmhints.initial_state = NormalState;
    XSetWMHints (dpy, w, &wmhints);
    tmp_win = AddWindow (w, FALSE, Scr->iconmgr);
    if (! tmp_win) {
	fprintf (stderr, "cannot create occupy window, exiting...\n");
	exit (1);
    }
    tmp_win->vs = None;
    tmp_win->occupation = 0;

    attrmask = 0;
    attr.cursor = Scr->ButtonCursor;
    attrmask |= CWCursor;
    XChangeWindowAttributes (dpy, w, attrmask, &attr);

    XGetWindowAttributes (dpy, w, &wattr);
    attrmask = wattr.your_event_mask | KeyPressMask | KeyReleaseMask | ExposureMask;
    XSelectInput (dpy, w, attrmask);

    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
        Window bw = occwin->obuttonw [ws->number];
	XSelectInput (dpy, bw, ButtonPressMask | ButtonReleaseMask);
	XSaveContext (dpy, bw, TwmContext,    (XPointer) tmp_win);
	XSaveContext (dpy, bw, ScreenContext, (XPointer) Scr);
    }
    XSelectInput (dpy, occwin->OK, ButtonPressMask | ButtonReleaseMask);
    XSaveContext (dpy, occwin->OK, TwmContext,    (XPointer) tmp_win);
    XSaveContext (dpy, occwin->OK, ScreenContext, (XPointer) Scr);
    XSelectInput (dpy, occwin->cancel, ButtonPressMask | ButtonReleaseMask);
    XSaveContext (dpy, occwin->cancel, TwmContext,    (XPointer) tmp_win);
    XSaveContext (dpy, occwin->cancel, ScreenContext, (XPointer) Scr);
    XSelectInput (dpy, occwin->allworkspc, ButtonPressMask | ButtonReleaseMask);
    XSaveContext (dpy, occwin->allworkspc, TwmContext,    (XPointer) tmp_win);
    XSaveContext (dpy, occwin->allworkspc, ScreenContext, (XPointer) Scr);

    SetMapStateProp (tmp_win, WithdrawnState);
    occwin->twm_win = tmp_win;
    Scr->workSpaceMgr.occupyWindow = occwin;
}

void PaintOccupyWindow () {
    WorkSpace    *ws;
    OccupyWindow *occwin;
    int 	 width, height;

    occwin = Scr->workSpaceMgr.occupyWindow;
    width  = occwin->width;
    height = occwin->height;

    Draw3DBorder (occwin->w, 0, 0, width, height, 2, occwin->cp, off, True, False);

    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
        Window bw = occwin->obuttonw [ws->number];
	if (occwin->tmpOccupation & (1 << ws->number))
	    PaintButton (OCCUPYWINDOW, NULL, bw, ws->label, ws->cp, on);
	else
	    PaintButton (OCCUPYWINDOW, NULL, bw, ws->label, ws->cp, off);
    }
    PaintButton (OCCUPYBUTTON, NULL, occwin->OK,         ok_string,         occupyButtoncp, off);
    PaintButton (OCCUPYBUTTON, NULL, occwin->cancel,     cancel_string,     occupyButtoncp, off);
    PaintButton (OCCUPYBUTTON, NULL, occwin->allworkspc, everywhere_string, occupyButtoncp, off);
}

static void PaintButton (which, vs, w, label, cp, state)
int       which;
virtualScreen *vs;
Window    w;
char      *label;
ColorPair cp;
int       state;
{
    OccupyWindow *occwin;
    int        bwidth, bheight;
    MyFont     font;
    int        strWid, strHei, hspace, vspace;
#ifdef I18N
    XFontSetExtents *font_extents;
    XFontStruct **xfonts;
    char **font_names;
    register int i;
    int descent;
    XRectangle inc_rect;
    XRectangle logical_rect;
    int fnum;
#endif
    
    occwin = Scr->workSpaceMgr.occupyWindow;
    if (which == WSPCWINDOW) {
	bwidth  = vs->wsw->bwidth;
	bheight = vs->wsw->bheight;
	font    = Scr->workSpaceMgr.buttonFont;
    }
    else
    if (which == OCCUPYWINDOW) {
	bwidth  = occwin->bwidth;
	bheight = occwin->bheight;
	font    = occwin->font;
    }
    else
    if (which == OCCUPYBUTTON) {
	bwidth  = occwin->owidth;
	bheight = occwin->oheight;
	font    = occwin->font;
    }
    else return;

#ifdef I18N
    font_extents = XExtentsOfFontSet(font.font_set);
    strHei = font_extents->max_logical_extent.height;
    vspace = ((bheight + strHei) / 2) - font.descent;
    XmbTextExtents(font.font_set, label, strlen (label), &inc_rect, &logical_rect);
    strWid = logical_rect.width;
#else
    strHei = font.font->max_bounds.ascent + font.font->max_bounds.descent;
    vspace = ((bheight + strHei) / 2) - font.font->max_bounds.descent;
    strWid = XTextWidth (font.font, label, strlen (label));
#endif
    hspace = (bwidth - strWid) / 2;
    if (hspace < (Scr->WMgrButtonShadowDepth + 1)) hspace = Scr->WMgrButtonShadowDepth + 1;
    XClearWindow (dpy, w);

    if (Scr->Monochrome == COLOR) {
	Draw3DBorder (w, 0, 0, bwidth, bheight, Scr->WMgrButtonShadowDepth,
			cp, state, True, False);

	switch (Scr->workSpaceMgr.buttonStyle) {
	    case STYLE_NORMAL :
		break;

	    case STYLE_STYLE1 :
		Draw3DBorder (w,
			Scr->WMgrButtonShadowDepth - 1,
			Scr->WMgrButtonShadowDepth - 1,
			bwidth  - 2 * Scr->WMgrButtonShadowDepth + 2,
			bheight - 2 * Scr->WMgrButtonShadowDepth + 2,
			1, cp, (state == on) ? off : on, True, False);
		break;

	    case STYLE_STYLE2 :
		Draw3DBorder (w,
			Scr->WMgrButtonShadowDepth / 2,
			Scr->WMgrButtonShadowDepth / 2,
			bwidth  - Scr->WMgrButtonShadowDepth,
			bheight - Scr->WMgrButtonShadowDepth,
			1, cp, (state == on) ? off : on, True, False);
		break;

	    case STYLE_STYLE3 :
		Draw3DBorder (w,
			1,
			1,
			bwidth  - 2,
			bheight - 2,
			1, cp, (state == on) ? off : on, True, False);
		break;
	}
#ifdef I18N
	FB (cp.fore, cp.back);
	XmbDrawString (dpy, w, font.font_set, Scr->NormalGC, hspace, vspace,
		       label, strlen (label));
#else
	FBF (cp.fore, cp.back, font.font->fid);
	XDrawString (dpy, w, Scr->NormalGC, hspace, vspace, label, strlen (label));
#endif
    }
    else {
	Draw3DBorder (w, 0, 0, bwidth, bheight, Scr->WMgrButtonShadowDepth,
		cp, state, True, False);
	if (state == on) {
#ifdef I18N
	    FB (cp.fore, cp.back);
	    XmbDrawImageString (dpy, w, font.font_set, Scr->NormalGC, hspace, vspace,
				label, strlen (label));
#else
	    FBF (cp.fore, cp.back, font.font->fid);
	    XDrawImageString (dpy, w, Scr->NormalGC, hspace, vspace, label, strlen (label));
#endif
	}
	else {
#ifdef I18N
	    FB (cp.back, cp.fore);
	    XmbDrawImageString (dpy, w, font.font_set, Scr->NormalGC, hspace, vspace,
				label, strlen (label));
#else
	    FBF (cp.back, cp.fore, font.font->fid);
	    XDrawImageString (dpy, w, Scr->NormalGC, hspace, vspace, label, strlen (label));
#endif
	}
    }
}

static unsigned int GetMaskFromResource (win, res)
TwmWindow *win;
char      *res;
{
    char      *name;
    char      wrkSpcName [64];
    WorkSpace *ws;
    int       mask, num, mode;

    mode = 0;
    if (*res == '+') {
	mode = 1;
	res++;
    }
    else
    if (*res == '-') {
	mode = 2;
	res++;
    }
    mask = 0;
    while (*res != '\0') {
	while (*res == ' ') res++;
	if (*res == '\0') break;
	name = wrkSpcName;
	while ((*res != '\0') && (*res != ' ')) {
	    if (*res == '\\') res++;
	    *name = *res;
	    name++; res++;
	}
	*name = '\0';
	if (strcmp (wrkSpcName, "all") == 0) {
	    mask = fullOccupation;
	    break;
	}
	if (strcmp (wrkSpcName, "current") == 0) {
	    virtualScreen *vs = Scr->currentvs;
	    if (vs) mask |= (1 << vs->wsw->currentwspc->number);
	    continue;
	}
	num  = 0;
	for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	    if (strcmp (wrkSpcName, ws->label) == 0) break;
	    num++;
	}
	if (ws != NULL) mask |= (1 << num);
	else {
	    twmrc_error_prefix ();
	    fprintf (stderr, "unknown workspace : %s\n", wrkSpcName);
	}
    }
    switch (mode) {
	case 0 :
	    return (mask);
	case 1 :
	    return (mask | win->occupation);
	case 2 :
	    return (win->occupation & ~mask);
    }
}

unsigned int GetMaskFromProperty (prop, len)
char *prop;
unsigned long len;
{
    char         wrkSpcName [256];
    WorkSpace    *ws;
    unsigned int mask;
    int          num, l;

    mask = 0;
    l = 0;
    while (l < len) {
	strcpy (wrkSpcName, prop);
	l    += strlen (prop) + 1;
	prop += strlen (prop) + 1;
	if (strcmp (wrkSpcName, "all") == 0) {
	    mask = fullOccupation;
	    break;
	}
	num = 0;
	for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	    if (strcmp (wrkSpcName, ws->label) == 0) break;
	    num++;
	}
	if (ws == NULL) {
	    fprintf (stderr, "unknown workspace : %s\n", wrkSpcName);
	}
	else {
	    mask |= (1 << num);
	}
    }
    return (mask);
}

static int GetPropertyFromMask (mask, prop, gwkspc)
unsigned int mask;
char *prop;
int *gwkspc;
{
    WorkSpace *ws;
    int       len;
    char      *p;

    if (mask == fullOccupation) {
	strcpy (prop, "all");
	return (3);
    }
    len = 0;
    p = prop;
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	if (mask & (1 << ws->number)) {
	    strcpy (p, ws->label);
	    p   += strlen (ws->label) + 1;
	    len += strlen (ws->label) + 1;
	    *gwkspc = ws->number;
	}
    }
    return (len);
}

void AddToClientsList (workspace, client)
char *workspace, *client;
{
    WorkSpace *ws;

    if (strcmp (workspace, "all") == 0) {
	for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	    AddToList (&ws->clientlist, client, "");
	}
	return;
    }

    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	if (strcmp (ws->label, workspace) == 0) break;
    }
    if (ws == NULL) return;
    AddToList (&ws->clientlist, client, "");    
}

void WMapToggleState (vs)
virtualScreen *vs;
{
    if (vs->wsw->state == BUTTONSSTATE) {
	WMapSetMapState (vs);
    } else {
	WMapSetButtonsState (vs);
    }
}

void WMapSetMapState (vs)
virtualScreen *vs;
{
    WorkSpace     *ws;

    if (vs->wsw->state == MAPSTATE) return;
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	XUnmapWindow (dpy, vs->wsw->bswl [ws->number]->w);
	XMapWindow   (dpy, vs->wsw->mswl [ws->number]->w);
    }
    vs->wsw->state = MAPSTATE;
    MaybeAnimate = True;
}

void WMapSetButtonsState (vs)
virtualScreen *vs;
{
    WorkSpace     *ws;

    if (vs->wsw->state == BUTTONSSTATE) return;
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	XUnmapWindow (dpy, vs->wsw->mswl [ws->number]->w);
	XMapWindow   (dpy, vs->wsw->bswl [ws->number]->w);
    }
    vs->wsw->state = BUTTONSSTATE;
}

void WMapAddWindow (win)
TwmWindow *win;
{
    virtualScreen *vs;
    WorkSpace     *ws;

    if (win->iconmgr) return;
    if (strcmp (win->name, Scr->workSpaceMgr.occupyWindow->name) == 0) return;
    for (vs = Scr->vScreenList; vs != NULL; vs = vs->next) {
      if (win == vs->wsw->twm_win) return;
    }
    if ((Scr->workSpaceMgr.noshowoccupyall) &&
	(win->occupation == fullOccupation)) return;
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	if (OCCUPY (win, ws)) WMapAddToList (win, ws);
    }
}

void WMapDestroyWindow (win)
TwmWindow *win;
{
    WorkSpace *ws;

    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	if (OCCUPY (win, ws)) WMapRemoveFromList (win, ws);
    }
    if (win == occupyWin) {
	OccupyWindow *occwin = Scr->workSpaceMgr.occupyWindow;
	XUnmapWindow (dpy, occwin->twm_win->frame);
	occwin->twm_win->mapped = FALSE;
	occwin->twm_win->occupation = 0;
	occupyWin = (TwmWindow*) 0;
    }
}

void WMapMapWindow (win)
TwmWindow *win;
{
    virtualScreen *vs;
    WorkSpace *ws;
    WinList   wl;

    for (vs = Scr->vScreenList; vs != NULL; vs = vs->next) {
      for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	for (wl = vs->wsw->mswl [ws->number]->wl; wl != NULL; wl = wl->next) {
	    if (wl->twm_win == win) {
		XMapWindow (dpy, wl->w);
		WMapRedrawName (vs, wl);
		break;
	    }
	}
      }
    }
}

void WMapSetupWindow (win, x, y, w, h)
TwmWindow *win;
int x, y, w, h;
{
    virtualScreen *vs;
    WorkSpace     *ws;
    WinList	  wl;
    float	  wf, hf;

    if (win->iconmgr) return;
    if (!win->vs) return;

    if (win->wspmgr) {
	win->vs->wsw->x = x;
	win->vs->wsw->y = y;
	if (w == -1) return;
	ResizeWorkSpaceManager (win->vs, win);
	return;
    }
    if (win == Scr->workSpaceMgr.occupyWindow->twm_win) {
	Scr->workSpaceMgr.occupyWindow->x = x;
	Scr->workSpaceMgr.occupyWindow->y = y;
	if (w == -1) return;
	ResizeOccupyWindow (win);
	return;
    }
    for (vs = Scr->vScreenList; vs != NULL; vs = vs->next) {
      WorkSpaceWindow *wsw = vs->wsw;
      wf = (float) (wsw->wwidth  - 2) / (float) vs->w;
      hf = (float) (wsw->wheight - 2) / (float) vs->h;
      for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	for (wl = wsw->mswl [ws->number]->wl; wl != NULL; wl = wl->next) {
	    if (win == wl->twm_win) {
		wl->x = (int) (x * wf);
		wl->y = (int) (y * hf);
		if (w == -1) {
		    XMoveWindow (dpy, wl->w, wl->x, wl->y);
		}
		else {
		    wl->width  = (unsigned int) ((w * wf) + 0.5);
		    wl->height = (unsigned int) ((h * hf) + 0.5);
		    if (!Scr->use3Dwmap) {
			wl->width  -= 2;
			wl->height -= 2;
		    }
		    if (wl->width  < 1) wl->width  = 1;
		    if (wl->height < 1) wl->height = 1;
		    XMoveResizeWindow (dpy, wl->w, wl->x, wl->y, wl->width, wl->height);
		}
		break;
	    }
	}
      }
    }
}

void WMapIconify (win)
TwmWindow *win;
{
    WorkSpace *ws;
    WinList    wl;

    if (!win->vs) return;
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	for (wl = win->vs->wsw->mswl [ws->number]->wl; wl != NULL; wl = wl->next) {
	    if (win == wl->twm_win) {
		XUnmapWindow (dpy, wl->w);
		break;
	    }
	}
    }
}

void WMapDeIconify (win)
TwmWindow *win;
{
    WorkSpace *ws;
    WinList    wl;

    if (!win->vs) return;
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	for (wl = win->vs->wsw->mswl [ws->number]->wl; wl != NULL; wl = wl->next) {
	    if (win == wl->twm_win) {
		if (Scr->NoRaiseDeicon)
		    XMapWindow (dpy, wl->w);
		else
		    XMapRaised (dpy, wl->w);
		WMapRedrawName (win->vs, wl);
		break;
	    }
	}
    }
}

void WMapRaiseLower (win)
TwmWindow *win;
{
    WorkSpace *ws;

    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	if (OCCUPY (win, ws)) WMapRestack (ws);
    }
}

void WMapLower (win)
TwmWindow *win;
{
    WorkSpace *ws;

    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	if (OCCUPY (win, ws)) WMapRestack (ws);
    }
}

void WMapRaise (win)
TwmWindow *win;
{
    WorkSpace *ws;

    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	if (OCCUPY (win, ws)) WMapRestack (ws);
    }
}

void WMapRestack (ws)
WorkSpace *ws;
{
    virtualScreen *vs;
    TwmWindow	*win;
    WinList	wl;
    Window	root;
    Window	parent;
    Window	*children, *smallws;
    unsigned int number;
    int		i, j;

    number = 0;
    XQueryTree (dpy, Scr->Root, &root, &parent, &children, &number);
    smallws = (Window*) malloc (number * sizeof (Window));

    for (vs = Scr->vScreenList; vs != NULL; vs = vs->next) {
      j = 0;
      for (i = number - 1; i >= 0; i--) {
	if (XFindContext (dpy, children [i], TwmContext, (XPointer *) &win) == XCNOENT) {
	    continue;
	}
	if (win->frame != children [i]) continue; /* skip icons */
	if (! OCCUPY (win, ws)) continue;
	if (tracefile) {
	    fprintf (tracefile, "WMapRestack : w = %x, win = %x\n", children [i], win);
	    fflush (tracefile);
	}
	for (wl = vs->wsw->mswl [ws->number]->wl; wl != NULL; wl = wl->next) {
	    if (win == wl->twm_win) {
		smallws [j++] = wl->w;
		break;
	    }
	}
      }
      XRestackWindows (dpy, smallws, j);
    }
    XFree ((char *) children);
    free  (smallws);
    return;
}

void WMapUpdateIconName (win)
TwmWindow *win;
{
    virtualScreen *vs;
    WorkSpace *ws;
    WinList   wl;

    for (vs = Scr->vScreenList; vs != NULL; vs = vs->next) {
      for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	for (wl = vs->wsw->mswl [ws->number]->wl; wl != NULL; wl = wl->next) {
	    if (win == wl->twm_win) {
		WMapRedrawName (vs, wl);
		break;
	    }
	}
      }
    }
}

void WMgrHandleKeyReleaseEvent (vs, event)
virtualScreen *vs;
XEvent *event;
{
    char	*keyname;
    KeySym	keysym;

    keysym  = XLookupKeysym ((XKeyEvent*) event, 0);
    if (! keysym) return;
    keyname = XKeysymToString (keysym);
    if (! keyname) return;
    if ((strcmp (keyname, "Control_R") == 0) || (strcmp (keyname, "Control_L") == 0)) {
	WMapToggleState (vs);
	return;
    }
}

void WMgrHandleKeyPressEvent (vs, event)
virtualScreen *vs;
XEvent *event;
{
    WorkSpace *ws;
    int	      len, i, lname;
    char      key [16], k;
    char      name [128];
    char      *keyname;
    KeySym    keysym;

    keysym  = XLookupKeysym   ((XKeyEvent*) event, 0);
    if (! keysym) return;
    keyname = XKeysymToString (keysym);
    if (! keyname) return;
    if ((strcmp (keyname, "Control_R") == 0) || (strcmp (keyname, "Control_L") == 0)) {
	WMapToggleState (vs);
	return;
    }
    if (vs->wsw->state == MAPSTATE) return;

    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	if (vs->wsw->bswl [ws->number]->w == event->xkey.subwindow) break;
    }
    if (ws == NULL) return;

    strcpy (name, ws->label);
    lname = strlen (name);
    len   = XLookupString (&(event->xkey), key, 16, NULL, NULL);
    for (i = 0; i < len; i++) {
        k = key [i];
	if (isprint (k)) {
	    name [lname++] = k;
	}
	else
	if ((k == 127) || (k == 8)) {
	    if (lname != 0) lname--;
	}
	else
	    break;
    }
    name [lname] = '\0';
    ws->label = realloc (ws->label, (strlen (name) + 1));
    strcpy (ws->label, name);
    if (ws == vs->wsw->currentwspc)
	PaintButton (WSPCWINDOW, vs, vs->wsw->bswl [ws->number]->w, ws->label, ws->cp,  on);
    else
	PaintButton (WSPCWINDOW, vs, vs->wsw->bswl [ws->number]->w, ws->label, ws->cp, off);
}

void WMgrHandleButtonEvent (vs, event)
virtualScreen *vs;
XEvent *event;
{
    WorkSpaceWindow	*mw;
    WorkSpace   	*ws, *oldws, *newws, *cws;
    WinList		wl;
    TwmWindow		*win;
    int			occupation;
    unsigned int	W0, H0, bw;
    int			cont;
    XEvent		ev;
    Window		w, sw, parent;
    int			X0, Y0, X1, Y1, XW, YW, XSW, YSW;
    Position		newX, newY, winX, winY;
    Window		junkW;
    unsigned int	junk;
    unsigned int	button;
    unsigned int	modifier;
    XSetWindowAttributes attrs;
    unsigned long	eventMask;
    float		wf, hf;
    Boolean		alreadyvivible, realmovemode, startincurrent;
    Time		etime;

    parent   = event->xbutton.window;
    sw       = event->xbutton.subwindow;
    mw       = vs->wsw;
    button   = event->xbutton.button;
    modifier = event->xbutton.state;
    etime    = event->xbutton.time;

    if (vs->wsw->state == BUTTONSSTATE) {
	for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	    if (vs->wsw->bswl [ws->number]->w == parent) break;
	}
	if (ws == NULL) return;
	GotoWorkSpace (vs, ws);
	return;
    }

    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	if (vs->wsw->mswl [ws->number]->w == parent) break;
    }
    if (ws == NULL) return;
    if (sw == (Window) 0) {
	GotoWorkSpace (vs, ws);
	return;
    }
    oldws = ws;

    if (XFindContext (dpy, sw, MapWListContext, (XPointer *) &wl) == XCNOENT) return;
    win = wl->twm_win;
    if ((! Scr->TransientHasOccupation) && win->transient) return;

    XTranslateCoordinates (dpy, Scr->Root, sw, event->xbutton.x_root, event->xbutton.y_root,
				&XW, &YW, &junkW);
    realmovemode = ( Scr->ReallyMoveInWorkspaceManager && !(modifier & ShiftMask)) ||
		   (!Scr->ReallyMoveInWorkspaceManager &&  (modifier & ShiftMask));
    startincurrent = (oldws == vs->wsw->currentwspc);
    if (win->OpaqueMove) {
	int sw, ss;

	sw = win->frame_width * win->frame_height;
	ss = vs->w * vs->h;
	if (sw > ((ss * Scr->OpaqueMoveThreshold) / 100))
	    Scr->OpaqueMove = FALSE;
	else
	    Scr->OpaqueMove = TRUE;
    } else {
	Scr->OpaqueMove = FALSE;
    }
    switch (button) {
	case 1 :
	    XUnmapWindow (dpy, sw);

	case 2 :
	    XGetGeometry (dpy, sw, &junkW, &X0, &Y0, &W0, &H0, &bw, &junk);
	    XTranslateCoordinates (dpy, vs->wsw->mswl [oldws->number]->w,
				mw->w, X0, Y0, &X1, &Y1, &junkW);

	    attrs.event_mask       = ExposureMask;
	    attrs.background_pixel = wl->cp.back;
	    attrs.border_pixel     = wl->cp.back;
	    w = XCreateWindow (dpy, mw->w, X1, Y1, W0, H0, bw,
				CopyFromParent,
				(unsigned int) CopyFromParent,
				(Visual *) CopyFromParent,
				CWEventMask | CWBackPixel | CWBorderPixel, &attrs);

	    XMapRaised (dpy, w);
	    WMapRedrawWindow (w, W0, H0, wl->cp, wl->twm_win->icon_name);
	    if (realmovemode && Scr->ShowWinWhenMovingInWmgr) {
		if (Scr->OpaqueMove) {
		    DisplayWin (vs, win);
		} else {
		    MoveOutline (Scr->Root,
			win->frame_x - win->frame_bw,
			win->frame_y - win->frame_bw,
			win->frame_width  + 2 * win->frame_bw,
			win->frame_height + 2 * win->frame_bw,
			win->frame_bw,
			win->title_height + win->frame_bw3D);
		}
	    }
	    break;

	case 3 :
	    occupation = win->occupation & ~(1 << oldws->number);
	    ChangeOccupation (win, occupation);
	    return;
	default :
	    return;
    }

    wf = (float) (mw->wwidth  - 1) / (float) vs->w;
    hf = (float) (mw->wheight - 1) / (float) vs->h;
    XGrabPointer (dpy, mw->w, False, ButtonPressMask | ButtonMotionMask | ButtonReleaseMask,
		  GrabModeAsync, GrabModeAsync, mw->w, Scr->MoveCursor, CurrentTime);

    alreadyvivible = False;
    cont = TRUE;
    while (cont) {
        MapSubwindow *msw;
	XMaskEvent (dpy, ButtonPressMask | ButtonMotionMask |
			 ButtonReleaseMask | ExposureMask, &ev);
	switch (ev.xany.type) {
	    case ButtonPress :
	    case ButtonRelease :
		if (ev.xbutton.button != button) break;
		cont = FALSE;
		newX = ev.xbutton.x;
		newY = ev.xbutton.y;

	    case MotionNotify :
		if (cont) {
		    newX = ev.xmotion.x;
		    newY = ev.xmotion.y;
		}
		if (realmovemode) {
		    for (cws = Scr->workSpaceMgr.workSpaceList;
			 cws != NULL;
			 cws = cws->next) {
		        msw = vs->wsw->mswl [cws->number];
			if ((newX >= msw->x) && (newX <  msw->x + mw->wwidth) &&
			    (newY >= msw->y) && (newY <  msw->y + mw->wheight)) {
			    break;
			}
		    }
		    if (!cws) break;
		    winX = newX - XW;
		    winY = newY - YW;
		    msw = vs->wsw->mswl [cws->number];
		    XTranslateCoordinates (dpy, mw->w, msw->w,
					winX, winY, &XSW, &YSW, &junkW);
		    winX = (int) (XSW / wf);
		    winY = (int) (YSW / hf);
		    if (Scr->DontMoveOff) {
			int w = win->frame_width;
			int h = win->frame_height;

			if ((winX < Scr->BorderLeft) && ((Scr->MoveOffResistance < 0) ||
                             (winX > Scr->BorderLeft - Scr->MoveOffResistance))) {
			    winX = Scr->BorderLeft;
			    newX = msw->x + XW + Scr->BorderLeft * mw->wwidth / vs->w;
			}
			if (((winX + w) > vs->x - Scr->BorderRight) &&
			    ((Scr->MoveOffResistance < 0) ||
			     ((winX + w) < vs->w - Scr->BorderRight + Scr->MoveOffResistance))) {
			    winX = vs->w - Scr->BorderRight - w;
			    newX = msw->x + mw->wwidth *
                                (1 - Scr->BorderRight / (double) vs->w) - wl->width + XW - 2;
			}
			if ((winY < Scr->BorderTop) && ((Scr->MoveOffResistance < 0) ||
                             (winY > Scr->BorderTop - Scr->MoveOffResistance))) {
			    winY = Scr->BorderTop;
			    newY = msw->y + YW + Scr->BorderTop * mw->height / vs->h;
			}
			if (((winY + h) > vs->h - Scr->BorderBottom) &&
			    ((Scr->MoveOffResistance < 0) ||
                             ((winY + h) < vs->h - Scr->BorderBottom + Scr->MoveOffResistance))) {
			    winY = vs->h - Scr->BorderBottom - h;
			    newY = msw->y + mw->wheight *
                                (1 - Scr->BorderBottom / (double) vs->h) - wl->height + YW - 2;
			}
		    }
		    WMapSetupWindow (win, winX, winY, -1, -1);
		    if (Scr->ShowWinWhenMovingInWmgr) goto movewin;
		    if (cws == vs->wsw->currentwspc) {
			if (alreadyvivible) goto movewin;
			if (Scr->OpaqueMove) {
			    XMoveWindow (dpy, win->frame, winX, winY);
			    DisplayWin (vs, win);
			} else {
			    MoveOutline (Scr->Root,
				winX - win->frame_bw, winY - win->frame_bw,
				win->frame_width  + 2 * win->frame_bw,
				win->frame_height + 2 * win->frame_bw,
				win->frame_bw,
				win->title_height + win->frame_bw3D);
			}
			alreadyvivible = True;
			goto move;
		    }
		    if (!alreadyvivible) goto move;
		    if (!OCCUPY (win, vs->wsw->currentwspc) ||
			(startincurrent && (button == 1))) {
			if (Scr->OpaqueMove) {
			    Vanish (vs, win);
			    XMoveWindow (dpy, win->frame, winX, winY);
			} else {
			    MoveOutline (Scr->Root, 0, 0, 0, 0, 0, 0);
			}
			alreadyvivible = False;
			goto move;
		    }
movewin:	    if (Scr->OpaqueMove) {
			XMoveWindow (dpy, win->frame, winX, winY);
		    } else {
			MoveOutline (Scr->Root,
				winX - win->frame_bw, winY - win->frame_bw,
				win->frame_width  + 2 * win->frame_bw,
				win->frame_height + 2 * win->frame_bw,
				win->frame_bw,
				win->title_height + win->frame_bw3D);
		    }
		}
move:		XMoveWindow (dpy, w, newX - XW, newY - YW);
		break;
	    case Expose :
		if (ev.xexpose.window == w) {
		    WMapRedrawWindow (w, W0, H0, wl->cp, wl->twm_win->icon_name);
		    break;
		}
		Event = ev;
		DispatchEvent ();
		break;
	}
    }
    if (realmovemode) {
	if (Scr->ShowWinWhenMovingInWmgr || alreadyvivible) {
	    if (Scr->OpaqueMove && !OCCUPY (win, vs->wsw->currentwspc)) {
		Vanish (vs, win);
	    }
	    if (!Scr->OpaqueMove) {
		MoveOutline (Scr->Root, 0, 0, 0, 0, 0, 0);
		WMapRedrawName (vs, wl);
	    }
	}
	SetupWindow (win, winX, winY, win->frame_width, win->frame_height, -1);
    }
    ev.xbutton.subwindow = (Window) 0;
    ev.xbutton.window = parent;
    XPutBackEvent   (dpy, &ev);
    XUngrabPointer  (dpy, CurrentTime);

    if ((ev.xbutton.time - etime) < 250) {
	KeyCode control_L_code, control_R_code;
	KeySym  control_L_sym,  control_R_sym;
	char keys [32];

	XMapWindow (dpy, sw);
	XDestroyWindow (dpy, w);
	GotoWorkSpace (vs, ws);
	if (!Scr->DontWarpCursorInWMap) WarpToWindow (win);
	control_L_sym  = XStringToKeysym  ("Control_L");
	control_R_sym  = XStringToKeysym  ("Control_R");
	control_L_code = XKeysymToKeycode (dpy, control_L_sym);
	control_R_code = XKeysymToKeycode (dpy, control_R_sym);
	XQueryKeymap (dpy, keys);
	if ((keys [control_L_code / 8] & ((char) 0x80 >> (control_L_code % 8))) ||
	     keys [control_R_code / 8] & ((char) 0x80 >> (control_R_code % 8))) {
	    WMapToggleState (vs);
	}
	return;
    }

    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
        MapSubwindow *msw = vs->wsw->mswl [ws->number];
	if ((newX >= msw->x) && (newX < msw->x + mw->wwidth) &&
	    (newY >= msw->y) && (newY < msw->y + mw->wheight)) {
	    break;
	}
    }
    newws = ws;
    switch (button) {
	case 1 :
	    if ((newws == NULL) || (newws == oldws) || OCCUPY (wl->twm_win, newws)) {
		XMapWindow (dpy, sw);
		break;
	    }
	    occupation = (win->occupation | (1 << newws->number)) & ~(1 << oldws->number);
	    ChangeOccupation (win, occupation);
	    if (newws == vs->wsw->currentwspc) {
		RaiseWindow (win);
		WMapRaise (win);
	    }
	    else WMapRestack (newws);
	    break;

	case 2 :
	    if ((newws == NULL) || (newws == oldws) ||
		OCCUPY (wl->twm_win, newws)) break;

	    occupation = win->occupation | (1 << newws->number);
	    ChangeOccupation (win, occupation);
	    if (newws == vs->wsw->currentwspc) {
		RaiseWindow (win);
		WMapRaise (win);
	    }
	    else WMapRestack (newws);
	    break;

	default :
	    return;
    }
    XDestroyWindow (dpy, w);
}

void InvertColorPair (cp)
ColorPair *cp;
{
    Pixel save;

    save = cp->fore;
    cp->fore = cp->back;
    cp->back = save;
    save = cp->shadc;
    cp->shadc = cp->shadd;
    cp->shadd = save;
}

void WMapRedrawName (vs, wl)
virtualScreen *vs;
WinList   wl;
{
    int       w = wl->width;
    int       h = wl->height;
    ColorPair cp;
    char      *label;

    label  = wl->twm_win->icon_name;
    cp     = wl->cp;

    if (Scr->ReverseCurrentWorkspace && wl->wlist == vs->wsw->currentwspc) {
	InvertColorPair (&cp);
    }
    WMapRedrawWindow (wl->w, w, h, cp, label);
}

static void WMapRedrawWindow (window, width, height, cp, label)
Window	window;
int	width, height;
ColorPair cp;
char 	*label;
{
    int		x, y, strhei, strwid;
    MyFont	font;
#ifdef I18N
    XFontSetExtents *font_extents;
    XRectangle inc_rect;
    XRectangle logical_rect;
    XFontStruct **xfonts;
    char **font_names;
    register int i;
    int descent;
    int fnum;
#endif

    XClearWindow (dpy, window);
    font = Scr->workSpaceMgr.windowFont;

#ifdef I18N
    font_extents = XExtentsOfFontSet(font.font_set);
    strhei = font_extents->max_logical_extent.height;

    if (strhei > height) return;

    XmbTextExtents(font.font_set, label, strlen (label),
		   &inc_rect, &logical_rect);
    strwid = logical_rect.width;
    x = (width  - strwid) / 2;
    if (x < 1) x = 1;

    fnum = XFontsOfFontSet(font.font_set, &xfonts, &font_names);
    for( i = 0, descent = 0; i < fnum; i++){
	/* xf = xfonts[i]; */
	descent = ((descent < (xfonts[i]->max_bounds.descent)) ? (xfonts[i]->max_bounds.descent): descent);
    }
    
    y = ((height + strhei) / 2) - descent;

    if (Scr->use3Dwmap) {
	Draw3DBorder (window, 0, 0, width, height, 1, cp, off, True, False);
	FB(cp.fore, cp.back);
    } else {
	FB (cp.back, cp.fore);
	XFillRectangle (dpy, window, Scr->NormalGC, 0, 0, width, height);
	FB (cp.fore, cp.back);
    }
    if (Scr->Monochrome != COLOR) {
	XmbDrawImageString (dpy, window, font.font_set, Scr->NormalGC, x, y, label, strlen (label));
    } else {
	XmbDrawString (dpy, window, font.font_set,Scr->NormalGC, x, y, label, strlen (label));
    }

#else
    strhei = font.font->max_bounds.ascent + font.font->max_bounds.descent;
    if (strhei > height) return;

    strwid = XTextWidth (font.font, label, strlen (label));
    x = (width  - strwid) / 2;
    if (x < 1) x = 1;
    y = ((height + strhei) / 2) - font.font->max_bounds.descent;

    if (Scr->use3Dwmap) {
	Draw3DBorder (window, 0, 0, width, height, 1, cp, off, True, False);
	FBF(cp.fore, cp.back, font.font->fid);
    } else {
	FBF (cp.back, cp.fore, font.font->fid);
	XFillRectangle (dpy, window, Scr->NormalGC, 0, 0, width, height);
	FBF (cp.fore, cp.back, font.font->fid);
    }
    if (Scr->Monochrome != COLOR) {
	XDrawImageString (dpy, window, Scr->NormalGC, x, y, label, strlen (label));
    } else {
	XDrawString (dpy, window, Scr->NormalGC, x, y, label, strlen (label));
    }
#endif    
}

static void WMapAddToList (win, ws)
TwmWindow *win;
WorkSpace *ws;
{
    virtualScreen *vs;
    WinList wl;
    float   wf, hf;
    ColorPair cp;
    XSetWindowAttributes attr;
    unsigned long attrmask;
    unsigned int bw;

    cp.back = win->title.back;
    cp.fore = win->title.fore;
    if (Scr->workSpaceMgr.windowcpgiven) {
	cp.back = Scr->workSpaceMgr.windowcp.back;
	GetColorFromList (Scr->workSpaceMgr.windowBackgroundL,
			win->full_name, &win->class, &cp.back);
	cp.fore = Scr->workSpaceMgr.windowcp.fore;
	GetColorFromList (Scr->workSpaceMgr.windowForegroundL,
		      win->full_name, &win->class, &cp.fore);
    }
    if (Scr->use3Dwmap && !Scr->BeNiceToColormap) {
	GetShadeColors (&cp);
    }
    for (vs = Scr->vScreenList; vs != NULL; vs = vs->next) {
      wf = (float) (vs->wsw->wwidth  - 2) / (float) vs->w;
      hf = (float) (vs->wsw->wheight - 2) / (float) vs->h;
      wl = (WinList) malloc (sizeof (struct winList));
      wl->wlist  = ws;
      wl->x      = (int) (win->frame_x * wf);
      wl->y      = (int) (win->frame_y * hf);
      wl->width  = (unsigned int) ((win->frame_width  * wf) + 0.5);
      wl->height = (unsigned int) ((win->frame_height * hf) + 0.5);
      bw = 0;
      if (!Scr->use3Dwmap) {
	bw = 1;
	wl->width  -= 2;
	wl->height -= 2;
      }
      if (wl->width  < 1) wl->width  = 1;
      if (wl->height < 1) wl->height = 1;
      wl->w = XCreateSimpleWindow (dpy, vs->wsw->mswl [ws->number]->w, wl->x, wl->y,
				   wl->width, wl->height, bw, Scr->Black, cp.back);
      attrmask = 0;
      if (Scr->BackingStore) {
	attr.backing_store = WhenMapped;
	attrmask |= CWBackingStore;
      }
      attr.cursor = handCursor;
      attrmask |= CWCursor;
      XChangeWindowAttributes (dpy, wl->w, attrmask, &attr);
      XSelectInput (dpy, wl->w, ExposureMask);
      XSaveContext (dpy, wl->w, TwmContext, (XPointer) vs->wsw->twm_win);
      XSaveContext (dpy, wl->w, ScreenContext, (XPointer) Scr);
      XSaveContext (dpy, wl->w, MapWListContext, (XPointer) wl);
      wl->twm_win = win;
      wl->cp      = cp;
      wl->next    = vs->wsw->mswl [ws->number]->wl;
      vs->wsw->mswl [ws->number]->wl = wl;
      if (win->mapped) XMapWindow (dpy, wl->w);
    }
}

static void WMapRemoveFromList (win, ws)
TwmWindow *win;
WorkSpace *ws;
{
    virtualScreen *vs;
    WinList wl, wl1;

    for (vs = Scr->vScreenList; vs != NULL; vs = vs->next) {
      wl = vs->wsw->mswl [ws->number]->wl;
      if (wl == NULL) continue;
      if (win == wl->twm_win) {
	vs->wsw->mswl [ws->number]->wl = wl->next;
	XDeleteContext (dpy, wl->w, TwmContext);
	XDeleteContext (dpy, wl->w, ScreenContext);
	XDeleteContext (dpy, wl->w, MapWListContext);
	XDestroyWindow (dpy, wl->w);
	free (wl);
	continue;
      }
      wl1 = wl;
      wl  = wl->next;
      while (wl != NULL) {
	if (win == wl->twm_win) {
	  wl1->next = wl->next;
	  XDeleteContext (dpy, wl->w, TwmContext);
	  XDeleteContext (dpy, wl->w, ScreenContext);
	  XDeleteContext (dpy, wl->w, MapWListContext);
	  XDestroyWindow (dpy, wl->w);
	  free (wl);
	  break;
	}
	wl1 = wl;
	wl  = wl->next;
      }
    }
}

static void ResizeWorkSpaceManager (vs, win)
virtualScreen *vs;
TwmWindow *win;
{
    int           bwidth, bheight;
    int		  wwidth, wheight;
    int           hspace, vspace;
    int           lines, columns;
    int		  neww, newh;
    WorkSpace     *ws;
    TwmWindow	  *tmp_win;
    WinList	  wl;
    int           i, j;
    static int    oldw = 0;
    static int    oldh = 0;
    float	  wf, hf;

    neww = win->attr.width;
    newh = win->attr.height;
    if ((neww == oldw) && (newh == oldh)) return;
    oldw = neww; oldh = newh;

    hspace  = vs->wsw->hspace;
    vspace  = vs->wsw->vspace;
    lines   = Scr->workSpaceMgr.lines;
    columns = Scr->workSpaceMgr.columns;
    bwidth  = (neww - (columns * hspace)) / columns;
    bheight = (newh - (lines   * vspace)) / lines;
    wwidth  = neww / columns;
    wheight = newh / lines;
    wf = (float) (wwidth  - 2) / (float) vs->w;
    hf = (float) (wheight - 2) / (float) vs->h;

    i = 0;
    j = 0;
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
        MapSubwindow *msw = vs->wsw->mswl [ws->number];
	XMoveResizeWindow (dpy, vs->wsw->bswl [ws->number]->w,
				i * (bwidth  + hspace) + (hspace / 2),
				j * (bheight + vspace) + (vspace / 2),
				bwidth, bheight);
	msw->x = i * wwidth;
	msw->y = j * wheight;
	XMoveResizeWindow (dpy, msw->w, msw->x, msw->y, wwidth - 2, wheight - 2);
	for (wl = msw->wl; wl != NULL; wl = wl->next) {
	    tmp_win    = wl->twm_win;
	    wl->x      = (int) (tmp_win->frame_x * wf);
	    wl->y      = (int) (tmp_win->frame_y * hf);
	    wl->width  = (unsigned int) ((tmp_win->frame_width  * wf) + 0.5);
	    wl->height = (unsigned int) ((tmp_win->frame_height * hf) + 0.5);
	    XMoveResizeWindow (dpy, wl->w, wl->x, wl->y, wl->width, wl->height);
	}
	i++;
	if (i == columns) {i = 0; j++;};
    }
    vs->wsw->bwidth    = bwidth;
    vs->wsw->bheight   = bheight;
    vs->wsw->width     = neww;
    vs->wsw->height    = newh;
    vs->wsw->wwidth	= wwidth;
    vs->wsw->wheight	= wheight;
    PaintWorkSpaceManager (vs);
}

static void ResizeOccupyWindow (win)
TwmWindow *win;
{
    int        bwidth, bheight, owidth, oheight;
    int        hspace, vspace;
    int        lines, columns;
    int        neww, newh;
    WorkSpace  *ws;
    int        i, j, x, y;
    static int oldw = 0;
    static int oldh = 0;
    OccupyWindow *occwin = Scr->workSpaceMgr.occupyWindow;

    neww = win->attr.width;
    newh = win->attr.height;
    if ((neww == oldw) && (newh == oldh)) return;
    oldw = neww; oldh = newh;

    hspace  = occwin->hspace;
    vspace  = occwin->vspace;
    lines   = Scr->workSpaceMgr.lines;
    columns = Scr->workSpaceMgr.columns;
    bwidth  = (neww -  columns    * hspace) / columns;
    bheight = (newh - (lines + 2) * vspace) / (lines + 1);
    owidth  = occwin->owidth;
    oheight = bheight;

    i = 0;
    j = 0;
    for (ws = Scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	XMoveResizeWindow (dpy, occwin->w,
			   i * (bwidth  + hspace) + (hspace / 2),
			   j * (bheight + vspace) + (vspace / 2),
			   bwidth, bheight);
	i++;
	if (i == columns) {i = 0; j++;}
    }
    hspace = (neww - 3 * owidth) / 3;
    x = hspace / 2;
    y = ((bheight + vspace) * lines) + ((3 * vspace) / 2);
    XMoveResizeWindow (dpy, occwin->OK, x, y, owidth, oheight);
    x += owidth + hspace;
    XMoveResizeWindow (dpy, occwin->cancel, x, y, owidth, oheight);
    x += owidth + hspace;
    XMoveResizeWindow (dpy, occwin->allworkspc, x, y, owidth, oheight);
    occwin->width   = neww;
    occwin->height  = newh;
    occwin->bwidth  = bwidth;
    occwin->bheight = bheight;
    occwin->owidth  = owidth;
    occwin->oheight = oheight;
    PaintOccupyWindow ();
}

void WMapCreateCurrentBackGround (border, background, foreground, pixmap)
char *border, *background, *foreground, *pixmap;
{
    virtualScreen *vs = Scr->vScreenList;
    Image *image;

    while (vs != NULL) {
      vs->wsw->curBorderColor = Scr->Black;
      vs->wsw->curColors.back = Scr->White;
      vs->wsw->curColors.fore = Scr->Black;
      vs->wsw->curImage       = None;

      if (border == NULL) return;
      GetColor (Scr->Monochrome, &(vs->wsw->curBorderColor), border);
      if (background == NULL) return;
      vs->wsw->curPaint = True;
      GetColor (Scr->Monochrome, &(vs->wsw->curColors.back), background);
      if (foreground == NULL) return;
      GetColor (Scr->Monochrome, &(vs->wsw->curColors.fore), foreground);

      if (pixmap == NULL) return;
      if ((image = GetImage (pixmap, vs->wsw->curColors)) == None) {
	fprintf (stderr, "Can't find pixmap %s\n", pixmap);
	return;
      }
      vs->wsw->curImage = image;
      vs = vs->next;
    }
}

void WMapCreateDefaultBackGround (border, background, foreground, pixmap)
char *border, *background, *foreground, *pixmap;
{
    virtualScreen *vs = Scr->vScreenList;
    Image *image;

    while (vs != NULL) {
      vs->wsw->defBorderColor = Scr->Black;
      vs->wsw->defColors.back = Scr->White;
      vs->wsw->defColors.fore = Scr->Black;
      vs->wsw->defImage       = None;

      if (border == NULL) return;
      GetColor (Scr->Monochrome, &(vs->wsw->defBorderColor), border);
      if (background == NULL) return;
      GetColor (Scr->Monochrome, &(vs->wsw->defColors.back), background);
      if (foreground == NULL) return;
      GetColor (Scr->Monochrome, &(vs->wsw->defColors.fore), foreground);

      if (pixmap == NULL) return;
      if ((image = GetImage (pixmap, vs->wsw->defColors)) == None) return;
      vs->wsw->defImage = image;
      vs = vs->next;
    }
}

Bool AnimateRoot () {
    virtualScreen *vs;
    ScreenInfo *scr;
    int	       scrnum;
    Image      *image;
    WorkSpace  *ws;
    Bool       maybeanimate;

    maybeanimate = False;
    for (scrnum = 0; scrnum < NumScreens; scrnum++) {
	if ((scr = ScreenList [scrnum]) == NULL) continue;
	if (! scr->workSpaceManagerActive) continue;

	for (vs = scr->vScreenList; vs != NULL; vs = vs->next) {
	  if (! vs->wsw->currentwspc) continue;
	  image = vs->wsw->currentwspc->image;
	  if ((image == None) || (image->next == None)) continue;
	  if (scr->DontPaintRootWindow) continue;

	  XSetWindowBackgroundPixmap (dpy, vs->window, image->pixmap);
	  XClearWindow (dpy, scr->Root);
	  vs->wsw->currentwspc->image = image->next;
	  maybeanimate = True;
	}
    }
    for (scrnum = 0; scrnum < NumScreens; scrnum++) {
	if ((scr = ScreenList [scrnum]) == NULL) continue;

	for (vs = scr->vScreenList; vs != NULL; vs = vs->next) {
	  if (vs->wsw->state == BUTTONSSTATE) continue;
	  for (ws = scr->workSpaceMgr.workSpaceList; ws != NULL; ws = ws->next) {
	    image = ws->image;

	    if ((image == None) || (image->next == None)) continue;
	    if (ws == vs->wsw->currentwspc) continue;
	    XSetWindowBackgroundPixmap (dpy, vs->wsw->mswl [ws->number]->w, image->pixmap);
	    XClearWindow (dpy, vs->wsw->mswl [ws->number]->w);
	    ws->image = image->next;
	    maybeanimate = True;
	  }
	}
    }
    return (maybeanimate);
}

static char **GetCaptivesList (scrnum)
int scrnum;
{
    unsigned char	*prop, *p;
    unsigned long	bytesafter;
    unsigned long	len;
    Atom		actual_type;
    int			actual_format;
    char		**ret;
    int			count;
    int			i, l;
    Window		root;

    _XA_WM_CTWMSLIST = XInternAtom (dpy, "WM_CTWMSLIST", True);
    if (_XA_WM_CTWMSLIST == None) return ((char**)0);

    root = RootWindow (dpy, scrnum);
    if (XGetWindowProperty (dpy, root, _XA_WM_CTWMSLIST, 0L, 512,
			False, XA_STRING, &actual_type, &actual_format, &len,
			&bytesafter, &prop) != Success) return ((char**) 0);
    if (len == 0) return ((char**) 0);

    count = 0;
    p = prop;
    l = 0;
    while (l < len) {
	l += strlen ((char*)p) + 1;
	p += strlen ((char*)p) + 1;
	count++;
    }
    ret = (char**) malloc ((count + 1) * sizeof (char*));

    p = prop;
    l = 0;
    i = 0;
    while (l < len) {
	ret [i++] = (char*) strdup ((char*) p);
	l += strlen ((char*)p) + 1;
	p += strlen ((char*)p) + 1;
    }
    ret [i] = (char*) 0;
    return (ret);
}

static void SetCaptivesList (scrnum, clist)
int scrnum;
char **clist;
{
    unsigned char	*prop, *p;
    unsigned long	bytesafter;
    unsigned long	len;
    Atom		actual_type;
    int			actual_format;
    char		**cl;
    char		*s, *slist;
    int			i;
    Window		root = RootWindow (dpy, scrnum);

    _XA_WM_CTWMSLIST = XInternAtom (dpy, "WM_CTWMSLIST", False);
    cl  = clist; len = 0;
    while (*cl) { len += strlen (*cl++) + 1; }
    if (len == 0) {
	XDeleteProperty (dpy, root, _XA_WM_CTWMSLIST);
	return;
    }
    slist = (char*) malloc (len * sizeof (char));
    cl = clist; s  = slist;
    while (*cl) {
	strcpy (s, *cl);
	s += strlen (*cl);
	*s++ = '\0';
	cl++;
    }
    XChangeProperty (dpy, root, _XA_WM_CTWMSLIST, XA_STRING, 8, 
		     PropModeReplace, (unsigned char *) slist, len);
}

static void freeCaptiveList (clist)
char **clist;
{
    while (clist && *clist) { free (*clist++); }
}

void AddToCaptiveList ()
{
    int		i, count;
    char	**clist, **cl, **newclist;
    int		busy [32];
    Atom	_XA_WM_CTWM_ROOT;
    char	*atomname;
    int		scrnum = Scr->screen;
    Window	croot  = Scr->Root;
    Window	root   = RootWindow (dpy, scrnum);

    for (i = 0; i < 32; i++) { busy [i] = 0; }
    clist = GetCaptivesList (scrnum);
    cl = clist;
    count = 0;
    while (cl && *cl) {
	count++;
	if (!captivename) {
	    if (!strncmp (*cl, "ctwm-", 5)) {
		int r, n;
		r = sscanf (*cl, "ctwm-%d", &n);
		cl++;
		if (r != 1) continue;
		if ((n < 0) || (n > 31)) continue;
		busy [n] = 1;
	    } else cl++;
	    continue;
	}
	if (!strcmp (*cl, captivename)) {
	    fprintf (stderr, "A captive ctwm with name %s is already running\n", captivename);
	    exit (1);
	}
	cl++;
    }
    if (!captivename) {
	for (i = 0; i < 32; i++) {
	    if (!busy [i]) break;
	}
	if (i == 32) { /* no one can tell we didn't try hard */
	    fprintf (stderr, "Cannot find a suitable name for captive ctwm\n");
	    exit (1);
	}
	captivename = (char*) malloc (8);
	sprintf (captivename, "ctwm-%d", i);
    }
    newclist = (char**) malloc ((count + 2) * sizeof (char*));
    for (i = 0; i < count; i++) {
	newclist [i] = (char*) strdup (clist [i]);
    }
    newclist [count] = (char*) strdup (captivename);
    newclist [count + 1] = (char*) 0;
    SetCaptivesList (scrnum, newclist);
    freeCaptiveList (clist);
    freeCaptiveList (newclist);
    free (clist); free (newclist);

    root = RootWindow (dpy, scrnum);
    atomname = (char*) malloc (strlen ("WM_CTWM_ROOT_") + strlen (captivename) +1);
    sprintf (atomname, "WM_CTWM_ROOT_%s", captivename);
    _XA_WM_CTWM_ROOT = XInternAtom (dpy, atomname, False);
    XChangeProperty (dpy, root, _XA_WM_CTWM_ROOT, XA_WINDOW, 32, 
		     PropModeReplace, (unsigned char *) &croot, 4);
}

void RemoveFromCaptiveList () {
    int	 i, count;
    char **clist, **cl, **newclist;
    Atom _XA_WM_CTWM_ROOT;
    char *atomname;
    int scrnum = Scr->screen;
    Window root = RootWindow (dpy, scrnum);

    if (!captivename) return;
    clist = GetCaptivesList (scrnum);
    cl = clist; count = 0;
    while (*cl) {
      count++;
      cl++;
    }
    newclist = (char**) malloc (count * sizeof (char*));
    cl = clist; count = 0;
    while (*cl) {
	if (!strcmp (*cl, captivename)) { cl++; continue; }
	newclist [count++] = *cl;
	cl++;
    }
    newclist [count] = (char*) 0;
    SetCaptivesList (scrnum, newclist);
    freeCaptiveList (clist);
    free (clist); free (newclist);

    atomname = (char*) malloc (strlen ("WM_CTWM_ROOT_") + strlen (captivename) +1);
    sprintf (atomname, "WM_CTWM_ROOT_%s", captivename);
    _XA_WM_CTWM_ROOT = XInternAtom (dpy, atomname, True);
    if (_XA_WM_CTWM_ROOT == None) return;
    XDeleteProperty (dpy, root, _XA_WM_CTWM_ROOT);
}

void SetPropsIfCaptiveCtwm (win)
TwmWindow *win;
{
    Window	window = win->w;
    Window	frame  = win->frame;
    Atom	_XA_WM_CTWM_ROOT;

    if (!CaptiveCtwmRootWindow (window)) return;
    _XA_WM_CTWM_ROOT = XInternAtom (dpy, "WM_CTWM_ROOT", True);
    if (_XA_WM_CTWM_ROOT == None) return;

    XChangeProperty (dpy, frame, _XA_WM_CTWM_ROOT, XA_WINDOW, 32, 
		     PropModeReplace, (unsigned char *) &window, 4);
}

Window CaptiveCtwmRootWindow (window)
Window window;
{
    unsigned char	*prop;
    unsigned long	bytesafter;
    unsigned long	len;
    Atom		actual_type;
    int			actual_format;
    int			scrnum = Scr->screen;
    Window		root = RootWindow (dpy, scrnum);
    Window		ret;
    Atom		_XA_WM_CTWM_ROOT;

    _XA_WM_CTWM_ROOT = XInternAtom (dpy, "WM_CTWM_ROOT", True);
    if (_XA_WM_CTWM_ROOT == None) return ((Window)0);

    if (XGetWindowProperty (dpy, window, _XA_WM_CTWM_ROOT, 0L, 1L,
			False, XA_WINDOW, &actual_type, &actual_format, &len,
			&bytesafter, &prop) != Success) return ((Window)0);
    if (len == 0) return ((Window)0);
    ret = *((Window*) prop);
    return (ret);
}

CaptiveCTWM GetCaptiveCTWMUnderPointer () {
    int		scrnum = Scr->screen;
    Window	root;
    Window	child, croot;
    CaptiveCTWM	cctwm;

    root = RootWindow (dpy, scrnum);
    while (1) {
	XQueryPointer (dpy, root, &JunkRoot, &child,
			&JunkX, &JunkY, &JunkX, &JunkY, &JunkMask);
	if (child && (croot = CaptiveCtwmRootWindow (child))) {
	    root = croot;
	    continue;
	}
	cctwm.root = root;
	XFetchName (dpy, root, &cctwm.name);
	if (!cctwm.name) cctwm.name = (char*) strdup ("Root");
	return (cctwm);
    }
}

void SetNoRedirect (window)
Window window;
{
    Atom	_XA_WM_NOREDIRECT;

    _XA_WM_NOREDIRECT = XInternAtom (dpy, "WM_NOREDIRECT", False);
    if (_XA_WM_NOREDIRECT == None) return;

    XChangeProperty (dpy, window, _XA_WM_NOREDIRECT, XA_STRING, 8, 
		     PropModeReplace, (unsigned char *) "Yes", 4);
}

static Bool DontRedirect (window)
Window window;
{
    unsigned char	*prop;
    unsigned long	bytesafter;
    unsigned long	len;
    Atom		actual_type;
    int			actual_format;
    int			scrnum = Scr->screen;
    Window		root = RootWindow (dpy, scrnum);
    Atom		_XA_WM_NOREDIRECT;

    _XA_WM_NOREDIRECT = XInternAtom (dpy, "WM_NOREDIRECT", True);
    if (_XA_WM_NOREDIRECT == None) return (False);

    if (XGetWindowProperty (dpy, window, _XA_WM_NOREDIRECT, 0L, 1L,
			False, XA_STRING, &actual_type, &actual_format, &len,
			&bytesafter, &prop) != Success) return (False);
    if (len == 0) return (False);
    return (True);
}

Bool visible (tmp_win)
TwmWindow *tmp_win;
{
  return (tmp_win->vs != NULL);
}

#ifdef BUGGY_HP700_SERVER
static void fakeRaiseLower (display, window)
Display *display;
Window   window;
{
    Window          root;
    Window          parent;
    Window          grandparent;
    Window         *children;
    unsigned int    number;
    XWindowChanges  changes;

    number = 0;
    XQueryTree (display, window, &root, &parent, &children, &number);
    XFree ((char *) children);
    XQueryTree (display, parent, &root, &grandparent, &children, &number);

    changes.stack_mode = (children [number-1] == window) ? Below : Above;
    XFree ((char *) children);
    XConfigureWindow (display, window, CWStackMode, &changes);
}
#endif


