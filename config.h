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

///////////////
// AUTOSTART //
///////////////
static const char *const autostart[] = {
  "/bin/sh", "-c", "pgrep -x WhatsApp        || (sleep 30 && whatsapp-nativefier) &", NULL,
  "/bin/sh", "-c", "pkill -x mattermost-desk || (sleep 30 && mattermost-desktop) &", NULL,
  "/bin/sh", "-c", "pgrep -x dwmblocks       || while true ; do dwmblocks ; done", NULL,
  NULL /* terminate */
};

////////////
// CONFIG //
////////////
static const unsigned int snap           = 32; /* snap pixel */

static const int tmux_motion_integration = 1;  /* 0 means no integration */


// LOOK
static const unsigned int borderpx       = 1;  /* border pixel of windows */

/////////////
// SYSTRAY //
/////////////
static const unsigned int systraypinning = 2;  /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayspacing = 2;  /* systray spacing */
static const int systraypinningfailfirst = 0;  /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor */
static const int showsystray             = 1;  /* 0 means no systray */
                                         
/////////
// BAR //
/////////
static const int statusmon               = 2;  /* 1: follow focus | 2: status on all mons | 0 status only on tray mon */

static const int centertitle             = 1;  /* 0 means no center */
                                         
static const int showbar                 = 1;  /* 0 means no bar */
static const int topbar                  = 1;  /* 0 means bottom bar */
static const int vertpadbar              = 10; /* vertical padding for all the statusbar */
static const int horizpadbar             = 4;  /* horizontal padding for statusbar tags indicators */

static const int uline                 = 1;	/* 1 enable uline, 0 disable unline */
static const unsigned int ulinepad	   = 5;	/* horizontal padding between the underline and tag */
static const unsigned int ulinestroke  = 2;	/* thickness / height of the underline */
static const unsigned int ulinevoffset = 3;	/* how far above the bottom of the bar the line should appear */
static const int ulineall              = 0;	/* 1 to show underline on all tags, 0 for just the active ones */

static unsigned int hidevacant  = 2;      /* 0: default | 1: hide unused tags | 2: 1 and do now draw squares */

static const char *fonts[]          = { " Ubuntu Mono:style=Regular:size=12",
                                        "Noto Color Emoji:pixelsize=14:antialias=true:autohint=true"};
static const char *colors[][4]      = {
  /*                  fg         bg         border   */
  [SchemeNorm]   = { "#ffffff", "#1d2021", "#222222"},
  // No color 
  [SchemeSel]    = { "#ffffff", "#1d2021", "#D3D3D3"},
  [SchemeSelAlt] = { "#ffffff", "#1d2021", "#D3D3D3"},
  // colorfull
  /* [SchemeSel]     = { "#ffffff", "#0058a0", "#D3D3D3"}, */
  /* [SchemeSelAlt]  = { "#ffffff", "#3d4041", "#D3D3D3"}, */
  [SchemeNormTab] = { "#ffffff", "#2d3031", "#222222"},
  [SchemeSelTab]  = { "#ffffff", "#1068b0", "#D3D3D3"},
};

/////////////
// LAYOUTS //
/////////////
enum showtab_modes { showtab_never, showtab_auto, showtab_nmodes, showtab_always};
static const int showtab			= showtab_auto;        /* Default tab bar show mode */
static const int toptab				= 1;                   /* Tab bar on top */

static const float largemfact   = 0.75;       /* uncollapsed */
static const float mfact        = largemfact; /* factor of master area size [0.05..0.95] */
static const int   nmaster      = 1;          /* number of clients in master area */
static const int   resizehints  = 0;          /* 1 means respect size hints in tiled resizals */

#define TALL     0
#define DECK     1
#define TABS     2
#define GRID     3
#define FLOATING 4

