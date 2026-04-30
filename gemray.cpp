//
// gemray.cpp (formerly called master.cpp)
//
// Version 1.2.0 30 Apr 2026 
// 
// By Robert W. Strickland
//
// This is an open source version of GemRayWin, Strickland's gemstone raytracing program. All of the
// registration key code has been removed.
//

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Input_Choice.H>
#include <FL/Fl_Float_Input.H>
#include <FL/Fl_Int_Input.H>
#include <FL/Fl_Color_Chooser.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Shared_Image.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Preferences.H>
#include <FL/Fl_XPM_Image.H> // for xpm button image
#include <FL/filename.H>

#undef FLTK_HELP

#ifdef FLTK_HELP
#include <FL/Fl_Help_Dialog.H> // for HTML help dialog
#include <FL/Fl_Image.H> // for xpm button image
#endif

#include <FL/Fl_Pixmap.H> // for xpm button image
#include "Fl_Image_Button.H" // for image button
#include <cstring>

#include "Numpad_Input.H" // Numpad_Input with numeric keypad popup

#include "stone.h"
#include "engine.h"
#include "time.h"         // for trial counter

//#include "XPM/gemray48x48.xpm"  // GemRay small icon (bigger version OK for FLTK 1.4)
#include "XPM/gemray150x150.xpm"// Larger GemRay icon image for about dialog
#include "XPM/palette.xpm"      // Color palette button icon
#include "XPM/fileopen.xpm"     // File Open button icon
#include "XPM/about.xpm"        // About button icon (i)
#include "XPM/help.xpm"         // help icon (?)
#include "XPM/go.xpm"           // Go button icon (check mark)
//#include "XPM/head.xpm"       // Head shadow icon
#include "XPM/gears.xpm"        // gears icon
#include "XPM/cancel.xpm"       // cancel icon (X)
#include "XPM/restart.xpm"      // restart icon (circular arrow)

static Fl_Double_Window *dlg = NULL; // The dialog window

#define DEGREE_LAB "\u00B0" // UTC8 degree sign (superscript circle)

// The parameters in the dialog

// Colors here are 24 bit RGB. FLTK colors are RGBA
static unsigned int stoneColor = 0xFF55FF;// stone color, amethyst
static unsigned int bkgColor = 0xFFFFFF;  // background color, white
static unsigned int leakColor = 0xFFFFFF; // leaked ray color, white
static unsigned int headColor = 0xBF9966; // leaked ray color, dark flesh
static int sz = 300;                      // image size in pixels
static float maxtilt = 30.;               // max tilt angle in degrees
static float tiltinc = 1.;                // tilt angle increment in degrees
static float elev = 0.;                   // rotation axis elevation
static float head = 10.;                  // head shadow 1/2 angle 
static float zeye = 1000.;                // eyepoint Z
static float crn = 1.0;                   // crown scale factor
static float pav = 1.0;                   // pavilion scale factor
static double crngrad;                    // tan(crown angle)*100
static char *filename = NULL;             // GemCad file name
static double cod = 0.0;                  // Coefficient of Dispersion
static double ri = 0.0;                   // Refractive Index
static double gamma = 1.0;                // gamma correction
static double gain = 1.0;                 // brightness gain
static Stone *stone = NULL;

static bool do_render = false;            // flag to start the renderer

// callback when user chooses from refractive index dropdown
static void ri_item_cb(Fl_Widget *, void *);

// Refractive Index dropdown menu table
#define NRI 23
static Fl_Menu_Item ri_choices[NRI+1] = {
  {"2.63 Moissanite"      , 0, ri_item_cb, (void*)  0},
  {"2.62 Rutile"          , 0, ri_item_cb, (void*)  1},
  {"2.42 Diamond"         , 0, ri_item_cb, (void*)  2},
  {"2.28 Wulfenite"       , 0, ri_item_cb, (void*)  3},
  {"2.16 Cubic Zirconia"  , 0, ri_item_cb, (void*)  4},
  {"2.03 GGG"             , 0, ri_item_cb, (void*)  5},
  {"1.88 Sphene"          , 0, ri_item_cb, (void*)  6},
  {"1.88 Demantoid"       , 0, ri_item_cb, (void*)  7},
  {"1.87 Andralite"       , 0, ri_item_cb, (void*)  8},
  {"1.83 YAG"             , 0, ri_item_cb, (void*)  9},
  {"1.81 Zircon"          , 0, ri_item_cb, (void*) 10},
  {"1.76 Corundum"        , 0, ri_item_cb, (void*) 11},
  {"1.72 Spinel"          , 0, ri_item_cb, (void*) 12},
  {"1.69 Tanzanite"       , 0, ri_item_cb, (void*) 13},
  {"1.65 Peridot"         , 0, ri_item_cb, (void*) 14},
  {"1.65 Dioptase"        , 0, ri_item_cb, (void*) 15},
  {"1.62 Tourmaline"      , 0, ri_item_cb, (void*) 16},
  {"1.61 Topaz"           , 0, ri_item_cb, (void*) 17},
  {"1.58 Beryl"           , 0, ri_item_cb, (void*) 18},
  {"1.54 Quartz"          , 0, ri_item_cb, (void*) 19},
  {"1.45 Opal"            , 0, ri_item_cb, (void*) 20},
  {"1.43 Fluorite"        , 0, ri_item_cb, (void*) 21},
  {"0 Get from GemCad file", 0, ri_item_cb, (void*) 22},
  {0}
};

// callback when user chooses from coefficient of dispersion dropdown
static void dch_item_cb(Fl_Widget *, void *);

// Coefficient of dispersion dropdown menu table
#define NDISP 14
static Fl_Menu_Item dispersion_choices[NDISP+1] = {
  {"0.280 Rutile"        , 0, dch_item_cb, (void*)  0},
  {"0.203 Wulfenite"     , 0, dch_item_cb, (void*)  1},
  {"0.104 Moissanite"    , 0, dch_item_cb, (void*)  2},
  {"0.060 Cubic Zirconia", 0, dch_item_cb, (void*)  3},
  {"0.057 Demantoid"     , 0, dch_item_cb, (void*)  4},
  {"0.057 Andralite"     , 0, dch_item_cb, (void*)  5},
  {"0.051 Sphene"        , 0, dch_item_cb, (void*)  6},
  {"0.044 Diamond"       , 0, dch_item_cb, (void*)  7},
  {"0.039 Zircon"        , 0, dch_item_cb, (void*)  8},
  {"0.038 GGG"           , 0, dch_item_cb, (void*)  9},
  {"0.036 Dioptase"      , 0, dch_item_cb, (void*) 10},
  {"0.030 Tanzanite"     , 0, dch_item_cb, (void*) 11},
  {"0.028 YAG"           , 0, dch_item_cb, (void*) 12},
  {"0 No Dispersion" , 0, dch_item_cb, (void*) 13},
  {0}
};

static Fl_Input_Choice *cod_inp_chc = NULL;

/**
  Get index of dispersion choice that matches text of refractive index choice
  When the user chooses a material from the dropdown box, if that material
  has a relatively high dispersion, the matching entry is automatically
  selected from the coefficeint of dispersion dropdown. If the user chooses
  a material that minimally dispersive (< 0.025 or so) or normally strongly
  coloered, the coefficient dispersion is set to zero. This is becuase 
  rendering with dispersion takes three times longer.
*/
int get_icod(int iri)
{
  const char *p = ri_choices[iri].text;
  p = strchr(p, ' '); // has number then space then mineral name; find space
  if (p == NULL)
    return NDISP-1;
  p++; // skip space
  for (int i = 0; i < NDISP-1; i++) { // look at RI choices
    const char *q = dispersion_choices[i].text;
    q = strchr(q, ' '); // find space
    if (q == NULL)
      return NDISP-1;
    q++; // space
    if (!strcmp(p, q))
      return i; // found match
  }
  return NDISP-1; // not found
}

/**
  Get index of dispersion choice that most nearly matches value of refractive
  index from file. If RI >= 1.8.
*/
double get_cod(double r_i)
{
  int iri;
  double cod = 0.;
  int iribest = 0;
  double dmin = 1.e10;
  double rbest = 0;
  for (iri = 0; iri < NRI; iri++) {
    double r = atof(ri_choices[iri].text);
    double d = fabs(r_i - r);
    if (d < dmin) {
      dmin = d;
      iribest = iri;
      rbest = r;
    }
  }
  int icod = get_icod(iribest);
  cod = atof(dispersion_choices[icod].text);
//fl_alert("chosen cod text %d = %s\n", icod, dispersion_choices[icod].text);
  return cod;
}

static Fl_Input_Choice *ri_inp_chc = NULL;

/**
  Callback for when user types in a refractive index
*/
static void ri_cb(Fl_Widget *o, void *u)
{
  Fl_Input_Choice *p = (Fl_Input_Choice *) o;
  int i = (intptr_t) u;
//printf("user data = %d\n", i);
  const char *s = p->value();
  sscanf(s, "%lf", &ri);
/*
  if (ri < 1.01) {
    ri = 1.01;
    char str[256];
    sprintf(str, "%.3f", ri);
    ri_inp_chc->value(str);
  }
  else if (ri > 10.) {
    ri = 10.;
    char str[256];
    sprintf(str, "%.3f", ri);
    ri_inp_chc->value(str);
  }
*/
//printf("value %.3f\n", ri);
}

