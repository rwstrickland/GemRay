/*
 * stone.h
 *
 * Written by Robert W. Strickland
 * Revised Wed Nov 10 16:50:50 CST 2010
 *         Mon Jan  3 20:54:59 CST 2011 findfacetp()
 */

//using namespace std;

#include <cmath>
#include <iostream>
#include <fstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846 // for Microsoft VC++; On most systems it's in math.h
#endif

// Enumeration for type of a facet

enum FACETTYPE {NO_SUCH_FACET=1, TOP, BOTTOM, SIDE};

// Enumeration for type of ray: Inside stone, outside stone, or already known
// to be outside the stone but inside the perimeter of stone.

enum RAYTYPE {INSIDE=1, OUTSIDE, INPERIM};
enum AXIS {XAXIS=0, YAXIS, ZAXIS};

#define SCL_ZPOS 1
#define SCL_ZNEG 2

//
// A point in 3 space
//
class Vertex
{
  friend class Facet;
  friend class Stone;
public:
protected:
  double x, y, z;
  double dist;
};

//
// A ray is used to represent a light ray or line of sight. A ray has an
// endpoint and a direction vector.  The endpoint is (x, y, z) and the
// direction vector is (a, b, c).  The direction vector (a, b, c) is a
// unit vector, so a*a + b*b + c*c = 1.
//
class Ray
{
  friend class Stone;
protected:
  double a, b, c; // unit vector along ray
  double x, y, z; // endpoint
public:
  void setdir(double aa, double bb, double cc) {
    a = aa; b = bb; c = cc;
  }
  void setdir(Ray & r) {
    a = r.a; b = r.b; c = r.c;
  }
  // Set ray from endpoint (x0, y0, z0) in the dirction of point (x1, y1, z1)
  void setray(double x0, double y0, double z0, double x1, double y1, double z1) {
    double dx = x1 - x0;
    double dy = y1 - y0;
    double dz = z1 - z0;
    double len = sqrt(dx*dx + dy*dy + dz*dz);
    x = x0;
    y = y0;
    z = z0;
    if (len == 0.) {
      a = 0.;
      b = 0.,
      c = -1.;
    }
    else {
      a = dx/len;
      b = dy/len;
      c = dz/len;
    }
  }
  void setend(double xx, double yy, double zz) {
    x = xx; y = yy; z = zz;
  }
  void getend(double *xx, double *yy, double *zz) {
    *xx = x; *yy = y; *zz = z;
  }
  void setend(Ray & r) {
    x = r.x; y = r.y; z = r.z;
  }
  void moveEnd(double dist) {
    x += dist*a; y += dist*b; z += dist*c;
  }
  double lensq() {
    return a*a + b*b + c*c;
  }
  double len() {
    return sqrt(lensq());
  }
  double geta() { return a; }
  double getb() { return b; }
  double getc() { return c; }
  bool isunit();
};

//
// Equation of plane of facet is a*x + b*y + c*z = d where |(a,b,c)| = 1
// (Sign convention on d is opposite that of most references.)
//
class Facet
{
  friend class Stone;
public:
  void setName(char *s);
  void setInst(char *s);
  void clear(void);
  void calcdir(void);
  Facet();
  ~Facet();
protected:
  double a, b, c, d; // equation of facet
  AXIS dir; // 0, 1, or 2 for largest of (a, b, c)
  unsigned int nvind; // number of vertices or edges
  unsigned int *vinds; // list of index numbers of vertices. These index
                       // the Stone's vertpool
  char *name; // facet's name
  char *inst; // tier's cutting instructions
};

//
// Class Stone encapsulates a convex faceted gemstone as a polyhedron.
// There are dynamic arrays of Facets and Vertexes (vertices). It would
// be have been more standard to implement these using the Vector class
// template.
//
class Stone
{
public:
  Stone(double size=1.5);
  Stone(const Stone &);
  Stone& operator=(const Stone &);
  ~Stone();
  void normals();
  int check();
  int newFacet(double a, double b, double c, double d);
  int newPolarFacet(double azi, double incl, double rho, int flip = 0);
  int readAscFile(std::ifstream & ascfile);
  int readGemFile(FILE *bfile);
  int readGemFile(std::ifstream & bfile);
  int openfile(const char *fname);
  void print(std::ostream &os);
  unsigned int interp(unsigned int v1, unsigned int v2);
  int allocVertex(double x, double y, double z);
  int newVertex(double x, double y, double z);
  int findfacet(Ray &r, RAYTYPE key, double *dp = NULL, double *dray = NULL);
  int findfacetp(int hint, Ray &r, RAYTYPE key, double *dp = NULL, double *dray = NULL);
  bool checksym(int sx, int sy, int sz); // symmetry detector
  bool checkxysym(void); // x-y symmetry detector
  void calcdir(void);
  FACETTYPE facettype(unsigned int i);
  // Light ray refraction and reflection
  int ref__ct(int jf, Ray& r, Ray& rr, double dp, double *t, double ri);
  double getRefractiveIndex() { return r_i; }
  void rotate(AXIS axis, double angle);
  void rotate(double x, double y, double z, double angle);
  void zscale(unsigned int key, double factor);
  bool infacet(int f, double x, double y, double z);
  int tablefacet();
  double getFacetAngle(int i);
  double getFacetArea(int i, double *xyarea);
  double getPavilionAngle();
  double getCrownAngle();
protected:
  unsigned int nfacet; // number of facets
  unsigned int maxfacet; // length of facet pool
  Facet *facetpool; // the pool of facets
  unsigned int nvert; // number of vertices
  unsigned int maxvert; // length of vertex pool
  Vertex *vertpool; // the pool of vertices
  //
  // Parameters
  //
  int igear; // index gear
  double gear_off; // bottom number of index gear
  double r_i; // refractive index
  int nsym; // degree of radial symmetry
  bool mirror_sym; // mirror-image symmetry?
  //
  // Header and footnote
  //
  char *header[4];
  char *footnote[4];
};
