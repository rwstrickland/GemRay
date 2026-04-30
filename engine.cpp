//
// engine.cpp
//
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Shared_Image.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Spinner.H>
#include <FL/filename.H>
#include <FL/fl_draw.H>
#include <FL/Fl_XPM_Image.H> // for xpm button image
#include <FL/Fl_Pixmap.H> // for xpm button image
#include "Fl_Image_Button.H" // for image button. Not standard
#include <FL/Fl_Copy_Surface.H> // clipboard

#include <jpeg/jpeglib.h>

#include <sys/time.h>     // for gettimeofday() for elapsed time

#include <cstdio>
#include <cstdlib>

#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include "stone.h"
#include "raytracer.h"
#include "engine.h"

#ifdef WIN32
//#include <windows.h>
#elif MACOS
#include <sys/param.h>
#include <sys/sysctl.h>
#else // linux
#include <unistd.h>
#endif

#include "XPM/pause.xpm"      // pause button
#include "XPM/play.xpm"       // play button
#include "XPM/saveas.xpm"     // save as button
#include "XPM/savemult.xpm"     // save as button
#include "XPM/graph.xpm"     // save as button
#include "XPM/gemray150x150.xpm" // icon

int getNumberOfCores() {
#ifdef WIN32
  return std::thread::hardware_concurrency(); // Will this work on MAC and linux?
//SYSTEM_INFO sysinfo;
//GetSystemInfo(&sysinfo);
//return sysinfo.dwNumberOfProcessors;
#elif MACOS
  int nm[2];
  size_t len = 4;
  uint32_t count;
 
  nm[0] = CTL_HW;
  nm[1] = HW_AVAILCPU;
  sysctl(nm, 2, &count, &len, NULL, 0);
 
  if(count < 1) {
    nm[1] = HW_NCPU;
    sysctl(nm, 2, &count, &len, NULL, 0);
    if(count < 1) { count = 1; }
  }
  return count;
#else
  return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

#ifndef WIN32
#define HAVE_PTHREAD_H 1       // RWS linux; undef for Windows
#define HAVE_PTHREAD 1         // RWS linux; undef for Windows
#endif
#include "threads_mod.h"

static int pix = 300; // image size in pixels
#define CTRL_HT 42   // height in pixels of control bar
#define PROG_HT 20   // height in pixels of progress bar

class Draggable_Window : public Fl_Double_Window {
protected:
  int drag_x, drag_y;
  int save_w, save_h;
public:
  int handle(int e);
  Draggable_Window(int x, int y, int w, int h, const char *l=0):
      Fl_Double_Window(x, y, w, h, l) {
    save_w = w;
    save_h = h;
    resize(x, y, w, h);
  }
};

int Draggable_Window::handle(int e)
{
  if (e == FL_PUSH) {
    drag_y = Fl::event_y();
    drag_x = Fl::event_x();
    if (drag_y >= pix)
      return Fl_Double_Window::handle(e);
    cursor(FL_CURSOR_MOVE);
    return 1;
  }
  else if (e == FL_DRAG) {
    cursor(FL_CURSOR_MOVE);
    int ny = Fl::event_y();
//  if (ny >= pix)
//    return Fl_Double_Window::handle(e);
    int nx = Fl::event_x();
    int ox = x();
    int oy = y();
    int dx = nx - drag_x;
    int dy = ny - drag_y;
//  drag_x = nx;
//  drag_y = ny;
    resize(ox + dx, oy + dy, save_w, save_h);
    return 1;
  }
  else if (e == FL_RELEASE) {
    int ny = Fl::event_y();
    cursor(FL_CURSOR_DEFAULT);
    if (ny < pix)
      return 1;
  }
  return Fl_Double_Window::handle(e);
}

static int nthreads = 1;
static int nfinished = 0;

static bool playing = false;
static bool interrupt = false;
static bool rendering = false;

bool xsym;
bool ysym;
bool xandysym;
bool sym4m;

static double fps = 30.;        // 30 frames per second rate

static uchar **bits[4] = {NULL, NULL, NULL, NULL};

double *brightness = NULL; // caller can extern

static int NFRAMES=61;         // for +/-30 degree tilt animation
static double maxtilt = 30.;
static double tiltinc = 1.0;  // tilt increment in degrees

static double pav = 1.0;  // pavilion scale factor
static double crn = 1.0;  // crown scale factor
static double elev = 0.;
static double head = 10.;
static double zeye = 10.;
static Draggable_Window *win = NULL;  // main window
static Fl_RGB_Image **img;            // loaded images
static Fl_Group *grp = 0;             // group in which images are displayed

Fl_Thread ray_thread[32]; // An identifier used by the threading system

int model = 0;

void
load_engine_defaults()
{
//Fl_Preferences app(Fl_Preferences::USER, "GemCad", "GemRay/preferences");
  Fl_Preferences app(Fl_Preferences::USER, "GemCad", "GemRay");
  Fl_Preferences reg(app, "Defaults");
  reg.get((const char *) "frames_per_second", fps, 30.);
  reg.get((const char *) "lighting_model", model, 0);
}

/**
  The purpose of the FrameRenderer is to store the parameters needed for the
  calls to Raytracer::render() to render a series of frames for the animation.
  The Raytracer::render() method is called by the FrameRenderer::run() method
  once for each frame of the animation.
*/
class FrameRenderer
{
protected:
  Raytracer *rt;
  int ithread;
  int nx;
  int ny;
  double xhead;
  double xzeye;
  unsigned int kolor;
  unsigned int bkgkolor;
  unsigned int leakkolor;
  unsigned int headkolor;
  double gamma;
  double gain;
  double xpav, xcrn;
  uchar *bits0, *bits1, *bits2, *bits3;
public:
  void init(int ith, Raytracer *prt, int inx, int iny,
      double ahead, double azeye,
      unsigned int ikolor, unsigned int ibkgkolor,
      unsigned int ileakkolor, unsigned int iheadkolor,
      double agamma, double again,
      double acrn, double apav) {
    ithread = ith;
    rt = prt;
    nx = inx;
    ny = iny;
    xhead = ahead;
    xzeye = azeye;
    kolor = ikolor;
    bkgkolor = ibkgkolor;
    leakkolor = ileakkolor;
    headkolor = iheadkolor;
    gamma = agamma;
    gain = again;
    xcrn = acrn;
    xpav = apav;
  };
  int thread() { return ithread; };
  void run();
};

/**
  The animation is performed in multiple threads. The number of threads
  is set equal to the number of processor cores, nthreads. There is
  one main thread and nthreads-1 background threads. This run() method
  controls the background threads. It gets called once per background
  thread. In order for FLTK to update the progress bar, the main thread
  updates the progress bar and displays the frames that it calculates.
  The main thread does frames 0 and multiples of nthreads. The zeroth
  background thread does frames 1, nthreads+1, 2*nthreads+1, etc. Back-
  ground threads do no FLTK calls.
*/
void
FrameRenderer::run() {
  for (int i = ithread; i < NFRAMES; i += nthreads) {
    if (interrupt)
      break;
    double b0, b1, b2, b3;
    if (elev != 0.) {
      b0 = 0.;
      b1 = cos(elev*M_PI/180.); // use pavilion angle or 18 for tritetra
      b2 = -sin(elev*M_PI/180.);
      b3 = (NFRAMES-1.)/2. - i;
      b3 *= tiltinc;
    }
    else {
      if (sym4m) {
        if (i <= (NFRAMES-1)/2) { // tilt to N
          b0 = 1.;
          b1 = 0.;
          b2 = 0.;
          b3 = i-(NFRAMES-1.)/2.;
          b3 *= tiltinc;
        }
        else { // tilt to SE
          b0 = sqrt(2.)/2.;
          b1 = b0;
          b2 = 0.;
          b3 = i-(NFRAMES-1.)/2.;
          b3 *= tiltinc;
        }
      }
      else if (xandysym) {
        if (i <= (NFRAMES-1)/2) { // tilt to N
          b0 = 1.;
          b1 = 0.;
          b2 = 0.;
          b3 = i-(NFRAMES-1.)/2.;
          b3 *= tiltinc;
        }
        else { // tilt to E
          b0 = 0.;
          b1 = 1.;
          b2 = 0.;
          b3 = i-(NFRAMES-1.)/2.;
          b3 *= tiltinc;
        }
      }
      else { // tilt N to S
        b0 = 1.;
        b1 = 0.;
        b2 = 0.;
        b3 = i-(NFRAMES-1.)/2.;
        b3 *= tiltinc;
      }
    }
    b0 = -b0;
    rt->render(nx, ny, xhead, xzeye, kolor, bkgkolor, leakkolor, headkolor,
        gamma, gain,
        xcrn, xpav, b0, b1, b2, b3, brightness+i*6,
        bits[0][i], bits[1][i], bits[2][i], bits[3][i]);
  }
}

static const char *light[4] = {"RND", "SC2", "COS", "ISO"};

/**
  Run the FrameRenderer to calculate one set of frames for the animation.
  There is one of these called for each background thread each started
  by its call to fl_create_thread().
*/
static void *run_bkg_thread(void *p)
{
  FrameRenderer *r = (FrameRenderer *) p;
  (void) r->run();
  int i = r->thread();
  nfinished++;
  void *q = (void *)(intptr_t) i;
  Fl::awake(q);
  return NULL;
}

static int xframe = NFRAMES-1;

static Fl_Spinner *pfns = NULL;

#define LSIZE 12     // label size for widgets

/**
  File exists?
*/
bool fexists (const char *filename) {
    struct stat buffer;   
      return stat(filename, &buffer) == 0; 
}

/**
  Save image buff as a JPEG file, prompting for name
*/
int write_jpeg(FILE *outfile, uchar *buff, int w, int h)
{
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr       jerr;
 
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, outfile);
 
  cinfo.image_width      = w;
  cinfo.image_height     = h;
  cinfo.input_components = 3;
  cinfo.in_color_space   = JCS_RGB;
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality (&cinfo, 95, TRUE); // set the quality [0..100]
  jpeg_start_compress(&cinfo, TRUE);

//int j = 0;
  const uchar *p = buff;
  while (cinfo.next_scanline < cinfo.image_height) {
//  JSAMPROW row_pointer = (JSAMPROW) p;
//  JSAMPROW row_pointer = (JSAMPROW) &buff[3L*w*j];
//  jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    jpeg_write_scanlines(&cinfo, (JSAMPROW *) &p, 1);
    p += 3L*w;
//  j++;
  }
  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);
  int iret = ferror(outfile) ;
  return iret;
}

