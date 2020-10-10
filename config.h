/* See LICENSE file for copyright and license details. */

#include <X11/X.h>
/* Buttons */
#include <X11/XF86keysym.h>
#define Button6 6
#define Button7 7
#define Button8 8
#define Button9 9

/* window placements */
#define CENTER_HINTS -1,-1,-1,-1
#define CENTER       10,10,80,80
#define HINTS        -2,-2,-2,-2

#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* autostart */
static const char *const autostart[] = {
	"/bin/sh", "-c", "(sleep 30 && whatsapp-nativefier-dark) &", NULL,
	"/bin/sh", "-c", "(sleep 30 && thunderbird) &", NULL,
	"/bin/sh", "-c", "ps -e | grep dwmblocks || while true ; do dwmblocks ; done", NULL,
	//"/bin/sh", "-c", "$DWM_BIN/date_to_statusbar &", NULL,
	NULL /* terminate */
};

/* appearance */
static const unsigned int borderpx  = 1;        /* border pixel of windows */
static const unsigned int snap      = 32;       /* snap pixel */

static const unsigned int systraypinning = 2;   /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayspacing = 2;   /* systray spacing */
static const int systraypinningfailfirst = 0;   /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/
static const int showsystray        = 1;     /* 0 means no systray */

static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */

enum showtab_modes { showtab_never, showtab_auto, showtab_nmodes, showtab_always};
static const int showtab			= showtab_auto;        /* Default tab bar show mode */
static const int toptab				= True;  

static const char *fonts[]          = { "Roboto Mono:style=Medium:antialias=true:size=10;1",
																				"monospace:size=10",
																				"Noto Color Emoji:pixelsize=12:antialias=true:autohint=true"};
static const char dmenufont[]       = "monospace:size=10";
static const char *colors[][3]      = {
	/*               fg         bg         border   */
	[SchemeNorm] = { "#ffffff", "#1d2021", "#222222" },
	[SchemeSel]  = { "#ffffff", "#0058a0", "#D3D3D3"  },
	[SchemeNormTab] = { "#ffffff", "#2d3031", "#222222" },
	[SchemeSelTab]  = { "#ffffff", "#1068b0", "#D3D3D3"  },
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* tag ~0 means sticky */
	/* class                      | instance | title         |tags  | isfloating | float x,y,w,h % | isterminal | noswallow | sp_id | monitor */
	{ "Gimp"                      , NULL     , NULL          , 0    , 1          , HINTS          , 0          , 0          ,     0 , -1 },
	{ "Thunderbird"               , NULL     , NULL          , 1<<8 , 0          , HINTS          , 0          , 0          ,     0 , 0  },
	{ "whatsapp-nativefier-d52542", NULL     , NULL          , ~0   , 1          , CENTER         , 0          , 0          ,     0 , -1 },
	{ "firefox"                   , "Toolkit", NULL          , ~0   , 1          , HINTS          , 0          , 0          ,     0 , -1 },
	{ "st-256color"               , NULL     , NULL          , 0    , 0          , CENTER_HINTS   , 1          , 1          ,     0 , -1 },
	{ "st-256color-c"             , NULL     , NULL          , ~0   , 1          , 10,10,80,80    , 1          , 1          ,     0 , -1 },
	{ "st-256color-ur"            , NULL     , NULL          , ~0   , 1          , 60,2,40,20     , 1          , 1          ,     0 , -1 },
	{ "SpeedCrunch"               , NULL     , NULL          , ~0   , 1          , CENTER         , 1          , 1          ,     0 , -1 },
	{ NULL                        , NULL     , "Event Tester", 0    , 0          , HINTS          , 0          , 1          ,     0 , -1 },
	{ "st-256color-docked"        , NULL     , NULL          , ~0   , 1          , 0,80,100,20    , 1          , 1          ,     1 , -1 },
	{ "st-256color-notes"         , NULL     , NULL          , ~0   , 1          , CENTER         , 1          , 1          ,     2 , -1 },
};


static const char* scratchpads_cmd[] = {
	"$TERMINAL -c st-256color-docked",
  "$TERMINAL -c st-256color-notes -e $SHELL -c \"$EDITOR \"$HOME\"/.local/share/notes\""
};

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 0;    /* 1 means respect size hints in tiled resizals */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[Tall]",     tile },    /* first entry is default */
	{ "[Tabs]",     monocle },
	{ "[Grid]",     gaplessgrid },
	{ "[Floating]", NULL },    /* no layout function means floating behavior */
};

/* key definitions */
#define MODKEY Mod1Mask
#define ALTMODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "rofi", "-show", "drun", NULL };