static const Layout layouts[] = {
  /* symbol     arrange function */
  { "| Tall |",     tile },    /* first entry is default */
	{ "| Deck |",     deck },
  { "| Tabs |",     monocle },
  { "| Grid |",     grid },
  { "| Floating |", NULL },    /* no layout function means floating behavior */
 	{ NULL,           NULL },
};

/////////////
// TAGGING //
/////////////
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

///////////
// RULES //
///////////
static const Rule rules[] = {
  /* xprop(1):
   *	WM_CLASS(STRING) = instance, class
   *	WM_NAME(STRING) = title
   */
  /* tag ~0 means sticky */
  /* class                      | instance | title               |tags  | isfloating | float x,y,w,h % | float zone | isterminal | noswallow | sp_id | monitor */
  { "Thunderbird"               , NULL     , NULL                , 1<<8 , 0          , HINTS           , NN         , 0          , 0         ,     0 ,  0      },
  { "tridactyl_editor"          , NULL     , NULL                , ~0   , 1          , CENTER          , NN         , 0          , 0         ,     0 , -1      },
  { NULL                        , NULL     , "Picture-in-Picture", ~0   , 1          , 70,70,30,30     , LR         , 0          , 0         ,     0 , -1      },
  { "st-256color"               , NULL     , NULL                , 0    , 0          , CENTER_HINTS    , NN         , 1          , 1         ,     0 , -1      },
  { "st-256color-c"             , NULL     , NULL                , 0    , 1          , 10,10,80,80     , NN         , 1          , 1         ,     0 , -1      },
  { "st-256color-ur"            , NULL     , NULL                , ~0   , 1          , 60,2,40,20      , UR         , 1          , 1         ,     0 , -1      },
  { "SpeedCrunch"               , NULL     , NULL                , ~0   , 1          , CENTER          , NN         , 1          , 1         ,     0 , -1      },
  { NULL                        , NULL     , "Event Tester"      , 0    , 0          , HINTS           , NN         , 0          , 1         ,     0 , -1      },
  { "Yad"                       , NULL     , NULL                , 0    , 1          , 60,2,40,20      , UR         , 0          , 0         ,     0 , -1      },
  { "Dragon-drag-and-drop"      , NULL     , NULL                , ~0   , 1          , CENTER_HINTS    , NN         , 0          , 0         ,     0 , -1      },
  { "st-256color-docked"        , NULL     , NULL                , ~0   , 1          , CENTER          , MM         , 1          , 1         ,     1 , -1      },
  { "st-256color-notes"         , NULL     , NULL                , ~0   , 1          , CENTER          , NN         , 0          , 0         ,     2 , -1      },
  { "st-256color-mail"          , NULL     , NULL                , ~0   , 1          , CENTER          , NN         , 0          , 0         ,     4 , -1      },
  { "st-256color-calendar"      , NULL     , NULL                , ~0   , 1          , CENTER          , NN         , 0          , 0         ,     5 , -1      },
  { "st-256color-music"         , NULL     , NULL                , ~0   , 1          , CENTER          , NN         , 0          , 0         ,     8 , -1      },
  { "st-256color-tmp-notes"     , NULL     , NULL                , ~0   , 1          , CENTER          , NN         , 0          , 0         ,     9 , -1      },
  { "gazebo"                    , NULL     , NULL                , 1<<8 , 0          , HINTS           , NN         , 0          , 0         ,     0 ,  0      },
  { "rviz"                      , NULL     , NULL                , 1<<8 , 0          , HINTS           , NN         , 0          , 0         ,     0 ,  0      },
  { "rqt_console"               , NULL     , NULL                , 1<<8 , 0          , HINTS           , NN         , 0          , 0         ,     0 ,  0      },

  { "whatsapp-nativefier-d40211" , NULL     , NULL               , ~0   , 1          , CENTER          , NN         , 0          , 0         ,     3 , -1      },
  { "YouTube Music"              , NULL     , NULL               , ~0   , 1          , CENTER          , NN         , 0          , 0         ,     6 , -1      },
  { "Mattermost"                 , NULL     , NULL               , ~0   , 1          , CENTER          , NN         , 0          , 0         ,     7 , -1      },
  { "Session"                    , NULL     , NULL               , ~0   , 1          , CENTER          , NN         , 0          , 0         ,    10 , -1      },
};