/**
  Callback to show the next image.
  Slaps next image up on screen, resets frame timer.
  Animation loops with auto reverse unless maximum tilt angle is 180.
  If so int loops full circle no reverse. It also communicates the
  frame number to the frame number widget.
*/
void ShowNextImage_CB(void* d) {
  if (interrupt) return;
//Fl::lock(); // prevent this from being interrupted by the timer
  static int inc = 1;
  // The following range checks /shouldn't/ be needed but I guess sometimes
  // the code below gets interrupted by the timeout in the middle changing
  // the count
  if (xframe < 0) xframe = 0;
  if (xframe >= NFRAMES) xframe = NFRAMES-1;
  Fl_RGB_Image *p = img[xframe];
  if (p) grp->image(p);
  int i = (intptr_t) d;
  if (i != 1) xframe += inc; // if called by ltModChooser_CB(), don't increment
  if (maxtilt == 180. && xframe > NFRAMES-1)
    xframe = 0; // full circle
  if (xframe > NFRAMES-1) {
    xframe = NFRAMES-1; // will repeat last frame
    inc = -1;
  }
  if (xframe < 0) {
    xframe = 0; // will repeat first frame
    inc = 1;
  }
  if (!playing && pfns != NULL) pfns->value(xframe);
  if (win)
    win->redraw();
  if (playing) Fl::repeat_timeout(1./fps, ShowNextImage_CB);
//Fl::unlock();
}

/**
  Callback for animation frame rate spinner value change
*/
static void fpsSpinCB(Fl_Widget *o)
{
  Fl_Spinner *p = (Fl_Spinner *)o;
  fps = p->value();
//Fl_Preferences app(Fl_Preferences::USER, "GemCad", "GemRay/preferences");
  Fl_Preferences app(Fl_Preferences::USER, "GemCad", "GemRay");
  Fl_Preferences reg(app, "Defaults");
  reg.set((const char *) "frames_per_second", fps);
}

/**
  Callback for animation frame number spinner value change
*/
static void fnumSpinCB(Fl_Widget *o)
{
  pfns = (Fl_Spinner *)o;
  xframe = pfns->value();
  if (xframe < 0) xframe = 0;
  if (xframe >= NFRAMES) xframe = NFRAMES-1;
  if (!playing) ShowNextImage_CB((void*) 1);
}

static Fl_Image_Button *psaveJPG = NULL;
static Fl_Image_Button *pgraph = NULL;

