/**
 * raytracer.cpp
 *
 * Written by Robert W. Strickland
 * Revised Wed Nov 10 16:51:33 CST 2010 RWS
 *         Wed Dec  8 01:41:34 CST 2010 RWS dispersion
 *         Fri Dec 10 09:32:05 CST 2010 RWS repackaging
 *         Mon Jan  3 17:35:52 CST 2011 RWS ray/polygon intersection
 *         Tue Mar 27 14:55:00 CDT 2012 RWS dsp light model makeover
 *         Fri Mar 30 17:12:56 CDT 2012 RWS dsp model gone
 *
 * This program renders images of a faceted gemstone using the technique
 * of raytracing. For each pixel of the image, a single light ray is traced
 * backward from the viewpoint to the gemstone. Partial reflections are
 * considered.
 *
 * Images for several different light models are calculated simultaneously.
 *
 * Iso        iso - uniform light coming from above the stone
 * Random     rnd - a showcase light model with light and dark areas
 * Cosine     cos - bright at the zenith and dark at the horizon
 * SC2        sc2 - 2*sin*cos bright at 45 degrees, dark at zenith and horizon
 * Outline    out - permimeter of stone, mostly for debugging
 *
 * The constructor reads a GemCad style .gem or .asc file.
 * then render() method renders a single frame.
 *
 * The refractive index and coefficient of dispersion are set by the
 * constructor. If the dispersion is nonzero, three sets of light rays
 * for red, green, and blue light are tracked, so the calculation takes
 * three times as long. These could be moved to the render() method,
 * but the random light model uses the refractive index to initialize
 * the light intensities to normalize the image for different refractive
 * indices. If the refractive index is set to zero, the refractive index
 * is taken from the .gem or .asc file
 *
 * Light models have a head shadow. This is a black circle above the stone
 * that minimally models the light blocked by the viewer's head.
 */
#include <iostream>
#include <fstream>
#include <cmath>
#include <cstring>
#include <cstdlib> // atof() and atoi()
#define NDEBUG // to turn off assertions
#include <cassert>

#include "stone.h" // gemstone geometry and GemCad stuff
#include "raytracer.h"

#define MAXRAYS 10 // number of partial internal reflections followed

#define APPROX_ATTEN // use faster attenuation look-up table

/**
 * Constructor
 * fname is name of GemCad .asc or .gem file
 * RI is index of refraction, 1.54 for quartz, etc
 * COD is the coefficient of diffraction. See below.
 * np is the number of patches in x and y directions for random
 * lighting model. OK to hardwire to 17.
 *
 * Only fname is required.
 * if RI is zero, use RI from GemCad file.
 */
Raytracer::Raytracer(char *fname, double RI, double COD, int np)
{
  int iret = 0;
  ns = NULL;
  patch = NULL;

  int l = strlen(fname) - 4;
  const char *ext = (l > 0) ? fname+l : "";

  if (!strcmp(ext, ".asc") || !strcmp(ext, ".Asc") || !strcmp(ext, ".ASC")) {
//  std::cout << "Found asc extension" << std::endl;
//    std::cout << "Extension: " << p << std::endl;
//    std::cout << "Base name: " << fname << std::endl;
    std::ifstream f(fname);
    if (f.is_open()) {
      ns = new Stone(2.0);
      iret = ns->readAscFile(f);
      f.close();
    }
    else
      iret = 1;
  }
  else if (!strcmp(ext, ".gem") || !strcmp(ext, ".Gem") ||
           !strcmp(ext, ".GEM")) {
//  std::cout << "Found gem extension" << std::endl;
    std::ifstream f;
    f.open(fname, std::ios::binary);
    if (f.is_open()) {
      ns = new Stone(2.0);
      iret = ns->readGemFile(f);
      f.close();
    }
    else
      iret = 1;
    /*
        FILE *fp = fopen(fname, "rb");
        if (fp == NULL) {
          iret = 1;
        }
        else {
          ns = new Stone(2.0);
          if (ns->readGemFile(fp))
            iret = 2;
          fclose(fp);
        }
    */
  }
  else
    iret = 1;

  if (iret) {
    std::cerr << "Error reading " << fname << std::endl;
    if (ns != NULL)
      delete ns;
    ns = NULL;
    return;
  }
  delstone = true;
  init(RI, COD, np);
  if (ns->check() != 0) {
    std::cerr << "Stone flunked consistency check" << std::endl;
    return;
  }
}

/**
 * Constructor
 * ps is a pointer to a Stone
 * RI is index of refraction, 1.54 for quartz, etc
 * COD is the coefficient of diffraction. See below.
 * np is the number of patches in x and y directions for random
 * lighting model. OK to hardwire to 17.
 *
 * Only fname is required.
 * if RI is zero, use RI from GemCad file.
 */
Raytracer::Raytracer(Stone *ps, double RI, double COD, int np)
{
  ns = ps;
  patch = NULL;

  if (ns->check() != 0) {
    std::cerr << "Stone flunked consistency check" << std::endl;
    return;
  }
  init(RI, COD, np);
  delstone = false;
}

/**
  initialization for constructors
*/
void Raytracer::init(double RI, double COD, int np)
{
  tablefacet = ns->tablefacet();
  if (np == 0)
    np = 17;
  npatch = np;
  patch = new double*[np];
  for (int i = 0; i < np; i++)
    patch[i] = new double[np];

  nspot = 20;
  xspot = new double[nspot];
  yspot = new double[nspot];
  zspot = new double[nspot];
  bspot = new double[nspot];
  if (RI == 0.) {
    RI = ns->getRefractiveIndex();
//  std::cout << "Refractive index from file = " << RI << std::endl;
  }
  if (COD == 0.) { // no dispersion
    nri = 1;
    ri[0] = RI;
//  std::cout << "No dispersion" << std::endl;
  }
  else { // dispersion
    /*
     * Refractive index is defined at D line of sodium, 589.3 nm
     * This wavelength is yellow-orange.
     *
     * cod, coefficient of dispersion, is definded as the difference in
     * refractive index between wavelengths 430.8 nm and 686.7 nm
     *
     * In the optical range, refractive index vs. wavelength is given by
     * n = A + B / (wavelength**2)
     *
     * Given refractive index at sodium D and cod, we can solve for
     * A and B and get refractive index at any wavelength, and in particular,
     * red, green and blue.
     */

    double B = COD*306033.;
    double A = RI - B / (589.3*589.3);
    nri = 3;
    ri[0] = A + B / (650.*650.); // red 650 nm
    ri[1] = A + B / (510.*510.); // green 510 nm
    ri[2] = A + B / (475.*475.); // blue 475 nm
//  printf("Refractive index rgb = %.3f %.3f %.3f\n",
//         ri[0], ri[1], ri[2]);
  }
  sky_init(RI); // initialize sky pattern for random light model
}