/**
  Callback when user chooses from refractive index dropdown
*/
static void ri_item_cb(Fl_Widget *o, void *u)
{
  Fl_Menu_Item *p = (Fl_Menu_Item *) o;
  int n = (intptr_t) u; // get item number from user data
  ri_inp_chc->value(n); const char *s = ri_choices[n].text;
  sscanf(s, "%lf", &ri); // parse the refractive index
  n = get_icod(n); // get the index of the corresponding coef. of dispersion
  cod_inp_chc->value(n); // set the coef. of dispersion dropdown
  s = dispersion_choices[n].text;
  sscanf(s, "%lf", &cod); // set the coef. of dispersion
}

/**
  Callback for when user types in a coefficient of dispersion
*/
static void dch_cb(Fl_Widget *o, void *u)
{
  Fl_Input_Choice *p = (Fl_Input_Choice *) o;
  int i = (intptr_t) u;
  const char *s = p->value(); // get string
  sscanf(s, "%lf", &cod); // convert from string to number
  // Range check
  if (cod < 0.) {
    cod = 0.;
    char str[256];
    snprintf(str,sizeof(str), "%.3f", cod);
    cod_inp_chc->value(str);
  }
  else if (cod > 10.) {
    cod = 10.;
    char str[256];
    snprintf(str,sizeof(str), "%.3f", cod);
    cod_inp_chc->value(str);
  }
}

/**
  Callback when user chooses from coefficient of dispersion dropdown
*/
static void dch_item_cb(Fl_Widget *o, void *u)
{
  Fl_Menu_Item *p = (Fl_Menu_Item *) o;
  int n = (intptr_t) u; // get the item number
  cod_inp_chc->value(n); // set the dropdown
  const char *s = dispersion_choices[n].text; // get string
  sscanf(s, "%lf", &cod); // converto from string to number
}

/**
  Color button callback
*/
static void color_cb(Fl_Widget *o, void *u)
{
//Fl_Button *btn = (Fl_Button *) o;
//const char *t = btn->label();
  const char *t = o->label();
  Fl_Box *box = (Fl_Box *) u;
  Fl_Preferences app(Fl_Preferences::USER, "GemCad", "GemRay");
  Fl_Preferences reg(app, "Defaults");
  //
  // Get the current color of the button. 
  // Fl colors are 32-bit index rgbi where i is the color index
  // (usually zero). But the fl_color_chooser wants r g b bytes separate,
  //  so shift and mask;
  Fl_Color col = box->color();
  uchar r = (col >> 24) & 0xFF;
  uchar g = (col >> 16) & 0xFF;
  uchar b = (col >>  8) & 0xFF;
  if (!fl_color_chooser(t, r, g, b, 0)) // run the color chooser
    return; // don't change if cancel button pressed
  col = ((((r << 8) | g) << 8) | b) << 8;
  box->color(col);   // set the button color
  const char *lbl = box->label();
/*
  if (r+g+b < 500)
    box->labelcolor(FL_WHITE);
  else
    box->labelcolor(FL_BLACK);
*/
  box->redraw();     // draw the button with the new color
  col = (col >> 8 & 0xFFFFFF); // convert from Fl_Color to 24-bit color
  int icol = col;
//Fl_Preferences app(Fl_Preferences::USER, "GemCad", "GemRay");
//Fl_Preferences reg(app, "Defaults");
  if (lbl) {
    if (*lbl == 'S') { // stone button
      stoneColor = col;
      reg.set("stoneColor", icol);
    }
    else if (*lbl == 'B') { // background button
      bkgColor = col;
      reg.set("bkgColor", icol);
    }
    else if (*lbl == 'L') { // background button
      leakColor = col;
      reg.set("leakColor", icol);
    }
    else if (*lbl == 'V') { // head shadow button
      headColor = col;
      reg.set("headColor", icol);
    }
  }
}

/**
  override of Fl_Box that handles mouse left click
*/
class ColorBox : public Fl_Box
{
public:
  ColorBox(int x, int y, int w, int h, const char *l = 0) :
      Fl_Box(x, y, w, h, l) {}
  int handle(int e) {
    if (e == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE)
      return 1;
    else if (e == FL_RELEASE && Fl::event_button() == FL_LEFT_MOUSE) {
      color_cb((Fl_Widget *) this, (void *) this);
      return 1;
    }
    return(Fl_Box::handle(e));
  }
};

static ColorBox *stone_color_box;

static Fl_Output *fileopen_output = NULL; // ptr to the text output widget

/**
  remove trailing .000 from float string
*/
void ftrunc(char *s)
{
  if (!strchr(s, '.')) return; // no decimal
  char *p = s + strlen(s) - 1;
  while (p > s && *p == '0')
    *p-- = '\0';
  if (p > s && *p == '.' || *p == ',')
    *p = '\0';
}

Numpad_Input *crn_inp;
Numpad_Input *pav_inp;
Fl_Output *crnoldang_out;
Fl_Output *pavoldang_out;
Numpad_Input *crnnewang_inp;
Numpad_Input *pavnewang_inp;
Fl_Output *obj_out;

void
update_pavcrn()
{
  char str[80];
  snprintf(str,sizeof(str), "%.4f", crn);
  crn_inp->value(str);
  snprintf(str,sizeof(str), "%.4f", pav);
  pav_inp->value(str);
}

void
update_objective(double objective)
{
  char s[80];
  if (objective == 0.)
    obj_out->value("");
  else {
    snprintf(s,sizeof(s), "%.2f", objective);
    obj_out->value(s);
  }
  obj_out->redraw();
}

void
update_newangs(double objective)
{
  char s[80];
  if (stone == NULL) {
    crnnewang_inp->value("");
    pavnewang_inp->value("");
    obj_out->value("");
  }
  else {
    double crnang = stone->getCrownAngle();
    if (crnang != 0. && fabs(crnang) != 90.) {
      crngrad = tan(crnang*M_PI/180.);
      crnang = atan(crn*crngrad)*180./M_PI;
      crngrad *= 100.;
      if (crngrad > 100.) crngrad = 100.;
    }
    snprintf(s,sizeof(s), "%.2f%s", crnang, DEGREE_LAB);
    crnnewang_inp->value(s);

    double pavang = stone->getPavilionAngle();
    if (pavang != 0. && fabs(pavang) != 90.) {
      pavang = atan(pav*tan(pavang*M_PI/180.))*180./M_PI;
    }
    snprintf(s,sizeof(s), "%.2f%s", pavang, DEGREE_LAB);
    pavnewang_inp->value(s);
    if (objective == 0.)
      obj_out->value("");
    else {
      snprintf(s,sizeof(s), "%.2f", objective);
      obj_out->value(s);
    }
  }
}

void
update_oldangs()
{
  char s[80];
  obj_out->value("");
  if (stone == NULL) {
    crnoldang_out->value("");
    crnnewang_inp->value("");
    pavnewang_inp->value("");
    pavoldang_out->value("");
  }
  else {
    double crnoldang = stone->getCrownAngle();
    snprintf(s,sizeof(s), "%.2f%s", crnoldang, DEGREE_LAB);
    crnoldang_out->value(s);
    crnnewang_inp->value(s);
    double pavoldang = stone->getPavilionAngle();
    snprintf(s,sizeof(s), "%.2f%s", pavoldang, DEGREE_LAB);
    pavoldang_out->value(s);
    pavnewang_inp->value(s);
  }
}

static double wtfac[13] =
   {1., 1., 1., 1., 1., 1., 1., 1., 1., 1, 1., 1., 0.};
static double pavmin = 0.1234;
static double pavmax = 1.5;
static double crnmin = 0.5;
static double crnmax = 1.5;
static double omaxtilt = 30.;
static double otiltinc = 10.;

void
init_wtfacs()
{
  for (int i = 0; i < 12; i++)
    wtfac[i] = 1.;
  wtfac[12] = 0.;
  pavmin = 0.1234;
  pavmax = 1.5;
  crnmin = 0.5;
  crnmax = 1.5;
  omaxtilt = 30.;
  otiltinc = 10.;
}

/**
  Objective function to be minimized by Nelder-Mead optimizer
*/
double
func(double *x)
{
  crn = x[0];
  pav = x[1];
  double penfac = 1.;
  if (pav < pavmin || pav > pavmax || crn < crnmin || crn > crnmax)
    penfac = 100.; // penalize unreasonably large changes
  
  double objective = 0.;
  int iret = oengine(stone, filename, wtfac, crngrad, 0xFFFFFF, 0xFFFFFF, 0xFF0000, 0x000000, gamma, gain, ri, cod,
      head, zeye, omaxtilt, otiltinc, 0., 100, crn, pav, &objective);
  if (objective > 1000.)
    fprintf(stderr, "objective func too high = %f\n", objective);

  printf("objective func = %f\n", objective);
  return penfac*(10000./objective); // Nelder-Mead minimizes instead of maximizes
}

void
update_relative_merit()
{
  double x[2];
  x[0] = crn;
  x[1] = pav;
  double y = func(x);
  if (y != 0.)
    update_objective(10000./y);
}

