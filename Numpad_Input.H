// Demonstrate how to use Fl_Input in a touchscreen application -erco 08/25/06

#if 1
//
// How to implement a copy/paste menu for Fl_Input -- erco 02/04/09
//
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Menu.H>
#include <cstdio>              // printf

#define LT_GRAY 0xE0E0E000

#define Numpad_Input Fl_Input

#endif

#if 0

//
// How to implement a copy/paste menu for Fl_Input -- erco 02/04/09
//
class Numpad_Input : public Fl_Input {
  static void Copy_CB(Fl_Widget*, void *userdata) {
    printf("*** COPY ***\n");
    Numpad_Input *in = (Numpad_Input*)userdata;
    in->copy(0);    // text selection clipboard
    in->copy(1);    // copy/paste clipboard
  }
  static void Paste_CB(Fl_Widget*, void *userdata) {
    printf("*** PASTE ***\n");
    Numpad_Input *in = (Numpad_Input*)userdata;
    Fl::paste(*in);
  }
public:
  int handle(int e) {
    switch (e) {
    case FL_PUSH:
      // RIGHT MOUSE PUSHED? Popup menu on right click
      if ( Fl::event_button() == FL_RIGHT_MOUSE ) {
        Fl_Menu_Item rclick_menu[] = {
          { "Copy",   0, Copy_CB,  (void*)this },
          { "Paste",  0, Paste_CB, (void*)this },
          { 0 }
        };
        const Fl_Menu_Item *m = rclick_menu->popup(Fl::event_x(),
            Fl::event_y(), 0, 0, 0);
        if ( m ) m->do_callback(0, m->user_data());
        return(1);          // (tells caller we handled this event)
      }
      break;
    case FL_RELEASE:
      // RIGHT MOUSE RELEASED? Mask it from Fl_Input
      if ( Fl::event_button() == FL_RIGHT_MOUSE ) {
        return(1);          // (tells caller we handled this event)
      }
      break;
    }
    return(Fl_Input::handle(e));    // let Fl_Input handle all other events
  }
  Numpad_Input(int X,int Y,int W,int H,const char*L=0):Fl_Input(X,Y,W,H,L) {
    type(1);
    box(FL_GTK_DOWN_BOX);
    color(FL_WHITE);
  }
};

#endif

#if 0

#include <FL/Fl.H>
#include <FL/Fl_Input.H>
#define LT_GRAY 0xE0E0E000
#define RETURNLAB "@returnarrow" // FLTK return key label
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>

#include <cctype>

//#define DELETELAB "@-5<-"      // back arrow for delete key label
#define DELETELAB "@-24UpArrow"  // FLTKB back arrow for delete key label
#define ZEROLAB   "0"            // zero but can have spaces to position

#define BSIZE 25 // button size in pixels

class MyNumPad;

/**
  Derived from Fl_Input but only allows keyboard input consistent with
  numeric keypad.
*/
class MyInput : public Fl_Input
{
public:
  MyInput(int X,int Y,int W,int H,const char *L=0):Fl_Input(X,Y,W,H,L) { }
  // Toss keyboard presses except keys on keypad
  int handle(int e) {
    if (e == FL_KEYDOWN) {
      const char k = Fl::event_text()[0];
      if (!isdigit(k) && k != '\b' && k != '.' && k != '-' && k != '\r') {
        printf("Ignoring key press %s\n", Fl::event_text());
        return 1;
      }
      if (k == '-' && position() != 1)
        return 1; // minus can only go in front
      if (k == '.' && strchr(value(), '.') != NULL)
        return 1; // only one decimal point
    }
    return(Fl_Input::handle(e)); // base class handle
  }
};

class MyNumPad : public Fl_Window {
  MyInput *in;               // input preview
  Fl_Button *b[14];           // the keypad buttons
  Fl_Callback *enter_cb;      // callback when user clicks 'enter'
  void *enter_data;