/**
  Callback for Save As JPG button
*/
static void save_jpg_btn_CB(Fl_Widget *o)
{
  // Create native chooser
  char *JPGfilename = NULL;
  int namelen = 0;
  Fl_Native_File_Chooser native;
  native.title("Save As JPEG file");
  native.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
#ifdef WIN32
  native.filter("Image files: *.jpg\t*.jpg\nImage files: *.jpeg\t*.jpeg");
#else
  native.filter("Image files \t*.jpg\nImage files \t*.jpeg");
#endif
  native.directory(".");
//native.preset_file(G_JPGfilename->value());
  // Show native chooser
  switch (native.show()) {
  case -1: // ERROR
    fl_alert("%s", native.errmsg());
    break;
  case  1: // CANCEL
    break;
  default: // PICKED FILE
    if (native.filename()) {
      namelen = strlen(native.filename()) + 20;
      JPGfilename = new char[namelen];
      strcpy(JPGfilename, native.filename());
    }
    else
      JPGfilename = NULL;
    break;
  }
  if (JPGfilename == NULL)
    return;
  if (playing) {
    for (int i = 0; i < NFRAMES; i++) {
      char ext[20];
      snprintf(ext,sizeof(ext), "_%03d.jpg", i);
      char *fname = new char[namelen];
      strcpy(fname, JPGfilename);
      fl_filename_setext(fname, namelen, ext);
      printf("File name for frame %d = %s\n", i, fname);
      FILE *outfile = fopen(fname, "wb");
      if (!outfile) {
        fl_alert("Cannot open %s to write", JPGfilename);
        delete[] JPGfilename;
        return;
      }
      write_jpeg(outfile, bits[model][i], pix, pix);
      fclose(outfile);
      delete[] fname;
    }
  }
  else {
    if (fexists(JPGfilename)) {
      int choice = fl_choice("%s exists.",NULL,"Cancel","Overwrite",
          JPGfilename);
      if (choice != 2) {
        delete[] JPGfilename;
        return;
      }
    }
    FILE *outfile = fopen(JPGfilename, "wb");
    if (!outfile) {
      fl_alert("Cannot open %s to write", JPGfilename);
      delete[] JPGfilename;
      return;
    }
    write_jpeg(outfile, bits[model][xframe], pix, pix);
    fclose(outfile);
    if (JPGfilename != NULL)
      delete[] JPGfilename;
  }
}

static char title[256];

bool screencopy = false;

class brightplot: public Fl_Double_Window {
private:
protected:
  void draw();
  static void Copy_Text_CB(Fl_Widget*, void *userdata) {
    printf("*** Copy text ***\n");
    int nang = (int) (maxtilt+0.01) * 2 + 1;
    char *buff, *lbuff;
    buff = new char[256L*nang];
    lbuff = new char[256];
    buff[0] = '\0';
    int ang = -maxtilt;
    strcat(buff, title);
    strcat(buff, "\n");
    strcat(buff, " ang   ISO   COS   SC2 T_ISO T_COS T_SC2\n");
    for (int i = 0; i < nang; i++) {
      snprintf(lbuff,256, "%4d %5.1f %5.1f %5.1f %5.1f %5.1f %5.1f\n",
        ang,
        brightness[6*i],
        brightness[6*i+1],
        brightness[6*i+2],
        brightness[6*i+3],
        brightness[6*i+4],
        brightness[6*i+5]);
      strcat(buff, lbuff);
      ang++;
    }
    Fl::copy(buff, strlen(buff), 1);
    delete[] lbuff;
    delete[] buff;
  }
  static void Copy_Image_CB(Fl_Widget* w, void *userdata) {
    screencopy = true;
  }
/*
  static void Paste_CB(Fl_Widget*, void *userdata) {
    printf("*** PASTE ***\n");
    MyInput *in = (MyInput*)userdata;
    Fl::paste(*in);
  }
*/
public:
  int handle(int e) {
    switch (e) {
    case FL_PUSH: // RIGHT MOUSE PUSHED? Popup menu on right click
      if ( Fl::event_button() == FL_RIGHT_MOUSE ) {
        Fl_Menu_Item rclick_menu[] = {
            { "Copy text",   0, Copy_Text_CB,  (void*)this },
            { "Copy image",   0, Copy_Image_CB,  (void*)this },
//          { "Paste",  0, Paste_CB, (void*)this },
            { 0 }
        };
        rclick_menu[0].labelsize(LSIZE);
        rclick_menu[1].labelsize(LSIZE);
        const Fl_Menu_Item *m = rclick_menu->popup(Fl::event_x(),
            Fl::event_y(), 0, 0, 0);
        if ( m ) {
          m->do_callback(0, m->user_data());
          return 1; // (tells caller we handled this event)
        }
      }
      break;
    case FL_RELEASE: // RIGHT MOUSE RELEASED? Mask it from base class
      if ( Fl::event_button() == FL_RIGHT_MOUSE ) {
        return 1;          // (tells caller we handled this event)
      }
      break;
    }
    return Fl_Double_Window::handle(e); // let base class handle other events
  }
  brightplot(int x, int y, int w, int h, const char *l=0):
    Fl_Double_Window(x, y, w, h, l) { screencopy = false;}
};