/**
  Open a Gemcad stone file
*/
void
open_gemcad_file(const char *fn)
{
  if (stone != NULL) {
    printf("Deleting stone\n");
    delete stone;
    printf("Deleted\n");
    stone = NULL;
  }
  stone = new Stone(2.);
  printf("Reading %s\n", fn);
  if (stone->openfile(fn) != 0) {
    if (stone != NULL) {
      delete stone;
      stone = NULL;
    }
    fl_alert("Cannot open %s", fn);
    return;
  }
  printf("Read\n");
  int ret = stone->check();
  if (ret != 0) fl_alert("stone check returned %d", ret);
  char out[80];
  ri = stone->getRefractiveIndex();
  cod = 0.;
  if (ri >= 1.8) {
    stoneColor = 0xFFFFFF;
    cod = get_cod(ri);
  }
  snprintf(out,sizeof(out), "%.3f", ri);
  ftrunc(out);
  strcat(out, " from GemCad file");
  ri_inp_chc->value(out);
  if (cod != 0.) {
    snprintf(out,sizeof(out), "%.3f", cod);
    cod_inp_chc->value(out);
  }
  else
    cod_inp_chc->value(NDISP-1);
  pav = 1.0;
  crn = 1.0;
  pav_inp->value("1.0000");
  crn_inp->value("1.0000");
  init_wtfacs();
  update_oldangs();
  if (stone == NULL)
    return;
  bool hastable = stone && stone->tablefacet() != -1;
  if (!hastable) { // no table, so zero table weights
    wtfac[3] = 0.;
    wtfac[4] = 0.;
    wtfac[5] = 0.;
    wtfac[9] = 0.;
    wtfac[10] = 0.;
    wtfac[11] = 0.;
  }
  update_relative_merit();
}

/**
  File name button callback
*/
static void fileopen_btn_cb(Fl_Widget *o, void *u)
{
/*
  Fl_Native_File_Chooser *fc;
  fc = new Fl_Native_File_Chooser(Fl_Native_File_Chooser::BROWSE_FILE,
      ".",                           // directory
      "GemCad files (*.gem)\nGemCad ASC files (*.asc)",  // pattern
      Fl_File_Chooser::SINGLE,       // chooser type
      "Choose GemCad file");         // title
  fc->show();
  while (fc->shown())
    Fl::wait(); // Wait for user to make selection
  if (fc->value() == NULL) {
    delete fc;
    return;
  }
  fc->hide();
  if (filename) {
    delete[] filename;
    filename = NULL;
  }
  filename = new char[strlen(fc->value()) + 1];
  strcpy(filename, fc->value());
  delete fc;
  if (fileopen_output) fileopen_output->value(filename);
*/
  // Create native chooser
  Fl_Native_File_Chooser native;
  native.title("Choose a GemCad file");
  native.type(Fl_Native_File_Chooser::BROWSE_FILE);
#ifdef WIN32
  native.filter("GemCad files: *.gem\t*.gem\nGemCad files: *.asc\t*.asc");
#else
  native.filter("GemCad files \t*.gem\nGemCad files \t*.asc");
#endif
  native.directory(".");
//native.preset_file(G_filename->value());
  // Show native chooser
  switch (native.show()) {
  case -1: // ERROR
    fl_alert("%s", native.errmsg());
    break;
  case  1: // CANCEL
    break;
  default: // PICKED FILE
    if (native.filename()) {
      if (filename != NULL) {
        delete[] filename;
        filename = NULL;
      }
      filename = new char[strlen(native.filename()) + 1];
      strcpy(filename, native.filename());
      if (fileopen_output) fileopen_output->value(filename);
      open_gemcad_file(filename);
      stone_color_box->color(stoneColor << 8 & 0xFFFFFF00);
      stone_color_box->redraw();
    }
    else {
      if (filename != NULL)
        delete[] filename;
      filename = NULL;
    }
    break;
  }
}

void
load_master_defaults()
{
//Fl_Preferences app(Fl_Preferences::USER, "GemCad", "GemRay/preferences");
  Fl_Preferences app(Fl_Preferences::USER, "GemCad", "GemRay");
  Fl_Preferences reg(app, "Defaults");
  reg.get((const char *) "image_size_pixels", sz, 300);
  int g100;
  reg.get((const char *) "gamma_times_100", g100, 100);
  gamma = g100/100.;
  reg.get((const char *) "gain_times_100", g100, 100);
  gain = g100/100.;
  int32_t col;
  reg.get((const char *) "bkgColor", col, 0xFFFFFF); // white default
  bkgColor = col;
  reg.get((const char *) "stoneColor", col, 0xFF55FF); // amethyst default
  stoneColor = col;
  reg.get((const char *) "leakColor", col, 0xFFFFFF); // white default
  leakColor = col;
  reg.get((const char *) "headColor", col, 0xBF9966); // dark flesh
  headColor = col;
}

enum INP_LIST {TILT_INP=1, ELEV_INP, HEAD_INP, ZEYE_INP, CRN_INP, PAV_INP,
    CRN_NEWANG_INP, PAV_NEWANG_INP};

/**
  Callback to get float value from the text widget.
*/
static void float_inp_cb(Fl_Widget *o, void *u)
{
  Numpad_Input *p = (Numpad_Input *) o;
  const char *v = p->value();
  float f;
  sscanf(v, "%f", &f);
  int j = (intptr_t) u;
  if (j == ELEV_INP) {
    if (f > 90.) f = 90.;
    if (f < -90.) f = -90.;
  }
  else if (j == TILT_INP) {
    if (f < 0.) f = 0.;
    if (f > 180.) f = 180.;
  }
  else if (j == HEAD_INP) {
    if (f < 0.) f = 0.;
    if (f > 90.) f = 90.;
  }
  else if (j == ZEYE_INP) {
    if (f < 1.) f = 1.;
    if (f > 1000.) f = 1000.;
  }
  char out[80];
  if (j == ELEV_INP) {
    if (f == 0.) {
      strcpy(out, "auto");
    }
    else {
      snprintf(out,sizeof(out), "%.2f", f);
      ftrunc(out);
      strcat(out, DEGREE_LAB);
    }
  }
  else if (j == TILT_INP || j == HEAD_INP ||
      j == CRN_NEWANG_INP || j == PAV_NEWANG_INP) {
    snprintf(out,sizeof(out), "%.2f", f);
    ftrunc(out);
    strcat(out, DEGREE_LAB);
  }
  else if (j == ZEYE_INP) {
    snprintf(out,sizeof(out), "%.1f", f);
    ftrunc(out);
  }
  else
    snprintf(out,sizeof(out), "%.4f", f);
  p->value(out);
  p->redraw();
  if (j == ELEV_INP)
    elev = f;
  else if (j == TILT_INP)
    maxtilt = f;
  else if (j == HEAD_INP) {
    head = f;
    update_relative_merit();
  }
  else if (j == ZEYE_INP) {
    zeye = f;
    update_relative_merit();
  }
  else if (j == CRN_INP) {
    crn = f;
    update_newangs(0);
    update_relative_merit();
  }
  else if (j == PAV_INP) {
    pav = f;
    update_newangs(0);
    update_relative_merit();
  }
  else if (j == PAV_NEWANG_INP) {
    double oldang = stone->getPavilionAngle();
    if (oldang == 0. || fabs(oldang) == 90.)
      pav = oldang;
    else
      pav = tan(f*M_PI/180.) / tan(oldang*M_PI/180.);
    update_newangs(0);
    snprintf(out,sizeof(out), "%.4f", pav);
    pav_inp->value(out);
    update_relative_merit();
  }
  else if (j == CRN_NEWANG_INP) {
    double oldang = stone->getCrownAngle();
    if (oldang == 0. || fabs(oldang) == 90.)
      crn = oldang;
    else
      crn = tan(f*M_PI/180.) / tan(oldang*M_PI/180.);
    update_newangs(0);
    snprintf(out,sizeof(out), "%.4f", crn);
    crn_inp->value(out);
    update_relative_merit();
  }
}

/**
  Callback to get maximum image size from the text input widget
*/
static void size_cb(Fl_Widget *o, void *u)
{
  Fl_Int_Input *p = (Fl_Int_Input *) o;
  const char *v = p->value();
  int i;
  sscanf(v, "%d", &i);
  // range check
  if (i < 16)
    i = 16;
  char out[80];
  snprintf(out,sizeof(out), "%d pixels", i);
  p->value(out);
  sz = i;
//Fl_Preferences app(Fl_Preferences::USER, "GemCad", "GemRay/preferences");
  Fl_Preferences app(Fl_Preferences::USER, "GemCad", "GemRay");
  Fl_Preferences reg(app, "Defaults");
  reg.set("image_size_pixels", sz);
}

/**
  Callback to get gain from the text input widget
*/
static void gain_cb(Fl_Widget *o, void *u)
{
  Fl_Float_Input *p = (Fl_Float_Input *) o;
  const char *v = p->value();
  double d;
  d = atof(v);
  // range check
  if (d < 0.5)
    d = 0.5;
  if (d > 2.0)
    d = 2.0;
  char out[80];
  snprintf(out,sizeof(out), "%.2f", d);
  p->value(out);
  gain = d;
//Fl_Preferences app(Fl_Preferences::USER, "GemCad", "GemRay/preferences");
  Fl_Preferences app(Fl_Preferences::USER, "GemCad", "GemRay");
  Fl_Preferences reg(app, "Defaults");
  int g100 = (int) (gain*100. + 0.5);
  reg.set("gain_times_100", g100);
}

/**
  Callback to get gamma from the text input widget
*/
static void gamma_cb(Fl_Widget *o, void *u)
{
  Fl_Float_Input *p = (Fl_Float_Input *) o;
  const char *v = p->value();
  double d;
  d = atof(v);
  // range check
  if (d < 0.2)
    d = 0.2;
  if (d > 5.0)
    d = 5.0;
  char out[80];
  snprintf(out,sizeof(out), "%.2f", d);
  p->value(out);
  gamma = d;
//Fl_Preferences app(Fl_Preferences::USER, "GemCad", "GemRay/preferences");
  Fl_Preferences app(Fl_Preferences::USER, "GemCad", "GemRay");
  Fl_Preferences reg(app, "Defaults");
  int g100 = (int) (gamma*100. + 0.5);
  reg.set("gamma_times_100", g100);
}