  // Handle numeric keypad buttons pressed
  void Button_CB2(Fl_Widget *w) {
    Fl_Button *b = (Fl_Button*)w;
    if ( strcmp(b->label(), DELETELAB) == 0 ) {           // handle Delete
      // Delete
      if (in->mark() != in->position())
        in->cut();
      else
        in->cut(-1);
    }
    else if ( strcmp(b->label(), "-") == 0 ) { // handle minus key
      if (in->position() == 0) // minus can only go in front
        in->replace(in->position(), in->mark(), "-", 1);
    }
    else if ( strcmp(b->label(), ".") == 0 ) { // handle decimal point
      if (strchr(in->value(), '.') == 0) // can only have one .
        in->replace(in->position(), in->mark(), ".", 1);
    }
    else if ( strcmp(b->label(), "@returnarrow") == 0 ) { // handle enter key
      // Do 'Enter' callback
      if ( enter_cb ) (*enter_cb)(in, enter_data);
      return;
    }
    else if ( strcmp(b->label(), ZEROLAB) == 0 ) {    // handle 0 key
      // Append zero character
      in->replace(in->position(), in->mark(), "0", 1);
    }
    else {                                            // handle all other keys
      // Appends label of button
      in->replace(in->position(), in->mark(), b->label(), 1);
    }
    if (in) Fl::focus(in); // reset focus to Fl_Input widget
  }
  static void Button_CB(Fl_Widget *w, void *data) {
    MyNumPad *numpad = (MyNumPad*)data;
    numpad->Button_CB2(w);
  }
public:
  MyNumPad(int X,int Y,int W=20+4*BSIZE,int H=35+5*BSIZE,const char *L=0):
      Fl_Window(X,Y,W,H,L) {
    const int bsize = BSIZE;
    // Preview input
//  in = new MyInput(X+10,Y+15,W-20,BSIZE);
    in = new MyInput(10,15,W-20,BSIZE);
    // Numeric keypad
    int colstart = 10;
    int col = colstart;
    int row = in->y()+in->h()+10;
    // b[n]: n could just as well be consecutive, 0-13
    b[7] = new Fl_Button(col,row,bsize,bsize,  "7"); col += bsize;
    b[8] = new Fl_Button(col,row,bsize,bsize,  "8"); col += bsize;
    b[9] = new Fl_Button(col,row,bsize,bsize,  "9"); col += bsize;
    b[10] = new Fl_Button(col,row,bsize,bsize,  DELETELAB);
    row += bsize;
    col = colstart;

    b[4] = new Fl_Button(col,row,bsize,bsize,  "4"); col += bsize;
    b[5] = new Fl_Button(col,row,bsize,bsize,  "5"); col += bsize;
    b[6] = new Fl_Button(col,row,bsize,bsize,  "6"); col += bsize;
    b[11] = new Fl_Button(col,row,bsize,bsize,  "-");
    b[11]->labelsize(12); col = colstart; row += bsize;

    b[1] = new Fl_Button(col,row,bsize,bsize,  "1"); col += bsize;
    b[2] = new Fl_Button(col,row,bsize,bsize,  "2"); col += bsize;
    b[3] = new Fl_Button(col,row,bsize,bsize,  "3"); col += bsize;

    b[13] = new Fl_Button(col,row,bsize,bsize*2,"@returnarrow");
    col = colstart;
    row += bsize;

    // extra spaces to line up 0 with numbers above since 0 button double wide
    b[0] = new Fl_Button(col,row,bsize*2,bsize,  ZEROLAB);
    col += 2*bsize;

    b[12] = new Fl_Button(col,row,bsize,bsize,  ".");
    col += bsize;

    for (int i = 0; i < 14; i++) {
//    b[i]->box(FL_GTK_UP_BOX);
      b[i]->callback(Button_CB, (void *) this);
    }

    end();
    enter_cb = 0;
    enter_data = 0;
  }
  ~MyNumPad() {
    delete in;
    for (int i = 0; i < 14; i++)
      delete b[i];
  }
  // Set current value
  void value(const char *val)
  {
    in->value(val);
    enter_data = (void *) in->value();
  }
  // Return current value
  const char *value() {
    return(in->value());
  }
  // Clear current value
  void clear() {
    in->value("");
  }
  void setfocus() {
    if (in) Fl::focus(in);
  }
  // Set callback for when Enter is pressed
  void SetEnterCallback(Fl_Callback *cb, void *data) {
    enter_cb = cb;
    enter_data = data;
  }
  // Handle Enter keys on keyboard
  int handle(int e) {
    if (e == FL_KEYDOWN && !strcmp(Fl::event_text(), "\r")) {
      if ( enter_cb ) (*enter_cb)(in, enter_data);
      return 1;
    }
    return(Fl_Window::handle(e)); // base class handle
  }
};

class Numpad_Input : public Fl_Input {
  MyNumPad *numpad;                   // local instance of numeric keypad widget
  Fl_Callback *cbfunc;
  void *cbudat;

  // Called when user finishes entering data with numeric keypad
  void SetNumPadValue_CB2() {
    value(numpad->value());         // pass value from numpad to our input
    numpad->hide();                 // hide numpad
    delete numpad;
    numpad = NULL;
    if (cbfunc) (*cbfunc)(this, cbudat);
  }
  static void SetNumPadValue_CB(Fl_Widget*,void *data) {
    Numpad_Input *in = (Numpad_Input*)data;
    in->SetNumPadValue_CB2();
  }
  // Handle when user left clicks on our input widget
  int handle(int e) {
    int ret = 0;
    switch (e) {
    case FL_KEYDOWN:
/*
	   {
        const char k = Fl::event_text()[0];
        printf("Key pressed: %s\n", Fl::event_text());
        if (!isdigit(k) && k != '\b' && k != '.' && k != '-' && k != '\r') {
          printf("Ignoring key press %s\n", Fl::event_text());
          ret = 1;
        }
      }
*/
      ret = 1;
      if ( !numpad ) numpad = new MyNumPad(0, 0);
      numpad->color(LT_GRAY);
      numpad->SetEnterCallback(SetNumPadValue_CB, (void*)this);
      numpad->position(parent()->x() + x(),parent()->y() + y());
      numpad->show();
      numpad->setfocus();
      break;
    // Left mouse click on input field? Open numpad dialog..
    case FL_PUSH:
      if ( Fl::event_button() == FL_LEFT_MOUSE ) { ret = 1; }
      break;
    case FL_RELEASE:
      if ( Fl::event_button() == FL_LEFT_MOUSE ) {
        int mx = Fl::event_x();
        int my = Fl::event_y();
        ret = 1;
        if ( !numpad ) numpad = new MyNumPad(0, 0);
        numpad->color(LT_GRAY);
        numpad->SetEnterCallback(SetNumPadValue_CB, (void*)this);
//      numpad->position(parent()->x(),parent()->y());
//      numpad->position(x(),y()); // doesn't move with dialog
        numpad->position(parent()->x() + x(),parent()->y() + y());
//      numpad->clear();
        numpad->show();
        numpad->setfocus();
      }
      break;
    }
    return(Fl_Input::handle(e)?1:ret);
  }
public:
  void callback(Fl_Callback *fn, void *u) {
    cbfunc = fn;
    cbudat = u;
  }
  Numpad_Input(int X,int Y,int W,int H,const char *L=0):Fl_Input(X,Y,W,H,L) {
    numpad = NULL;
    cbfunc = NULL;
  }
  ~Numpad_Input() {
    if (numpad) delete numpad;
  }
};
#endif