/**
  plot brightness vs. tilt
*/
void brightplot::draw()
{
  Fl_Double_Window::draw();

  fl_font(0, 12);
  int left = 40;
  int right = w()-20;
  int bot = h()-40;
  int top = 20;
  int xp, yp, xn, yn;

  fl_rectf(0, 0, w(), h(), Fl_Color(0xc0f0ff00)); // margin light blue

  fl_rectf(left, top, right-left, bot-top, FL_WHITE); // interior white

  // Grid lines

  fl_color(0xE0E0E000); // light gray

  for (int i = 0; i <= 100; i += 10) {
    yp = bot + 0.5 + i*(top-bot)/100.;
    fl_line(left, yp, right, yp);
  }
  if (maxtilt > 179.) {
    for (int j = 0; j <= 360; j += 30) {
      xp = left + 0.5 + j*(right - left)/360.;
      fl_line(xp, bot, xp, top);
    }
  }
  else if (maxtilt > 0.) {
    int k = maxtilt + 0.5;
    for (int j = -k; j <= k; j += 5) {
      xp = left + 0.5 + (j+k)*(right - left)/(2*k);
      fl_line(xp, bot, xp, top);
    }
  }
  // Calculate left and right averages of a
  double leftavg = 0;
  for (int i = 0; i < 3; i++)
    leftavg += brightness[i];
  leftavg /= 3.;
  double rightavg = 0.;
  for (int i = 0; i < 3; i++)
    rightavg += brightness[(NFRAMES-1)*6 + i];
  rightavg /= 3.;

  // legend block

  fl_font(0, 9);
  int legleft, legright, legbot, legtop;
  // default: upper right
  legleft = right - 90;
  legright = right;
  legtop = top;
  legbot = top + 60;
  if (leftavg > 50. && leftavg > rightavg) {
    // lower left
    legleft = left;
    legright = left + 90;
    legbot = bot;
    legtop = bot - 60;
  }
  else if (rightavg > 50. && rightavg > leftavg) {
    // lower right
    legleft = right - 90;
    legright = right;
    legbot = bot;
    legtop = bot - 60;
  }
  else if (leftavg < 50. && leftavg < rightavg) {
    // upper left
    legleft = left;
    legright = left + 90;
    legtop = top;
    legbot = top + 60;
  }

//fl_rectf(left, top, 90, 80, FL_WHITE);
//fl_rect(left, top, 90, 80, FL_BLACK);
  fl_rectf(legleft, legtop, 90, 60, FL_WHITE);
  fl_rect(legleft, legtop, 90, 60, FL_BLACK);
  
  const char *labs[3] = { "ISO", "COS", "SC2"};
  int cols[6] = {FL_BLACK, FL_RED, 0x00D00000,
                 FL_BLACK, FL_RED, 0x00D00000}; // 0x00D00000 is green
  char dashes[3];
  dashes[0] = char(6);
  dashes[1] = char(3);
  dashes[2] = 0;

  for (int i = 0; i < 3; i++) {
    fl_color(cols[i]);
    fl_line_style(FL_SOLID, 0);
    fl_line(legleft+5, legtop+25+12*i, legleft+30, legtop+25+12*i);
    fl_line_style(FL_DASH, 0);
    fl_line(legleft+35, legtop+25+12*i, legleft+55, legtop+25+12*i);
    fl_color(0);
    xp = legleft+60;
    yp = legtop+30+12*i;
    fl_draw(labs[i], xp, yp);
  }
  xp = legleft+10;
  yp = legtop+17;

  fl_font(0, 12);

  fl_draw("all", xp, yp);
  xp = legleft+35;
  fl_draw("table", xp, yp);

  // Draw curves

  for (int i = 0; i < 6; i++) {
    xp = left;
    yp = bot + 0.5 + brightness[i]*(top-bot)/100.;
    fl_color(cols[i]);
    if (i > 2)
      fl_line_style(FL_DASH, 0);
    else
      fl_line_style(FL_SOLID, 0);
    if (maxtilt == 0.) {
      fl_line(left, yp, right, yp);
    }
    else {
      for (int j = 1; j < NFRAMES; j++) {
        xn = left + 0.5 + j*(right - left)/(NFRAMES-1);
        yn = bot + 0.5 + brightness[j*6+i]*(top-bot)/100.;
        fl_line(xp, yp, xn, yn);
        xp = xn;
        yp = yn;
      }
    }
    fl_line_style(FL_SOLID, 0);
  }

  // frame

  fl_color(FL_BLACK);
  fl_line_style(FL_SOLID | FL_CAP_FLAT, 0);
  fl_line(left, bot, left, top);
  fl_line(left, top, right, top);
  fl_line(right, top, right, bot);
  fl_line(right, bot, left, bot);

  // y axis tic marks

  fl_font(0, 9);

  for (int i = 0; i <= 100; i += 10) {
    yp = bot + 0.5 + i*(top-bot)/100.;
    fl_line(left-5, yp, left, yp);
  }

  // y axis labels

  int wid, ht;
  for (int i = 0; i <= 100; i += 20) {
    yp = bot + 0.5 + i*(top-bot)/100.;
    char s[20];
    snprintf(s,sizeof(s), "%d", i);
    fl_measure(s, wid, ht);
    fl_draw(s, left-wid-7, yp+ht/3);
  }


//yp = (bot + top)/2;
  // x axis

  if (maxtilt > 179.) { // full circle: every 30 deg.
    // x tics
    for (int j = 0; j <= 360; j += 30) {
      xp = left + 0.5 + j*(right - left)/360.;
      fl_line(xp, bot, xp, bot+5);
    }

    // x axis labels

    for (int j = 0; j <= 360; j += 60) {
      xp = left + 0.5 + j*(right - left)/360.;
      int tilt = j-180;
      char s[20];
      snprintf(s,sizeof(s), "%d", tilt);
      fl_measure(s, wid, ht);
      fl_draw(s, xp-wid/2, bot+3+ht);
    }
  }
  else if (maxtilt > 0.) { // not full circle: every 5 deg.
    // x axis tics

    int k = maxtilt + 0.5;
    for (int j = -k; j <= k; j += 5) {
      xp = left + 0.5 + (j+k)*(right - left)/(2*k);
      fl_line(xp, bot, xp, bot+5);
    }

    // x axis labels

    for (int j = -k; j < 2*k; j += 10) {
      xp = left + 0.5 + (j+k)*(right - left)/(2*k);
      char s[20];
      snprintf(s,sizeof(s), "%d", j);
      fl_measure(s, wid, ht);
      fl_draw(s, xp-wid/2, bot+3+ht);
    }
  }

  // x axis legend

  fl_font(0, 12);
  xp = (left + right)/2 - 25;
  yp = bot+30;
  fl_draw("Tilt, deg.", xp, yp);

  // y axis legend

  xp = left-25;
  yp = (bot + top)/2+35;
  fl_draw(90, "Brightness, %", xp, yp);
  if (screencopy) {
    Fl::check(); // let everything redraw
#if defined(WIN32)
    keybd_event(VK_MENU, 0, 0, 0); // press Alt Key
    keybd_event(44, 0, 0, 0); // press Print Scrn key
    keybd_event(44, 0, 2, 0); // release Print Scrn key
    keybd_event(VK_MENU, 0, 2, 0); // release Alt Key
#elif defined(MACOS)
    system("screencapture -c -x -R0,0,400,300");
#else
    system("import -window root -crop 400x300+0+0 png:- | xclip -selection clipboard -t image/png");
#endif
    screencopy = false;
  }
}

/**
  Callback for graph button
*/
static void graph_btn_CB(Fl_Widget *o)
{
  brightplot bp(win->x()+50, win->y()+50, 400, 300, title);
  bp.resizable(&bp);
  bp.show();
  while (bp.shown()) {
    if (screencopy)
      bp.redraw();
    Fl::wait();
  }
}

static Fl_Pixmap *play_img = NULL;  // image for the play button
static Fl_Pixmap *pause_img = NULL; // image for the pause button
static Fl_Pixmap *saveas_img = NULL;
static Fl_Pixmap *savemult_img = NULL;
static Fl_Image *saveas_disabled_img = NULL;
static Fl_Pixmap *graph_img = NULL;
static Fl_Image *graph_disabled_img = NULL;