#ifdef FLTK_HELP
Fl_Help_Dialog *help_dlg = NULL;
#endif

bool optimizing = false;
bool interrupt = false;

/**
  Callback for main window close event or render button.
*/ 
static void render_btn_cb(Fl_Widget *o, void *u)
{
  // Catch the escape key and ignore it
  int i = (intptr_t) u;
  printf("render_btn_cb(%d)\n", i);
  if (i != 3) { // 3 is for forced call instead of callback
    if ((Fl::event() == FL_KEYDOWN || Fl::event() == FL_SHORTCUT)
        && Fl::event_key() == FL_Escape) {
      return; // ignore ESC
    }
  }
#ifdef FLTK_HELP
  if (help_dlg != NULL) {
    delete help_dlg;
    help_dlg = NULL;
  }
#endif
  if (i == 0 || i == 2) { // exit button or close widget button
    dlg->hide();
    do_render = false;
    return;
  }
  else if (i == 1 || i == 3) { // render button
    if (optimizing)
      return;
    if (stone == NULL) { // no stone; call file open instead
      fileopen_btn_cb((Fl_Widget *) 0, (void *) 0);
      if (stone == NULL)
        return;
    }
    dlg->hide();
    do_render = true;
    return;
  }
}


/**
  Ok button callback just hides parent passed in user data u
*/
static void ok_cb(Fl_Widget *o, void *u)
{
  Fl_Window *w = (Fl_Window *) u;
  w->hide();
}

#define LSIZE 12
//unsigned int BUTTON_COLOR = 0xD0D0D000;

#include "XPM/ok.xpm"           // OK check button icon

/**
  Callback for the about_btn button
*/ 
static void about_btn_cb(Fl_Widget *o, void *u)
{
  printf("About button callback\n");
  Fl_Double_Window w(50, 50, 350, 205, "About GemRay");
  w.color(BKG_COLOR);
  Fl_Box b(10, 20, 150, 150);
  b.box(FL_GTK_UP_BOX);
//b.color(0x72AACC00);
  b.color(0xcceeff00);
  Fl_Pixmap icon150(gemray150x150_xpm);
  b.image(icon150);
  Fl_Box tb(160, 10, 200, 150);
//tb.align(Fl_Align(FL_ALIGN_RIGHT));
  tb.labelsize(LSIZE);
  tb.color(BKG_COLOR);
  tb.label("GemRay Version 1.1.6\n"
           "Copyright \u00A9 2016-2026\n"
           "Robert W. Strickland\n"
           "All Rights Reserved\n"
           "www.gemcad.com\n\n"
           "GemRay is based in part on\n"
           "the work of the FLTK project\n"
           "(http://www.fltk.org)");
  Fl_Image_Button ok(237,157,100,35);
  Fl_Pixmap ok_img(ok_xpm);
  ok.up_image(&ok_img);
//ok.color(BUTTON_COLOR);
  ok.labelsize(LSIZE);
  ok.callback(ok_cb, (void *) &w);
  w.show();
  while(w.shown())
    Fl::wait();
}


/**
  Callback for the help_btn button
*/ 
static void help_btn_cb(Fl_Widget *o, void *u)
{
  fl_open_uri("https://www.gemcad.com/GemRay/UsersGuide/");
}

/*
static void rotate_cb(Fl_Widget *o, void *u)
{
  Fl_Round_Button *b = (Fl_Round_Button *) o;
  int i = (intptr_t) u;
  printf("Button %d selected\n", i);
}
*/

/**
  Nelder-Mead Downhill Simplex Method a.k.a. Amoeba Method.
 
  Minimize an objective function f of n variables where n is a
  small integer. The objective function is a scalar. The method
  moves a simplex of n+1 points until a local minimum is located.
 
  The calling program must initialize the simplex and evaluate the function
  at the vertices of the simplex.
 
  int neldermead(double *x[], double y[], int n, double ftol, int maxfev)
 
  On entry, the array x[] is an array of pointers to the n+1 points of the
  simplex. Each entry points to an array of the independent variables of the
  objective function. On return, x[0] points to the array of best guesses.
  
  On entry, y[] is an array of the n+1 values of the objective function at
  the n+1 points on the simplex. On return y[0] is the minimum value of the
  objective function.
 
  maxfev is the upper limit for the number of function evaluations.
 
  ftol is the convergence criterion. On success
 
  f is the function pointer. It takes as input a vector of input
  variables.
 
  Returns
  
  Success: number of function evaluations
  Failure: -1 for memory allocation error.
           -2 maximum number of function reached; x[0] will contain the
              best estimate so far.
 
  Reference:
  Nelder, John A.; R. Mead (1965). "A simplex method for function
  minimization". Computer Journal 7: 308-313.
 
  Code based on Wikipedia entry:
  http://en.wikipedia.org/wiki/Nelder%E2%80%93Mead_method#cite_note-YKL-1
*/
int
neldermead(double *x[], double y[], int n, double ftol, int maxfev)
{
  int i, j, nfev, best, worst, second_worst;
  double *x0, *x1, *x2;
  double yr, ye, yc;
  double swap;
  double rtol;

  /*
   * Allocate temporary arrays
   */
  x0 = new double[n]; // centroid 
  x1 = new double[n]; // reflection or contraction 
  x2 = new double[n]; // expansion 
  if (x0 == NULL || x1 == NULL || x2 == NULL)
    return -1;
  nfev = 0;
  /*
   * The simplex consists of n+1 vertices x[j]
   * The objective function at these vertices is
   * y[i] = func(x[i])
   */
  for (;;) {
    /*
     * 1. Identify best (smallest), worst, and second worst values of y[x]
     */
    best = 0;
    worst = 1;
    for (i = 0; i <= n; i++) {
      if (y[i] < y[best])
        best = i;
      if (y[i] > y[worst])
        worst = i;
    }
    second_worst = best;
    for (i = 0; i <= n; i++) {
      if (i != best && i != worst && y[i] >= y[second_worst])
        second_worst = i;
    }
    crn = x[best][0];
    pav = x[best][1];
    update_pavcrn();
    update_newangs(10000./y[best]);
    if (interrupt)
      break;
    dlg->cursor(FL_CURSOR_WAIT);
    Fl::wait();
    /*
     * Check for divergence (too many function evaluations)
     */
    if (nfev >= maxfev) {
      nfev = -2;
      break;
    }
    /*
     * Check for convergence
     */
    rtol = 2.0*fabs(y[worst]-y[best])/(fabs(y[worst])+fabs(y[best]));
    if (rtol < ftol)
      break;
    /*
     * 2. Calculate center of gravity of all vertices except worst
     */
    for (j = 0; j < n; j++)
      x0[j] = 0.;
    for (i = 0; i <= n; i++) {
      if (i == worst)
        continue;
      for (j = 0; j < n; j++)
        x0[j] += x[i][j];
    }
    for (j = 0; j < n; j++)
      x0[j] /= (double) n;
    /*
     * 3. Reflection
     */
    for (j = 0; j < n; j++)
      x1[j] = x0[j] + 1.*(x0[j] - x[worst][j]); // alpha 
    yr = func(x1); // evaluate function at reflected point 
    nfev++;
    /*
     * If the reflected point is bettter than the second worst but not
     * better than the best...
     */
    if (y[best] <= yr && yr < y[second_worst]) {
      /*
       * Replace worst with reflected point
       */
      for (j = 0; j < n; j++)
        x[worst][j] = x1[j];
      y[worst] = yr;
      continue; // next iteration 
    }
    /*
     * 4. Expansion
     */
    if (yr < y[best]) {
      for (j = 0; j < n; j++)
        x2[j] = x0[j] + 2.*(x0[j] - x[worst][j]); // gamma=2 
      ye = func(x2);
      nfev++;
      if (ye < yr) {
        /*
         * Replace worst with expanded point
         */
        for (j = 0; j < n; j++)
          x[worst][j] = x2[j];
        y[worst] = ye;
        continue;
      }
      else {
        /*
         * Replace worst with reflected point
         */
        for (j = 0; j < n; j++)
          x[worst][j] = x1[j];
        y[worst] = yr;
        continue;
      }
    }
    /*
     * 5. Contraction
     */
    for (j = 0; j < n; j++)
      x1[j] = x[worst][j] + 0.5*(x0[j] - x[worst][j]); // rho=0.5 
    yc = func(x1);
    nfev++;
    if (yc < y[worst]) {
      /*
       * Replace worst with contracted point
       */
      for (j = 0; j < n; j++)
        x[worst][j] = x1[j];
      y[worst] = yc;
      continue;
    }
    /*
     * 6. Reduction
     */
    for (i = 0; i <= n; i++) {
      if (i == best)
        continue;
      for (j = 0; j < n; j++) {
        x[i][j] = x[best][j] + 0.5*(x[i][j] - x[best][j]); // sigma = 0.5
      }
      y[i] = func(x[i]);
      nfev++;
    }
  }
  /*
   * Best one is ever so slightly better than worst one if method converged.
   * Swap best vertex to initial position.
   */
  for (i = 0; i < n; i++) {
    swap = x[0][i];
    x[0][i] = x[best][i];
    x[best][i] = swap;
  }
  swap = y[0];
  y[0] = y[best];
  y[best] = swap;
  /*
   * Free memory
   */
  delete[] x0;
  delete[] x1;
  delete[] x2;
  crn = x[best][0];
  pav = x[best][1];
  update_pavcrn();
  update_newangs(10000./y[best]);
  return nfev;
}

