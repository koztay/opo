/*4 way video splitter */
#include <gst/gst.h>
#include <gtk/gtk.h>
#include <gst/interfaces/xoverlay.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include "gtk-app.h"



static void
post_tee_pipeline(GstPipeline *pipeline, GstElement *tee, GstElement *sink,
		  int id){
  GstElement *queue = gst_element_factory_make("queue", NULL);
  gst_bin_add_many (GST_BIN(pipeline),
      queue,
      sink,
      NULL);

  gst_element_link_many(tee,
      queue,
      sink,
      NULL);
}

static GstElement *
pre_tee_pipeline(GstPipeline *pipeline){
  if (pipeline == NULL){
    pipeline = GST_PIPELINE(gst_pipeline_new("wha_pipeline"));
  }
  char * src_name = (option_fake) ? "videotestsrc" : "v4l2src";
  GstElement *src = gst_element_factory_make(src_name, NULL);
  GstElement *tee = gst_element_factory_make ("tee", NULL);

  gst_bin_add_many(GST_BIN(pipeline),
      src,
      tee,
      NULL);

  gst_element_link_many(src,
      tee,
      NULL);
  return tee;
}


static void hide_mouse(GtkWidget *widget){
  GdkWindow *w = GDK_WINDOW(widget->window);
  GdkDisplay *display = gdk_display_get_default();
  GdkCursor *cursor = gdk_cursor_new_for_display(display, GDK_BLANK_CURSOR);
  gdk_window_set_cursor(w, cursor);
  gdk_cursor_unref (cursor);
}


static GstPipeline *
make_multi_pipeline(windows_t *windows, int count)
{
  GstPipeline *pipeline = GST_PIPELINE(gst_pipeline_new("e_wha"));
  GstElement *tee = pre_tee_pipeline(pipeline);
  int i;
  for (i = 0; i < count; i++){
    GstElement *sink = windows->sinks[i];
    post_tee_pipeline(pipeline, tee, sink, i);
  }
  return pipeline;
}


static void
bus_call(GstBus * bus, GstMessage *msg, gpointer data)
{
  windows_t *windows = (windows_t *)data;
  if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ELEMENT &&
      gst_structure_has_name(msg->structure, "prepare-xwindow-id")){
    g_print("Got prepare-xwindow-id msg. option screens: %d\n", option_screens);
    for (int i = 0; i < option_screens; i++){
      gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(windows->sinks[i]),
          windows->xwindows[i]);
      g_print("connected sink %d to window %lu\n", i, windows->xwindows[i]);
      hide_mouse(windows->gtk_windows[i]);
    }
  }
}

static void
toggle_fullscreen(GtkWidget *widget){
  GdkWindowState state = gdk_window_get_state(GDK_WINDOW(widget->window));
  if (state == GDK_WINDOW_STATE_FULLSCREEN){
    gtk_window_unfullscreen(GTK_WINDOW(widget));
  }
  else{
    gtk_window_fullscreen(GTK_WINDOW(widget));
  }
}

static gboolean
key_press_event_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  g_print("got key %c\n", event->keyval);
  switch (event->keyval){
  case 'f':
    toggle_fullscreen(widget);
    break;
  case 'q':
    g_signal_emit_by_name(widget, "destroy");
    break;
  default:
    break;
  }
  return TRUE;
}

static void
destroy_cb(GtkWidget * widget, gpointer data)
{
  GMainLoop *loop = (GMainLoop*) data;
  g_print("Window destroyed\n");
  g_main_loop_quit(loop);
}

static void
video_widget_realize_cb(GtkWidget *widget, gpointer data)
{
  windows_t *windows = (windows_t *)data;
  int r = windows->realised;
  if (r < MAX_SCREENS){
    windows->xwindows[r] = GDK_WINDOW_XID(GDK_WINDOW(widget->window));
    g_print("realised window %d with XID %lu\n", r, windows->xwindows[r]);
  }
  else {
    g_print("wtf, there seem to be %d windows!\n", r);
  }
  windows->realised++;
  hide_mouse(widget);
}


static void
set_up_window(GMainLoop *loop, GtkWidget *window, int screen_no){
  static const GdkColor black = {0, 0, 0, 0};
  gtk_window_set_default_size(GTK_WINDOW(window), WIDTH, HEIGHT);

  if (option_fullscreen){
    gtk_window_fullscreen(GTK_WINDOW(window));
  }

  /*if more than one screen is requested, set the screen number.
    otherwise let it fall were it falls */
  if (option_screens > 1){
    GdkScreen * screen = gdk_screen_get_default();
    int width = gdk_screen_get_width(screen);
    /* XXX placement heuristic is crap: this is better:

       int monitors = gdk_screen_get_primary_monitor(GdkScreen *screen);

       void gdk_screen_get_monitor_geometry(GdkScreen *screen,
                                            gint monitor_num,
                                            GdkRectangle *dest);
      or

       gint gdk_screen_get_monitor_at_point(GdkScreen *screen,
                                             gint x,
                                             gint y);
    */
    int x = (width / option_screens) * screen_no + 1;
    gtk_window_move(GTK_WINDOW(window),
		    x, 50);
    g_print("putting window %d at %d\n", screen_no, x);
  }

  // attach key press signal to key press callback
  gtk_widget_set_events(window, GDK_KEY_PRESS_MASK);
  g_signal_connect(G_OBJECT(window), "key-press-event", G_CALLBACK(key_press_event_cb), NULL);
  g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy_cb), loop);

  gtk_widget_modify_bg(window, GTK_STATE_NORMAL, &black);
  gtk_widget_show_all(window);
  hide_mouse(window);
}





static GstElement *
make_video_windows(GMainLoop *loop)
{
  windows_t windows;
  windows.realised = 0;

  int i;
  for (i = 0; i < option_screens; i++){
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(window, "realize",
        G_CALLBACK(video_widget_realize_cb), &windows);
    /* set up sink here */
    GstElement *sink = gst_element_factory_make("ximagesink", NULL);
    set_up_window(loop, window, i);
    windows.gtk_windows[i] = window;
    windows.sinks[i] = sink;
  }

  GstElement *pipeline = (GstElement *)make_multi_pipeline(&windows, option_screens);

  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  gst_bus_add_watch(bus, (GstBusFunc)bus_call, &windows);
  gst_object_unref(bus);

  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  return pipeline;
}



gint main (gint argc, gchar *argv[])
{
  //initialise threads before any gtk stuff (because not using gtk_init)
  g_thread_init(NULL);
  /*this is more complicated than plain gtk_init/gst_init, so that options from
    all over can be gathered and presented together.
   */
  GOptionGroup *gst_opts = gst_init_get_option_group();
  GOptionGroup *gtk_opts = gtk_get_option_group(TRUE);
  GOptionContext *ctx = g_option_context_new("...!");
  g_option_context_add_main_entries(ctx, entries, NULL);
  g_option_context_add_group(ctx, gst_opts);
  g_option_context_add_group(ctx, gtk_opts);
  GError *error = NULL;
  if (!g_option_context_parse(ctx, &argc, &argv, &error)){
    g_print ("Error initializing: %s\n", GST_STR_NULL(error->message));
    exit (1);
  }
  g_option_context_free(ctx);

  GMainLoop *loop = g_main_loop_new(NULL, FALSE);

  GstElement *pipeline = make_video_windows(loop);

  g_main_loop_run(loop);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  return 0;
}