/**
  Callback for Play/Pause button
*/
static void playPause_CB(Fl_Widget *o)
{
  Fl_Image_Button *p = (Fl_Image_Button *) o;
  playing = !playing;
  if (playing) {
    if (play_img != NULL) p->up_image(pause_img);
    ShowNextImage_CB(NULL);
    if (pfns != NULL) {
      pfns->deactivate();
    }
    if (psaveJPG != NULL) {
//    psaveJPG->deactivate();
//    psaveJPG->up_image(saveas_disabled_img);
      psaveJPG->up_image(savemult_img);
    }
/*
    if (pgraph != NULL) {
      pgraph->deactivate();
      pgraph->up_image(graph_disabled_img);
    }
*/
  }
  else {
    if (pause_img != NULL) p->up_image(play_img);
    if (pfns != NULL) {
      pfns->activate();
//    pfns->set_visible();
    }
    if (psaveJPG != NULL) {
      psaveJPG->activate();
      psaveJPG->up_image(saveas_img);
//    psaveJPG->set_visible();
    }
/*
    if (pgraph != NULL) {
      pgraph->activate();
      pgraph->up_image(graph_img);
    }
*/
  }
}

static Fl_Image_Button *play_pause_btn = NULL;

/**
  Handle window close event
*/
static void windowCB(Fl_Widget *o, void *userdata)
{
  // Catch the escape key and ignore it
  if ((Fl::event() == FL_KEYDOWN || Fl::event() == FL_SHORTCUT)
      && Fl::event_key() == FL_Escape) {
    return; // ignore ESC
  }
  else { // but allow close button
    interrupt = true;
    if (rendering)
      return;
    Fl_Window *w = (Fl_Window *) o;
    w->hide(); // hiding the main window ends the Fl::run() loop
  }
}

/**
 Callback for light model chooser
*/
void ltModChooser_CB(Fl_Widget *, void *v) {
  model = (intptr_t) v;
  if (model < 0) model = 0;
  model %= 4;

//Fl_Preferences app(Fl_Preferences::USER, "GemCad", "GemRay/preferences");
  Fl_Preferences app(Fl_Preferences::USER, "GemCad", "GemRay");
  Fl_Preferences reg(app, "Defaults");
  reg.set((const char *) "lighting_model", model);

  bool firsttime = true;
  for (int i = 0; i < NFRAMES; i++) {
    if (img[i] != NULL) {
      delete img[i];
      img[i] = NULL;
    }
    img[i] = new Fl_RGB_Image(bits[model][i], pix, pix);
    if (firsttime && img[i] == NULL) {
      fl_alert("Out of memory\n");
      firsttime = false;
    }
  }
  if (!playing) ShowNextImage_CB((void *)1); // redisplay current frame
}

/**
  The munu items for the light model chooser
*/
Fl_Menu_Item ltModChoosers[] = {
  {"RND",0,ltModChooser_CB,(void*)0},
  {"SC2",0,ltModChooser_CB,(void*)1},
  {"COS",0,ltModChooser_CB,(void*)2},
  {"ISO",0,ltModChooser_CB,(void*)3},
  {0}
};