Fl_Image_Button *optim_btn;

Fl_Pixmap* gears_img;
Fl_Pixmap* cancel_img;
Fl_Pixmap* restart_img;

/**
 callback for the optimation
*/
static void optim_cb(Fl_Widget *o, void *u)
{
  if (stone == NULL) { // no stone; call file open instead
    fileopen_btn_cb((Fl_Widget *) 0, (void *) 0);
    if (stone == NULL)
      return;
  }

  if (optimizing) {
    interrupt = true;
    return;
  }

  bool hastable = stone && stone->tablefacet() != -1;
  if (!hastable) { // no table, so zero table weights
    wtfac[3] = 0.;
    wtfac[4] = 0.;
    wtfac[5] = 0.;
    wtfac[9] = 0.;
    wtfac[10] = 0.;
    wtfac[11] = 0.;
  }
 
  Fl_Window opt(dlg->x(), dlg->y(), 290, 425, "Optimization");
  opt.color(BKG_COLOR);

#ifdef X11
  int lsize = 10;
#else
  int lsize = 12;
#endif
//Fl_Box b0(10, 20, 270, 240);
  Fl_Box b0(10, 20, 265, 220);
//b0.box(FL_GTK_THIN_DOWN_FRAME);
  b0.box(FL_EMBOSSED_FRAME);
  b0.color(BKG_COLOR);
  Fl_Box l0(12, 8, 0, 10, "Brightness Weights");
  l0.labelsize(LSIZE);
  l0.align(Fl_Align(FL_ALIGN_RIGHT));

  Fl_Box o0(130, 30, 100, 0, "Face Up");
  o0.align(Fl_Align(FL_ALIGN_LEFT));
  o0.labelsize(lsize);
  Numpad_Input w0 ( 90,  40, 40, 22, "ISO \u00D7");
  w0.labelsize(lsize); w0.textsize(lsize);
  Numpad_Input w1 ( 90,  70, 40, 22, "+ COS \u00D7");
  w1.labelsize(lsize); w1.textsize(lsize);
  Numpad_Input w2 ( 90, 100, 40, 22, "+ SC2 \u00D7");
  w2.labelsize(lsize); w2.textsize(lsize);
  Numpad_Input w3 ( 90, 140, 40, 22, "+ table ISO \u00D7");
  w3.labelsize(lsize); w3.textsize(lsize);
  Numpad_Input w4 ( 90, 170, 40, 22, "+ table COS \u00D7");
  w4.labelsize(lsize); w4.textsize(lsize);
  Numpad_Input w5 ( 90, 200, 40, 22, "+ table SC2 \u00D7");
  w5.labelsize(lsize); w5.textsize(lsize);

  Fl_Box o1(250, 30, 100, 0, "Tilted");
  o1.align(Fl_Align(FL_ALIGN_LEFT));
  o1.labelsize(lsize);

  // Note: \u00D7 is the multiplication X symbol
  Numpad_Input w6 (220,  40, 40, 22, "+ ISO \u00D7");
  w6.labelsize(lsize); w6.textsize(lsize);
  Numpad_Input w7 (220,  70, 40, 22, "+ COS \u00D7");
  w7.labelsize(lsize); w7.textsize(lsize);
  Numpad_Input w8 (220, 100, 40, 22, "+ SC2 \u00D7");
  w8.labelsize(lsize); w8.textsize(lsize);
  Numpad_Input w9(220, 140, 40, 22, "+ table ISO \u00D7");
  w9.labelsize(lsize); w9.textsize(lsize);
  Numpad_Input w10(220, 170, 40, 22, "+ table COS \u00D7");
  w10.labelsize(lsize); w10.textsize(lsize);
  Numpad_Input w11(220, 200, 40, 22, "+ table SC2 \u00D7");
  w11.labelsize(lsize); w11.textsize(lsize);

  double pavangle = stone->getPavilionAngle();
  if (pavmin == 0.1234) {
    pavmin = 0.5;
    if (pavangle != 0. && ri != 0. && hastable) { 
      double minpavangle = asin(1./ri)*180./M_PI;
      //    printf("Critical angle = %f\n", minpavangle);
      minpavangle += 0.5; // critical angle plus half a degree
      pavmin = tan(minpavangle*M_PI/180.)/tan(pavangle*M_PI/180.);
    }
  }

  Fl_Box lb1(12, 263, 0, 10, "Scale Factor Limits");
  lb1.align(Fl_Align(FL_ALIGN_RIGHT));
  lb1.labelsize(LSIZE);

  Fl_Box b1(10, 275, 155, 85);
//b1.box(FL_GTK_THIN_DOWN_FRAME);
  b1.box(FL_EMBOSSED_FRAME);
  b1.color(BKG_COLOR);

  Numpad_Input cmin(60, 295, 40, 22, "Crown");
  cmin.labelsize(LSIZE); cmin.textsize(LSIZE);
  cmin.tooltip("Enter crown minimum scale factor.");
  char valcmin[20]; snprintf(valcmin,sizeof(valcmin), "%f", crnmin); ftrunc(valcmin); cmin.value(valcmin);

  Fl_Box lb1a(88, 288, 10, 0, "min.");
  lb1a.labelsize(LSIZE);
  lb1a.align(Fl_Align(FL_ALIGN_LEFT));
  Fl_Box lb1b(140, 288, 10, 0, "max.");
  lb1b.labelsize(LSIZE);
  lb1b.align(Fl_Align(FL_ALIGN_LEFT));

  Numpad_Input cmax(110, 295, 40, 22);
  cmax.labelsize(LSIZE); cmax.textsize(LSIZE);
  cmax.tooltip("Enter crown maximum scale factor.");
  char valcmax[20]; snprintf(valcmax,sizeof(valcmax), "%f", crnmax); ftrunc(valcmax); cmax.value(valcmax);
  
//Fl_Box lb2(125, 60, 10, 0, "Pavilion Scale Factor");
//lb2.align(Fl_Align(FL_ALIGN_LEFT));
  Numpad_Input pmin(60, 325, 40, 22, "Pavilion");
  pmin.labelsize(LSIZE); pmin.textsize(LSIZE);
  pmin.tooltip("Enter pavilion minimum scale factor.");
  char valpmin[20]; snprintf(valpmin,sizeof(valpmin), "%.3f", pavmin); ftrunc(valpmin); pmin.value(valpmin);

  Numpad_Input pmax(110, 325, 40, 22);
  pmax.labelsize(LSIZE); pmax.textsize(LSIZE);
  pmax.tooltip("Enter pavilion maximum scale factor.");
  char valpmax[20]; snprintf(valpmax,sizeof(valpmax), "%.3f", pavmax); ftrunc(valpmax); pmax.value(valpmax);
//////////////////

  Fl_Box lb2(182, 263, 0, 10, "Tilt Angle");
  lb2.labelsize(LSIZE);
  lb2.align(Fl_Align(FL_ALIGN_RIGHT));
  
  Fl_Box b2(175, 275, 100, 85);
//b2.box(FL_GTK_THIN_DOWN_FRAME);
  b2.box(FL_EMBOSSED_FRAME);
  b2.color(BKG_COLOR);

  Fl_Box lb3(260, 285, 10, 0, "degrees");
  lb3.labelsize(LSIZE);
  lb3.align(Fl_Align(FL_ALIGN_LEFT));

  Numpad_Input mt( 220, 295, 40, 22, "max.");
  char valmt[20]; snprintf(valmt,sizeof(valmt), "%.2f", omaxtilt); ftrunc(valmt); mt.value(valmt);
  mt.labelsize(LSIZE); mt.textsize(LSIZE);
  mt.tooltip("Enter maximum tilt angle\n"
             "used in optimization.");

  Numpad_Input ti( 220, 325, 40, 22, "incre-\nment");
  ti.labelsize(LSIZE); ti.textsize(LSIZE);
  ti.tooltip("Enter increment between tilt\n"
             "angles used in optimization.");
  char valti[20]; snprintf(valti,sizeof(valti), "%.2f", otiltinc); ftrunc(valti); ti.value(valti);

//////////////////////

  Numpad_Input w12(110, 375, 40, 22, "Crown ht. weight");
  w12.labelsize(LSIZE); w12.textsize(LSIZE);
  w12.tooltip("Enter weight for crown height importance.");

//////////////////////

  Fl_Image_Button go(175, 375, 100, 35);
  Fl_Pixmap img(go_xpm);
  go.up_image(&img);
  go.when(0); // don't do callback but just set changed() if pressed
  go.tooltip("Begin the optization.");

  char val0[20]; snprintf(val0,sizeof(val0), "%f", wtfac[0]); ftrunc(val0); w0.value(val0);
  char val1[20]; snprintf(val1,sizeof(val1), "%f", wtfac[1]); ftrunc(val1); w1.value(val1);
  char val2[20]; snprintf(val2,sizeof(val2), "%f", wtfac[2]); ftrunc(val2); w2.value(val2);
  char val3[20]; snprintf(val3,sizeof(val3), "%f", wtfac[3]); ftrunc(val3); w3.value(val3);
  char val4[20]; snprintf(val4,sizeof(val4), "%f", wtfac[4]); ftrunc(val4); w4.value(val4);
  char val5[20]; snprintf(val5,sizeof(val5), "%f", wtfac[5]); ftrunc(val5); w5.value(val5);
  char val6[20]; snprintf(val6,sizeof(val6), "%f", wtfac[6]); ftrunc(val6); w6.value(val6);
  char val7[20]; snprintf(val7,sizeof(val7), "%f", wtfac[7]); ftrunc(val7); w7.value(val7);
  char val8[20]; snprintf(val8,sizeof(val8), "%f", wtfac[8]); ftrunc(val8); w8.value(val8);
  char val9[20]; snprintf(val9,sizeof(val9), "%f", wtfac[9]); ftrunc(val9); w9.value(val9);
  char val10[20]; snprintf(val10,sizeof(val10), "%f", wtfac[10]); ftrunc(val10); w10.value(val10);
  char val11[20]; snprintf(val11,sizeof(val11), "%f", wtfac[11]); ftrunc(val11); w11.value(val11);
  char val12[20]; snprintf(val12,sizeof(val12), "%f", wtfac[12]); ftrunc(val12); w12.value(val12);

  opt.end();
  opt.show();

  // wait for go button to be clicked or window close button
  bool ok = false;
  while (opt.shown()) {
    if (go.changed()) { // ok button clicked
      opt.hide();
      ok = true;
    }
    Fl::wait();
  }
  if (!ok) return;

  sscanf(w0.value(), "%lf", &wtfac[0]);
  sscanf(w1.value(), "%lf", &wtfac[1]);
  sscanf(w2.value(), "%lf", &wtfac[2]);
  sscanf(w3.value(), "%lf", &wtfac[3]);
  sscanf(w4.value(), "%lf", &wtfac[4]);
  sscanf(w5.value(), "%lf", &wtfac[5]);
  sscanf(w6.value(), "%lf", &wtfac[6]);
  sscanf(w7.value(), "%lf", &wtfac[7]);
  sscanf(w8.value(), "%lf", &wtfac[8]);
  sscanf(w9.value(), "%lf", &wtfac[9]);
  sscanf(w10.value(), "%lf", &wtfac[10]);
  sscanf(w11.value(), "%lf", &wtfac[11]);
  sscanf(w12.value(), "%lf", &wtfac[12]);

  sscanf(mt.value(),   "%lf", &omaxtilt);
  sscanf(ti.value(),   "%lf", &otiltinc);
  sscanf(cmin.value(), "%lf", &crnmin);
  sscanf(cmax.value(), "%lf", &crnmax);
  sscanf(pmin.value(), "%lf", &pavmin);
  sscanf(pmax.value(), "%lf", &pavmax);

  for (int i = 0; i < 12; i++)
    printf("wtfac[%d] = %f\n", i, wtfac[i]);
  printf("max tilt = %f\n", omaxtilt);
  printf("tilt increment = %f\n", tiltinc);
  printf("crn. min = %f\n", crnmin);
  printf("crn. max = %f\n", crnmax);
  
  if (crn < crnmin) crn = crnmin;
  if (crn > crnmax) crn = crnmax;
  if (pav < pavmin) pav = pavmin;
  if (pav > pavmax) pav = pavmax;

  bool restart = false;
  double* x[3];
  for (int i = 0; i < 3; i++)
    x[i] = new double[2];
  optimizing = true;
  interrupt = false;
  do {
    dlg->cursor(FL_CURSOR_WAIT);
    dlg->redraw();
    Fl::check();
  
    double y[3];
    x[0][0] = crn;
    x[0][1] = pav;
    y[0] = func(x[0]);
    update_objective(10000./y[0]);
    optim_btn->up_image(cancel_img);
    optim_btn->redraw();
    Fl::check();
    x[1][0] = crn;
    x[1][1] = pav*1.1;
    y[1] = func(x[1]);
    x[2][0] = crn*1.1;
    x[2][1] = pav;
    y[2] = func(x[2]);
  //optim_btn->redraw();
    int niter = neldermead(x, y, 2, 0.0005, 200);
    if (interrupt) {
      optim_btn->up_image(gears_img);
      optim_btn->redraw();
      break;
    }
    std::cout << "Nelder-Mead optimization:" << std::endl;
    std::cout << "optimum crown scale factor = " << x[0][0] << std::endl;
    std::cout << "optimum pavilion scale factor = " << x[0][1] << std::endl;
    std::cout << niter << " function evaluations" << std::endl;
    crn = x[0][0];
    pav = x[0][1];
    char str[80];
    update_pavcrn();
    optim_btn->up_image(gears_img);
    optim_btn->redraw();
    Fl::check();
    Fl_Double_Window rsdlg(dlg->x()+70, dlg->y()+140, 230, 130,
      "Optimization Complete");
    rsdlg.set_modal();
    Fl_Box lb(10, 20, 210, 50, "Restart optimization from\ncurrent point?");
    lb.color(FL_WHITE);
    lb.box(FL_DOWN_BOX);
    lb.labelsize(LSIZE);
    Fl_Image_Button restart_btn(10, 80, 100, 35);
    restart_btn.up_image(restart_img);
    restart_btn.tooltip("Restart");
    restart_btn.when(0);
    Fl_Image_Button cancel_btn(120, 80, 100, 35);
    cancel_btn.up_image(cancel_img);
    cancel_btn.tooltip("Cancel");
    cancel_btn.when(0);
    rsdlg.color(BKG_COLOR);
    rsdlg.end();
    rsdlg.show();
    dlg->cursor(FL_CURSOR_DEFAULT);
    Fl::check();
    restart = false;
    while (rsdlg.shown()) {
      if (cancel_btn.changed()) { // cancel button clicked
//      cancel_btn.when(FL_WHEN_RELEASE);
        rsdlg.hide();
      }
      if (restart_btn.changed()) { // restart button clicked
        restart = true;
//      restart_btn.when(FL_WHEN_RELEASE);
        rsdlg.hide();
      }
      Fl::wait();
    }
  } while (restart);
  optimizing = false;
  interrupt = false;
  
  dlg->cursor(FL_CURSOR_DEFAULT);
  dlg->redraw();

  for (int i = 0; i < 3; i++)
    delete[] x[i];
  Fl::wait();
}

