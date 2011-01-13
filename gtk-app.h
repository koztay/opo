#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)

static gint option_fake = 0;
static gboolean option_fullscreen = FALSE;
static gint option_screens = 4;
static gint option_width = 160;
static gint option_height = 480;

#define MAX_SCREENS 8
#define MIN_SCREENS 1
#define MAX_PIXELS 999999

static GOptionEntry entries[] =
{
  { "fake-source", 'F', 0, G_OPTION_ARG_INT, &option_fake,
    "use videotestsrc (1, 2 ->different patterns)", NULL },
  { "full-screen", 'f', 0, G_OPTION_ARG_NONE, &option_fullscreen, "run full screen", NULL },
  { "width", 'w', 0, G_OPTION_ARG_INT, &option_width, "width of each screen", NULL },
  { "height", 'h', 0, G_OPTION_ARG_INT, &option_height, "height of screen", NULL },
  { "screens", 's', 0, G_OPTION_ARG_INT, &option_screens, "Use this many screens, (max "
    QUOTE(MAX_SCREENS) ")", "S" },
  { NULL, 0, 0, 0, NULL, NULL, NULL }
};

typedef struct window_s {
  GstElement *sink;
  XID xid;
  GtkWidget *widget;
  int id;
} window_t;