static const char* scratchpads_cmd[] = {
  "$TERMINAL -c st-256color-docked -e tmux",
  "$TERMINAL -c st-256color-notes -T vimwiki -e $SHELL -c \"cd \"$HOME\"/.local/share/vimwiki && $EDITOR index.md\"",
  "whatsapp-nativefier",
  "$TERMINAL -c st-256color-mail -T neomutt -e $SHELL -c \"neomutt ; pkill -RTMIN+17 dwmblocks \"",
  "$TERMINAL -c st-256color-calendar -T calcurse -e $SHELL -c calcurse",
  "youtube-music",
  "mattermost-desktop",
  "$TERMINAL -c st-256color-music -T ncmpcpp -e ncmpcpp",
  "$TERMINAL -c st-256color-tmp-notes -T tmp-notes -e $EDITOR $(mktemp) -c \"set spell\"",
  "session-desktop",
};

typedef int dims[4];

static dims floatzones[][3] = {
  /*      |  L         | M             |  R           */
  /* U */	{ {0,0,30,30} , {0,0,100,20} , {70,0,30,30} },
  /* M */	{ {0,0,25,100}, {10,10,80,80}, {75,0,25,100}},
  /* L */	{ {0,70,30,30}, {0,80,100,20}, {70,70,30,30}},
};

static const int floatingdims[] = { CENTER };

////////////////////
// KEYBOARD BINDS //
////////////////////

/* key definitions */
#define MODKEY Mod1Mask
#define ALTMODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* commands */
static char drun[] = "rofi -show drun";