/**
  Main control run raytrace for optimization
*/
int oengine(Stone *stone, const char *filename, double *wtfac, double crngrad,
  unsigned int kolor, unsigned int bkgkolor,
  unsigned int leakkolor, unsigned int headkolor,
  double gamma, double gain,
  double ri, double cod, double xhead, double xzeye,
  double xmaxtilt, double xtiltinc, double xelev,
  int sz,
  double xcrn, double xpav, double *objective)
{
  load_engine_defaults();
  static bool out_of_threads = false;
  cod = 0.; // No dispersion

  if (stone == NULL) {
    fl_alert("Bad stone file");
    return 1;
  }
  head = xhead;
  zeye = xzeye;
  maxtilt = xmaxtilt;
  tiltinc = xtiltinc;
  elev = xelev;
  pix = sz;
  crn = xcrn;
  pav = xpav;

  interrupt = false;
  rendering = false;
  playing = false;

  if (maxtilt > 180.) maxtilt = 180.;
  NFRAMES = 1 + 2 * (int)(maxtilt/tiltinc + 0.5);
  if (NFRAMES < 1) NFRAMES = 1;
  xframe = NFRAMES-1;

//int x = -1;
//int y = -1;

  brightness = new double [NFRAMES*6];

  // allocate memory for animation
  // TODO: calculate maximum image size from available memory
  try {
    for (int i = 0; i < 4; i++)  {
      bits[i] = NULL;
      bits[i] = new uchar*[NFRAMES];
      for (int j = 0; j < NFRAMES; j++) {
        bits[i][j] = NULL;
        bits[i][j] = new uchar[3L*pix*pix];
        if (bits[i][j] == NULL) {
          fl_alert("Not enough memory.\nTry reducing image size.");
          return 1;
        }
      }
    }
  }
  catch (std::bad_alloc) {
    for (int i = 0; i < 4; i++)  {
      if (bits[i] != NULL) {
        for (int j = 0; j < NFRAMES; j++) {
          if (bits[i][j] != NULL)
            delete[] bits[i][j];
        }
        delete[] bits[i];
      }
    }
    fl_alert("Not enough memory.\nTry reducing image size.");
    return 1;
  }

  nthreads = out_of_threads ? 1 : getNumberOfCores(); // Count processor cores;
 
  Raytracer *rt;
  srand(1); // reseed random number generator so "random" light matches
  rt = new Raytracer(stone, ri, cod);

  xsym = stone->checksym(-1, 1, 1);
  ysym = stone->checksym(1, -1, 1);
  xandysym = xsym && ysym;
  sym4m = xandysym && stone->checkxysym();
  
  rendering = true;
  //
  // Initialize and run the background threads
  //
  FrameRenderer *frame_renderer = NULL;
  nfinished = 0;
  if (nthreads > 1) {
    // The nthreads-1 is because the main thread counts as a thread
    frame_renderer = new FrameRenderer[nthreads-1]; 
    Fl::lock(); // main thread must lock() to initialize fltk threading
    for (int i = 0; i < nthreads-1; i++) {
      FrameRenderer *p = frame_renderer + i;
      p->init(i+1, rt, pix, pix, head, zeye, kolor, bkgkolor, leakkolor, headkolor,
          gamma, gain,
          crn, pav);
      int ret = fl_create_thread(ray_thread[i], run_bkg_thread,
          (void *) (frame_renderer + i));
      if (ret) {
        fl_alert("Cannot create thread: error %d\n", ret);
        nthreads = 1; // TODO: doesn't clean up properly
        out_of_threads = true;
        break;
      }
    }
  }
  //
  // This here main thread manages the FLTK stuff
  //
  for (int i = 0; i < NFRAMES; i += nthreads) {
    if (interrupt)
      break;
    double b0, b1, b2, b3;
    if (elev != 0.) {
      b0 = 0.;
      b1 = cos(elev*M_PI/180.); // use 49 for 41 deg pavilion, 72 for tritetra
      b2 = -sin(elev*M_PI/180.);
      b3 = (NFRAMES-1.)/2. - i;
      b3 *= tiltinc;
    }
    else {
      if (sym4m) {
        if (i <= (NFRAMES-1)/2) { // tilt to N
          b0 = 1.;
          b1 = 0.;
          b2 = 0.;
          b3 = i-(NFRAMES-1.)/2.;
          b3 *= tiltinc;
        }
        else { // tilt to SE
          b0 = sqrt(2.)/2.;
          b1 = b0;
          b2 = 0.;
          b3 = i-(NFRAMES-1.)/2.;
          b3 *= tiltinc;
        }
      }
      else if (xandysym) {
        if (i <= (NFRAMES-1)/2) { // tilt to N
          b0 = 1.;
          b1 = 0.;
          b2 = 0.;
          b3 = i-(NFRAMES-1.)/2.;
          b3 *= tiltinc;
        }
        else { // tilt to E
          b0 = 0.;
          b1 = 1.;
          b2 = 0.;
          b3 = i-(NFRAMES-1.)/2.;
          b3 *= tiltinc;
        }
      }
      else { // tilt N to S
        b0 = 1.;
        b1 = 0.;
        b2 = 0.;
        b3 = i-(NFRAMES-1.)/2.;
        b3 *= tiltinc;
      }
    }
    b0 = -b0;
    rt->render(pix, pix, head, zeye, kolor, bkgkolor, leakkolor, headkolor,
       gamma, gain,
       crn, pav, b0, b1, b2, b3, brightness+i*6,
       bits[0][i], bits[1][i], bits[2][i], bits[3][i]);
  }
  timeval t1;
  gettimeofday(&t1, NULL); // start watchdog timer
  while (nfinished < nthreads-1) {
    timeval t2;
    gettimeofday(&t2, NULL); // check timer
    double elapsedTime = (t2.tv_sec - t1.tv_sec);
    elapsedTime += (t2.tv_usec - t1.tv_usec) / 1.e6;
    if (elapsedTime > 30.) { // 10 for testing
      printf("background thread didn't finish\n");
      printf("%d of %d background threads finished\n", nfinished, nthreads-1);
      break;
    }
  }
  if (nthreads > 1) {
    delete[] frame_renderer;
  }
  // calculate brightness metric
  *objective = 0.;
  if (!interrupt) {
    double wtsum = 0.;
    for (int j = 0; j < NFRAMES; j++) {
      if (j == (NFRAMES-1)/2) { // face up, no tilt
        for (int k = 0; k < 6; k++) {
          *objective += wtfac[k]*brightness[j*6+k];
          wtsum += wtfac[k];
        }
      }
      else { // tilted
        for (int k = 0; k < 6; k++) {
          *objective += wtfac[k+6]*brightness[j*6+k];
          wtsum += wtfac[k+6];
        }
      }
    }
    crngrad *= xcrn;
    if (crngrad > 100.) crngrad = 100.;
    *objective += wtfac[12] * crngrad;
    wtsum += wtfac[12];
    if (wtsum != 0.)
      *objective /= wtsum;
  }
  rendering = false;
  delete rt;

  if (brightness)
    delete[] brightness;
  for (int i = 0; i < 4; i++)  {
    for (int j = 0; j < NFRAMES; j++)
      delete[] bits[i][j];
    delete[] bits[i];
  }
  return 0;
}