int main(int argc, char **argv)
{
  fl_register_images();


  char *p = getenv("FLTK_SCHEME");
  if (p == NULL)
//  Fl::scheme("gleam"); // looks better than the default "none" scheme
//  Fl::scheme("plastic"); // looks better than the default "none" scheme
    Fl::scheme("gtk+"); // looks better than the default "none" scheme
  else
    Fl::scheme(p);

  load_master_defaults();
  dlg = new Fl_Double_Window(400,485,"GemRay Options");

  Fl_Pixmap gemrayIcon(gemray150x150_xpm);
  Fl_RGB_Image *gemrayIconRGB = new Fl_RGB_Image(&gemrayIcon);
  dlg->icon(gemrayIconRGB);
  dlg->color(BKG_COLOR);
  dlg->callback(render_btn_cb, (void *) 2); // catch escape key and cancel

  Fl_Pixmap *fileopen_img = NULL; // image for the File Open button
  fileopen_img = new Fl_Pixmap(fileopen_xpm);

//Fl_Pixmap *head_img = NULL; // image for the head shadow icon
//head_img = new Fl_Pixmap(head_xpm);

  Fl_Image_Button *fileopen_btn = new Fl_Image_Button(10, 10, 60, 35);
  Fl::focus(fileopen_btn);
//fileopen_btn->color(BUTTON_COLOR);
  fileopen_btn->labelsize(LSIZE);
  fileopen_btn->tooltip("Open a GemCad file.\n");
  fileopen_btn->callback(fileopen_btn_cb);
  fileopen_btn->up_image(fileopen_img);

  fileopen_output = new Fl_Output(70, 25, 320, 20, "");
  fileopen_output->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  fileopen_output->labelsize(LSIZE);
  fileopen_output->color(BKG_COLOR);
  fileopen_output->textsize(LSIZE);
  fileopen_output->label(" GemCad File");
  fileopen_output->tooltip("Use Open button to select GemCad file.\n");

//Fl_Group *color_grp = new Fl_Group(10, 70, 225, 120);
//int gtop = 70;
  int gtop = 115;
  Fl_Group *color_grp = new Fl_Group(10, gtop, 190, 120);
//color_grp->box(FL_THIN_DOWN_FRAME);
//color_grp->box(FL_GTK_THIN_DOWN_FRAME); // better
  color_grp->box(FL_EMBOSSED_FRAME);
  color_grp->color(BKG_COLOR);
  color_grp->labelsize(LSIZE);
  color_grp->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  color_grp->label(" Colors");

//Fl_Pixmap *palette_img = NULL; // image for the palette button
//palette_img = new Fl_Pixmap(palette_xpm);

//int gwid = 98;
  int gwid = 80;
  stone_color_box = new ColorBox(20, gtop+20, gwid, 35);
  stone_color_box->box(FL_GTK_DOWN_BOX);
  stone_color_box->labelsize(LSIZE);
  stone_color_box->label("Stone");
  stone_color_box->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  stone_color_box->tooltip("Choose color of gemstone");
  stone_color_box->color(stoneColor << 8 & 0xFFFFFF00);

  ColorBox *bkg_color_box = new ColorBox(110, gtop+20, gwid, 35);
  bkg_color_box->box(FL_GTK_DOWN_BOX);
  bkg_color_box->labelsize(LSIZE);
  bkg_color_box->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  bkg_color_box->label("Background");
  bkg_color_box->color(bkgColor << 8 & 0xFFFFFF00);
  bkg_color_box->tooltip("Choose color of background");

  ColorBox *leak_color_box = new ColorBox(20, gtop+75, gwid, 35);
  leak_color_box->box(FL_GTK_DOWN_BOX);
  leak_color_box->labelsize(LSIZE);
  leak_color_box->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  leak_color_box->label("Leak");
  leak_color_box->color(leakColor << 8 & 0xFFFFFF00);
  leak_color_box->tooltip("Choose color windowed (leaked) through stone");

  ColorBox *head_color_box = new ColorBox(110, gtop+75, gwid, 35);
  head_color_box->box(FL_GTK_DOWN_BOX);
  head_color_box->labelsize(LSIZE);
  head_color_box->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  head_color_box->label("Viewer's Head");
  head_color_box->color(headColor << 8 & 0xFFFFFF00);
  head_color_box->tooltip("Choose viewer's head shadow\n(camera reflection) color");

  color_grp->end();

  Fl_Group *rotate_grp = new Fl_Group(210, gtop, 180, 50);
//Fl_Group *rotate_grp = new Fl_Group(245, 60, 145, 85);
//rotate_grp->box(FL_GTK_THIN_DOWN_FRAME); // better
  rotate_grp->box(FL_EMBOSSED_FRAME); // better
  rotate_grp->color(BKG_COLOR);
  rotate_grp->labelsize(LSIZE);
  rotate_grp->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  rotate_grp->label(" Tilt or Rotate");

  Numpad_Input *elev_inp = new Numpad_Input(220, gtop+20, 40, 22,
      "Rot. Axis Elev. Ang.");
  elev_inp->labelsize(LSIZE);
  elev_inp->textsize(LSIZE);
  char s[80];
  if (elev < 0.1) {
    elev = 0.;
    strcpy(s, "auto");
  }
  else {
    snprintf(s,sizeof(s), "%.2f", elev);
    ftrunc(s);
    strcat(s, DEGREE_LAB);
  }
  elev_inp->value(s);
  elev_inp->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  elev_inp->callback(float_inp_cb, (void *) ELEV_INP);
  elev_inp->tooltip("Enter the elevation angle\n"
                    "of the rotation axis.\nEnter 0 to tilt.\n"
                    "Hint: Try 45\u00B0 to rotate.");

  Numpad_Input *maxtilt_inp = new Numpad_Input(340, gtop+20, 40, 22,
      "Amount");
  maxtilt_inp->labelsize(LSIZE);
  maxtilt_inp->textsize(LSIZE);
  snprintf(s,sizeof(s), "%.2f", maxtilt);
  ftrunc(s);
  strcat(s, DEGREE_LAB);
  maxtilt_inp->value(s);
  maxtilt_inp->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  maxtilt_inp->callback(float_inp_cb, (void *) TILT_INP);
  maxtilt_inp->tooltip("Enter the maximum rotation\n"
                       "angle in degrees.\n"
                       "Enter 180 to rotate a\n"
                       "full circle and loop.");
  rotate_grp->end();
  
  gtop += 70;
  Fl_Group *viewer_grp = new Fl_Group(210, gtop, 180, 50);
//Fl_Group *viewer_grp = new Fl_Group(245, 60, 145, 85);
//viewer_grp->box(FL_GTK_THIN_DOWN_FRAME); // better
  viewer_grp->box(FL_EMBOSSED_FRAME); // better
  viewer_grp->color(BKG_COLOR);
  viewer_grp->labelsize(LSIZE);
  viewer_grp->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  viewer_grp->label(" Viewer");

  Numpad_Input *head_inp = new Numpad_Input(220, gtop+20, 40, 22,
     "Head \u00BD Angle");
  head_inp->labelsize(LSIZE);
  head_inp->textsize(LSIZE);
  snprintf(s,sizeof(s), "%.1f", head);
  ftrunc(s);
  strcat(s, DEGREE_LAB);
  head_inp->value(s);
  head_inp->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  head_inp->callback(float_inp_cb, (void *) HEAD_INP);
  head_inp->tooltip("Enter the head shadow cone\n"
                    "half angle in degrees");

  Numpad_Input *zeye_inp = new Numpad_Input(340, gtop+20, 40, 22,
      "Eye Height");
  zeye_inp->labelsize(LSIZE);
  zeye_inp->textsize(LSIZE);
  snprintf(s,sizeof(s), "%.1f", zeye);
  ftrunc(s);
  zeye_inp->value(s);
  zeye_inp->align(Fl_Align(FL_ALIGN_TOP_RIGHT));
  zeye_inp->callback(float_inp_cb, (void *) ZEYE_INP);
  zeye_inp->tooltip("Enter the height of\n"
		    "viewer's eye above\n"
		    "the center of stone\n"
		    "in GemCad units");

  viewer_grp->end();
//gtop = 70+145;
  gtop = 70;
  ri_inp_chc = new Fl_Input_Choice(10,gtop,180,22, " Refractive Index");
  ri_inp_chc->menubutton()->type(1);
  ri_inp_chc->menubutton()->box(FL_GTK_DOWN_BOX);
  ri_inp_chc->menubutton()->color(FL_WHITE);
  ri_inp_chc->menu(ri_choices);
  ri_inp_chc->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  ri_inp_chc->tooltip("Choose an index of refraction\n"
                      "or enter a value not in list.");
  ri_inp_chc->when(FL_WHEN_RELEASE | FL_WHEN_NOT_CHANGED);
  ri_inp_chc->color(BKG_COLOR);
  ri_inp_chc->value(NRI-1);
  ri_inp_chc->callback(ri_cb);
  ri_inp_chc->labelsize(LSIZE);
  ri_inp_chc->textsize(LSIZE);

  cod_inp_chc = new Fl_Input_Choice(210,gtop,180,22,
      " Coefficient of Dispersion");
  cod_inp_chc->menubutton()->type(1);
  cod_inp_chc->menubutton()->box(FL_GTK_DOWN_BOX);
  cod_inp_chc->menubutton()->color(FL_WHITE);
  cod_inp_chc->menu(dispersion_choices);
  cod_inp_chc->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  cod_inp_chc->tooltip("Choose a coefficient of dispersion\n"
                      "or enter a value not in list.");
  cod_inp_chc->when(FL_WHEN_RELEASE | FL_WHEN_NOT_CHANGED);
  cod_inp_chc->color(BKG_COLOR);
  cod_inp_chc->value(NDISP-1);
  cod_inp_chc->callback(dch_cb);
  cod_inp_chc->labelsize(LSIZE);
  cod_inp_chc->textsize(LSIZE);

  Fl_Group *scale_grp = new Fl_Group(10, 255, 380, 100);
//scale_grp->box(FL_GTK_THIN_DOWN_FRAME); // better
  scale_grp->box(FL_EMBOSSED_FRAME); // better
  scale_grp->color(BKG_COLOR);
  scale_grp->labelsize(LSIZE);
  scale_grp->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  scale_grp->label(" Scale and Optimize");


//------------left column x = 65

  Fl_Box oldlab(45, 270, 100, 20, "Old Angle");
  oldlab.labelsize(LSIZE);

  crnoldang_out = new Fl_Output(65, 288, 60, 22, "Crown");
  crnoldang_out->color(BKG_COLOR);
  crnoldang_out->labelsize(LSIZE);
  crnoldang_out->textsize(LSIZE);
  if (stone == NULL) {
    crnoldang_out->value("");
  }
  else {
    double crnoldang = stone->getCrownAngle();
    snprintf(s,sizeof(s), "%.2f%s", crnoldang, DEGREE_LAB);
    crnoldang_out->value(s);
  }
  crnoldang_out->align(Fl_Align(FL_ALIGN_LEFT));
  crnoldang_out->tooltip("Old crown angle: angle of\nlargest crown facet\n(but not table)");

  pavoldang_out = new Fl_Output(65, 318, 60, 22, "Pavilion");
  pavoldang_out->color(BKG_COLOR);
  pavoldang_out->labelsize(LSIZE);
  pavoldang_out->textsize(LSIZE);
  if (stone == NULL) {
    pavoldang_out->value("");
  }
  else {
    double pavoldang = stone->getPavilionAngle();
    snprintf(s,sizeof(s), "%.2f%s", pavoldang, DEGREE_LAB);
    pavoldang_out->value(s);
  }
  pavoldang_out->align(Fl_Align(FL_ALIGN_LEFT));
  pavoldang_out->tooltip("Old angle: lowest\npavilion angle");

//------------Center column
  Fl_Box newlab(115, 270, 100, 20, "New Angle");
  newlab.labelsize(LSIZE);

  crnnewang_inp = new Numpad_Input(135, 288, 60, 22);
  crnnewang_inp->labelsize(LSIZE);
  crnnewang_inp->textsize(LSIZE);
  if (stone == NULL) {
    crnnewang_inp->value("");
  }
  else {
    double crnnewang = stone->getCrownAngle();
    snprintf(s,sizeof(s), "%.2f%s", crnnewang, DEGREE_LAB);
    crnnewang_inp->value(s);
  }
  crnnewang_inp->align(Fl_Align(FL_ALIGN_LEFT));
  crnnewang_inp->callback(float_inp_cb, (void *) CRN_NEWANG_INP);
  crnnewang_inp->tooltip("Enter new crown angle.");

  pavnewang_inp = new Numpad_Input(135, 318, 60, 22);
  pavnewang_inp->labelsize(LSIZE);
  pavnewang_inp->textsize(LSIZE);
  if (stone == NULL) {
    pavnewang_inp->value("");
  }
  else {
    double pavnewang = stone->getPavilionAngle();
    snprintf(s,sizeof(s), "%.2f%s", pavnewang, DEGREE_LAB);
    pavnewang_inp->value(s);
  }
  pavnewang_inp->align(Fl_Align(FL_ALIGN_LEFT));
  pavnewang_inp->callback(float_inp_cb, (void *) PAV_NEWANG_INP);
  pavnewang_inp->tooltip("Enter new pavilion angle.");

//------------right column
//Fl_Box sclab(195, 260, 100, 20, "Scale\nFactors");
//sclab.labelsize(LSIZE);
//sclab.align(Fl_Align(FL_ALIGN_TOP_LEFT));

  crn_inp = new Numpad_Input(205, 288, 60, 22, "Scale\nFactors");
  crn_inp->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  crn_inp->labelsize(LSIZE);
  crn_inp->textsize(LSIZE);
  snprintf(s,sizeof(s), "%.4f", crn);
  crn_inp->value(s);
  crn_inp->callback(float_inp_cb, (void *) CRN_INP);
  crn_inp->tooltip("Enter crown (top) scale factor.");

  pav_inp = new Numpad_Input(205, 318, 60, 22);
  pav_inp->labelsize(LSIZE);
  pav_inp->textsize(LSIZE);
  snprintf(s,sizeof(s), "%.4f", pav);
  pav_inp->value(s);
  pav_inp->align(Fl_Align(FL_ALIGN_LEFT));
  pav_inp->callback(float_inp_cb, (void *) PAV_INP);
  pav_inp->tooltip("Enter pavilion (bottom) scale factor.");

////////////

  obj_out = new Fl_Output(320, 275, 60, 22, "Merit");
  obj_out->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  obj_out->color(BKG_COLOR);
  obj_out->labelsize(LSIZE);
  obj_out->textsize(LSIZE);
  obj_out->value("");
  obj_out->tooltip("Objective function");
////////////

  gears_img = new Fl_Pixmap(gears_xpm);
  cancel_img = new Fl_Pixmap(cancel_xpm);
  restart_img = new Fl_Pixmap(restart_xpm);

  optim_btn = new Fl_Image_Button(280, 305, 100, 35);
  optim_btn->up_image(gears_img);
  optim_btn->labelsize(LSIZE);
  optim_btn->tooltip("Optimize pavilion and crown\n"
                      "angles for best performance.");
  optim_btn->callback(optim_cb);

  scale_grp->end();

  Fl_Group *image_grp = new Fl_Group(10, 375, 380, 55);
  image_grp->box(FL_EMBOSSED_FRAME); // better
  image_grp->color(BKG_COLOR);
  image_grp->labelsize(LSIZE);
  image_grp->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  image_grp->label(" Image");

  Fl_Input *gain_inp = new Fl_Input(70,400,60,22,"Brightness");
  gain_inp->labelsize(LSIZE);
  gain_inp->textsize(LSIZE);
  snprintf(s,sizeof(s), "%.2f", gain);
  gain_inp->value(s);
  gain_inp->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  gain_inp->callback(gain_cb);
  gain_inp->tooltip("Enter gain (brightness) factor between 0.5 (dark image) to 2 (bright image). Enter 1 for no correction.");


  Fl_Input *gamma_inp = new Fl_Input(180,400,60,22,"Contrast");
  gamma_inp->labelsize(LSIZE);
  gamma_inp->textsize(LSIZE);
  snprintf(s,sizeof(s), "%.2f", gamma);
  gamma_inp->value(s);
  gamma_inp->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  gamma_inp->callback(gamma_cb);
  gamma_inp->tooltip("Enter contrast gamma correction between 0.2 (very light image) to 5 (very dark image). Enter 1 for no correction.");

  Fl_Input *size_inp = new Fl_Input(290,400,75,22,"Size");
  size_inp->labelsize(LSIZE);
  size_inp->textsize(LSIZE);
  snprintf(s,sizeof(s), "%d pixels", sz);
  size_inp->value(s);
  size_inp->align(Fl_Align(FL_ALIGN_TOP_LEFT));
  size_inp->callback(size_cb);
  size_inp->tooltip("Enter the image size in pixels.");

  image_grp->end();

  Fl_Pixmap *help_img = NULL; // image for the help button
  help_img = new Fl_Pixmap(help_xpm);

  Fl_Image_Button *help_btn;
  help_btn = new Fl_Image_Button(70, 440, 100, 35);
  help_btn->tooltip("GemRay Help");
  help_btn->labelsize(LSIZE);
//help_btn->color(BUTTON_COLOR);
  help_btn->callback(help_btn_cb);
  help_btn->up_image(help_img);

  Fl_Pixmap *about_img = NULL; // image for the about button
  about_img = new Fl_Pixmap(about_xpm);

  Fl_Image_Button *about_btn;
  about_btn = new Fl_Image_Button(180, 440, 100, 35);
  about_btn->tooltip("About GemRay");
  about_btn->labelsize(LSIZE);
//about_btn->color(BUTTON_COLOR);
  about_btn->callback(about_btn_cb);
  about_btn->up_image(about_img);

  Fl_Pixmap *go_img = NULL; // image for the go button
  go_img = new Fl_Pixmap(go_xpm);

  Fl_Image_Button *render_btn;
  render_btn = new Fl_Image_Button(290, 440, 100, 35);
//render_btn->color(BUTTON_COLOR);
  render_btn->tooltip("Go render the raytraced images\n"
                      "and play the animation");
  render_btn->labelsize(LSIZE);
  render_btn->callback(render_btn_cb, (void *) 1);
  render_btn->up_image(go_img);

  if (argc == 2) {
    if (fileopen_output) fileopen_output->value(argv[1]);
    open_gemcad_file(argv[1]);
    filename = new char[strlen(argv[1]) + 1];
    strcpy(filename, argv[1]);
    stone_color_box->color(stoneColor << 8 & 0xFFFFFF00);
  }

  for (;;) {
    int iret = 0;
    dlg->show();
    if (argc == 2) {
      render_btn_cb((Fl_Widget *) render_btn, (void *) 3);
      argc = 0;    
    }
    iret = Fl::run();
    if (!do_render)
      break;
/*
    int xargc;
    char *xargv[100]; // too many
    int i = 0;
    xargv[i++] = argv[0];

    xargv[i++] = (char *) "-c";
    xargv[i] = new char[20];
    sprintf(xargv[i++], "%06x", stoneColor);

    xargv[i++] = (char *) "-b";
    xargv[i] = new char[20];
    sprintf(xargv[i++], "%06x", bkgColor);

    xargv[i++] = (char *) "-l";
    xargv[i] = new char[20];
    sprintf(xargv[i++], "%06x", leakColor);

    xargv[i++] = (char *) "-n";
    xargv[i] = new char[20];
    sprintf(xargv[i++], "%d", sz);

    xargv[i++] = (char *) "-h";
    xargv[i] = new char[20];
    sprintf(xargv[i++], "%.1f", head);

    xargv[i++] = (char *) "-t";
    xargv[i] = new char[20];
    sprintf(xargv[i++], "%.2f", maxtilt);

    xargv[i++] = (char *) "-i";
    xargv[i] = new char[20];
    sprintf(xargv[i++], "%.2f", tiltinc);

    xargv[i++] = (char *) "-e";
    xargv[i] = new char[20];
    sprintf(xargv[i++], "%.2f", elev);

    xargv[i++] = (char *) "-r";
    xargv[i] = new char[20];
    sprintf(xargv[i++], "%f", ri);

    xargv[i++] = (char *) "-d";
    xargv[i] = new char[20];
    sprintf(xargv[i++], "%f", cod);

    xargv[i++] = (char *) "-s";
    xargv[i] = new char[20];
    sprintf(xargv[i++], "%f", crn);

    xargv[i++] = (char *) "-p";
    xargv[i] = new char[20];
    sprintf(xargv[i++], "%f", pav);

    xargv[i++] = filename;
    xargc = i;
    iret = vmain(xargc, xargv);
*/
    double objective;
    iret = engine(dlg->x()+20, dlg->y()+20, stone, filename, stoneColor, bkgColor, leakColor, headColor, gamma, gain, ri, cod,
      head, zeye, maxtilt, tiltinc, elev, sz, crn, pav, &objective);

    printf("Objective function = %.3f\n", objective);
/*
    if (iret)
      Fl::flush();
    for (i = 2; i < xargc; i += 2) 
      delete[] xargv[i];
*/
//  system(cmd);
  }
  delete fileopen_output;
  delete fileopen_btn;
//delete stone_color_btn;
//delete bkg_color_btn;
//delete leak_color_btn;
  delete stone_color_box;
  delete bkg_color_box;
  delete leak_color_box;
  delete head_color_box;
  delete maxtilt_inp;
  delete head_inp;
  delete size_inp;
  delete ri_inp_chc;
  delete cod_inp_chc;
  delete crn_inp;
  delete pav_inp;
  delete render_btn;
  delete about_btn;
  delete help_btn;
  delete fileopen_img;
//delete palette_img;
  delete about_img;
  delete help_img;
  delete go_img;
  delete gears_img;
  delete cancel_img;
  delete restart_img;
  delete gemrayIconRGB;
  if (filename) delete[] filename;
//delete dlg;
  if (stone != NULL) {
    delete stone;
    stone = NULL;
    printf("Deleted\n");
  }
  return 0;
}
