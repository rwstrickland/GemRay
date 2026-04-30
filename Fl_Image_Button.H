#ifndef __FL_IMAGE_BUTTON_H
#define __FL_IMAGE_BUTTON_H
#include <FL/Fl_Button.H>
#include <FL/Fl.H>

//#define BRTCOLOR 0x7070ff00
#define BRTCOLOR 0xb0b0ff00
#define DIMCOLOR 0xd0d0d000

class Fl_Image_Button: public Fl_Button {
private:
  Fl_Image* upimage;
  Fl_Image* downimage;
  Fl_Image* dimimage;
public:
  Fl_Image* up_image();
  Fl_Image* down_image();
  void up_image(Fl_Image* p_up_image);
  void down_image(Fl_Image* p_down_image);
  void draw();
  Fl_Image_Button(int a,int b,int c,int d,const char* e);
  Fl_Image_Button(int a,int b,int c,int d);
  int handle(int e);
};

#endif