static Key keys[] = {
	/* modifier                     key        function        argument */
	// Mine
	{ MODKEY|ShiftMask,             XK_q,      spawn,          SHCMD("$DMENU_BIN/dmenu_leave") },
	{ MODKEY|ShiftMask,             XK_r,      spawn,          SHCMD("lxsu /bin/sh -c \"cd $HOME/git/dwm/ && make all && make install\" && pkill dwm") },
	{ MODKEY,                       XK_r,      spawn,          SHCMD("rofi -show drun") },
	{ MODKEY,                       XK_Tab,    view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_Tab,    spawn,          SHCMD("rofi -show window") },
	{ 0,                            XK_Print,  spawn,          SHCMD("screengrab") },

	{ MODKEY,                       XK_q,      killclient,      {0} },
	{ MODKEY,                       XK_t,      togglefloating,  {0} },
	{ MODKEY,                       XK_s,      togglesticky,    {0} },

	{ MODKEY,                       XK_h,      horizontalfocus, {.i = -1 } },
	{ MODKEY,                       XK_l,      horizontalfocus, {.i = +1 } },
	{ MODKEY|ShiftMask|ControlMask, XK_h,      setmfact,        {.f = -0.05} },
	{ MODKEY|ShiftMask|ControlMask, XK_l,      setmfact,        {.f = +0.05} },
	{ MODKEY|ShiftMask,             XK_j,      pushdown,        {0} },
	{ MODKEY|ShiftMask,             XK_k,      pushup,          {0} },
	{ MODKEY,                       XK_a,      view,            {.ui = ~0 } },
	{ MODKEY,                       XK_grave,  focusmon,        {.i = 1 } },
	{ MODKEY|ShiftMask,             XK_grave,  tagmon,          {.i = 1 } },
	{ MODKEY|ShiftMask,             XK_h,      zoom,            {-1} },
	{ MODKEY|ShiftMask,             XK_l,      zoom,            {+1} },
	{ MODKEY|ControlMask,           XK_j,      incview,         {.i =  1} },
	{ MODKEY|ControlMask,           XK_k,      incview,         {.i = -1} },
	//{ MODKEY|ShiftMask,           XK_h,      incnmaster,      {.i = +1 } },
	//{ MODKEY|ShiftMask,           XK_l,      incnmaster,      {.i = -1 } },

	{ MODKEY|ShiftMask,             XK_g,      setlayout,        {.v = &layouts[2]} },
	{ MODKEY|ShiftMask,             XK_d,      setlayout,        {.v = &layouts[0]} },
	{ MODKEY,                       XK_f,      setlayout,        {.v = &layouts[1]} },
	{ MODKEY|ShiftMask,             XK_f,      togglefullscreen, {0} },
	{ 0,                            XK_F11,    togglefullscreen, {0} },

	//{ MODKEY|ShiftMask,             XK_Return, spawn,          SHCMD("$TERMINAL -c st-256color-docked") },
  { MODKEY|ShiftMask,             XK_Return, toggle_sp,       {.i = 1} },
  { MODKEY,                       XK_n,      toggle_sp,       {.i = 2} },

	{ MODKEY,                       XK_Return, spawn,          SHCMD("$TERMINAL") },
	{ MODKEY,                       XK_e,      spawn,          SHCMD("$TERMINAL -e ranger-standalone") },
	{ MODKEY,                       XK_w,      spawn,          SHCMD("$BROWSER") },
	{ MODKEY|ShiftMask,             XK_w,      spawn,          SHCMD("whatsapp-nativefier-dark") },

	{ 0, XF86XK_Calculator,                    spawn,          SHCMD("pkill -TERM speedcrunch || speedcrunch") },
	{ 0, XF86XK_MonBrightnessUp,               spawn,          SHCMD("light -A 15") },
	{ 0, XF86XK_MonBrightnessDown,             spawn,          SHCMD("light -U 15") },
	{ 0, XF86XK_AudioMute,                     spawn,          SHCMD("amixer sset Master toggle && pkill -RTMIN+2 dwmblocks") },
	{ 0, XF86XK_AudioRaiseVolume,              spawn,          SHCMD("amixer sset Master 5%+ && pkill -RTMIN+2 dwmblocks") },
	{ 0, XF86XK_AudioLowerVolume,              spawn,          SHCMD("amixer sset Master 5%- && pkill -RTMIN+2 dwmblocks") },


	// Default
	{ MODKEY,                       XK_b,      togglebar,      {0} },
	{ MODKEY,                       XK_j,      focusstack,     {.i = +1 } },
	{ MODKEY,                       XK_k,      focusstack,     {.i = -1 } },
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)
	/* { MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	 * { MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	 * { MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	 * { MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } }, */
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkStatusText,        0,              Button1,        sigdwmblocks,   {.i = 1} },
	{ ClkStatusText,        0,              Button2,        sigdwmblocks,   {.i = 2} },
	{ ClkStatusText,        0,              Button3,        sigdwmblocks,   {.i = 3} },
	{ ClkStatusText,        0,              Button4,        sigdwmblocks,   {.i = 4} },
	{ ClkStatusText,        0,              Button5,        sigdwmblocks,   {.i = 5} },
	{ ClkStatusText,        ShiftMask,      Button1,        sigdwmblocks,   {.i = 6} },
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button2,        view,           {.ui = ~0 }  },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[1]} },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {.i=0} },
	{ ClkClientWin,         MODKEY,         Button2,        killclient,     {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkClientWin,         0,              Button8,        movemouse,      {.i=1} },
	{ ClkClientWin,         0,              Button9,        killclient,     {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            0,              Button8,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
	{ ClkTabBar,            0,              Button1,        focuswin,       {0} },
	{ ClkTabBar,            0,              Button2,        killwin ,       {0} },
	{ ClkTagBar,            0,              Button4,        incview,        {.i =  1} },
	{ ClkTagBar,            0,              Button5,        incview,        {.i = -1} },
	{ ClkWinTitle,          0,              Button5,        focusstack,     {.i =  1} },
  { ClkWinTitle,          0,              Button4,        focusstack,     {.i = -1} },
	{ ClkWinTitle,          0,              Button9,        killclient,     {0} },
	{ ClkWinTitle,         0,              Button8,        tagmon,         {.i=1} },
};