/**
  Main control to run raytrace and animation
*/
int engine(int xx, int yy, Stone *stone, const char *filename,
  unsigned int kolor, unsigned int bkgkolor,
  unsigned int leakkolor, unsigned int headkolor,
  double gamma, double gain,
  double ri, double cod, double xhead, double xzeye,
  double xmaxtilt, double xtiltinc, double xelev,
  int sz,
  double xcrn, double xpav, double *objective)
{
  load_engine_defaults();
  static bool out_of_threads = false;
  if (stone == NULL) {
    fl_alert("Bad stone file");
    return 1;
  }
  head = xhead;
  zeye = xzeye;
  maxtilt = xmaxtilt;
  tiltinc = xtiltinc;
  elev = xelev;
  pix = sz;
  crn = xcrn;
  pav = xpav;

  interrupt = false;
  rendering = false;
  playing = false;

  if (maxtilt > 180.) maxtilt = 180.;
  NFRAMES = 1 + 2 * (int)(maxtilt/tiltinc + 0.5);
  if (NFRAMES < 1) NFRAMES = 1;
  xframe = NFRAMES-1;

  timeval t1;
  gettimeofday(&t1, NULL); // start timer
//int x = -1;
//int y = -1;

  brightness = new double [NFRAMES*6];

  // allocate memory for animation
  // TODO: calculate maximum image size from available memory
  try {
    for (int i = 0; i < 4; i++)  {
      bits[i] = NULL;
      bits[i] = new uchar*[NFRAMES];
      for (int j = 0; j < NFRAMES; j++) {
        bits[i][j] = NULL;
        bits[i][j] = new uchar[3L*pix*pix];
      }
    }
  }
  catch (std::bad_alloc) {
    for (int i = 0; i < 4; i++)  {
      if (bits[i] != NULL) {
        for (int j = 0; j < NFRAMES; j++) {
          if (bits[i][j] != NULL)
            delete[] bits[i][j];
        }
        delete[] bits[i];
      }
    }
    fl_alert("Not enough memory.\nTry reducing image size.");
    return 1;
  }

  Fl_Box box(0, 0, pix, pix);

  nthreads = out_of_threads ? 1 : getNumberOfCores(); // Count processor cores;
  std::cout << nthreads << " core" << ((nthreads == 1) ? "" : "s") << std::endl;

  Raytracer *rt;
  srand(1); // reseed random number generator so "random" light matches
  rt = new Raytracer(stone, ri, cod);

  xsym = stone->checksym(-1, 1, 1);
  ysym = stone->checksym(1, -1, 1);
  xandysym = xsym && ysym;
  sym4m = xandysym && stone->checkxysym();
  if (sym4m) {
    std::cout << "4- or 8-fold, mirror-image symmetry" << std::endl;
  }
  else {
    if (ysym)
      std::cout << "Symmetry detected:    y" << std::endl;
    if (xsym)
      std::cout << "Symmetry detected: -x   x" << std::endl;
    if (ysym)
      std::cout << "                     -y" << std::endl;
  }
  if (!sym4m && xandysym)
    std::cout << "xysym" << std::endl;
  
  int w = (pix < 284) ? 284 : pix; // Minimum width for widgets
  win = new Draggable_Window(xx, yy, w, pix+CTRL_HT, "GemRay Test");
  win->color(bkgkolor << 8);
  Fl_Pixmap gemrayIcon(gemray150x150_xpm);
  Fl_RGB_Image *gemrayIconRGB = new Fl_RGB_Image(&gemrayIcon);
  win->icon(gemrayIconRGB);
  const char *base = fl_filename_name(filename);
  if (base != NULL) {
    if (xcrn > 1.00001 || xcrn < 0.99999 || xpav > 1.00001 || xpav < 0.99999)
      snprintf(title,sizeof(title), "%s %.4fC %.4fP", base, xcrn, xpav);
    else
      strcpy(title, base);
    win->label(title);
  }
  win->callback(windowCB);

  Fl_Box *border = new Fl_Box(0, pix, w, CTRL_HT);
  border->box(FL_THIN_DOWN_BOX);
  border->color(BKG_COLOR);
  char str[256];
  if (maxtilt == 180.)
    snprintf(str,sizeof(str), "360\u00B0 Rotation Animation");
  else if (elev == 0.)
    snprintf(str,sizeof(str), "\u00B1%.0f\u00B0 Tilt Animation", maxtilt);
  else
    snprintf(str,sizeof(str), "\u00B1%.0f\u00B0 Rotation Animation", maxtilt);
  Fl_Progress *progress;
  progress = new Fl_Progress(8, pix+(CTRL_HT-PROG_HT)/2, w-16, PROG_HT, str);
  progress->labelsize(LSIZE);
//progress->color(BKG_COLOR, FL_BLUE);
//progress->color(BKG_COLOR, FL_GREEN);
  progress->color(BKG_COLOR, Fl_Color(0x00AA0000)); // darker green
  progress->minimum(0.);
  progress->maximum(1.);
  progress->value(0.);
  progress->box(FL_UP_BOX); // a bit fancier

  grp = new Fl_Group((w-pix)/2, 0, pix, pix);
  grp->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);

  img = new Fl_RGB_Image *[NFRAMES]; // 5 = # of lighting models
  for (int i = 0; i < NFRAMES; i++)
    img[i] = NULL; // threads may finish out of order; prevent delete
  Fl_RGB_Image *img0 = NULL;
  rendering = true;
  progress->value(0.);
  Fl::check(); // allow Fl to get rid of file selector and update progress
  win->show();
  //
  // Initialize and run the background threads
  //
  FrameRenderer *frame_renderer = NULL;
  nfinished = 0;
  if (nthreads > 1) {
    // The nthreads-1 is because the main thread counts as a thread
    frame_renderer = new FrameRenderer[nthreads-1]; 
    Fl::lock(); // main thread must lock() to initialize fltk threading
    for (int i = 0; i < nthreads-1; i++) {
      FrameRenderer *p = frame_renderer + i;
      p->init(i+1, rt, pix, pix, head, zeye, kolor, bkgkolor, leakkolor, headkolor,
          gamma, gain,
          crn, pav);
      int ret = fl_create_thread(ray_thread[i], run_bkg_thread,
          (void *) (frame_renderer + i));
      if (ret) {
        fl_alert("Cannot create thread: error %d\n", ret);
        nthreads = 1; // TODO: doesn't clean up properly
        out_of_threads = true;
        break;
      }
    }
  }
  //
  // This here main thread manages the FLTK stuff
  //
  for (int i = 0; i < NFRAMES; i += nthreads) {
    if (interrupt)
      break;
    double b0, b1, b2, b3;
    if (elev != 0.) {
      b0 = 0.;
      b1 = cos(elev*M_PI/180.); // use 49 for 41 deg pavilion, 72 for tritetra
      b2 = -sin(elev*M_PI/180.);
      b3 = (NFRAMES-1.)/2. - i;
      b3 *= tiltinc;
    }
    else {
      if (sym4m) {
        if (i <= (NFRAMES-1)/2) { // tilt to N
          b0 = 1.;
          b1 = 0.;
          b2 = 0.;
          b3 = i-(NFRAMES-1.)/2.;
          b3 *= tiltinc;
        }
        else { // tilt to SE
          b0 = sqrt(2.)/2.;
          b1 = b0;
          b2 = 0.;
          b3 = i-(NFRAMES-1.)/2.;
          b3 *= tiltinc;
        }
      }
      else if (xandysym) {
        if (i <= (NFRAMES-1)/2) { // tilt to N
          b0 = 1.;
          b1 = 0.;
          b2 = 0.;
          b3 = i-(NFRAMES-1.)/2.;
          b3 *= tiltinc;
        }
        else { // tilt to E
          b0 = 0.;
          b1 = 1.;
          b2 = 0.;
          b3 = i-(NFRAMES-1.)/2.;
          b3 *= tiltinc;
        }
      }
      else { // tilt N to S
        b0 = 1.;
        b1 = 0.;
        b2 = 0.;
        b3 = i-(NFRAMES-1.)/2.;
        b3 *= tiltinc;
      }
    }
    b0 = -b0;
    rt->render(pix, pix, head, zeye, kolor, bkgkolor, leakkolor, headkolor,
       gamma, gain,
       crn, pav, b0, b1, b2, b3, brightness+i*6,
       bits[0][i], bits[1][i], bits[2][i], bits[3][i]);
    if (img0 != NULL) {
      delete img0;
      img0 = NULL;
    }
    img0 = new Fl_RGB_Image(bits[model][i], pix, pix);
    grp->image(img0);
    progress->value((float) i / (float) NFRAMES);
    win->redraw();
    Fl::check(); // Let Fl update
  }
  if (img0 != NULL) {
    delete img0;
    img0 = NULL;
  }
  while (nfinished < nthreads-1)
    Fl::check(); // Let Fl update
  if (nthreads > 1) {
    if (interrupt)
      win->hide();
    delete[] frame_renderer;
  }
  grp->end();
  printf("loading images\n");
  // load images
  bool firsttime = true;
  for (int i = 0; i < NFRAMES; i++) {
    if (img[i] != NULL) {
      delete img[i];
      img[i] = NULL;
    }
    img[i] = new Fl_RGB_Image(bits[model][i], pix, pix);
    if (firsttime && img[i] == NULL) {
      fl_alert("Out of memory\n");
      firsttime = false;
    }
  }
