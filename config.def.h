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

#define UL            0,  0
#define UM            1,  0
#define UR            2,  0
#define ML            0,  1
#define MM            1,  1
#define MR            2,  1
#define LL            0,  2
#define LM            1,  2
#define LR            2,  2
#define NN           -1, -1

#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* autostart */
static const char *const autostart[] = {
  "/bin/sh", "-c", "pgrep -x WhatsApp || (sleep 30 && whatsapp-nativefier) &", NULL,
  /* "/bin/sh", "-c", "(sleep 30 && thunderbird) &", NULL, */
  "/bin/sh", "-c", "pgrep -x dwmblocks || while true ; do dwmblocks ; done", NULL,
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

static const char *fonts[]          = { "Roboto Mono:size=10",
  "monospace:size=10",
  "Noto Color Emoji:pixelsize=12:antialias=true:autohint=true"};
static const char *colors[][4]      = {
  /*                  fg         bg         border   */
  [SchemeNorm]    = { "#ffffff", "#1d2021", "#222222"},
  [SchemeSel]     = { "#ffffff", "#0058a0", "#D3D3D3"},
  [SchemeNormTab] = { "#ffffff", "#2d3031", "#222222"},
  [SchemeSelTab]  = { "#ffffff", "#1068b0", "#D3D3D3"},
  [SchemeSelAlt]  = { "#ffffff", "#3d4041", "#D3D3D3"},
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
  /* xprop(1):
   *	WM_CLASS(STRING) = instance, class
   *	WM_NAME(STRING) = title
   */
  /* tag ~0 means sticky */
  /* class                      | instance | title         |tags  | isfloating | float x,y,w,h % | float zone | isterminal | noswallow | sp_id | monitor */
  { "Thunderbird"               , NULL     , NULL          , 1<<8 , 0          , HINTS           , NN         , 0          , 0         ,     0 ,  0      },
  { "tridactyl_editor"          , NULL     , NULL          , ~0   , 1          , CENTER          , NN         , 0          , 0         ,     0 , -1      },
  { "firefox"                   , "Toolkit", NULL          , ~0   , 1          , HINTS           , LR         , 0          , 0         ,     0 , -1      },
  { "st-256color"               , NULL     , NULL          , 0    , 0          , CENTER_HINTS    , NN         , 1          , 1         ,     0 , -1      },
  { "st-256color-c"             , NULL     , NULL          , 0    , 1          , 10,10,80,80     , NN         , 1          , 1         ,     0 , -1      },
  { "st-256color-ur"            , NULL     , NULL          , ~0   , 1          , 60,2,40,20      , UR         , 1          , 1         ,     0 , -1      },
  { "SpeedCrunch"               , NULL     , NULL          , ~0   , 1          , CENTER          , NN         , 1          , 1         ,     0 , -1      },
  { NULL                        , NULL     , "Event Tester", 0    , 0          , HINTS           , NN         , 0          , 1         ,     0 , -1      },
  { "Yad"                       , NULL     , NULL          , 0    , 1          , HINTS           , NN         , 0          , 0         ,     0 , -1      },
  { "Dragon-drag-and-drop"      , NULL     , NULL          , ~0   , 1          , CENTER_HINTS    , NN         , 0          , 0         ,     0 , -1      },
  { "st-256color-docked"        , NULL     , NULL          , ~0   , 1          , CENTER          , MM         , 1          , 1         ,     1 , -1      },
  { "st-256color-notes"         , NULL     , NULL          , ~0   , 1          , CENTER          , NN         , 0          , 0         ,     2 , -1      },
  { "whatsapp-nativefier-d40211", NULL     , NULL          , ~0   , 1          , CENTER          , NN         , 0          , 0         ,     3 , -1      },
  { "st-256color-mail"          , NULL     , NULL          , ~0   , 1          , CENTER          , NN         , 0          , 0         ,     4 , -1      },
  { "st-256color-calendar"      , NULL     , NULL          , ~0   , 1          , CENTER          , NN         , 0          , 0         ,     5 , -1      },
};


static const char* scratchpads_cmd[] = {
  "$TERMINAL -c st-256color-docked -e tmux",
  "$TERMINAL -c st-256color-notes -T vimwiki -e $SHELL -c \"cd \"$HOME\"/.local/share/vimwiki && $EDITOR index.wiki\"",
  "whatsapp-nativefier",
  "$TERMINAL -c st-256color-mail -T neomutt -e $SHELL -c neomutt",
  "$TERMINAL -c st-256color-calendar -T calcurse -e $SHELL -c calcurse"
};

typedef int dims[4];

static dims floatzones[][3] = {
  /*      |  L         | M             |  R           */
  /* U */	{ {0,0,30,30} , {0,0,100,20} , {70,0,30,30} },
  /* M */	{ {0,0,25,100}, {10,10,80,80}, {75,0,25,100}},
  /* L */	{ {0,70,30,30}, {0,80,100,20}, {70,70,30,30}},
};

static const int floatingdims[] = { CENTER };

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 0;    /* 1 means respect size hints in tiled resizals */

static const Layout layouts[] = {
  /* symbol     arrange function */
  { "[Tall]",     tile },    /* first entry is default */
  { "[Tabs]",     monocle },
  { "[Grid]",     grid },
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

static Key keys[] = {
  /* modifier                     key        function        argument */
  // Mine
  { MODKEY,                       XK_F5,     spawn,          SHCMD("lxsu /bin/sh -c \"cd $HOME/.build/dwm/ && make all && make install\" && pkill dwm") },
  { MODKEY|ShiftMask,             XK_r,      spawn,          SHCMD("lxsu /bin/sh -c \"cd $HOME/.build/dwm/ && make all && make install\" && pkill dwm") },
  { MODKEY,                       XK_p,      spawn,          SHCMD("dmenu-pass") },
  { MODKEY|ShiftMask,             XK_p,      spawn,          SHCMD("dmenu-passotp") },
  { MODKEY|ShiftMask|ControlMask, XK_p,      spawn,          SHCMD("passmenu --type") },
  { MODKEY,                       XK_i,      spawn,          SHCMD("$DMENU_BIN/pulse_conf source ; pkill -RTMIN+2 dwmblocks") },
  { MODKEY,                       XK_o,      spawn,          SHCMD("$DMENU_BIN/pulse_conf sink ; pkill -RTMIN+2 dwmblocks") },
  { MODKEY,                       XK_u,      spawn,          SHCMD("$DMENU_BIN/dmenu_unicode") },
  { MODKEY|ShiftMask,             XK_q,      spawn,          SHCMD("$DMENU_BIN/dmenu_leave") },
  { MODKEY,                       XK_r,      spawn,          SHCMD("rofi -show drun") },
  { MODKEY|ShiftMask,             XK_m,      spawn,          SHCMD("$DMENU_BIN/monitor_layout") },
  { MODKEY,                       XK_Tab,    view,           {.ui = ~0 } },
  { MODKEY|ShiftMask,             XK_Tab,    spawn,          SHCMD("rofi -show window") },
  { 0,                            XK_Print,  spawn,          SHCMD("flameshot gui") },

  { MODKEY,                       XK_q,      killclient,      {0} },
  { MODKEY,                       XK_a,      togglefloating,  {.i = 1} },
  { MODKEY,                       XK_s,      togglesticky,    {0} },

  { MODKEY,                       XK_semicolon, togglefocustype, {0} },

  { MODKEY,                       XK_comma,  swappos,      {.i = -1 } },
  { MODKEY,                       XK_period, swappos,      {.i = +1 } },

  { MODKEY,                       XK_h,      horizontalfocus, {.i = -1 } },
  { MODKEY,                       XK_l,      horizontalfocus, {.i = +1 } },
  { MODKEY|ShiftMask,             XK_h,      movehorizontal,  {.i = -1} },
  { MODKEY|ShiftMask,             XK_l,      movehorizontal,  {.i = +1} },
  { MODKEY|ShiftMask|ControlMask, XK_h,      resizehorizontal,{.i = -1} },
  { MODKEY|ShiftMask|ControlMask, XK_l,      resizehorizontal,{.i = +1} },
  /* { MODKEY|ShiftMask|ControlMask, XK_h,      setmfact,        {.f = -0.05} }, */
  /* { MODKEY|ShiftMask|ControlMask, XK_l,      setmfact,        {.f = +0.05} }, */
  { MODKEY,                       XK_j,      focusstack,      {.i = +1 } },
  { MODKEY,                       XK_k,      focusstack,      {.i = -1 } },
  { MODKEY|ShiftMask,             XK_j,      movevertical,    {.i =  1 } },
  { MODKEY|ShiftMask,             XK_k,      movevertical,    {.i = -1 } },
  { MODKEY|ShiftMask|ControlMask, XK_j,      resizevertical,  {.i =  1 } },
  { MODKEY|ShiftMask|ControlMask, XK_k,      resizevertical,  {.i = -1 } },
  { MODKEY|ControlMask,           XK_h,      reorganizetags,  {.i =  1 } },
  { MODKEY|ControlMask,           XK_l,      reorganizetags,  {.i = -1 } },
  { MODKEY,                       XK_Escape, focusmon,        {.i =  1 } },
  { MODKEY|ShiftMask,             XK_Escape, tagmon,          {.i =  1 } },
  { MODKEY|ControlMask,           XK_j,      incview,         {.i =  1 } },
  { MODKEY|ControlMask,           XK_k,      incview,         {.i = -1 } },
  { MODKEY,                       XK_grave,  freeview,        {.i =  1 } },
  { MODKEY|ShiftMask,             XK_grave,  freetag,         {.i =  1 } },

  { MODKEY,                       XK_0,      lastfreeview,    {.i =  1 } },
  { MODKEY|ShiftMask,             XK_0,      lastfreetag,     {.i =  1 } },
  //{ MODKEY|ShiftMask,           XK_h,      incnmaster,      {.i = +1 } },
  //{ MODKEY|ShiftMask,           XK_l,      incnmaster,      {.i = -1 } },

  { MODKEY,                       XK_d,      setlayout,        {.v = &layouts[0]} },
  { MODKEY|ShiftMask,             XK_f,      setlayout,        {.v = &layouts[1]} },
  { MODKEY,                       XK_g,      setlayout,        {.v = &layouts[2]} },
  { MODKEY,                       XK_t,      setlayout,        {.v = &layouts[1]} },
  /* { MODKEY,                       XK_Tab,    setlayout,        {.v = &layouts[2]} }, */
  { MODKEY,                       XK_f,      togglefullscreen,     {0} },
  { MODKEY|ShiftMask|ControlMask, XK_f,      togglefullfullscreen, {0} },
  { 0,                            XK_F11,    togglefullscreen,     {0} },

  //{ MODKEY|ShiftMask,             XK_Return, spawn,          SHCMD("$TERMINAL -c st-256color-docked") },
  { MODKEY,                       XK_space,  togglesp,       {.i = 1} },
  { MODKEY,                       XK_n,      togglesp,       {.i = 2} },
  { MODKEY|ShiftMask,             XK_w,      togglesp,       {.i = 3} },
  { MODKEY,                       XK_m,      togglesp,       {.i = 4} },
  { MODKEY,                       XK_c,      togglesp,       {.i = 5} },

  { MODKEY,                       XK_Return, spawn,          SHCMD("$TERMINAL -e tmux") },
  { MODKEY|ShiftMask,             XK_Return, spawn,          SHCMD("$TERMINAL -c st-256color-c -e tmux") },
  { MODKEY|ControlMask,           XK_Return, spawn,          SHCMD("$TERMINAL -e tmux new \"$TMUX_BIN/open_and_kill_session\"") },
  { MODKEY,                       XK_e,      spawn,          SHCMD("$TERMINAL -e tmux new ranger-standalone") },
  { MODKEY|ShiftMask,             XK_e,      spawn,          SHCMD("$TERMINAL -c st-256color-c -e tmux new ranger-standalone") },
  { MODKEY,                       XK_w,      spawn,          SHCMD("$BROWSER") },

  { MODKEY|ShiftMask,             XK_t,      spawn,          SHCMD("pkill picom || picom --experimental-backend") },

  { 0, XF86XK_Calculator,                    spawn,          SHCMD("pkill -TERM speedcrunch || speedcrunch") },

  { 0, XF86XK_MonBrightnessUp,               spawn,          SHCMD("light -A 15") },
  { 0, XF86XK_MonBrightnessDown,             spawn,          SHCMD("light -U 15") },
  { 0, XF86XK_AudioMute,                     spawn,          SHCMD("amixer -q -D pulse sset Master toggle && pkill -RTMIN+2 dwmblocks") },
  { 0, XF86XK_AudioRaiseVolume,              spawn,          SHCMD("pactl set-sink-volume @DEFAULT_SINK@ +5% && pkill -RTMIN+2 dwmblocks") },
  { 0, XF86XK_AudioLowerVolume,              spawn,          SHCMD("pactl set-sink-volume @DEFAULT_SINK@ -5% && pkill -RTMIN+2 dwmblocks") },
  { 0, XF86XK_AudioPlay,                     spawn,          SHCMD("playerctl play-pause") },
  { 0, XF86XK_AudioNext,                     spawn,          SHCMD("playerctl next") },
  { 0, XF86XK_AudioPrev,                     spawn,          SHCMD("playerctl previous") },


  // Default
  { MODKEY,                       XK_b,      togglebar,      {0} },
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
  { ClkTagBar,            0,              Button4,        incview,        {.i = -1} },
  { ClkTagBar,            0,              Button5,        incview,        {.i =  1} },
  { ClkWinTitle,          0,              Button5,        focusstack,     {.i =  1} },
  { ClkWinTitle,          0,              Button4,        focusstack,     {.i = -1} },
  { ClkWinTitle,          0,              Button9,        killclient,     {0} },
  { ClkWinTitle,          0,              Button8,        tagmon,         {.i=1} },
};