Raytracer::~Raytracer()
{
  if (delstone && ns != NULL) delete ns;
  if (patch != NULL) {
//  printf("Deleting patch\n");
    for (int i = 0; i < npatch; i++)
      delete[] patch[i];
    delete[] patch;
  }
  if (xspot != NULL) delete[] xspot;
  if (yspot != NULL) delete[] yspot;
  if (zspot != NULL) delete[] zspot;
  if (bspot != NULL) delete[] bspot;
}

/*
 * Initialize sky pattern for random light model.
 * This sets up a 2-d array of random intensity values. The
 * x and y components of the exit ray's unit normal are used to
 * interpolate between the points of this array of intensities using
 * bilinear interpolation. Only a cirular portion of the square
 * is used. This represents a hemisphere infinitely far away from the
 * stone.
 *
 * To this random, mottled pattern. Spotlights are added.
 */
void Raytracer::sky_init(double nstone)
{
  int i, j;
  double gn;
  double offset = 0.5;
//int r;

  gn = (1.0 + sqrt(1.54*nstone)) / 2.2; // make high RI brighter
//offset = 0.6;
//gn *= 0.4;
  offset = 0.2;
  for (j = 0; j < npatch; j++) {
    for (i = 0; i < npatch; i++) {
      patch[i][j] = offset + gn * rand() / RAND_MAX;
    }
  }
  patch[npatch/2][npatch/2] = 0.; // the viewer reflects no light
  /*
   * Initialize spotlights
   */
  for (i = 0; i < nspot; i++) {
    double x, y, z;
    do {
      // generate random point on half sphere
      z = rand() / (double) RAND_MAX;
      double a = rand()*2.*M_PI/RAND_MAX;
      double r = sqrt(1.-z*z);
      x = r*cos(a);
      y = r*sin(a);
    } while (fabs(fabs(x)-fabs(y))< 0.1 || z < 0.3 || z > 0.94 || (y > -0.1 && fabs(x) < 0.4) ||
             fabs(x) < 0.05 || fabs(y) < 0.05);

    // the first expression blocks out a rectangle representing the viewer
    // the second prevents lights on the x axis
    // the third prevents lights on th y axis
    // the fourth prevents radii outside the unit circle so sqrt arg below
    // is positive
    xspot[i] = x;
    yspot[i] = y;
    zspot[i] = sqrt(1.-x*x-y*y); // unit vector
    bspot[i] = 15.; // 15x brighter
//  std::cout << "Spotlight " << i << " x=" << x << " y=" << y << " intensity=" << bspot[i] << std::endl;
//    std::cout << "length^2=" << x*x+y*y+zspot[i]*zspot[i] << std::endl;
  }
}

/*
 * Determine intensity of exit ray for random light model.
 * If the stone is has symmetry, only part of the image is rendered
 * and reflected. For example, if the stone has 4-fold, mirror-image
 * symmetry, only an eighth of the image is rendered and the other 7/8 is
 * just the 1/8 reflected. The random lighting model is not axisymmetric,
 * so the intensity of each reflection must be considered.
 */
double Raytracer::skypat(Ray& r, int part)
{
  double px, py;
  double x, y, z;
  double y0, y1;
  int i, j;
  double b;

  z = r.getc();
  if (z <= 0.)
    return 0.; // no light coming from behind stone
  x = r.geta(); // x component of unit normal
  y = r.getb(); // y component of unit normal
  double t;
  switch (part) { // To take advantage of any bilateral 180 degree rotational symmetry in the model
  default: 
    break;
  case 1:
    x = -x;
    break;
  case 2:
    y = -y;
    break;
  case 3:
    x = -x;
    y = -y;
    break;
  case 4:
    t = x;
    x = y;
    y = t;
    break;
  case 5:
    t = x;
    x = -y;
    y = t;
    break;
  case 6:
    t = x;
    x = y;
    y = -t;
    break;
  case 7:
    t = x;
    x = -y;
    y = -t;
    break;
  }
  /*
   * Bilinear interpolation
   */
  double fx, fy;
  fx = (npatch-1.)*(x+1.)/2.;
  fy = (npatch-1.)*(y+1.)/2.;
  i = (int) fx;
  j = (int) fy;
  if (i > npatch-2)
    i = npatch-2;
  if (j > npatch-2)
    j = npatch-2;
  if (i < 0)
    i = 0;
  if (j < 0)
    j = 0;
  px = fx - i;
  py = fy - j;
  y0 = (1.-px)*patch[i][j] + px*patch[i+1][j];
  y1 = (1.-px)*patch[i][j+1] + px*patch[i+1][j+1];
  b = (1.-py) * y0 + py*y1;

  /*
   * If dot product of the ray's unit normal and spotlight direction vector is
   * near 1, then use spotlight's intensity. This makes round spotlights.
   */
  for (i = 0; i < nspot; i++) {
    double len = xspot[i]*x + yspot[i]*y + zspot[i]*z;
    if (len > 1.0)
      std::cerr << "Length = " << len << " > 1" << std::endl;
    if (len > 0.999) // 2.56 deg cone
      return bspot[i];
  }
  return b;
}

/*
 * Find the perimeter or silhoette of the stone. For each horizontal raster
 * find the left and right extent of the stone in pixels
 */