//progress.value(1.);
  // calculate brightness metric
  *objective = 0.;
  if (!interrupt) {
    for (int j = 0; j < NFRAMES; j++) {
      *objective += brightness[j*6+2] + brightness[j*6+5]; // sc2 + table sc2
    }
    *objective /= 2.*NFRAMES;
  }
  rendering = false;
  delete rt;

  timeval t2;
  gettimeofday(&t2, NULL); // stop timer
  double elapsedTime = (t2.tv_sec - t1.tv_sec);
  elapsedTime += (t2.tv_usec - t1.tv_usec) / 1.e6;
  fprintf(stderr, "%.2f sec\n", elapsedTime);
  fprintf(stderr, "%.1f fps\n", NFRAMES/elapsedTime);
//win->resize(win->x(), win->y(), pix, pix);

/*
  Fl_Button *playPause = new Fl_Button(5, pix+7, 30, 30, "@||");
  playPause->callback(playPauseCB);
  playPause->labelsize(18); // if smaller than 18, the two bars join together
  playPause->tooltip("Play/Pause the animation");
*/

  play_img = new Fl_Pixmap(play_xpm);
  pause_img = new Fl_Pixmap(pause_xpm);

  play_pause_btn = new Fl_Image_Button(5, pix+7, 30, 30);
  play_pause_btn->up_image(pause_img);
  play_pause_btn->callback(playPause_CB);
  play_pause_btn->tooltip("Play/Pause the animation");

//Fl_Menu_::textsize(LSIZE);

  //
  // The frame number spinner
  //
  Fl_Spinner *fnumSpin = new Fl_Spinner(45, pix+17, 38, PROG_HT, "Frame");
  fnumSpin->labelsize(LSIZE);
  fnumSpin->textsize(LSIZE);
  fnumSpin->minimum(0);
  fnumSpin->maximum(NFRAMES-1);
  fnumSpin->step(1);
  fnumSpin->value(xframe);
  fnumSpin->align(Fl_Align(FL_ALIGN_TOP | FL_ALIGN_LEFT));
  fnumSpin->callback(fnumSpinCB);
  fnumSpin->tooltip("Select the frame number");
  fnumSpin->deactivate();
//fnumSpin->clear_visible();
  fnumSpinCB(fnumSpin); // sync frame number with control

  //
  // The frame rate fps (frames per second) spinner
  //
  Fl_Spinner *fpsSpin = new Fl_Spinner(92, pix+17, 38, PROG_HT, "FPS");
  fpsSpin->labelsize(LSIZE);
  fpsSpin->textsize(LSIZE);
  fpsSpin->minimum(1);
  fpsSpin->maximum(120);
  fpsSpin->step(1);
  fpsSpin->value(fps);
  fpsSpin->align(Fl_Align(FL_ALIGN_TOP | FL_ALIGN_LEFT));
  fpsSpin->callback(fpsSpinCB);
  fpsSpin->tooltip("Change the animation speed\n(frames per second).");

  //
  // The lighting model chooser
  //
  Fl_Choice *ltModChooser = new Fl_Choice(140, pix+17, 60, 20, "Light Model");
//ltModChooser->type(1);
  ltModChooser->box(FL_GTK_DOWN_BOX);
  ltModChooser->color(FL_WHITE);
  ltModChooser->labelsize(LSIZE);
  ltModChooser->textsize(LSIZE);
  ltModChooser->tooltip("Choose the lighing model");
  ltModChooser->menu(ltModChoosers);
  ltModChooser->align(Fl_Align(FL_ALIGN_TOP | FL_ALIGN_LEFT)); // testing
  ltModChooser->callback(ltModChooser_CB);
  ltModChooser->when(FL_WHEN_RELEASE|FL_WHEN_NOT_CHANGED);
  ltModChooser->value(model);

  //
  // The save as JPEG button
  //
  saveas_img = new Fl_Pixmap(saveas_xpm);
  savemult_img = new Fl_Pixmap(savemult_xpm);
  Fl_Image_Button *save_jpg_btn = new Fl_Image_Button(214, pix+7, 30, 30);
  saveas_disabled_img = saveas_img->copy();
  saveas_disabled_img->color_average(BKG_COLOR, 0.2); // gray version
//save_jpg_btn->up_image(saveas_disabled_img);
  save_jpg_btn->up_image(savemult_img);
//save_jpg_btn->down_image(saveas_img);
//save_jpg_btn->deactivate();
//save_jpg_btn->clear_visible();
  save_jpg_btn->callback(save_jpg_btn_CB);
  save_jpg_btn->tooltip("Save image(s) as jpeg file(s)");
  psaveJPG = save_jpg_btn;

  //
  // The graph button
  //
  graph_img = new Fl_Pixmap(graph_xpm);
  Fl_Image_Button *graph_btn = new Fl_Image_Button(244, pix+7, 30, 30);
  graph_disabled_img = graph_img->copy();
  graph_disabled_img->color_average(BKG_COLOR, 0.2); // gray version
  graph_btn->up_image(graph_img);
//graph_btn->up_image(graph_disabled_img);
//graph_btn->down_image(graph_img);
//graph_btn->deactivate();
//graph_btn->clear_visible();
  graph_btn->callback(graph_btn_CB);
  graph_btn->tooltip("Graph brightness vs. tilt.");
  pgraph = graph_btn;

  xframe = (maxtilt == 180.) ? 0 : NFRAMES-1;
  
  progress->hide();
  win->redraw();
  printf("Calling Fl::check()\n");
  Fl::check(); // Let Fl update
  printf("back from Fl::check()\n");
  printf("Starting animation loop\n");
  Fl::add_timeout(1./fps, ShowNextImage_CB); // Begin the animation loop
  int iret = 0;
  win->end();
  playing = true;
  Fl::focus(play_pause_btn);
  iret = Fl::run();
  if (interrupt)
    iret = 1;
  if (brightness)
    delete[] brightness;
  for (int i = 0; i < 4; i++)  {
    for (int j = 0; j < NFRAMES; j++)
      delete[] bits[i][j];
    delete[] bits[i];
  }
  for (int i = 0; i < NFRAMES; i++)
    if (img[i] != NULL)
      delete img[i];
  delete[] img;
  delete play_pause_btn;
  delete fpsSpin;
  delete fnumSpin;
  delete border;
  delete ltModChooser;
  delete save_jpg_btn;
  delete play_img;
  delete pause_img;
  delete saveas_img;
  delete savemult_img;
  delete saveas_disabled_img;
  delete gemrayIconRGB;
  delete win;
  win = NULL;
  return iret;
}
