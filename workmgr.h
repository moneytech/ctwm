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
#ifndef _WORKMGR_
#define _WORKMGR_

#define MAXWORKSPACE 32
#define MAPSTATE      0
#define BUTTONSSTATE  1

#define STYLE_NORMAL	0
#define STYLE_STYLE1	1
#define STYLE_STYLE2	2
#define STYLE_STYLE3	3

void InitWorkSpaceManager ();
int ConfigureWorkSpaceManager ();
void CreateWorkSpaceManager ();
void GotoWorkSpaceByName ();
void GotoWorkSpaceByNumber ();
void GotoPrevWorkSpace ();
void GotoNextWorkSpace ();
void GotoRightWorkSpace ();
void GotoLeftWorkSpace ();
void GotoUpWorkSpace ();
void GotoDownWorkSpace ();
void GotoWorkSpace ();
void AddWorkSpace ();
void SetupOccupation ();
void Occupy ();
void OccupyHandleButtonEvent ();
void OccupyAll ();
void AddToWorkSpace ();
void RemoveFromWorkSpace ();
void ToggleOccupation ();
void AllocateOthersIconManagers ();
void ChangeOccupation ();
void WmgrRedoOccupation ();
void WMgrRemoveFromCurrentWosksace ();
void WMgrAddToCurrentWosksaceAndWarp ();
void WMgrHandleExposeEvent ();
void PaintWorkSpaceManager ();
void PaintOccupyWindow ();
unsigned int GetMaskFromProperty ();
void AddToClientsList ();
void WMapToggleState ();
void WMapSetMapState ();
void WMapSetButtonsState ();
void WMapAddWindow ();
void WMapDestroyWindow ();
void WMapMapWindow ();
void WMapSetupWindow ();
void WMapIconify ();
void WMapDeIconify ();
void WMapRaiseLower ();
void WMapLower ();
void WMapRaise ();
void WMapRestack ();
void WMapUpdateIconName ();
void WMgrHandleKeyReleaseEvent ();
void WMgrHandleKeyPressEvent ();
void WMgrHandleButtonEvent ();
void WMapRedrawName ();
void WMapCreateCurrentBackGround ();
void WMapCreateDefaultBackGround ();
char *GetCurrentWorkSpaceName ();
Bool AnimateRoot ();
void AddToCaptiveList ();
void RemoveFromCaptiveList ();
Bool RedirectToCaptive ();
void SetPropsIfCaptiveCtwm ();
Window CaptiveCtwmRootWindow ();

#ifdef GNOME
/* 6/19/1999 nhd for GNOME compliance */
void InitGnome ();
void GnomeAddClientWindow ();
void GnomeDeleteClientWindow ();
#endif /* GNOME */

typedef struct winList {
    struct WorkSpace	*wlist;
    Window		w;
    int			x, y;
    int			width, height;
    TwmWindow		*twm_win;
    ColorPair		cp;
    MyFont		font;
    struct winList	*next;
} *WinList;

typedef struct WorkSpaceMgr {
    struct WorkSpace	   *workSpaceList;
    struct WorkSpaceWindow *workSpaceWindowList;
    struct OccupyWindow    *occupyWindow;
    MyFont     	    buttonFont;
    MyFont     	    windowFont;
    ColorPair  	    windowcp;
    Bool       	    windowcpgiven;
    ColorPair       cp;
    int		    count;
    char	    *geometry;
    int		    lines, columns;
    int		    noshowoccupyall;
    int             initialstate;
    short      	    buttonStyle;
    name_list	    *windowBackgroundL;
    name_list	    *windowForegroundL;
} WorkSpaceMgr;

typedef struct WorkSpace {
  int	              number;
  char	              *name;
  char	              *label;
  Image	              *image;
  name_list           *clientlist;
  IconMgr             *iconmgr;
  ColorPair           cp;
  ColorPair           backcp;
  struct WindowRegion *FirstWindowRegion;
  struct WorkSpace *next;
} WorkSpace;

typedef struct MapSubwindow {
  Window  w;
  Window  blanket;
  int     x, y;
  WinList wl;
} MapSubwindow;

typedef struct ButtonSubwindow {
  Window w;
} ButtonSubwindow;

typedef struct WorkSpaceWindow {
  virtualScreen   *vs;
  Window	  w;
  TwmWindow       *twm_win;
  MapSubwindow    **mswl;
  ButtonSubwindow **bswl;
  WorkSpace       *currentwspc;

  int	       	x, y;
  char		*name;
  char		*icon_name;
  int	       	state;

  int	       	width, height;
  int	       	bwidth, bheight;
  int	       	hspace, vspace;
  int	       	wwidth, wheight;
  
  ColorPair    	curColors;
  Image		*curImage;
  unsigned long	curBorderColor;
  Bool		curPaint;

  ColorPair    	defColors;
  Image		*defImage;
  unsigned long	defBorderColor;
  struct WorkSpaceWindow *next;
} WorkSpaceWindow;

typedef struct OccupyWindow  {
  Window       	w;
  TwmWindow    	*twm_win;
  char		*geometry;
  Window       	*obuttonw;
  Window       	OK, cancel, allworkspc;
  int	       	x, y;
  int	       	width, height;
  char		*name;
  char		*icon_name;
  int	       	lines, columns;
  int	       	hspace, vspace;
  int	       	bwidth, bheight;
  int	       	owidth, oheight;
  ColorPair    	cp;
  MyFont       	font;
  int	       	tmpOccupation;
} OccupyWindow;

typedef struct CaptiveCTWM {
  Window	root;
  String	name;
} CaptiveCTWM;

CaptiveCTWM GetCaptiveCTWMUnderPointer ();
void SetNoRedirect ();

#endif