void Raytracer::perimeter(Stone *s, int nx, int ny,
                          int ileft[], int iright[], unsigned int kolor, unsigned int bkgkolor, double zeye)
{
  int j;
  int ii, jj;
  Ray ray0;
  int icen;
  icen = 0;
  int jstart, jstop, jstep;
  int istart, istop, itest;
  double x, y;

  for (jj = 0; jj < 2; jj++) { // top half then bottom half
    icen = nx/2;
    jstart = (jj == 0) ? ny/2 : ny/2-1;
    jstop = (jj == 0) ? ny-1 : 0;
    jstep = (jj == 0) ? 1 : -1;
    for (j = jstart; j*jstep <= jstop*jstep; j += jstep) {
      y = (j - ny/2 + 0.5) / (0.4*ny);
      if (icen == -1) {
        ileft[j] = -1;
        iright[j] = -1;
        continue;
      }
      for (ii = 0; ii < 2; ii++) { // right half then left half
        istart = icen;
        istop = (ii == 0) ? nx-1 : 0;
        x = (istart - nx/2 + 0.5) / (0.4*nx);
        ray0.setdir(0., 0., -1.); // orthographic
        ray0.setend(x, y, 100.); // orthographic
        ray0.setray(0., 0., zeye, x, y, 0.); // perspective
        /*
         * For stones with sharp points, it would be preferable to
         * look at a pixel on each side instead of giving up
         */
        if (s->findfacetp(0, ray0, OUTSIDE) == -1) {
          ileft[j] = -1;
          iright[j] = -1;
          break;
        }
        x = (istop - nx/2 + 0.5) / (0.4*nx);
        ray0.setdir(0., 0., -1.); // orthographic
        ray0.setend(x, y, 100.); // orthographic
        ray0.setray(0., 0., zeye, x, y, 0.); // perspective
        if (s->findfacetp(0, ray0, OUTSIDE) != -1) {
          std::cerr << "Problem: istop ray inside stone perimeter" << std::endl;
          ileft[j] = -1;
          iright[j] = -1;
          break;
        }
        while (abs(istart-istop) > 1) {
          itest = (istart + istop)/2;
          x = (itest - nx/2 + 0.5) / (0.4*nx);
          ray0.setdir(0., 0., -1.); // orthographic
          ray0.setend(x, y, 100.); // orthographic
          ray0.setray(0., 0., zeye, x, y, 0.); // perspective
          if (s->findfacetp(0, ray0, OUTSIDE) == -1) {
            istop = itest;
          }
          else {
            istart = itest;
          }
        }
        if (ii == 0) {
          iright[j] = istart;
        }
        else {
          ileft[j] = istart;
        }
      }
      icen = (ileft[j] + iright[j])/2;
    }
  }
}

