/**
 * raytracer.h
 *
 * Written by Robert W. Strickland
 * Revised Mon Jan  3 21:36:14 CST 2011
 *
 * class Raytracer
 * encapsulates a raytracing renderer for a gemstone. When this object
 * is created, it opens and reads a GemCad .gem or .asc file. The renderer
 * is then called once for each frame. The renderer render() does either
 * a tilt or rotation or an angle transformation.
 *
 * The Raytracer class  has a pointer to a Stone object and data for the
 * random lighting model.
 */

typedef unsigned char uchar;

class Raytracer
{
public:
  Raytracer(char *fname, double rid = 0., double cod = 0., int np=17);
  Raytracer(Stone *ps, double rid = 0., double cod = 0., int np=17);
  ~Raytracer();
  void init(double rid = 0., double cod = 0., int np=17);
  int render(int nx, int ny, double headha, double zeye,
    unsigned int kolor, unsigned int bkgkolor,
    unsigned int leakkolor, unsigned int headkolor,
    double gamma, double gain,
    double crn, double pav,
    double b0, double b1, double b2, double b3, double *brightness,
    uchar *iso_p, uchar *cos_p, uchar *sc2_p, uchar *rnd_p);
protected:
  int nspot; // number of spotlights
  double *xspot; // x coordinate of direction normal of spotlight
  double *yspot; // y coordinate of direction normal of spotlight
  double *zspot; // z coordinate of direction normal of spotlight
  double *bspot; // intensity of spotlight
  double coshead; // cosine of head shadow half angle
  int npatch; // number of patches in x and y directions for random
  double **patch; // grid of intensity values for random light model
  double thresh1, thresh2; // spotlight thresholds
  Stone *ns;       // the stone
  bool delstone;   // true if constructor allocated Stone object
  int nri;         // number of refractive indices
  double ri[3];    // refractive indices
  uchar *iso_bits; // RGB image for iso light model
  uchar *rnd_bits; // RGB image for random light model
  uchar *cos_bits; // RGB image for cosine light model
  uchar *sc2_bits; // RGB image for sc2 light model
  int tablefacet;  // index of table facet or -1 if no table
private:
  void sky_init(double nstone);
  double skypat(Ray& ray, int symkey);
  void perimeter(Stone *s, int nx, int ny,
    int ileft[], int iright[], unsigned int kolor, unsigned int bkgkolor, double zeye);
  void normcolor(double *r, double *g, double *b, double gain, double gamma);
public:
  Stone *getStone() { return ns; };
};