static Key keys[] = {
  /* modifier                     key        function        argument */
  { MODKEY,                       XK_F5,     spawn,          SHCMD("lxsudo /bin/sh -c \"cd $HOME/.local/share/uur/dwm/dwm && make all && make install\" && pkill dwm") },
  { MODKEY|ShiftMask,             XK_r,      spawn,          SHCMD("lxsudo /bin/sh -c \"cd $HOME/.local/share/uur/dwm/dwm && make all && make install\" && pkill dwm") },
  { MODKEY,                       XK_p,      spawn,          SHCMD("dmenu-pass") },
  { MODKEY|ShiftMask,             XK_p,      spawn,          SHCMD("dmenu-passotp") },
  { MODKEY|ShiftMask|ControlMask, XK_p,      spawn,          SHCMD("passmenu --type") },
  { MODKEY,                       XK_i,      spawn,          SHCMD("$DMENU_BIN/pulse_conf source ; pkill -RTMIN+2 dwmblocks") },
  { MODKEY,                       XK_o,      spawn,          SHCMD("$DMENU_BIN/pulse_conf sink ; pkill -RTMIN+2 dwmblocks") },
  { MODKEY,                       XK_u,      spawn,          SHCMD("$DMENU_BIN/dmenu_unicode") },
  { MODKEY|ShiftMask,             XK_q,      spawn,          SHCMD("$DMENU_BIN/dmenu_leave") },
  { MODKEY,                       XK_r,      spawn,          SHCMD(drun) },
  { MODKEY|ShiftMask,             XK_o,      spawn,          SHCMD("$DMENU_BIN/monitor_layout") },
  { MODKEY,                       XK_Tab,    view,           {.ui = ~0 } },
  { MODKEY|ShiftMask,             XK_Tab,    spawn,          SHCMD("rofi -show window") },
  { 0,                            XK_Print,  spawn,          SHCMD("flameshot gui") },
  { MODKEY|ShiftMask|ControlMask, XK_x,      spawn,          SHCMD("xkill") },
  { MODKEY                      , XK_v,      spawn,          SHCMD("xournal") },

  { MODKEY,                       XK_q,      killclient,               {0} },
  { MODKEY,                       XK_a,      togglefloating,           {.i = 1} },
  { MODKEY,                       XK_s,      togglesticky,             {0} },
                                                                       
  { MODKEY,                       XK_semicolon,  togglefocustype,      {.i = 0} },
  { MODKEY,                       XK_apostrophe, togglefocustype,      {0} },
  { MODKEY,                       XK_f,          togglefullscreen,     {0} },

  { MODKEY,                       XK_comma,     rotate,                {.i = -1 } },
  { MODKEY,                       XK_period,    rotate,                {.i = +1 } },
  { MODKEY|ShiftMask,             XK_semicolon, hardresizehorizontal,  {.i = 0 } },
  { MODKEY|ShiftMask|ControlMask, XK_semicolon, zoom,                  {.i = 0 } },

  { MODKEY,                       XK_h,      horizontalfocus,          {.i = -1 } },
  { MODKEY,                       XK_l,      horizontalfocus,          {.i = +1 } },
  { MODKEY|ShiftMask,             XK_h,      movehorizontal,           {.i = -1} },
  { MODKEY|ShiftMask,             XK_l,      movehorizontal,           {.i = +1} },
  { MODKEY|ShiftMask|ControlMask, XK_h,      resizehorizontal,         {.i = -1} },
  { MODKEY|ShiftMask|ControlMask, XK_l,      resizehorizontal,         {.i = +1} },
  { MODKEY,                       XK_j,      focusstack,               {.i = +1 } },
  { MODKEY,                       XK_k,      focusstack,               {.i = -1 } },
  { MODKEY|ShiftMask,             XK_j,      movevertical,             {.i =  1 } },
  { MODKEY|ShiftMask,             XK_k,      movevertical,             {.i = -1 } },
  { MODKEY|ShiftMask|ControlMask, XK_j,      resizevertical,           {.i =  1 } },
  { MODKEY|ShiftMask|ControlMask, XK_k,      resizevertical,           {.i = -1 } },
  { MODKEY|ControlMask,           XK_h,      reorganizetags,           {.i =  1 } },
  { MODKEY|ControlMask,           XK_l,      reorganizetags,           {.i = -1 } },
  { MODKEY,                       XK_grave, focusmon,                  {.i =  1 } },
  { MODKEY|ShiftMask,             XK_grave, tagmon,                    {.i =  1 } },
  { MODKEY|ControlMask,           XK_j,      incview,                  {.i =  1 } },
  { MODKEY|ControlMask,           XK_k,      incview,                  {.i = -1 } },

  { MODKEY,                       XK_0,      lastfreeviewwrap,         {.i =  1 } },
  { MODKEY|ShiftMask,             XK_0,      lastfreetagwrap,          {.i =  1 } },
  { MODKEY|ControlMask,           XK_0,      freeview,                 {.i =  1 } },
  { MODKEY|ShiftMask|ControlMask, XK_0,      freetag,                  {.i =  1 } },
 	{ MODKEY|ControlMask,		        XK_comma,  cyclelayout,              {.i = -1 } },
 	{ MODKEY|ControlMask,           XK_period, cyclelayout,              {.i = +1 } },
 	{ MODKEY|ControlMask,           XK_Tab,    cyclelayout,              {.i = +1 } },
  { MODKEY,                       XK_d,      setlayout,                {.v = &layouts[DECK]} },
  { MODKEY,                       XK_g,      setlayout,                {.v = &layouts[GRID]} },
  { MODKEY,                       XK_t,      setlayout,                {.v = &layouts[TABS]} },
  { MODKEY|ShiftMask,             XK_d,      setlayout,                {.v = &layouts[TALL]} },
  { MODKEY|ShiftMask,             XK_f,      togglefullscreen,         {0} },
  { MODKEY|ShiftMask|ControlMask, XK_f,      togglefullfullscreen,     {0} },
  { 0,                            XK_F11,    togglefullscreen,         {0} },

  { MODKEY,                       XK_space,  togglesp,                 {.i = 1} },
  { MODKEY,                       XK_n,      togglesp,                 {.i = 2} },
  { MODKEY|ShiftMask,             XK_w,      togglesp,                 {.i = 3} },
  { MODKEY,                       XK_m,      togglesp,                 {.i = 4} },
  { MODKEY,                       XK_c,      togglesp,                 {.i = 5} },
  { MODKEY,                       XK_y,      togglesp,                 {.i = 6} },
  { MODKEY|ShiftMask,             XK_m,      togglesp,                 {.i = 7} },
  { MODKEY|ShiftMask,             XK_n,      togglesp,                 {.i = 8} },
  { MODKEY|ShiftMask|ControlMask, XK_space,  togglesp,                 {.i = 9} },
  { MODKEY|ShiftMask,             XK_s,      togglesp,                 {.i = 10} },

  { MODKEY,                       XK_Return, spawn,                    SHCMD("$TERMINAL -e tmux") },
  { MODKEY|ShiftMask,             XK_Return, spawn,                    SHCMD("$TERMINAL -c st-256color-c -e tmux") },
  { MODKEY|ControlMask,           XK_Return, spawn,                    SHCMD("$TERMINAL -e tmux new \"$TMUX_BIN/open_and_kill_session\"") },
  { MODKEY,                       XK_e,      spawn,                    SHCMD("$TERMINAL -e tmux new \"zsh -is ranger\"") },
  { MODKEY|ShiftMask,             XK_e,      spawn,                    SHCMD("$TERMINAL -c st-256color-c -e tmux new \"zsh -is ranger\"") },
  { MODKEY,                       XK_w,      spawn,                    SHCMD("$BROWSER") },
                                                                  
  { MODKEY|ShiftMask,             XK_t,      spawn,                    SHCMD("pkill picom || picom --experimental-backend") },
                                                                  
  { 0, XF86XK_Calculator,                    spawn,                    SHCMD("pkill -TERM speedcrunch || speedcrunch") },
                                                                  
  { 0, XF86XK_MonBrightnessUp,               spawn,                    SHCMD("light -A 15") },
  { 0, XF86XK_MonBrightnessDown,             spawn,                    SHCMD("light -U 15") },
  { 0, XF86XK_AudioMute,                     spawn,                    SHCMD("amixer -q -D pulse sset Master toggle && pkill -RTMIN+2 dwmblocks") },
  { 0, XF86XK_AudioRaiseVolume,              spawn,                    SHCMD("pactl set-sink-volume @DEFAULT_SINK@ +5% && pkill -RTMIN+2 dwmblocks") },
  { 0, XF86XK_AudioLowerVolume,              spawn,                    SHCMD("pactl set-sink-volume @DEFAULT_SINK@ -5% && pkill -RTMIN+2 dwmblocks") },
  { 0, XF86XK_AudioPlay,                     spawn,                    SHCMD("playerctl play-pause") },
  { 0, XF86XK_AudioNext,                     spawn,                    SHCMD("playerctl next") },
  { 0, XF86XK_AudioPrev,                     spawn,                    SHCMD("playerctl previous") },
  { MODKEY, XK_equal,                        spawn,                    SHCMD("pactl set-sink-volume @DEFAULT_SINK@ +5% && pkill -RTMIN+2 dwmblocks") },
  { MODKEY, XK_minus,                        spawn,                    SHCMD("pactl set-sink-volume @DEFAULT_SINK@ -5% && pkill -RTMIN+2 dwmblocks") },
  { MODKEY, XK_BackSpace,                    spawn,                    SHCMD("amixer -q -D pulse sset Master toggle && pkill -RTMIN+2 dwmblocks") },
  { MODKEY|ShiftMask, XK_equal,              spawn,                    SHCMD("mpc next") },  
  { MODKEY|ShiftMask, XK_minus,              spawn,                    SHCMD("mpc prev") },  
  { MODKEY|ShiftMask, XK_BackSpace,          spawn,                    SHCMD("mpc toggle") },

  { MODKEY|ShiftMask|ControlMask, XK_equal,              spawn,        SHCMD("playerctl next") },
  { MODKEY|ShiftMask|ControlMask, XK_minus,              spawn,        SHCMD("playerctl previous") },
  { MODKEY|ShiftMask|ControlMask, XK_BackSpace,          spawn,        SHCMD("playerctl play-pause") },


  { MODKEY,                         XK_b,      togglebar,         {0} },
  { MODKEY| ShiftMask,              XK_b,      togglehidevaccant, {0} },

    TAGKEYS(                        XK_1,                          0)
    TAGKEYS(                        XK_2,                          1)
    TAGKEYS(                        XK_3,                          2)
    TAGKEYS(                        XK_4,                          3)
    TAGKEYS(                        XK_5,                          4)
    TAGKEYS(                        XK_6,                          5)
    TAGKEYS(                        XK_7,                          6)
    TAGKEYS(                        XK_8,                          7)
    TAGKEYS(                        XK_9,                          8)
};