/**
int Raytracer::render(int nx, int ny, double headha, double zeye,
  unsigned int kolor, unsigned int bkgkolor,
  unsigned int leakkolor, unsigned int headkolor,
  double gamma, double gain,
  double crn, double pav,
  double b0, double b1, double b2, double b3, double *brightness)

int nx, int ny image size, number of pixels in the x and y directions
double headha  head shadow half-angle. The angle from the z axis to the
               edge of the cone representing the viewer's head, degrees.
double zeye    viewpoint height above center of stone in GemCad units

Colors:

unsinged int kolor
               24 bit integer RGB stone color. This is the color of the
               rough stone when viewed on a white background. The cut
               stone will typically appear darker.

unsigned int bkgkolor
               24 bit ingeger RGB background color

unsigned int leakkolor
               24 bit ingeger RGB color for leaked (windowed) rays

unsigned int headkolor
               24 bit ingeger RGB color for head shadowed rays

Tangent ratio scaling:

double a0      Old crown reference angle
double a1      New crown reference angle
double a2      old pavilion reference angle
double a3      New pavilion reference angle

Tilt or rotation:

double b0      (b0, b1, b2) are the (x, y, z)...
double b1      components of the unit vector of...
double b2      the axis of rotation
double b3      The rotation amount in degrees

double *brightness   calculated brightness value, percent, or NULL for none

   brightness[0] = ISO brightness
   brightness[1] = COS brightness
   brightness[2] = SC2 brightness
   brightness[3] = ISO table brightness
   brightness[4] = COS table brightness
   brightness[5] = SC2 table brightness

Z axis rotation is performed before X or Y axis tilt. Angles in degrees.

Returns
   0 no error

Colors are specified by 24 bit RGB values.:
The most significant two nybbles are red, next green and least
significant are blue.  Each nybble can range from 00x00 to 0xFF,
zero to 255.
*/
int Raytracer::render(int nx, int ny, double headha, double zeye,
                      unsigned int kolor, unsigned int bkgkolor,
                      unsigned int leakkolor, unsigned int headkolor,
                      double gamma, double gain,
                      double crn, double pav,
                      double b0, double b1, double b2, double b3,
                      double *brightness,
                      uchar *rnd_bits,
                      uchar *sc2_bits,
                      uchar *cos_bits,
                      uchar *iso_bits)
{
  if (ns == NULL)
    return 2;
  Stone *s = new Stone(*ns); // copy stone so tilt or angle transform is local
  // Tangent ratio
  if (crn != 0. && crn != 1.) // crown
    s->zscale(SCL_ZPOS, crn);
  if (pav != 0. && pav != 1.) // pavilion
    s->zscale(SCL_ZNEG, pav);
  // Rotate
  if (b3 != 0.)
    s->rotate(b0, b1, b2, b3);
  //
  // The stone's color is translated to three attenuation coefficients.
  // These are proportional to the log of the saturation.
  // The color that the user chooses is similar to the color of the
  // rough when viewed against a sheet of white paper. This is converted
  // to an attenuation factor for red, green and blue. This is similar
  // to the color on the highlights, but most cuts will appear darker
  // in color that the color the user picks.
  //
  double kfac[3];
//std::cout << "Stone color rgb=";
  kfac[0] = (kolor >> 16) & 0xff; // mask red
//std::cout << kfac[0] << " ";
  kfac[1] = (kolor >> 8) & 0xff; // mask green
//std::cout << kfac[1] << " ";
  kfac[2] = kolor & 0xff; // mask blue
//std::cout << kfac[2] << std::endl;
  int i, j;
  for (i = 0; i < 3; i++) {
    if (kfac[i] == 0.)
      kfac[i] = 0.06;
    else
//    kfac[i] = -0.75/log(kfac[i]/255.); // was too dark
      kfac[i] = -2.00/log(kfac[i]/255.);
  }
  uchar bkgRed = (bkgkolor >> 16) & 0xff;
  uchar bkgGrn = (bkgkolor >>  8) & 0xff;
  uchar bkgBlu =  bkgkolor        & 0xff;

  double leak[3];
  leak[0] = ((double) ((leakkolor >> 16) & 0xff)) / 255.;
  leak[1] = ((double) ((leakkolor >>  8) & 0xff)) / 255.;
  leak[2] = ((double) ( leakkolor        & 0xff)) / 255.;

  double head[3];
  head[0] = ((double) ((headkolor >> 16) & 0xff)) / 255.;
  head[1] = ((double) ((headkolor >>  8) & 0xff)) / 255.;
  head[2] = ((double) ( headkolor        & 0xff)) / 255.;

  int rgb;

#ifdef APPROX_ATTEN
  /*
   * Light rays decay exponentially inside the stone. For speed, we use a
   * lookup table to avoid calculating this exponential.
   */
#define NATTEN 300 // color attenuation lookup table size
#define ATTEN_SCALE 100.
  double atten[NATTEN][3];

  for (i = 0; i < NATTEN; i++) {
    double x = (double) i / (double) ATTEN_SCALE;
    for (rgb = 0; rgb < 3; rgb++)
      atten[i][rgb] = exp(-x/kfac[rgb]);
  }

#endif // def APPROX_ATTEN

  Ray ray0; // incident and refracted ray
  Ray ray1; // partially reflected ray

  double dp; // dot product
  double dist; // distance

  /*
   * Make sure origin is inside stone perimiter
   */
  double x, y;
  x = 0.;
  y = 0.;
  ray0.setdir(0., 0., -1.); // orthographic
  ray0.setend(x, y, 100.); // orthographic
  ray0.setray(0., 0., zeye, x, y, 0.); // perspective
  int f = s->findfacetp(0, ray0, OUTSIDE);
  if (f == -1) {
    std::cerr << "Origin not inside stone" << std::endl;
    double dp, dist;
    f = s->findfacet(ray0, OUTSIDE, &dp, &dist);
    if (f == -1) {
      std::cerr << "fatal" << std::endl;
      return 1;
    }
    std::cerr << "Recovered" << std::endl;
  }
  /*
   * Determine symmetry of the stone. If the stone is symmetric
   * or antisymmetric, only a portion of the rays are calculated,
   * and they are rotated or mirrored. This can speed up the calculation
   * up to a factor of 8 for stones with 4-fold, mirror-image symmetry.
   */
  bool xsym = s->checksym(-1, 1, 1);
  bool ysym = s->checksym(1, -1, 1);
  bool xandysym = xsym && ysym;
  bool sym4m = xandysym && s->checkxysym();
/*
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
*/

  int *ileft = new int[ny];
  int *iright = new int[ny];

  perimeter(s, nx, ny, ileft, iright, kolor, bkgkolor, zeye);
  std::cout << "Found perimeter" << std::endl;
  coshead = cos(headha*M_PI/180.); // headha = head shadow half angle, deg.

  double iso_brightness = 0.;
  double cos_brightness = 0.;
  double sc2_brightness = 0.;
  double iso_table_brightness = 0.;
  double cos_table_brightness = 0.;
  double sc2_table_brightness = 0.;
  unsigned numhit = 0;
  unsigned numhit_table = 0;
  int istart, jstart;
  istart = xsym ? nx/2 : 0;
  jstart = ysym ? ny/2 : 0;
//long bitsize = 3L*nx*ny;
  /*
   * Initialize facet hit hints used by findfacetp(). It's likely that
   * adjacent pixels rays hit same sequence of facets
   */
  int hint[MAXRAYS][3];
  for (int ii = 0; ii < MAXRAYS; ii++)
    for (int jj = 0; jj < nri; jj++)
      hint[ii][jj] = 0;
  /*
   * Go down image, one raster (row) at a time
   */
  for (j = jstart; j < ny; j++) {
    int jmir = ny-j-1;
    y = (j - ny/2 + 0.5) / (0.4*ny);
    if (sym4m)
      istart = jmir;
    /*
     * Go across image raster, one pixel at a time
     */
    for (i = istart; i < nx; i++) {
      int imir = nx-i-1;
      x = (i - nx/2 + 0.5) / (0.4*nx);
      bool is_table = false;
      /*
       * The intensities for the light rays for the light models
       * are maintained separately.
       * The iso, cos, and sc2 calculations are based on an
       * axisymmetric light model.
       * For the axisymmetric light models,
       * the RGB and colorless stone intensities are calculated
       * separately.
       * For the rnd light model, we have
       * to keep track of all the symmetry reflections simultaneously.
       * The second index of rndInten is for the different symmetry
       * reflections.
       */
      double isoInten[4] = {0., 0., 0., 0.};
      double cosInten[4] = {0., 0., 0., 0.};
      double sc2Inten[4] = {0., 0., 0., 0.};
      double rndInten[4][8] = {{0.,0.,0.,0.,0.,0.,0.,0.},
        {0.,0.,0.,0.,0.,0.,0.,0.},
        {0.,0.,0.,0.,0.,0.,0.,0.},
        {0.,0.,0.,0.,0.,0.,0.,0.}
      };
      double g[8];
      if ((ileft[j] == -1 && iright[j] == -1) ||
          i < ileft[j] || i > iright[j]) {
        /*
         * Then outside stone perimeter. Set pixel to background color
         */
        long int k;
        k = 3L*((long) i + j*nx);
        assert(k >= 0 && k < bitsize);
        iso_bits[k] = bkgRed;
        rnd_bits[k] = bkgRed;
        cos_bits[k] = bkgRed;
        sc2_bits[k] = bkgRed;
        k++;
        assert(k >= 0 && k < bitsize);
        iso_bits[k] = bkgGrn;
        rnd_bits[k] = bkgGrn;
        cos_bits[k] = bkgGrn;
        sc2_bits[k] = bkgGrn;
        k++;
        assert(k >= 0 && k < bitsize);
        iso_bits[k] = bkgBlu;
        rnd_bits[k] = bkgBlu;
        cos_bits[k] = bkgBlu;
        sc2_bits[k] = bkgBlu;
        if (xsym) {
          k = 3L*((long) imir + nx*j);
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgRed;
          rnd_bits[k] = bkgRed;
          cos_bits[k] = bkgRed;
          sc2_bits[k] = bkgRed;
          k++;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgGrn;
          rnd_bits[k] = bkgGrn;
          cos_bits[k] = bkgGrn;
          sc2_bits[k] = bkgGrn;
          k++;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgBlu;
          rnd_bits[k] = bkgBlu;
          cos_bits[k] = bkgBlu;
          sc2_bits[k] = bkgBlu;
        }
        if (ysym) {
          k = 3L*((long) i + nx*jmir);
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgRed;
          rnd_bits[k] = bkgRed;
          cos_bits[k] = bkgRed;
          sc2_bits[k] = bkgRed;
          k++;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgGrn;
          rnd_bits[k] = bkgGrn;
          cos_bits[k] = bkgGrn;
          sc2_bits[k] = bkgGrn;
          k++;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgBlu;
          rnd_bits[k] = bkgBlu;
          cos_bits[k] = bkgBlu;
          sc2_bits[k] = bkgBlu;
        }
        if (xandysym) {
          k = 3L*((long) imir + nx*jmir);
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgRed;
          rnd_bits[k] = bkgRed;
          cos_bits[k] = bkgRed;
          sc2_bits[k] = bkgRed;
          k++;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgGrn;
          rnd_bits[k] = bkgGrn;
          cos_bits[k] = bkgGrn;
          sc2_bits[k] = bkgGrn;
          k++;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgBlu;
          rnd_bits[k] = bkgBlu;
          cos_bits[k] = bkgBlu;
          sc2_bits[k] = bkgBlu;
        }
        if (sym4m) {
          k = 3L*((long) j + nx*i);
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgRed;
          rnd_bits[k] = bkgRed;
          cos_bits[k] = bkgRed;
          sc2_bits[k] = bkgRed;
          k++;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgGrn;
          rnd_bits[k] = bkgGrn;
          cos_bits[k] = bkgGrn;
          sc2_bits[k] = bkgGrn;
          k++;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgBlu;
          rnd_bits[k] = bkgBlu;
          cos_bits[k] = bkgBlu;
          sc2_bits[k] = bkgBlu;

          k = 3L*((long) jmir + nx*i);
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgRed;
          rnd_bits[k] = bkgRed;
          cos_bits[k] = bkgRed;
          sc2_bits[k] = bkgRed;
          k++;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgGrn;
          rnd_bits[k] = bkgGrn;
          cos_bits[k] = bkgGrn;
          sc2_bits[k] = bkgGrn;
          k++;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgBlu;
          rnd_bits[k] = bkgBlu;
          cos_bits[k] = bkgBlu;
          sc2_bits[k] = bkgBlu;

          k = 3L*((long) j + nx*imir);
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgRed;
          rnd_bits[k] = bkgRed;
          cos_bits[k] = bkgRed;
          sc2_bits[k] = bkgRed;
          k++;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgGrn;
          rnd_bits[k] = bkgGrn;
          cos_bits[k] = bkgGrn;
          sc2_bits[k] = bkgGrn;
          k++;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgBlu;
          rnd_bits[k] = bkgBlu;
          cos_bits[k] = bkgBlu;
          sc2_bits[k] = bkgBlu;

          k = 3L*((long) jmir + nx*imir);
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgRed;
          rnd_bits[k] = bkgRed;
          cos_bits[k] = bkgRed;
          sc2_bits[k] = bkgRed;
          k++;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgGrn;
          rnd_bits[k] = bkgGrn;
          cos_bits[k] = bkgGrn;
          sc2_bits[k] = bkgGrn;
          k++;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = bkgBlu;
          rnd_bits[k] = bkgBlu;
          cos_bits[k] = bkgBlu;
          sc2_bits[k] = bkgBlu;
        }
        continue;
      }
      /*
       * Loop through the number of refractive indices
       */
      double firstdp, firstx, firsty, firstz;
      int firstf;
      for (int iri = 0; iri < nri; iri++) {
        ray0.setdir(0., 0., -1.); // orthographic
        ray0.setend(x, y, 100.); // orthographic
        ray0.setray(0., 0., zeye, x, y, 0.); // perspective
        if (iri == 0) {
          /*
           * Find which facet ray interects
           */
          f = s->findfacetp(hint[0][0], ray0, INPERIM, &dp, &dist);
          if (f == -1) {
//          std::cout << "findfacetp(INPERIM) found none" << std::endl;
            f = s->findfacet(ray0, INPERIM, &dp, &dist);
            if (f == -1)
              std::cout << "Bad news: findfacet(INPERIM) found none either" << std::endl;
//          else // debug
//            (void) s->findfacetp(-f, ray0, INPERIM, &dp, &dist);
          }
          is_table = (tablefacet != -1 && tablefacet == f);
          hint[0][0] = f;
          ray0.moveEnd(dist); // move ray endpoint to facet
//          double xt, yt, zt;
//          ray0.getend(&xt, &yt, &zt);
//          if (!s->infacet(f, xt, yt, zt))
//            std::cout << "Point in poly test failed" << std::endl;
          firstf = f;
          firstdp = dp;
          ray0.getend(&firstx, &firsty, &firstz);
        }
        else { // first facet hit does not depend on refractive index
          f = firstf;
          dp = firstdp;
          ray0.setend(firstx, firsty, firstz);
        }
        /*
         * Refract/reflect ray
         */
        double r, t;
        int outside;
        outside = s->ref__ct(f, ray0, ray1, dp, &t, ri[iri]);
        /*
         * Caclulate surface reflection (ray1)
         */
        r = 1.-t;
        ray1.setend(ray0);
        double d = ray1.getc();
        if (d > 0. && d < coshead) { // upwards and outside head shadow
          double e = 1.-d*d;
          e = (e < 0.) ? 0. : 2.*d*sqrt(e); // sc2 model rolloff
          g[0] = e * skypat(ray1, 0);
          if (xsym)
            g[1] = e * skypat(ray1, 1);
          if (ysym)
            g[2] = e * skypat(ray1, 2);
          if (xandysym)
            g[3] = e * skypat(ray1, 3);
          if (sym4m) {
            g[4] = e * skypat(ray1, 4);
            g[5] = e * skypat(ray1, 5);
            g[6] = e * skypat(ray1, 6);
            g[7] = e * skypat(ray1, 7);
          }
          if (nri == 1) {
            for (rgb = 0; rgb < 4; rgb++) {
              if (rgb == 3 && s->facettype(f) != TOP)
                break; // bezel mount for brightness metrics
              isoInten[rgb] += r;
              cosInten[rgb] += r * d;
              sc2Inten[rgb] += r * e;
              if (rgb == 3)
                break;
              rndInten[rgb][0] += r * g[0];
              if (xsym)
                rndInten[rgb][1] += r * g[1];
              if (ysym)
                rndInten[rgb][2] += r * g[2];
              if (xandysym)
                rndInten[rgb][3] += r * g[3];
              if (sym4m) {
                rndInten[rgb][4] += r * g[4];
                rndInten[rgb][5] += r * g[5];
                rndInten[rgb][6] += r * g[6];
                rndInten[rgb][7] += r * g[7];
              }
            }
          }
          else {
            isoInten[iri] += r;
            cosInten[iri] += r * d;
            sc2Inten[iri] += r * e;
            if (iri == 0 && s->facettype(f) != TOP) { // bezel mount
              isoInten[3] += r;
              cosInten[3] += r * d;
              sc2Inten[3] += r * e;
            }
            rndInten[iri][0] += r * g[0];
            if (xsym)
              rndInten[iri][1] += r * g[1];
            if (ysym)
              rndInten[iri][2] += r * g[2];
            if (xandysym)
              rndInten[iri][3] += r * g[3];
            if (sym4m) {
              rndInten[iri][4] += r * g[4];
              rndInten[iri][5] += r * g[5];
              rndInten[iri][6] += r * g[6];
              rndInten[iri][7] += r * g[7];
            }
          }
        }
        else { /* head shadow or leak */
          int headshadow = (d >= coshead);
          if (nri == 1) {
            for (rgb = 0; rgb < 3; rgb++) { // metrics have black head shadow
              if (rgb == 3 && s->facettype(f) != TOP)
                break; // bezel mount for brightness metrics
              double h = headshadow ? r*head[rgb] : r*leak[rgb];
              isoInten[rgb] += h;
              cosInten[rgb] += h;
              sc2Inten[rgb] += h;
              for (int m = 0; m < 8; m++)
                rndInten[rgb][m] += h;
            }
          }
          else {
            double h = headshadow ? r*head[iri] : r*leak[iri];
            isoInten[iri] += h;
            cosInten[iri] += h;
            sc2Inten[iri] += h;
            for (int m = 0; m < 8; m++)
              rndInten[rgb][m] += h;
          }
        }

//        std::cout << " surface reflection value " << r << std::endl;
        /*
         * The inten_val[4] array stores intensity values for red, green,
         * blue and white light. The white light value is for the clear
         * stone brightness value.
         */
        double inten_val[4] = {t, t, t, t}; // start with white light
        for (unsigned int raynum = 1; raynum < MAXRAYS; raynum++) {
          /*
           * Find which facet ray intersects
           */
          f = s->findfacetp(hint[raynum][iri], ray0, INSIDE, &dp, &dist);
          if (f == -1) {
//          std::cout << "findfacetp(INSIDE) found none" << std::endl;
            f = s->findfacet(ray0, INSIDE, &dp, &dist);
            if (f == -1)
              std::cout << "Bad news: findfacet(INSIDE) found none either" << std::endl;
//          else
//            (void) s->findfacetp(-f, ray0, INSIDE, &dp, &dist);
          }
          for (int ctr = iri; ctr < nri; ctr++)
            hint[raynum][ctr] = f;
          ray0.moveEnd(dist);
          dist = fabs(dist);
//          double xt, yt, zt;
//          ray0.getend(&xt, &yt, &zt);
//          if (!s->infacet(f, xt, yt, zt))
//            std::cout << "Point in poly test failed" << std::endl;
//          std::cout << "Ray " << raynum << " hit facet " << f << std::endl;

          /*
           * Attenuate intensity by color of stone. Leave inten_val[3]
           * alone for clear stone brightness calculation.
           * Convert distance inside stone to integer for lookup table
           */
#ifdef APPROX_ATTEN
          int idist = (int) (ATTEN_SCALE*dist + 0.5);
          if (idist > NATTEN-1)
            idist = NATTEN-1;
#endif
          if (nri == 1) {
            for (rgb = 0; rgb < 3; rgb++) {
#ifdef APPROX_ATTEN
              /*
               * faster (but approx.) lookup table for atten. factor
               * vs length
               */
              inten_val[rgb] *= atten[idist][rgb];
#else
              inten_val[rgb] *= exp(-dist/kfac[rgb]); // exact
#endif
            }
          }
          else if (iri < 3) {
            if (iri == 0)
              // duplicate unattenuated red for brightness numbers
              inten_val[3] = inten_val[0];
#ifdef APPROX_ATTEN
            /*
             * faster (but approx.) lookup table for atten factor
             *  vs length
             */
            inten_val[iri] *= atten[idist][iri];
#else
            inten_val[iri] *= exp(-dist/kfac[iri]); // exact
#endif
          }
          outside = s->ref__ct(f, ray0, ray1, dp, &t, ri[iri]);
          if (outside) { // evaluate refracted (exiting) ray
//            std::cout << "refracted ray fraction " << t << std::endl;
            double c = 1.0;                    // iso
            double dot = ray0.getc();
            double d = dot;                    // cos
            double e = 1.-d*d;
            e = (e < 0.) ? 0. : 2.*d*sqrt(e);  // sc2
            if (d > 0. && d <= coshead) { // toward viewer && not head shadow
              g[0] = e * skypat(ray0, 0);
              if (xsym)
                g[1] = e * skypat(ray0, 1);
              if (ysym)
                g[2] = e * skypat(ray0, 2);
              if (xandysym)
                g[3] = e * skypat(ray0, 3);
              if (sym4m) {
                g[4] = e * skypat(ray0, 4);
                g[5] = e * skypat(ray0, 5);
                g[6] = e * skypat(ray0, 6);
                g[7] = e * skypat(ray0, 7);
              }
            }
            if (nri == 1) { // no dispersion
              for (rgb = 0; rgb < 4; rgb++) {
                double ss = t * inten_val[rgb];
                if (d > 0. && d <= coshead) { // toward viewer && not head shadow
                  isoInten[rgb] += ss * c;
                  cosInten[rgb] += ss * d;
                  sc2Inten[rgb] += ss * e;
                  if (rgb < 3) {
                    rndInten[rgb][0] += ss * g[0];
                    if (xsym)
                      rndInten[rgb][1] += ss * g[1];
                    if (ysym)
                      rndInten[rgb][2] += ss * g[2];
                    if (xandysym)
                      rndInten[rgb][3] += ss * g[3];
                    if (sym4m) {
                      rndInten[rgb][4] += ss * g[4];
                      rndInten[rgb][5] += ss * g[5];
                      rndInten[rgb][6] += ss * g[6];
                      rndInten[rgb][7] += ss * g[7];
                    }
                  }
                }
                else if (d > coshead) {
                  if (rgb < 3) {
                    isoInten[rgb] += ss * head[rgb];
                    cosInten[rgb] += ss * head[rgb];
                    sc2Inten[rgb] += ss * head[rgb];
                    for (int m = 0; m < 8; m++)
                      rndInten[rgb][m] += ss * head[rgb];
                  }
                }
                else { // ray points away; use background color
                  if (rgb < 3) {
                    isoInten[rgb] += ss * leak[rgb];
                    cosInten[rgb] += ss * leak[rgb];
                    sc2Inten[rgb] += ss * leak[rgb];
                    for (int m = 0; m < 8; m++)
                      rndInten[rgb][m] += ss * leak[rgb];
                  }
                }
              }
            }
            else { // dispersion
              double ss = t * inten_val[iri];
              if (d > 0. && d <= coshead) { // toward viewer && not head shadow
                isoInten[iri] += ss * c;
                cosInten[iri] += ss * d;
                sc2Inten[iri] += ss * e;
                if (iri == 0) {
                  isoInten[3] += ss * c;
                  cosInten[3] += ss * d;
                  sc2Inten[3] += ss * e;
                }
                rndInten[iri][0] += ss * g[0];
                if (xsym)
                  rndInten[iri][1] += ss * g[1];
                if (ysym)
                  rndInten[iri][2] += ss * g[2];
                if (xandysym)
                  rndInten[iri][3] += ss * g[3];
                if (sym4m) {
                  rndInten[iri][4] += ss * g[4];
                  rndInten[iri][5] += ss * g[5];
                  rndInten[iri][6] += ss * g[6];
                  rndInten[iri][7] += ss * g[7];
                }
              }
              else if (d > coshead) {
                isoInten[iri] += ss * head[iri];
                cosInten[iri] += ss * head[iri];
                sc2Inten[iri] += ss * head[iri];
                for (int m = 0; m < 8; m++)
                  rndInten[iri][m] += ss * head[iri];
              }
              else { // ray points away; use background color
                isoInten[iri] += ss * leak[iri];
                cosInten[iri] += ss * leak[iri];
                sc2Inten[iri] += ss * leak[iri];
                for (int m = 0; m < 8; m++)
                  rndInten[iri][m] += ss * leak[iri];
              }
            }
            /*
             * keep intensity of partially reflected ray
             */
            if (nri == 1) {
              for (rgb = 0; rgb < 4; rgb++)
                inten_val[rgb] *= (1.-t);
            }
            else {
              inten_val[iri] *= (1.-t);
            }
            if (inten_val[3] < 0.001) // partially reflected ray too dim?
              break;
            /*
             * Continue to follow partially reflected ray
             */
            outside = false;
            ray0.setdir(ray1);
          }
        }
      }
      iso_brightness += isoInten[3];
      cos_brightness += cosInten[3];
      sc2_brightness += sc2Inten[3];
      if (is_table) {
        iso_table_brightness += isoInten[3];
        cos_table_brightness += cosInten[3];
        sc2_table_brightness += sc2Inten[3];
      }
      numhit++;
      if (is_table) numhit_table++;
      /*
       * Plot pixel
       */
      int icol;
      long int k;
      uchar isobyte[4];
      uchar cosbyte[4];
      uchar sc2byte[4];
      normcolor(&isoInten[0], &isoInten[1], &isoInten[2], gain, gamma);
      normcolor(&cosInten[0], &cosInten[1], &cosInten[2], gain, gamma);
      normcolor(&sc2Inten[0], &sc2Inten[1], &sc2Inten[2], gain, gamma);
      for (icol = 0; icol < 3; icol++) {
        isobyte[icol] = (uchar) (isoInten[icol] * 255. + 0.5);
        cosbyte[icol] = (uchar) (cosInten[icol] * 255. + 0.5);
        sc2byte[icol] = (uchar) (sc2Inten[icol] * 255. + 0.5);
      }
      normcolor(&rndInten[0][0], &rndInten[1][0], &rndInten[2][0], gain, gamma);
      for (icol = 0; icol < 3; icol++) {
        k = 3L*((long) i + nx*j) + icol;
        assert(k >= 0 && k < bitsize);
        iso_bits[k] = isobyte[icol];
        cos_bits[k] = cosbyte[icol];
        sc2_bits[k] = sc2byte[icol];
        rnd_bits[k] = (uchar) (rndInten[icol][0] * 255. + 0.5);
      }
      if (xsym) {
        normcolor(&rndInten[0][1], &rndInten[1][1], &rndInten[2][1], gain, gamma);
        for (icol = 0; icol < 3; icol++) {
          k = 3L*((long) imir + nx*j) + icol;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = isobyte[icol];
          cos_bits[k] = cosbyte[icol];
          sc2_bits[k] = sc2byte[icol];
          rnd_bits[k] = (uchar) (rndInten[icol][1] * 255. + 0.5);
        }
      }
      if (ysym) {
        normcolor(&rndInten[0][2], &rndInten[1][2], &rndInten[2][2], gain, gamma);
        for (icol = 0; icol < 3; icol++) {
          k = 3L*((long) i + nx*jmir) + icol;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = isobyte[icol];
          cos_bits[k] = cosbyte[icol];
          sc2_bits[k] = sc2byte[icol];
          rnd_bits[k] = (uchar) (rndInten[icol][2] * 255. + 0.5);
        }
      }
      if (xandysym) {
        normcolor(&rndInten[0][3], &rndInten[1][3], &rndInten[2][3], gain, gamma);
        for (icol = 0; icol < 3; icol++) {
          k = 3L*(imir + nx*jmir) + icol;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = isobyte[icol];
          cos_bits[k] = cosbyte[icol];
          sc2_bits[k] = sc2byte[icol];
          rnd_bits[k] = (uchar) (rndInten[icol][3] * 255. + 0.5);
        }
      }
      if (sym4m) {
        normcolor(&rndInten[0][4], &rndInten[1][4], &rndInten[2][4], gain, gamma);
        for (icol = 0; icol < 3; icol++) {
          k = 3L*((long) j + nx*i) + icol;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = isobyte[icol];
          cos_bits[k] = cosbyte[icol];
          sc2_bits[k] = sc2byte[icol];
          rnd_bits[k] = (uchar) (rndInten[icol][4] * 255. + 0.5);
        }
        normcolor(&rndInten[0][5], &rndInten[1][5], &rndInten[2][5], gain, gamma);
        for (icol = 0; icol < 3; icol++) {
          k = 3L*((long) jmir + nx*i) + icol;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = isobyte[icol];
          cos_bits[k] = cosbyte[icol];
          sc2_bits[k] = sc2byte[icol];
          rnd_bits[k] = (uchar) (rndInten[icol][5] * 255. + 0.5);
        }
        normcolor(&rndInten[0][6], &rndInten[1][6], &rndInten[2][6], gain, gamma);
        for (icol = 0; icol < 3; icol++) {
          k = 3L*((long) j + nx*imir) + icol;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = isobyte[icol];
          cos_bits[k] = cosbyte[icol];
          sc2_bits[k] = sc2byte[icol];
          rnd_bits[k] = (uchar) (rndInten[icol][6] * 255. + 0.5);
        }
        normcolor(&rndInten[0][7], &rndInten[1][7], &rndInten[2][7], gain, gamma);
        for (icol = 0; icol < 3; icol++) {
          k = 3L*((long) jmir + nx*imir) + icol;
          assert(k >= 0 && k < bitsize);
          iso_bits[k] = isobyte[icol];
          cos_bits[k] = cosbyte[icol];
          sc2_bits[k] = sc2byte[icol];
          rnd_bits[k] = (uchar) (rndInten[icol][7] * 255. + 0.5);
        }
      }
    }
  }
//std::cout << "numhit = " << numhit << std::endl;
  iso_brightness /= (double) numhit;
  cos_brightness /= (double) numhit;
  sc2_brightness /= (double) numhit;
  if (numhit_table > 0) {
    iso_table_brightness /= (double) numhit_table;
    cos_table_brightness /= (double) numhit_table;
    sc2_table_brightness /= (double) numhit_table;
  }
  printf("Average brightness (colorless):\n");
  printf("SC2:%7.2f\n", sc2_brightness*100.);
  printf("COS:%7.2f\n", cos_brightness*100.);
  printf("ISO:%7.2f\n", iso_brightness*100.);
  printf("Average table brightness (colorless):\n");
  printf("SC2:%7.2f\n", sc2_table_brightness*100.);
  printf("COS:%7.2f\n", cos_table_brightness*100.);
  printf("ISO:%7.2f\n", iso_table_brightness*100.);
  delete[] ileft;
  delete[] iright;
  delete s;
  if (brightness != NULL) {
    brightness[0] = 100.*iso_brightness;
    brightness[1] = 100.*cos_brightness;
    brightness[2] = 100.*sc2_brightness;
    brightness[3] = 100.*iso_table_brightness;
    brightness[4] = 100.*cos_table_brightness;
    brightness[5] = 100.*sc2_table_brightness;
  }
  return 0;
}

/**
 If one component is bigger than 1, fiddle to try to preserve hue
 (Didn't work. Just clip to one.)
*/
void Raytracer::normcolor(double *r, double *g, double *b,
    double gain, double gamma)
{
  *r = pow(*r*gain, gamma);
  *g = pow(*g*gain, gamma);
  *b = pow(*b*gain, gamma);
  if (*r > 1.0) *r = 1.0;
  if (*g > 1.0) *g = 1.0;
  if (*b > 1.0) *b = 1.0;
  return;
  /*
    if (*r > 0.8) *r = 0.2/(*r + 0.2);
    if (*g > 0.8) *g = 0.2/(*g + 0.2);
    if (*b > 0.8) *b = 0.2/(*b + 0.2);
  */
}
