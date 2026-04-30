#ifndef ENGINE_H
#define ENGINE_H
//#define BKG_COLOR 0xF0F0FF00
#define BKG_COLOR 0xF0F0F000
int engine(int xx, int yy, Stone *stone, const char *fname,
    unsigned int kolor, unsigned int bkgkolor,
    unsigned int leakkolor, unsigned int headkolor,
     double gamma, double gain,
     double ri, double cod, double xhead, double zeye, double xmaxtilt, double xtiltinc,
     double xelev, int sz, double xcrn, double xpav, double *objective);

int oengine(Stone *stone, const char *fname, double *wtfac, double crngrad,
    unsigned int kolor, unsigned int bkgkolor,
    unsigned int leakkolor, unsigned int headkolor,
    double gamma, double gain,
    double ri, double cod, double xhead, double zeye, double xmaxtilt, double xtiltinc,
    double xelev, int sz, double xcrn, double xpav, double *objective);
#endif