/////////////////
// MOUSE BINDS //
/////////////////

/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
  /* click                event mask      button          function        argument */
  { ClkStatusText,        0,              Button1,        sigdwmblocks,      {.i = 1} },
  { ClkStatusText,        0,              Button2,        sigdwmblocks,      {.i = 2} },
  { ClkStatusText,        0,              Button3,        sigdwmblocks,      {.i = 3} },
  { ClkStatusText,        0,              Button4,        sigdwmblocks,      {.i = 4} },
  { ClkStatusText,        0,              Button5,        sigdwmblocks,      {.i = 5} },
  { ClkStatusText,        ShiftMask,      Button1,        sigdwmblocks,      {.i = 6} },

  { ClkLtSymbol,          0,              Button1,        cyclelayout,       {.i = 1} },
  { ClkLtSymbol,          0,              Button2,        view,              {.ui = ~0 }  },
  { ClkLtSymbol,          0,              Button3,        setlayout,         {.v = &layouts[1]} },

  { ClkClientWin,         MODKEY,         Button1,        movemouse,         {.i=0} },
  { ClkClientWin,         MODKEY,         Button2,        killclient,        {0} },
  { ClkClientWin,         MODKEY,         Button3,        resizemouse,       {0} },
  { ClkClientWin,         0,              Button8,        movemouse,         {.i=1} },
  { ClkClientWin,         0,              Button9,        killclient,        {0} },

  { ClkTagBar,            0,              Button3,        togglehidevaccant, {0} },
  { ClkTagBar,            0,              Button1,        view,              {0} },
  { ClkTagBar,            0,              Button2,        togglehidevaccant, {0} },
  { ClkTagBar,            0,              Button8,        tag,               {0} },
  { ClkTagBar,            MODKEY,         Button3,        toggletag,         {0} },
  { ClkTabBar,            0,              Button1,        focuswin,          {0} },
  { ClkTabBar,            0,              Button2,        killwin ,          {0} },
  { ClkTagBar,            0,              Button4,        incview,           {.i = -1} },
  { ClkTagBar,            0,              Button5,        incview,           {.i =  1} },

  { ClkWinTitle,          0,              Button5,        focusstack,        {.i =  1} },
  { ClkWinTitle,          0,              Button4,        focusstack,        {.i = -1} },
  { ClkWinTitle,          0,              Button9,        killclient,        {0} },
  { ClkWinTitle,          0,              Button8,        tagmon,            {.i=1} },

  { ClkRootWin,           0,              Button3,        spawn,             SHCMD(drun) },
};                                                                           
