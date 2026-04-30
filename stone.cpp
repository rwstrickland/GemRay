/**
 * stone.cpp
 *
 * Written by Robert W. Strickland
 * Revised Wed Nov 10 16:52:10 CST 2010
 *         Mon Jan  3 20:51:10 CST 2011 findfacetp
 *
 * This software models faceted gemstones.
 * Convex polyhedra, flat facets.
 * It reads files made by GemCad for Windows
 * www.gemcad.com 
 */

#include <iostream>
#include <fstream>
#include <cmath>
#include <cstring>
#include <vector>
#include <assert.h>
#include <stdlib.h> // atof() and atoi()
#include <stdint.h> // int32_t

#include "stone.h"

#define BAD_VERT 32767
#define EPSILON 1.e-12

/*
 * Is the ray a unit vector?
 */
bool
Ray::isunit()
{
  double l2 = lensq();
  return 0.9999 < l2 && l2 < 1.0001; 
}

/**
 * Facet default constructor
 */
Facet::Facet()
{
//std::cout << "Facet default constructor" << std:std::endl;
  nvind = 0;
  vinds = NULL;
  name = NULL;
  inst = NULL;
}

/*
 * Stone copy constructor
 */
Stone::Stone(const Stone &s)
{
//printf("Stone copy constructor\n");
  maxfacet = s.nfacet;
  maxvert = s.nvert;
  nfacet = s.nfacet;
  nvert = s.nvert;

  facetpool = new Facet[maxfacet];
  vertpool = new Vertex[maxvert];

  memcpy(facetpool, s.facetpool, nfacet * sizeof(Facet));
  memcpy(vertpool, s.vertpool, nvert * sizeof(Vertex));

  igear = s.igear;
  gear_off = s.gear_off;
  r_i = s.r_i;
  nsym = s.nsym;
  mirror_sym = s.mirror_sym;
  unsigned int i;
  for (i = 0; i < 4; i++) {
    char *p;
    if  (s.header[i] == NULL) {
      p = NULL;
    }
    else {
      p = new char[strlen(s.header[i])+1];
      strcpy(p, s.header[i]);
    }
    header[i] = p;;
  }
  for (i = 0; i < 4; i++) {
    char *p;
    if  (s.footnote[i] == NULL) {
      p = NULL;
    }
    else {
      p = new char[strlen(s.footnote[i])+1];
      strcpy(p, s.footnote[i]);
    }
    footnote[i] = p;;
  }
  //
  // Since pointers were copied, need to duplicate strings so freeing one
  // won't free source. Deep copy of name, inst, and vind
  //
  for (i = 0; i < nfacet; i++) {
    char *p;
    Facet *f = facetpool + i;
    if (f->name != NULL)  {
      p = f->name;
      f->name = NULL;
      f->setName(p);
    }
    if (f->inst != NULL) {
      p = f->inst;
      f->inst = NULL;
      f->setInst(p);
    }
    if (f->vinds != NULL) {
//    printf("New %d vinds line %d in Stone copy constructor\n",
//        f->nvind, __LINE__);
      unsigned int *v = new unsigned int[f->nvind];
      for (unsigned int j = 0; j < f->nvind; j++)
        v[j] = f->vinds[j];
      f->vinds = v;
    }
  }
}

/*
 * Stone assignment operator
 */
Stone& Stone::operator =(const Stone &s)
{
  if (this == &s)
    return *this;
  if (facetpool != NULL) {
    delete[] facetpool;
    facetpool = NULL;
    nfacet = 0;
    maxfacet = 0;
  }
  if (vertpool != NULL) {
    delete[] vertpool;
    vertpool = NULL;
    nvert = 0;
    maxvert = 0;
  }
  unsigned int i;
  for (i = 0; i < 4; i++) {
    if (header[i] != NULL) {
      delete header[i];
      header[i] = NULL;
    }
    if (footnote[i] != NULL) {
      delete footnote[i];
      footnote[i] = NULL;
    }
  }

  maxfacet = s.nfacet;
  maxvert = s.nvert;
  nfacet = s.nfacet;
  nvert = s.nvert;

  facetpool = new Facet[maxfacet];
  vertpool = new Vertex[maxvert];

  memcpy(facetpool, s.facetpool, nfacet * sizeof(Facet));
  memcpy(vertpool, s.vertpool, nvert * sizeof(Vertex));

  igear = s.igear;
  gear_off = s.gear_off;
  r_i = s.r_i;
  nsym = s.nsym;
  mirror_sym = s.mirror_sym;
  for (i = 0; i < 4; i++) {
    char *p;
    if  (s.header[i] == NULL) {
      p = NULL;
    }
    else {
      p = new char[strlen(s.header[i])+1];
      strcpy(p, s.header[i]);
    }
    header[i] = p;;
  }
  for (i = 0; i < 4; i++) {
    char *p;
    if  (s.footnote[i] == NULL) {
      p = NULL;
    }
    else {
      p = new char[strlen(s.footnote[i])+1];
      strcpy(p, s.footnote[i]);
    }
    footnote[i] = p;;
  }
  /*
   * Since pointers were shallow copied, need to duplicate strings so freeing
   * one won't free source. Deep copy of name, inst, and vind
   */
  for (i = 0; i < nfacet; i++) {
    char *p;
    Facet *f = facetpool + i;
    if (f->name != NULL)  {
      p = f->name;
      f->name = NULL;
      f->setName(p);
    }
    if (f->inst != NULL) {
      p = f->inst;
      f->inst = NULL;
      f->setInst(p);
    }
    if (f->vinds != NULL) {
//    printf("New %d vinds line %d in Stone assignment operator\n",
//        f->nvind, __LINE__);
      unsigned int *v = new unsigned int[f->nvind];
      for (unsigned int j = 0; j < f->nvind; j++)
        v[j] = f->vinds[j];
      f->vinds = v;
    }
  }
  return *this;
}

/*
 * Stone constructor to create a cube 2*size units on a side
 */
Stone::Stone(double size)
{
  maxfacet = 100;
  nfacet = 6; // cube has 6 facets
  maxvert = 100;
  nvert = 8; // and 8 vertices

  unsigned int i;
  for (i = 0; i < 4; i++) {
    header[i] = NULL;
    footnote[i] = NULL;
  }

  facetpool = new Facet[maxfacet];
//for (i = 0; i < maxfacet; i++)
//  facetpool[i].clear();
  for (i = 0; i < nfacet; i++) {
    facetpool[i].nvind = 4;
    facetpool[i].vinds = new unsigned int[4];
  }
  vertpool = new Vertex[maxvert];

  vertpool[0].x =  size;
  vertpool[0].y =  size;
  vertpool[0].z =  size;

  vertpool[1].x =  size;
  vertpool[1].y =  size;
  vertpool[1].z = -size;

  vertpool[2].x = -size;
  vertpool[2].y =  size;
  vertpool[2].z = -size;

  vertpool[3].x = -size;
  vertpool[3].y =  size;
  vertpool[3].z =  size;

  vertpool[4].x =  size;
  vertpool[4].y = -size;
  vertpool[4].z =  size;

  vertpool[5].x =  size;
  vertpool[5].y = -size;
  vertpool[5].z = -size;

  vertpool[6].x = -size;
  vertpool[6].y = -size;
  vertpool[6].z = -size;

  vertpool[7].x = -size;
  vertpool[7].y = -size;
  vertpool[7].z =  size;

  for (i = 0; i < 8; i++)
    vertpool[i].dist = 0.;

  facetpool[0].vinds[0] = 0;
  facetpool[0].vinds[1] = 1;
  facetpool[0].vinds[2] = 2;
  facetpool[0].vinds[3] = 3;

  facetpool[1].vinds[0] = 7;
  facetpool[1].vinds[1] = 6;
  facetpool[1].vinds[2] = 5;
  facetpool[1].vinds[3] = 4;

  facetpool[2].vinds[0] = 0;
  facetpool[2].vinds[1] = 4;
  facetpool[2].vinds[2] = 5;
  facetpool[2].vinds[3] = 1;

  facetpool[3].vinds[0] = 5;
  facetpool[3].vinds[1] = 6;
  facetpool[3].vinds[2] = 2;
  facetpool[3].vinds[3] = 1;

  facetpool[4].vinds[0] = 6;
  facetpool[4].vinds[1] = 7;
  facetpool[4].vinds[2] = 3;
  facetpool[4].vinds[3] = 2;

  facetpool[5].vinds[0] = 0;
  facetpool[5].vinds[1] = 3;
  facetpool[5].vinds[2] = 7;
  facetpool[5].vinds[3] = 4;

  normals();
//print(std::cout);

//std::cout << "leaving default Stone() constructor" << std::endl;
}

/*
 * Facet destructor
 */
Facet::~Facet()
{
//printf("Deleting facet nvind = %d\n", nvind);
  fflush(stdout);
  if (nvind > 0) {
    assert(vinds != NULL);
//  printf("deleting %d vinds line %d in facet destructor\n", nvind, __LINE__);
//  fflush(stdout);
    nvind = 0;
    delete[] vinds;
    vinds = NULL;
  }
  if (name != NULL)
    delete[] name;
  if (inst != NULL)
    delete[] inst;
}

/*
 * Stone destructor
 */
Stone::~Stone()
{
//printf("Stone destructor\n");
  if (facetpool != NULL) {
//  for (int i = 0; i < maxfacet; i++) {
//    if (i == nfacet) printf("Unused:\n");
//    Facet *f = &facetpool[i];
//    printf("Facet %d has %d vertices\n", i, f->nvind);
//  }
//  printf("Deleting facet pool\n");
    delete[] facetpool;
    facetpool = NULL;
    nfacet = 0;
    maxfacet = 0;
  }
  if (vertpool != NULL) {
    delete[] vertpool;
    vertpool = NULL;
    nvert = 0;
    maxvert = 0;
  }
  unsigned int i;
  for (i = 0; i < 4; i++) {
    if (header[i] != NULL) {
      delete[] header[i];
      header[i] = NULL;
    }
    if (footnote[i] != NULL) {
      delete[] footnote[i];
      footnote[i] = NULL;
    }
  }
}

/*
 * Calculate facet plane coefficients (unit normals) from vertices
 * Plane equation is:
 * a*x + b*y + c*z = d
 */
void
Stone::normals()
{
  if (nfacet == 0)
    return;
  unsigned int i;
//std::cout << nfacet << " facets" << std::endl;
  for (i = 0; i < nfacet; i++) {
    Facet *f = &facetpool[i];
    unsigned int nvind = f->nvind;
//  std::cout << "facet " << i << " has " << nvind << " vertices" << std::endl;
    if (nvind == 0)
      continue;
/*
 *    calculate twice area projected on coordinate planes.
 */
    double a, b, c, d;
    a = 0.;
    b = 0.;
    c = 0.;
    unsigned int j;
    Vertex *v, *vp;
    vp = &vertpool[f->vinds[nvind-1]];
    for (j = 0; j < nvind; j++){
      v = &vertpool[f->vinds[j]];
//    std::cout << "vertex " << j << " " << v->x << "," << v->y << "," << v->z << std::endl;
      a += (vp->z + v->z) * (vp->y - v->y);
      b += (vp->x + v->x) * (vp->z - v->z);
      c += (vp->y + v->y) * (vp->x - v->x);
      vp = v;
    }
    d = sqrt(a*a + b*b + c*c);
    /*
     * Unit normals
     */
    if (d != 0.) {
      a /= d;
      b /= d;
      c /= d;
    }
    d = 0.;
    for (j = 0; j < f->nvind; j++){
      v = &vertpool[f->vinds[j]];
      d += a*v->x + b*v->y + c*v->z;
    }
    d /= nvind;
    f->a = a;
    f->b = b;
    f->c = c;
    f->d = d;
//  std::cout << a << "," << b << "," << c << "," << d << std::endl;
  }
  calcdir();
}

/*
 * Calculate approximate direction of each facet
 */
void Stone::calcdir(void)
{
  for (unsigned int i = 0; i < nfacet; i++) {
    Facet *f = facetpool + i;
    f->calcdir();
  }
}

/*
 * Calculate approximate direction of facet
 */
void Facet::calcdir(void)
{
  double fa = fabs(a);
  double fb = fabs(b);
  double fc = fabs(c);
  if (fa >= fb && fa >= fc)
    dir = XAXIS;
  else if (fb >= fa && fb >= fc)
    dir = YAXIS;
  else
    dir = ZAXIS;
}

/*
 * Put a new vertex in the vertPool. Enlarge the pool if needed.
 * Return the index of the new vertex.
 *
 * Could instead use Vector class template.
 */
int
Stone::allocVertex(double x, double y, double z)
{
  if (nvert >= maxvert-1) {
//  std::cout << "Enlarging vertex pool from " << maxvert << " to ";
    maxvert *= 3;
    maxvert /= 2;
    if (maxvert < 8)
      maxvert = 8;
//  std::cout << maxvert << std::endl;
    Vertex *newvertpool;
    newvertpool = new Vertex[maxvert];
    memcpy(newvertpool, vertpool, nvert*sizeof(Vertex));
    delete[] vertpool;
    vertpool = newvertpool;
  }
  vertpool[nvert].x = x;
  vertpool[nvert].y = y;
  vertpool[nvert].z = z;
  vertpool[nvert].dist = 0.;
//std::cout << "vertex " << nvert << " (" << x << "," << y << "," << z << ") created" << std::endl;

  nvert++;
  return nvert-1;
}

/*
 * Check to see if vertex already in pool. If so, return index to it.
 * If not, put it in the pool.
 */
int
Stone::newVertex(double x, double y, double z)
{
  unsigned int i;
  for (i = 0; i < nvert; i++) {
    Vertex *v = &vertpool[i];
//    if (v->dist != 0.) // only look at recent vertices
//      continue;
    if (fabs(v->x - x) + fabs(v->y - y) + fabs(v->z - z) < 3.*EPSILON) {
      v->dist = 0.;
      return i;
    }
  }
  return allocVertex(x, y, z);
}

/*
 * Set the name of a facet, deleting the old one
 */
void
Facet::setName(char *p)
{
  if (name != NULL) {
//  std::cout << "Existing facet name " << name << " replaced" << std::endl;
    delete[] name;
  }
  name = NULL;
  if (p != NULL && *p != '\0') {
    name = new char[strlen(p)+1];
    strcpy(name, p);
  }
}

/*
 * Set the cutting instructions of a facet, deleting the old ones
 */
void
Facet::setInst(char *p)
{
  if (inst != NULL) {
//  std::cout << "Existing cutting instr replaced: " << inst << std::endl;
    delete[] inst;
    inst = NULL;
  }
  if (p != NULL && *p != '\0') {
    inst = new char[strlen(p)+1];
    strcpy(inst, p);
  }
}

/*
 * Cut a new facet. Return the index of new facet, or -1 if new
 * facet misses stone.
 */
int
Stone::newFacet(double a, double b, double c, double d)
{
//std::cout << "     <press enter>" << std::endl;
//cin.get();
//std::cout << "entering newFacet(a, b, c, d)" << std::endl;
//std::cout << "a,b,c,d == " << a << ',' << b << ',' << c << ',' << d << std::endl;
  unsigned int i, j;
  Vertex *q, *r;
  std::vector <unsigned int> vpool;
//printf("\nCutting facet %d abcd = %10.4g %10.4g %10.4g\n\n", nfacet, a, b, c, d);

  // Calculate distance of new plane to every vertex. Vertices with
  // positive dist will be cut off by new facet.
  for (i = 0; i < nvert; i++) {
    q = &vertpool[i];
    q->dist = a*q->x + b*q->y + c*q->z - d;
//  std::cout << "vertex " << i << " " << q->dist << std::endl;
  }
//print(std::cout, 1);

  unsigned int nfacetcut = 0;

  // Go through facets one at a time and see which are cut off or
  // have vertices cut off
  unsigned int nf_cut_off = 0;
  Facet *f;
  unsigned int nvind, nvind2;
  unsigned int *vinds, *vinds2;
  for (i = 0; i < nfacet; i++) {
//  printf("Test facet %d  ", i);
    f = &facetpool[i];
    nvind = f->nvind;
    vinds = f->vinds;
    if (nvind == 0 || vinds == NULL)
      continue;
    //
    // Determine which facets are cut by new facet
    //
    unsigned int nv_cut_off = 0;
    unsigned int nv_touch = 0;
    for (j = 0; j < nvind; j++) {
      q = &vertpool[vinds[j]];
      if (q->dist > EPSILON)
        nv_cut_off++;
      else if (fabs(q->dist) <= EPSILON) {
        nv_touch++;
        q->dist = 0.;
      }
    }
//  printf(" vertex cut off count: %d", nv_cut_off);
//  printf(" touched count: %d\n", nv_touch);
    if (nv_cut_off == 0) {
//    printf("Facet %d missed by new\n", i);
      continue;
    }
    if (nv_cut_off + nv_touch == nvind) {
      /*
       * then all vertices on this facet are cut off by new facet.
       */
//    printf("Facet %d cut off\n", i);
      if (nv_touch == 2) {
//      printf("Wait: edge case: Still need to add touched edge to new facet's list\n");

        // Edge case where the new facet cuts off an existing facet but intersects one edge.
        // Find the touched edge and add its vertices to the new facet's list
        for (j = 0; j < nvind; j++) {
           q = &vertpool[vinds[j]];
           r = &vertpool[vinds[(j+1)%nvind]];
           if (q->dist == 0. && r->dist == 0.) {
             vpool.push_back(vinds[j]);
             vpool.push_back(vinds[(j+1)%nvind]);
           }
        }
      }
      nf_cut_off++;
      delete[] f->vinds;
      f->vinds = NULL;
      f->nvind = 0;
      if (f->name != NULL) {
        delete[] f->name;
        f->name = NULL;
      }
      if (f->inst != NULL) {
        delete[] f->inst;
        f->inst = NULL;
      }
      continue;
    }

    nfacetcut++;

//  printf("Facet before: x y z dist\n");
    for (j = 0; j < nvind; j++) {
      q = &vertpool[vinds[j]];
//    printf("%10.6f %10.6f %10.6f %12.3e\n", q->x, q->y, q->z, q->dist);
    }

    nvind2 = nvind - nv_cut_off - nv_touch + 2;
//  printf("New %d vinds line %d\n", nvind2, __LINE__);
    vinds2 = new unsigned int[nvind2];
    assert(nv_touch <= 2);
    if (nv_touch == 0) {
      // Find the last vertex (counter-clockwise) that will be cut off
      for (j = 0; j < nvind; j++) {
        q = &vertpool[vinds[j]];
        r = &vertpool[vinds[(j+1) % nvind]];
        if (q->dist > 0. && r->dist < 0.)
          break;
      }
      assert(j < nvind);
      unsigned int vistart = j;  
      
      /*
        Shift vinds so that last cut off is at beginning and...
        if one vertex will be cut off, duplicate it at the end.
        if two will be cut off, keep the first cut off at the end.
        if more than two will be cut off, copy the first cut off
        at the end and do not copy the other cut off verts.
      */

      for (j = 0; j < nvind2; j++)
        vinds2[j] = vinds[(vistart+j) % nvind];

      delete[] f->vinds;
      f->vinds = vinds2;
      f->nvind = nvind2;
      vinds = vinds2;
      nvind = nvind2;

      vinds[0] = interp(vinds[1], vinds[0]);
      vinds[nvind-1] = interp(vinds[nvind-2], vinds[nvind-1]);
    }
    else if (nv_touch == 1) {
      for (j = 0; j < nvind; j++) {
        q = &vertpool[vinds[j]];
        if (q->dist == 0.)
          break;
      }
      assert(j < nvind);
      unsigned int vistart = j;  
      r = &vertpool[vinds[(vistart + 1) % nvind]];
      if (r->dist > 0.) {
//      std::cout << "case 1 touched, A:" << std::endl;
        vistart = (vistart + nv_cut_off) % nvind;

        /* circular shift vinds */

        for (j = 0; j < nvind2; j++)
          vinds2[j] = vinds[(vistart+j)%nvind];

//      printf("Deleting %d vinds line %d for shift 2\n", f->nvind, __LINE__);
        delete[] f->vinds;
        f->vinds = vinds2;
        f->nvind = nvind2;
        vinds = vinds2;
        nvind = nvind2;
        vinds[0] = interp(vinds[0], vinds[1]);
      }
      else {
//      std::cout << "case 1 touched, B:" << std::endl;
        // Shift vinds

        for (j = 0; j < nvind2; j++)
          vinds2[j] = vinds[(vistart+j)%nvind];

//      printf("Deleting %d vinds line %d for shift 3\n", f->nvind, __LINE__);
        delete[] f->vinds;
        f->vinds = vinds2;
        f->nvind = nvind2;
        vinds = vinds2;
        nvind = nvind2;
        vinds[nvind-1] = interp(vinds[nvind-2], vinds[nvind-1]);
      }
    }
    else if (nv_touch == 2) {
//    std::cout << "case 2 touched" << std::endl;
      /*
       * Find the last vertex (counter-clockwise) that will be cut off
       */
      for (j = 0; j < nvind; j++) {
        q = &vertpool[vinds[j]];
        r = &vertpool[vinds[(j+1)%nvind]];
        if (q->dist == 0. && r->dist < 0.)
          break;
      }
//    if (j >= nvind) {
//      fprintf(stderr, "Problem: j=%d >= nvind (%d)\n", j, nvind);
//      fprintf(stderr, "nv_touch = %d, nv_cut_off = %d\n",
//          nv_touch, nv_cut_off);
//      for (j = 0; j < nvind; j++) {
//        q = &vertpool[vinds[j]];
//        if (q->dist == 0.)
//          fprintf(stderr, "vind[%d] = %d, dist = zero\n", j, vinds[j]);
//        else
//          fprintf(stderr, "vind[%d] = %d, dist = %e\n", j, vinds[j], q->dist);
//      }
//    }
      assert(j < nvind);
      unsigned int vistart = j;  
      // Shift vinds

      for (j = 0; j < nvind2; j++)
        vinds2[j] = vinds[(vistart+j)%nvind];

//    printf("Deleting %d vinds line %d for shift 4\n", f->nvind, __LINE__);
      delete[] f->vinds;
      f->vinds = vinds2;
      f->nvind = nvind2;
      vinds = vinds2;
      nvind = nvind2;
    }
//  printf("Facet after: x y z dist\n");
    for (j = 0; j < nvind; j++) {
      q = &vertpool[vinds[j]];
//    printf("%10.6f %10.6f %10.6f %12.3e\n", q->x, q->y, q->z, q->dist);
    }
//  std::cout << "After interpolation, facet " << i << " has " << nvind << " vertices:\n";
//  for (j = 0; j < nvind; j++) {
//    q = &vertpool[vinds[j]];
//    std::cout << vinds[j] << ":" << q->dist << " ";
//  }
//  std::cout << std::endl;

    if (vinds[nvind-2] == vinds[nvind-1]) {
      std::cout << "null right edge with both ends on vertex " << vinds[nvind-1] << std::endl;
      std::cout << "Attempting to repair" << std::endl;
      nvind--;
      f->nvind = nvind;
    }

    if (vinds[0] == vinds[1]) {
      std::cout << "null left edge with both ends on vertex " << vinds[0] << std::endl;
      std::cout << "Attempting to repair" << std::endl;
      nvind--;
      for (j = 0; j < nvind; j++) {
        vinds[j] = vinds[j+1];
      }
      f->vinds = vinds;
      f->nvind = nvind;
    }

    if (vinds[nvind-1] == vinds[0]) {
      std::cout << "Ignoring null new edge with both ends on vertex " << vinds[0] << std::endl;
      nvind--;
      f->nvind = nvind;
      continue;
    }
    /*
     * Store new edge so that new facet can be stitched together.
     * CCW order.
     */
    vpool.push_back(vinds[0]);
    vpool.push_back(vinds[nvind-1]);
  }
  if (nfacetcut == 0) {
//  printf("New facet misses stone\n");
//    std::cout << "new facet misses stone" << std::endl;
    return -1;
  }
  /*
   * Enlarge facetPool if needed
   */

  if (nfacet >= maxfacet-1) {
    std::cout << "Enlarging facet pool from " << maxfacet << " to ";
//  int oldnfacet = maxfacet;
    maxfacet *= 3;
    maxfacet /= 2;
    if (maxfacet < 6)
      maxfacet = 6;
    std::cout << maxfacet << std::endl;
    Facet *newfacetpool;
    newfacetpool = new Facet[maxfacet];
    // Copy old facetpool on top of newfacetpool
    memcpy(newfacetpool, facetpool, nfacet*sizeof(Facet)); // shallow copy
    // Since pointers were copied, we need to NULL the pointers so
    // the destructor doesn't delete them
    unsigned int i;
    for (i = 0; i < nfacet; i++)
      facetpool[i].clear();
    delete[] facetpool;
    facetpool = newfacetpool;
  }
  f = &facetpool[nfacet];
  f->a = a;
  f->b = b;
  f->c = c;
  f->d = d;
  f->name = NULL;
  f->inst = NULL;
  /*
   * Collect all of the edges in the vpool vector, sort them to stitch
   * them together and install them in the new facet's vind array.
   * Each vertex is in the vpool twice, once for each of the two new
   * edges that share it. We need to sort the edges so that the tail of
   * one matches the head of the next and omit the duplicates.
   */
  nvind = vpool.size()/2;
//printf("New %d vinds line %d collect\n", nvind, __LINE__);
  vinds = new unsigned int[nvind];
  f->nvind = nvind;
  f->vinds = vinds;
//std::cout << "nvind = " << nvind << std::endl;
//std::cout << "vpool size = " << vpool.size() << std::endl;
//std::cout << "vpool: ";
//for (i = 0; i < vpool.size(); i++) {
//  if (i > 0 && (i%2) == 0)
//    std::cout << " |";
//  std::cout << " " << vpool[i];
//}
//std::cout << std::endl;
  assert(nvind > 2);
  for (i = 0; i < nvind-1; i++) {
    for (j = i+1; j < nvind; j++) {
      if (vpool[2*i+1] == vpool[2*j]) {
        // swap pair i+1 and pair j
        unsigned int temp = vpool[2*(i+1)];
        vpool[2*(i+1)] = vpool[2*j];
        vpool[2*j] = temp;
        temp = vpool[2*(i+1)+1];
        vpool[2*(i+1)+1] = vpool[2*j+1];
        vpool[2*j+1] = temp;
        break;
      }
    }
  }
//std::cout << "sorted:";
  unsigned int knot = 0;
  for (i = 0; i < vpool.size(); i++) {
    if (i > 0 && (i%2) == 0) {
      if (vpool[i] != vpool[i-1]) {
//      std::cout << " #";
        knot++;
      }
//    else {
//      std::cout << " |";
//    }
    }
//  std::cout << " " << vpool[i];
  }
//std::cout << std::endl;
  if (knot) {
     std::cout << "Knot detectd. After sort\n";
     for (i = 0; i < vpool.size(); i++) {
        std::cout << vpool[i];
        std::cout << " ";
     }
     std::cout << std::endl;
//   print(std::cout);
  }
  assert(!knot);
  /*
   * Copy the sorted vertex indices to the new facets vind  
   */
  for (i = 0; i < nvind; i++) {
    vinds[i] = vpool[2*i];
  }
  
//std::cout << "vinds:";
//for (i = 0; i < nvind; i++) {
//  std::cout << " " << vinds[i];
//}
//std::cout << std::endl;

  nfacet++;
  /*
   * Cleanup: delete cut off facets
   */
//std::cout << "nfacet =" << nfacet << std::endl;
  for (i = 0; i < nfacet; i++) {
//  std::cout << "i=" << i << ";";
    f = &facetpool[i];
    nvind = f->nvind;
//  std::cout << "nvind = " << nvind << std::endl;
    vinds = f->vinds;
    for (j = 0; j < nvind; j++) {
//    std::cout << "vinds[" << j << "]=" << vinds[j] << std::endl;
      q = &vertpool[vinds[j]];
//    if (q->dist > 0.) {
//      std::cout << "Warning 1 : facet " << i << " has vertex " << vinds[j] << " with dist = " << q->dist << " > 0." << std::endl;
//    }
      assert (q->dist <= 0.);
    }
  }

  if (nf_cut_off > 0) {
//  std::cout << "Before deleting " << nf_cut_off << " cut off facet(s)" << std::endl;
//  print(std::cout);
    j = 0;
    for (i = 0; i < nfacet; i++) {
      if (facetpool[i].nvind != 0) {
        facetpool[j] = facetpool[i]; // default assignmet operator:
                                     // memberwise copy
        j++;
      }
    }
    nfacet -= nf_cut_off;
    for (i = 0; i < nf_cut_off; i++) {
      facetpool[i + nfacet].clear();
    }
  }

  for (i = 0; i < nfacet; i++) {
    f = &facetpool[i];
    nvind = f->nvind;
    vinds = f->vinds;
    for (j = 0; j < nvind; j++) {
      unsigned int iv = vinds[j];
//    if (iv >= nvert) {
//      std::cout << "facet " << i << " vinds[" << j << "] bad = " << iv;
//    }
      assert(iv < nvert);
      q = &vertpool[iv];
//    if (q->dist > 0.) {
//      std::cout << "Warning 2 : facet " << i << " has vertex " << vinds[j] << " with dist = " << q->dist << " > 0." << std::endl;
//    }
//    assert(q->dist <= 0.);
    }
  }
  /*
   * Cleanup: delete cut off vertices.
   */
//std::cout << "After deleting cut off facets but before deleting vertices" << std::endl;
//print(std::cout);
  unsigned int *vmap = new unsigned int[nvert];
  j = 0;
  unsigned int nv_cut_off = 0;
  for (i = 0; i < nvert; i++) {
    Vertex *v;
    v = &vertpool[i];
    if (v->dist <= 0.) {
      vmap[i] = j;
      vertpool[j] = vertpool[i];
      j++;
    }
    else {
      vmap[i] = BAD_VERT;
      nv_cut_off++;
    }
  }
//  for (i = 0; i < nvert; i++) {
//    std::cout << "vertex " << i;
//    if (vmap[i] == BAD_VERT)
//      std::cout << " is cut off" << std::endl;
//    else
//      std::cout << " maps to " << vmap[i] << std::endl;
//  }
  nvert -= nv_cut_off;
  unsigned int oops = 0;
  for (i = 0; i < nfacet; i++) {
    f = &facetpool[i];
    for (j = 0; j < f->nvind; j++) {
      unsigned int k = vmap[f->vinds[j]];
      if (k == BAD_VERT) {
//        std::cout << "cut off vertex " << f->vinds[j] << " in facet's vinds" << std::endl;
        oops++;
      }
      f->vinds[j] = k;
    }
  }
  delete[] vmap;

//std::cout << "After deleting vertices & remapping facets" << std::endl;
//print(std::cout, 1);

  assert(!oops);
//std::cout << "--------------------------" << std::endl;
  
//std::cout << "Final check in newFacet(a, b, c, d)" << std::endl;
  assert(check() == 0);
  return nfacet-1;
}

/*
 * Add a new facet to the Stone.
 * Translate from spherical coordinates to cartesian coordinates
 *
 * azi is the azimuth (index angle) in radians
 * incl is the facet inclination angle in radians
 * rho is the center to facet distance
 */
int
Stone::newPolarFacet(double azi, double incl, double rho, int flip)
{
  double a, b, c, d;
  double st, ct, spr, cpr;

  spr = sin(incl)/rho;
  cpr = cos(incl)/rho;
  st = sin(azi);
  ct = cos(azi);
  a = st * spr;
  b = ct * spr;
  c = cpr;
  d = 1./sqrt(a*a + b*b + c*c);
  a *= d;
  b *= d;
  c *= d;
  if (flip && incl < 0.) {
    a = -a;
    b = -b;
    c = -c;
  }
  return newFacet(a, b, c, d);
}

/*
 * v1 and v2 are indices into vertpool. Their dist values should
 * be set prior to call. Returns an index to a possibly new vertex,
 * or an index to an existing vertex if the new vertex is close to
 * an existing one, or may return v1 or v2 if distances are close
 * enough to zero.
 */
unsigned int
Stone::interp(unsigned int v1, unsigned int v2)
{
  if (v1 == v2)
    return v1;

  Vertex *q = &vertpool[v1];
  Vertex *r = &vertpool[v2];
  double den = q->dist - r->dist;
  double p = (fabs(den) < EPSILON) ? 0. : q->dist / den;
  if (fabs(p) < EPSILON) {
    return v1;
  }
  if (fabs(p - 1.0) < EPSILON) {
    return v2;
  }

  double x, y, z;

  x = q->x + p*(r->x - q->x);
  y = q->y + p*(r->y - q->y);
  z = q->z + p*(r->z - q->z);
//  double dist = q->dist + p*(r->dist - q->dist);
//  std::cout << "iterp check: " << dist << std::endl;

  return newVertex(x, y, z);
}

/*
 * Classify a facet
 */
FACETTYPE
Stone::facettype(unsigned int i)
{
  if (i > nfacet)
    return NO_SUCH_FACET;
  Facet *f = facetpool + i;
  if (f->c > 0.0001)
    return TOP; // or crown
  else if (f->c < -0.0001)
    return BOTTOM; // or pavilion
  else
    return SIDE; // or girdle
}

// Debug print to console
void
Stone::print(std::ostream & os)
{
  unsigned int i;
  os << nvert << " vertices\n";
  for (i = 0; i < nvert; i++) {
    Vertex *v;
    v = &vertpool[i];
    os << "vertex " << i << ": " << v->x << ',' << v->y << ',' << v->z << ',' << v->dist << std::endl;
    if (v->dist > 0.)
      std::cout << " <cut off>";
    else if (v->dist == 0.)
      std::cout << " <on new facet>";
    else if (fabs(v->dist) < 1.e-12)
      std::cout << " <fuzz>";
    std::cout << '\n';
  }
  os << nfacet << " facets\n";
  for (i = 0; i < nfacet; i++) {
    Facet *f;
    f = &facetpool[i];
    os << "facet " << i << ": " << f->a << ',' << f->b << ',' << f->c << ',' << f->d << '\n';
    unsigned int j;
    for (j = 0; j < f->nvind; j++) {
      if (f->vinds[j] < 0 || f->vinds[j] > nvert) {
        if (f->vinds[j] == BAD_VERT)
          os << " <BAD_VERT>";
          else
          os << " <" << f->vinds[j] << ">";
      }
      else {
        os << " " << f->vinds[j];
        Vertex *v = &vertpool[f->vinds[j]];
        if (v->dist > 0.)
          os << "*";
      }
    }
    if (f->nvind == 0)
      std::cout << "<cut off>";
    os << std::endl;
  }
}

/**
 Rotate stone about axis of unit vector (x, y, z) by angle in degrees
*/
void
Stone::rotate(double x, double y, double z, double angle)
{
// printf("rotate(%.2f, %.2f, %.2f, %.2f)\n", x, y, z, angle);
  angle *= M_PI/180.; // radians
  double s = sin(angle);
   double c = cos(angle);
  double t = 1. - cos(angle);
  double a00 = t*x*x + c;
   double a01 = t*x*y - s*z;
   double a02 = t*x*z + s*y;
   double a10 = t*x*y + s*z;
   double a11 = t*y*y + c;
   double a12 = t*y*z - s*x;
   double a20 = t*x*z - s*y;
   double a21 = t*y*z + s*x;
   double a22 = t*z*z + c;
/*
   double a01 = -z*s+t*x*y;
  double a02 = y*s+t*x*z;
   double a10 = z*s+t*x*y;
  double a11 = 1. + t*(y*y-1.);
   double a12 = -x*s+t*y*z;
  double a20 = -y*s+t*x*z;
  double a21 = x*s+t*y*z;
  double a22 = 1. + t*(z*z-1.);
*/
  // Rotate facet normals
  for (unsigned int i = 0; i < nfacet; i++) {
    Facet *f = facetpool + i;
    double a = a00 * f->a + a01 * f->b + a02 * f->c;
    double b = a10 * f->a + a11 * f->b + a12 * f->c;
    double c = a20 * f->a + a21 * f->b + a22 * f->c;
    f->a = a;
    f->b = b;
    f->c = c;
  }
  // Rotate vertices
  for (unsigned int i = 0; i < nvert; i++) {
    Vertex *v = vertpool + i;
    double xn = a00 * v->x + a01 * v->y + a02 * v->z;
    double yn = a10 * v->x + a11 * v->y + a12 * v->z;
    double zn = a20 * v->x + a21 * v->y + a22 * v->z;
    v->x = xn;
    v->y = yn;
    v->z = zn;
  }
   normals();
//calcdir();
}

/*
 * Rotate a Stone
 *
 * axis is 0, 1, 2 for x, y, z
 * angle is in degrees
 */
void
Stone::rotate(AXIS axis, double angle)
{
  if (angle == 0. || nfacet == 0 || axis < 0 || axis > 2)
    return;
  double ang = angle * M_PI/180.;
  double st = sin(ang);
  double ct = cos(ang);
  double x, y, z;
  /*
   * Rotate facets
   */
  for (unsigned int i = 0; i < nfacet; i++) {
    Facet *f = facetpool + i;
    x = f->a;
    y = f->b;
    z = f->c;
    switch (axis) {
    case XAXIS:
      f->b = y*ct - z*st;
      f->c = y*st + z*ct;
      break;
    case YAXIS:
      f->c = z*ct - x*st;
      f->a = z*st + x*ct;
      break;
    case ZAXIS:
      f->a = x*ct - y*st;
      f->b = x*st + y*ct;
      break;
    }
  }
  /*
   * Rotate vertices
   */
  for (unsigned int i = 0; i < nvert; i++) {
    Vertex *v = vertpool + i;
    x = v->x;
    y = v->y;
    z = v->z;
    switch (axis) {
    case XAXIS:
      v->y = y*ct - z*st;
      v->z = y*st + z*ct;
      break;
    case YAXIS:
      v->z = z*ct - x*st;
      v->x = z*st + x*ct;
      break;
    case ZAXIS:
      v->x = x*ct - y*st;
      v->y = x*st + y*ct;
      break;
    }
  }
  calcdir();
}

/*
 * Scale (stretch or shrink) a Stone.
 * key = bit field that indicates which part and direction to scale
 * s = scale factor
 */
void
Stone::zscale(unsigned key, double s)
{
  double z, ztopmin, zbotmax;
  ztopmin = 1.e308;
  zbotmax = -1.e308;
  FACETTYPE topbot;
  unsigned int i, j;
  for (i = 0; i < nfacet; i++) {
    topbot = facettype(i);
    if (topbot == SIDE)
      continue;
    Facet *f = facetpool + i;
    unsigned int *vinds = f->vinds;
    for (j = 0; j < f->nvind; j++) {
      Vertex *v = vertpool + vinds[j];
      double z = v->z;
      if (topbot == TOP && z < ztopmin)
        ztopmin = z;
      else if (topbot == BOTTOM && z > zbotmax)
        zbotmax = z;
    }
  }
  Vertex *v;
  Facet *f;
  double a, b, c, d, x0, y0, z0;
  for (i = 0; i < nvert; i++) {
    v = vertpool + i;
    if ((key & SCL_ZPOS) && (key & SCL_ZNEG)) {
      v->z *= s;
    }
    else if (key & SCL_ZNEG) {
      if (v->z < zbotmax) {
        v->z -= zbotmax;
        v->z *= s;
        v->z += zbotmax;
      }
    }
    else if (key & SCL_ZPOS) {
      if (v->z > ztopmin) {
        v->z -= ztopmin;
        v->z *= s;
        v->z += ztopmin;
      }
    }
  }
  for (i = 0; i < nfacet; i++) {
    f = facetpool + i;
    a = f->a;
    b = f->b;
    c = f->c;
    d = f->d;
    x0 = a*d;
    y0 = b*d;
    z0 = c*d;
    if ((key & SCL_ZPOS) && (key & SCL_ZNEG)) {
      z0 *= s;
      c /= s;
    }
    else if (key & SCL_ZPOS && f->c > EPSILON) {
      z0 -= ztopmin;
      z0 *= s;
      z0 += ztopmin;
      c /= s;
    }
    else if (key & SCL_ZNEG && f->c < -EPSILON) {
      z0 -= zbotmax;
      z0 *= s;
      z0 += zbotmax;
      c /= s;
    }
    d = sqrt(a*a + b*b + c*c);
    a /= d;
    b /= d;
    c /= d;
    d = a*x0 + b*y0 + c*z0;
    f->a = a;
    f->b = b;
    f->c = c;
    f->d = d;
  }
  calcdir();
}

/*
 * Clear a facet. This is to prevent the facet destructor from
 * deleting things.
 */
void
Facet::clear(void)
{
  vinds = NULL;
  nvind = 0;
  name = NULL;
  inst = NULL;
}

/*
 * This is a reentrant version of the strtok() library tokenizer
 */
char *
gettok(std::ifstream & ascfile, bool *newtokline, char *tokline, char **ssave);

#define RADIANS(index) (((index) - gear_off) * 2.0 * M_PI / (double) igear)

#define errormsg(s) std::cerr << s << std::endl
#define TOKLINESIZE 512

/*
 * Open GemCad file name based on 3 character extension
 */
int
Stone::openfile(const char *fname)
{
  int iret = 0;

  int l = strlen(fname) - 4;
  const char *ext = (l > 0) ? fname+l : "";

  if (!strcmp(ext, ".asc") || !strcmp(ext, ".Asc") || !strcmp(ext, ".ASC")) {
    std::cout << "Found asc extension" << std::endl;
//    std::cout << "Extension: " << p << std::endl;
//    std::cout << "Base name: " << fname << std::endl;
    std::ifstream f(fname);
    if (f.is_open()) {
      readAscFile(f);
      f.close();
    }
    else
      iret = 1;
  }
  else if (!strcmp(ext, ".gem") || !strcmp(ext, ".Gem") ||
           !strcmp(ext, ".GEM")) {
    std::cout << "Found gem extension" << std::endl;
    std::ifstream f;
    f.open(fname, std::ios::binary);
    if (f.is_open()) {
      readGemFile(f);
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

  return iret;
}

/*
 * Read an ascii .ASC file of a gemstone design as written by GemCad
 */
int
Stone::readAscFile(std::ifstream & ascfile)
{
  char *p;
  double angle, index, azi, incl, rho;
  int sav_nsym = 1;
  bool sav_mirror_sym = false;
  char *s;
  int nheader;
  char **headfoot;
  int badfile;
  double dversion;
  char *tokline;
  bool newtokline;
  char *ssave;
  
  s = new char[512];
  tokline = new char[TOKLINESIZE];
  newtokline = true;
  badfile = 0;
  p = gettok(ascfile, &newtokline, tokline, &ssave);
  if (p == NULL || strcmp(p, "GemCad"))
    badfile = 1;
  if (!badfile) {
    p = gettok(ascfile, &newtokline, tokline, &ssave);
    if (p == NULL)
      badfile = 1;
    if (!badfile) {
      dversion = atof(p);
      if (dversion < 3.99) {
        badfile = 1;
      }
    }
  }
  if (badfile) {
    errormsg("Not a GemCad text file");
    goto freeret;
  }
  newtokline = true;
  nsym = 1;
  mirror_sym = false;
  while ((p = gettok(ascfile, &newtokline, tokline, &ssave)) != NULL) {
    if (p[0] != 'H' && p[0] != 'F')
      nheader = 0;
    switch (p[0]) {
    case 'a':
      if ((p = gettok(ascfile, &newtokline, tokline, &ssave)) != NULL)
        angle = atof(p);
      incl = angle * M_PI / 180.;
      if ((p = gettok(ascfile, &newtokline, tokline, &ssave)) != NULL)
        rho = atof(p);
      rho *= 0.9;
      if (angle < 0.)
        rho = -rho;
      break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
      index = atof(p);
      if (angle == 0.)
        index = 0.;
      azi = RADIANS(index);
      if (rho == 0.) {
        errormsg("Facet through center ignored");
      }
      else {
        newPolarFacet(azi, incl, rho);
      }
      break;
    case 'n':
      if ((p = gettok(ascfile, &newtokline, tokline, &ssave)) != NULL)
        strcpy(s, p);
      if (s[0] != '\0')
        facetpool[nfacet-1].setName(s);
      break;
    case 'G':
      p += 2; // skip G and NULL
      if (*p == '\0')
        errormsg("G but no cutting instructions");
      else
        facetpool[nfacet-1].setInst(p);
      newtokline = true;
      break;
    case 'H':
    case 'F':
      if (*p == 'H')
        headfoot = header;
      if (*p == 'F')
        headfoot = footnote;
      if (nheader < 4) {
        p += 2; // skip H and NULL
        if (*p != '\0') {
          headfoot[nheader] = new char[strlen(p)+1];
          strcpy(headfoot[nheader], p);
        }
        else
          headfoot[nheader] = NULL;
        newtokline = true;
        nheader++;
      }
      break;
    case 'g':
      if ((p = gettok(ascfile, &newtokline, tokline, &ssave)) != NULL)
        igear = atoi(p);
      if ((p = gettok(ascfile, &newtokline, tokline, &ssave)) != NULL)
        gear_off = atof(p);
      break;
    case 'y':
      if ((p = gettok(ascfile, &newtokline, tokline, &ssave)) != NULL)
        sav_nsym = atoi(p);
      if ((p = gettok(ascfile, &newtokline, tokline, &ssave)) != NULL) {
        sav_mirror_sym = (p[0] == 'y' || p[0] == 'Y');
      }
      nsym = sav_nsym;
      mirror_sym = sav_mirror_sym;
      nsym = 1;
      mirror_sym = false;
      break;
    case 'I':
      if ((p = gettok(ascfile, &newtokline, tokline, &ssave)) != NULL)
        r_i = atof(p);
      break;
    }
  }
  nsym = sav_nsym;
  mirror_sym = sav_mirror_sym;
  calcdir();
freeret:
  delete s;
  delete tokline;
  return badfile;
}

#define SILLY -99999.


#define pread(ptr, size, nitems, fp)  (((fp.read((char *) ptr, nitems*size)).gcount())/size)

/*
 * Read a binary GemCad .gem file of a gemstone design as written by GemCad.
 */
int
Stone::readGemFile(std::ifstream  & bfile)
{
  int32_t i; // must be 32 bits
  double a, b, c, d;
  double x, y, z;
  char s[256];
  int error;

  error = 0;
  while (pread((char *) &a, sizeof(a), 1, bfile) == 1) {
    if (a == SILLY)
      break;
    error = error || (pread((char *) &b, sizeof(b), 1, bfile) != 1);
    error = error || (pread((char *) &c, sizeof(c), 1, bfile) != 1);
    d = sqrt(a*a+b*b+c*c);
    if (d == 0.) {
      error = 1;
      break;
    }
    a /= d;
    b /= d;
    c /= d;
    d = 1./d;
    d *= 0.9;
//    std::cout << "abc = " << a << " " << b << " " << c << std::endl;
    newFacet(a, b, c, d);
    assert(check() == 0);
    error = error || (pread((char *) &i, sizeof(i), 1, bfile) != 1);
    i = bfile.get();
    if (i == 0) {
      facetpool[nfacet-1].setName(NULL);
      facetpool[nfacet-1].setInst(NULL);
    }
    else {
//      std::cout << "Name is " << i << " characters long" << std::endl;
      if (i  < 0 || i > 79) {
        snprintf(s,sizeof(s), "Name too long: %d characters", (int)i);
        errormsg(s);
        return 1;
      }
      int j;
      int k = -1;
      for (j = 0; j < i; j++) { // was i+i
        char c = bfile.get();
        if (c == '\n')
          c = '\0';
        if (c == '\t') {
          c = '\0';
                    k = j+1;
        }
        s[j] = c;
      }
      s[i] = '\0';
//      std::cout << "Name = \"" << s << "\"" << std::endl;
      facetpool[nfacet-1].setName(s);
      if (k != -1) {
        facetpool[nfacet-1].setInst(s+k);
//        std::cout << "Cutting instructions = \"" << s+k << "\"" << std::endl;
      }
    }
    error = error || (pread((char *) &i, sizeof(i), 1, bfile) != 1);
    while (i == 1) {
      error = error || (pread((char *) &x, sizeof(x), 1, bfile) != 1);
      error = error || (pread((char *) &y, sizeof(y), 1, bfile) != 1);
      error = error || (pread((char *) &z, sizeof(z), 1, bfile) != 1);
      error = error || (pread((char *) &i, sizeof(i), 1, bfile) != 1);
      if (error)
        break;
    }
    if (error)
      break;
  }
// calcdir();
  normals();
  if (error)
    return 1;
  if (pread((char *) &i, sizeof(i), 1, bfile) == 1)
    nsym = i;
  else
    return 0;
  if (pread((char *) &i, sizeof(i), 1, bfile) == 1)
    mirror_sym = i != 0;
  else
    return 0;
  if (pread((char *) &i, sizeof(i), 1, bfile) == 1)
    igear = i;
  else
    return 0;
  if (pread((char *) &a, sizeof(a), 1, bfile) == 1)
    r_i = a;
  else
    return 0;
  return 0;
}

#if 0
/*
 * Read an ascii .ASC file of a gemstone design as written by GemCad
 */
int
Stone::readAscFile(std::ifstream & ascfile)
{
  char *p;
  double angle, index, azi, incl, rho;
  int sav_nsym = 1;
  bool sav_mirror_sym = false;
  char *s;
  int nheader;
  char **headfoot;
  int badfile;
  double dversion;
  char *tokline;
  bool newtokline;
  char *ssave;
  
  s = new char[512];
  tokline = new char[TOKLINESIZE];
  newtokline = true;
  badfile = 0;
  p = gettok(ascfile, &newtokline, tokline, &ssave);
  if (p == NULL || strcmp(p, "GemCad"))
    badfile = 1;
  if (!badfile) {
    p = gettok(ascfile, &newtokline, tokline, &ssave);
    if (p == NULL)
      badfile = 1;
    if (!badfile) {
      dversion = atof(p);
      if (dversion < 3.99) {
        badfile = 1;
      }
    }
  }
  if (badfile) {
    errormsg("Not a GemCad text file");
    goto freeret;
  }
  newtokline = true;
  nsym = 1;
  mirror_sym = false;
  while ((p = gettok(ascfile, &newtokline, tokline, &ssave)) != NULL) {
    if (p[0] != 'H' && p[0] != 'F')
      nheader = 0;
    switch (p[0]) {
    case 'a':
      if ((p = gettok(ascfile, &newtokline, tokline, &ssave)) != NULL)
        angle = atof(p);
      incl = angle * M_PI / 180.;
      if ((p = gettok(ascfile, &newtokline, tokline, &ssave)) != NULL)
        rho = atof(p);
      rho *= 0.9;
      if (angle < 0.)
        rho = -rho;
      break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
      index = atof(p);
      if (angle == 0.)
        index = 0.;
      azi = RADIANS(index);
      if (rho == 0.) {
        errormsg("Facet through center ignored");
      }
      else {
        newPolarFacet(azi, incl, rho);
      }
      break;
    case 'n':
      if ((p = gettok(ascfile, &newtokline, tokline, &ssave)) != NULL)
        strcpy(s, p);
      if (s[0] != '\0')
        facetpool[nfacet-1].setName(s);
      break;
    case 'G':
      p += 2; // skip G and NULL
      if (*p == '\0')
        errormsg("G but no cutting instructions");
      else
        facetpool[nfacet-1].setInst(p);
      newtokline = true;
      break;
    case 'H':
    case 'F':
      if (*p == 'H')
        headfoot = header;
      if (*p == 'F')
        headfoot = footnote;
      if (nheader < 4) {
        p += 2; // skip H and NULL
        if (*p != '\0') {
          headfoot[nheader] = new char[strlen(p)+1];
          strcpy(headfoot[nheader], p);
        }
        else
          headfoot[nheader] = NULL;
        newtokline = true;
        nheader++;
      }
      break;
    case 'g':
      if ((p = gettok(ascfile, &newtokline, tokline, &ssave)) != NULL)
        igear = atoi(p);
      if ((p = gettok(ascfile, &newtokline, tokline, &ssave)) != NULL)
        gear_off = atof(p);
      break;
    case 'y':
      if ((p = gettok(ascfile, &newtokline, tokline, &ssave)) != NULL)
        sav_nsym = atoi(p);
      if ((p = gettok(ascfile, &newtokline, tokline, &ssave)) != NULL) {
        sav_mirror_sym = (p[0] == 'y' || p[0] == 'Y');
      }
      nsym = sav_nsym;
      mirror_sym = sav_mirror_sym;
      nsym = 1;
      mirror_sym = false;
      break;
    case 'I':
      if ((p = gettok(ascfile, &newtokline, tokline, &ssave)) != NULL)
        r_i = atof(p);
      break;
    }
  }
  nsym = sav_nsym;
  mirror_sym = sav_mirror_sym;
//  calcdir();
  normals();
freeret:
  delete s;
  delete tokline;
  return badfile;
}

#endif 
#define SILLY -99999.

/*
 * Read a binary GemCad .gem file of a gemstone design as written by GemCad
 * This uses C-style fread calls, but could be rewritten to use std::ifstream.read() 
 * calls.
 */
int
Stone::readGemFile(FILE *bfile)
{
  long i;
  double a, b, c, d;
  double x, y, z;
  int iwheel, mirror;
  int error;

  if (bfile == NULL)
    return 1;

  char s[256];

  error = 0;
  while (fread((char *) &a, sizeof(a), 1, bfile) == 1) {
    if (a == SILLY)
      break;
    error = error || (fread((char *) &b, sizeof(b), 1, bfile) != 1);
    error = error || (fread((char *) &c, sizeof(c), 1, bfile) != 1);
    d = sqrt(a*a+b*b+c*c);
    a /= d;
    b /= d;
    c /= d;
    d = 1./d;
    d *= 0.9;
    newFacet(a, b, c, d);
    error = error || (fread((char *) &i, sizeof(i), 1, bfile) != 1);
    i = getc(bfile);
    if (i != 0) {
      if (i > 79) {
        snprintf(s,sizeof(s), "%ld characters", i);
        errormsg(s);
        return 1;
      }
      if (fgets(s, i+1, bfile) != NULL)
      {
        facetpool[nfacet-1].setName(s);
      }
    }
    error = error || (fread((char *) &i, sizeof(i), 1, bfile) != 1);
    //
    // Skip vertices
    //
    while (i == 1) {
      error = error || (fread((char *) &x, sizeof(x), 1, bfile) != 1);
      error = error || (fread((char *) &y, sizeof(y), 1, bfile) != 1);
      error = error || (fread((char *) &z, sizeof(z), 1, bfile) != 1);
      error = error || (fread((char *) &i, sizeof(i), 1, bfile) != 1);
    }
    if (error) {
      std::cerr << "error reading binary file" << std::endl;
      return 1;
    }
  }
//  calcdir();
  normals();
  if (fread((char *) &i, sizeof(i), 1, bfile) == 1)
    nsym = i;
  else
    return 0;
  if (fread((char *) &i, sizeof(i), 1, bfile) == 1)
    mirror_sym = i != 0;
  else
    return 0;
  if (fread((char *) &i, sizeof(i), 1, bfile) == 1)
    igear = i;
  else
    return 0;
  if (fread((char *) &a, sizeof(a), 1, bfile) == 1)
    r_i = a;
  return 0;
}

/*
 * Terminate a C-style string at first newline if there is one.
 */
void
rmnl(char *p)
{
  while (*p) {
    if (*p == '\r' || *p == '\n') {
      *p = '\0';
      return;
    }
    ++p;
  }
}

/*
 * Find fisrt occurence of character c in string s.
 * Used by strtoken()
 */
const char *
strchar(const char *s, const int c)
{
  while (*s) {
    if (*s == c)
      return s;
    s++;
  }
  return NULL;
}

/*
 * Reentrant replacement for strtok(). Calling function maintains
 * ssave pointer.
 *
 * char *ssave;
 * char *delims;
 * char *string;
 * char *p;
 *
 * First token:
 *  p = strtoken(string, delims, &ssave);
 * Subsequent tokens
 *  p = strtoken(NULL, delims, &ssave);
 */
char *
strtoken(char *s, const char *delims, char **ssave)
{
  char *ret;

  if (s == NULL)
    s = *ssave;
  while (*s && strchar(delims, *s))
    ++s;
  if (*s == '\0')
    return NULL;
  ret = s;
  while (*s && !strchar(delims, *s))
    ++s;
  if (*s)
    *s++ = '\0';
  *ssave = s;
  return ret;
}

/*
 * Get a token from the stream
 */
char *
gettok(std::ifstream & ascfile, bool *newtokline, char *tokline, char **ssave)
{
  char *p;
  
  if (*newtokline)
    p = NULL;
  else
    p = strtoken(NULL, " \t", ssave);
  while (p == NULL) {
    ascfile.getline(tokline, TOKLINESIZE);
    if (ascfile.eof())
      return NULL;
    rmnl(tokline);
    p = strtoken(tokline, " \t", ssave);
    *newtokline = false;
  }
  return(p);
}

/*
 * Test whether point (x, y, z) is inside facet i. It is assumed that
 * point is already on plane of facet, but no test for that here.
 */
bool Stone::infacet(int i, double x, double y, double z)
{
  Facet *f = &facetpool[i];
  unsigned int j;
  bool inpoly = false;
  AXIS dir = f->dir;
  Vertex *v, *w;
  v = vertpool + f->vinds[(f->nvind)-1];
  if (dir == XAXIS) {
    for (j = 0; j < f->nvind; j++) {
      w = v;
      v = vertpool + f->vinds[j];
      if (((v->z > z) != (w->z > z)) &&
        (y < (w->y - v->y) * (z - v->z) / (w->z-v->z) + v->y))
        inpoly = !inpoly;
    }
  }
  else if (dir == YAXIS) {
    for (j = 0; j < f->nvind; j++) {
      w = v;
      v = vertpool + f->vinds[j];
      if (((v->x > x) != (w->x > x)) &&
        (z < (w->z - v->z) * (x - v->x) / (w->x-v->x) + v->z))
        inpoly = !inpoly;
    }
  }
  else if (dir == ZAXIS) {
    for (j = 0; j < f->nvind; j++) {
      w = v;
      v = vertpool + f->vinds[j];
      if (((v->y > y) != (w->y > y)) &&
        (x < (w->x - v->x) * (y - v->y) / (w->y-v->y) + v->x))
        inpoly = !inpoly;
    }
  }
   else {
    std::cerr << "Bad direction in infacet()" << std::endl;
   }
  return inpoly;
}

/*
 * int
 * Stone::findfacetp(int hint, Ray& r, RAYTYPE key, double *dp, double *dray)
 *
 * Find the facet that a ray intersects--point in polygon perimiter version
 *
 * int hint     facet to test first
 * Ray& r       the light ray
 * RAYTYPE key  INSIDE, OUTSIDE, or INPERIM
 * double *dp   the returned dot product of ray normal and facet
 * double *dray the distance traversed
 *
 * returns the index of the facet int the stone's facetpool 
 * returns -1 if ray is OUTSIDE and misses stone
 *
 * If ray is outside the stone but we already know it is inside the perimeter,
 * we can ignore facets on the bottom of the stone (whose unit normals point
 * in the direction of the ray). In this case, set key to INPERIM.
 */
int
Stone::findfacetp(int hint, Ray& r, RAYTYPE key, double *dp, double *dray)
{
  int i;
/*
   bool debug = false;
   if (hint < 0) {
     hint = -hint;
     debug = true;
   }
*/
  if (hint < 0) hint = 0;
  for (unsigned int it = 0; it < nfacet; it++) {
    /*
     * Swap first facet and hint facet test so test hint facet first
     */
    if (it == 0)
      i = hint;
    else if (it == hint)
      i = 0;
    else
      i = it;
    Facet *f = &facetpool[i];
    /*
     * Dot product of plane's unit normal with ray's direction normal
     */
    double d = f->a * r.a + f->b * r.b + f->c *r.c;
//    if (debug && it == 0) printf("dot = %f\n", d);
    if (d < 0.) {
      if (key == INSIDE)
        continue;
    }
    else if (d > 0.) {
      if (key == INPERIM)
        continue;
    }
    else
      continue; // Ray parallel to plane if (d == 0.)
    /*
     * Dot product of plane's unit normal with ray's endpoint
     */
    double e = f->a * r.x + f->b * r.y + f->c * r.z;
    /*
     * Distance from endpoint to plane
     */
    double dist = (f->d - e)/d;
    /*
     * point of intersection of ray with plane of facet
     */
    double x = r.x + dist*r.a;
    double y = r.y + dist*r.b;
    double z = r.z + dist*r.c;
//    if (debug && it == 0) printf("intersection: %.3f %.3f %.3f\n", x, y, z);
//    if (debug && it == 0) printf("f->abc: %.3f %.3f %.3f\n", f->a, f->b, f->c);
//    if (debug && it == 0) printf("f->dir: %d\n", f->dir);
    unsigned int j;
    bool inpoly = false;
    int dir = f->dir;
    Vertex *v, *w;
    v = vertpool + f->vinds[(f->nvind)-1];
    if (dir == XAXIS) {
      for (j = 0; j < f->nvind; j++) {
        w = v;
        v = vertpool + f->vinds[j];
//          if (debug && it == 0) printf("vert %.3f %.3f %.3f\n", v->x, v->y, v->z);
        if (((v->z > z) != (w->z > z)) &&
          (y < (w->y - v->y) * (z - v->z) / (w->z-v->z) + v->y))
          inpoly = !inpoly;
      }
    }
    else if (dir == YAXIS) {
      for (j = 0; j < f->nvind; j++) {
        w = v;
        v = vertpool + f->vinds[j];
//          if (debug && it == 0) printf("vert %.3f %.3f %.3f\n", v->x, v->y, v->z);
        if (((v->x > x) != (w->x > x)) &&
          (z < (w->z - v->z) * (x - v->x) / (w->x-v->x) + v->z))
          inpoly = !inpoly;
      }
    }
    else if (dir == ZAXIS) {
      for (j = 0; j < f->nvind; j++) {
        w = v;
        v = vertpool + f->vinds[j];
//          if (debug && it == 0) printf("vert %.3f %.3f %.3f\n", v->x, v->y, v->z);
        if (((v->y > y) != (w->y > y)) &&
          (x < (w->x - v->x) * (y - v->y) / (w->y-v->y) + v->x))
          inpoly = !inpoly;
      }
    }
    else {
      std::cerr << "Bad direction in findfacetp()" << std::endl;
    }
    if (inpoly) { // if (infacet(i, x, y, z))
      if (dp != NULL)
        *dp = d;
      if (dray != NULL)
        *dray = dist;
      return i;
    }
  }
  return -1; // ray misses stone
}

/*
 * int Stone::findfacet(Ray& r, RAYTYPE key, double *dp,  double *dray)
 *
 * Find the facet that a ray intersects
 *
 * Ray& r       the light ray
 * RAYTYPE key  INSIDE, OUTSIDE, or INPERIM
 * double *dp   the returned dot product of ray normal and facet
 * double *dray the distance traversed
 *
 * returns the index of the facet int the stone's facetpool 
 * returns -1 if ray is OUTSIDE and misses stone
 *
 * The endpoint of the ray is moved to the plane of the facet.
 *
 * A facet's unit normal points out of the stone.
 *
 *            V
 *          \ |
 *           \|
 *            +    _________
 *            |\  /         \
 *            | \/           \
 *            | /\   Stone   /     
 *            |/  \         /            
 *            +    \       /             
 *           /|     \     /                
 *          / |      \   /               
 *            |       \ /                
 *            V        V                 
 *                                       
 *
 * When a ray is OUTSIDE the stone, the intersections with both the top and
 * the bottom are found. (Here, "top" means the facets whose unit normal faces
 * away from the ray's unit normal.) If the intersection with the bottom
 * is closer than the intersection with the top, then the ray is OUTSIDE the
 * perimeter of the stone. If the ray is inside the stone, the situation
 * is simpler, and only the facets whose unit normals face in the same
 * direction need to be considered.
 *
 * If ray is outside the stone but we already know it is inside the perimeter,
 * we can ignore facets on the bottom of the stone (whose unit normals point
 * in the direction of the ray). In this case, set key to INPERIM.
 */
int
Stone::findfacet(Ray& r, RAYTYPE key, double *dp,  double *dray)
{
  int i;
  double dmin = 1.e308;
  double dmax = -1.e308;
  double dotsav;
  double distsav;
  int ifacet = -1;
  for (i = 0; i < nfacet; i++) {
    Facet *f = &facetpool[i];
    /*
     * Dot product of plane's unit normal with ray's direction normal
     */
    double d = f->a * r.a + f->b * r.b + f->c *r.c;
    if (d < 0.) {
      if (key == INSIDE)
        continue;
    }
    else if (d > 0.) {
      if (key == INPERIM)
        continue;
    }
    else
      continue; // Ray parallel to plane if (d == 0.)
    /*
     * Dot product of plane's unit normal with ray's endpoint
     */
    double e = f->a * r.x + f->b * r.y + f->c * r.z;
    /*
     * Distance from endpoint to plane
     */
    double dist = (f->d - e)/d;
    if (d < 0.) {
      if (dist > dmax) {
        dmax = dist;
        distsav = dist;
        ifacet = i;
        dotsav = d;
      }
    }
    else {
      if (dist < dmin) {
        dmin = dist;
        if (key == INSIDE) {
          distsav = dist;
          ifacet = i;
          dotsav = d;
        }
      }
    }
  }
  if (key == OUTSIDE && dmax > dmin)
    return -1; // ray misses stone

  if (dp != NULL)
    *dp = dotsav;

  if (dray != NULL)
    *dray = distsav;
  return ifacet;
}

/*
 * This function refracts and
 *               reflects a light ray,
 * thus the name ref__ct.
 *
 * The refraction follows Snell's law.
 * The fraction of light is determined by Fresnel's equations, but
 * here, the two polarizations are averaged.
 *
 * This formulation of Snell's Law uses but a single square root and no
 * trigonometric functions.
 * ref__ct() returns 1 if the primary ray r is exiting the stone
 */
int
Stone::ref__ct(
int jf,    // index of facet
           // (f->a, f->b, f->c) is unit normal pointing out from stone)
Ray& r,    // incident ray, changed to outgoing ray
Ray& rr,   // partially reflected Ray
double dp, // dot (scalar) product of Ray and Facet unit normals
double *t, // fraction of light in Ray r. Ray rr has 1.-t
double ri) // refractive index or zero  if using this.r_i
{
  double nrat;
  double rad;
  int outside; // true if outgoing ray is outside stone 
  double adp;
  double d1, d2, num;
  Facet *f = facetpool + jf;

  if (ri == 0.)
    ri = r_i;
  if (dp < 0.) { // from air to stone 
    nrat = 1. / ri;
    adp = -dp;
    outside = 0;
  }
  else { // from stone to air
    adp = dp;
    nrat = ri;
    outside = 1;
  }
  rad = 1 - nrat*nrat*(1. - adp*adp);
  if (rad <= 0.) { // below critical angle, total internal reflection
    adp *= 2.;
    r.a -= f->a * adp;
    r.b -= f->b * adp;
    r.c -= f->c * adp;
//    if (!r.isunit())
//      std::cout << "Ray r is not a unit vector" << std::endl;
    *t = 1.;
    return 0;
  }
  rad = sqrt(rad);
  d1 = adp + rad * nrat;
  d2 = nrat * adp + rad;
  if (d1 == 0. || d2 == 0.) {
    fprintf(stderr, "divide by zero inside ref__ct()\n");
    return 0;
  }
  num = 2. * nrat * adp * rad;
  *t = num / (d1*d1) + num / (d2*d2);
  rad -= nrat * adp;
  if (outside)
    rad = -rad;
  adp = 2. * dp;
  rr.a = -f->a * adp + r.a;
  rr.b = -f->b * adp + r.b;
  rr.c = -f->c * adp + r.c;
//  if (!rr.isunit())
//    std::cout << "Ray rr is not a unit vector" << std::endl;
  r.a = nrat * r.a - rad * f->a;
  r.b = nrat * r.b - rad * f->b;
  r.c = nrat * r.c - rad * f->c;
//  if (!r.isunit())
//    std::cout << "Ray r is not a unit vector" << std::endl;
  return outside;
}

/*
 * fuzzy equality check: Is a approximately equal to b within a tolerance
 * of fuzz?
 */
#define FUZZEQ(a, b, fuzz) (fabs((a)-(b)) < (fuzz))

/*
 * Check symmetry. The user interface is a bit peculiar, but to check for
 * symmetry about the y axis (x = -x), do checksym(-1, 1, 1). To check for
 * symmetry about the x axis (y = -y), do checksym(1, -1, 1)
 */
bool
Stone::checksym(int sx, int sy, int sz)
{
  unsigned i, j;
  Facet *f, *g;
  bool hasmate;
  for (i = 0; i < nfacet; i++) {
    hasmate = 0;
    f = &facetpool[i];
    for (j = 0; j < nfacet; j++) {
      g = &facetpool[j];
      if (FUZZEQ(f->c, sz*g->c, 0.0001) &&
          FUZZEQ(f->b, sy*g->b, 0.0001) &&
          FUZZEQ(f->a, sx*g->a, 0.0001) &&
             FUZZEQ(f->d,    g->d, 0.0001)) {
        hasmate = 1;
        break;
      }
    }
    if (!hasmate)
      return 0;
  }
  return 1;
}

/*
 * Check for xy symmetry.
 */
bool
Stone::checkxysym(void)
{
  unsigned i, j;
  Facet *f, *g;
  bool hasmate;
  for (i = 0; i < nfacet; i++) {
    hasmate = 0;
    f = &facetpool[i];
    for (j = 0; j < nfacet; j++) {
      g = &facetpool[j];
      if (FUZZEQ(f->a, g->b, 0.0001) &&
          FUZZEQ(f->b, g->a, 0.0001) &&
         FUZZEQ(f->c, g->c, 0.0001) &&
             FUZZEQ(f->d, g->d, 0.0001)) {
        hasmate = 1;
        break;
      }
    }
    if (!hasmate)
      return 0;
  }
  return 1;
}

/*
 * Consistency check of stone
 */
int
Stone::check(void)
{
  unsigned int i;
  unsigned int nedge = 0;
  for (i = 0; i < nfacet; i++) {
    Facet *f = &facetpool[i];
    nedge += f->nvind;
    unsigned int nvind = f->nvind;
    unsigned int *vinds = f->vinds;
    for (unsigned int j = 0; j < nvind; j++) {
      unsigned int match = 0;
      unsigned int v = vinds[j];
      unsigned int w = vinds[(j+1) % nvind];
      for (unsigned int i2 = 0; i2 < nfacet; i2++) {
        if (i == i2)
          continue;
        Facet *g = &facetpool[i2];
        unsigned int nvind2 = g->nvind;
        unsigned int *vinds2 = g->vinds;
        for (unsigned int j2 = 0; j2 < nvind2; j2++) {
          unsigned int v2 = vinds2[j2];
          unsigned int w2 = vinds2[(j2+1) % nvind2];
          if (v == w2 && w == v2)
            match++;
        }
      }
      if (match != 1) {
        std::cout << "Warning: facet " << i << " edge from vertex " << v << " to " << w << " has " << match << " matches" << std::endl;
        return 1;
      }
    }
  }
  if (nedge % 2) {
    std::cout << "Warning: twice the number of edges " << nedge << " is an odd number" << std::endl;
    return 2;
  }
  nedge /= 2;
  int sum = nvert + nfacet - nedge;
  if (sum != 2) {
    std::cout << "Warning: flunked euler identity test: " << nedge << " edges" << std::endl;
    std::cout << "nvert + nfacet - nedge != 2" << std::endl;
    std::cout << nvert << " + " << nfacet << " - " << nedge << " = " << sum << std::endl;
    return 3;
  }
  for (i = 0; i < nvert-1; i++) {
    Vertex *v = &vertpool[i];
    for (unsigned int j = i+1; j < nvert; j++) {
      Vertex *w = &vertpool[j];
      if ((fabs(v->x-w->x) + fabs(v->y-w->y) + fabs(v->z-w->z)) < 3.*EPSILON) {
        std::cout << "vertex " << i << " and vertex " << j << " very close" << std::endl;
        return 4;
      }
    }
  }
  /*
   * Make sure vertices lie on their respective facets
   */
  for (i = 0; i < nfacet; i++) {
    Facet *f = &facetpool[i];
    for (int j = 0; j < f->nvind; j++) {
      Vertex *v = &vertpool[f->vinds[j]];
      if (fabs(f->a*v->x + f->b*v->y + f->c*v->z - f->d) > EPSILON) {
        std::cout << "vertex " << f->vinds[j] << " not on facet " << i << std::endl;
        return 5;
      }
    }
  }
  /*
   * Make sure no trash in tail end of facetpool
   */
  for (i = nfacet; i < maxfacet; i++) {
    Facet *f = &facetpool[i];
    if (f->nvind != 0) {
      std::cout << "facet pool tail " << i << " nvind = " << f->nvind << ", not zero" << std::endl;
      return 6;
    }
    if (f->vinds != NULL) {
      std::cout << "facet pool tail " << i << " vinds not NULL" << std::endl;
      return 7;
    }
  }
  return 0;
}

/**
  get index of table facet or -1 if no table.
*/
int
Stone::tablefacet()
{
  for (unsigned int i = 0; i < nfacet; i++) {
    Facet *f = &facetpool[i];
    if (f->c > 0.9999)
      return i;
  }
  return -1;
}

/**
  Get lowest non-culet facet angle
*/
double
Stone::getPavilionAngle()
{
  int i;
  double cmin = 0.;
  int imin = -1;
  for (i = 0; i < nfacet; i++) {
    Facet *f = &facetpool[i];
    if (f->c > -0.001 || f->c < -0.999)
      continue;
    if (f->c < cmin) {
      cmin = f->c;
      imin = i;
    }
  }
  return (imin == -1) ? 0. : fabs(getFacetAngle(imin));
}

/**
  Get the angle of facet i
*/
double
Stone::getFacetAngle(int i)
{
   if (i < 0 || i > nfacet)
     return 0.;
  Facet *f = &facetpool[i];
  double angle = atan2(hypot(f->a, f->b), f->c);
  angle *= 180./M_PI;
  while (angle < -90.)
    angle += 180.;
  while (angle > 90.)
    angle -= 180.;
  return angle;
}

/**
  Get angle of non-table crown facet with biggest xy area
*/
double
Stone::getCrownAngle()
{
  int i;
  double amax = 0.;
  int imax = -1;
  for (i = 0; i < nfacet; i++) {
    Facet *f = &facetpool[i];
    if (f->c < 0.001 || f->c > 0.999)
      continue;
    double xyarea;
    (void) getFacetArea(i, &xyarea);
    if (xyarea > amax) {
      amax = xyarea;
      imax = i;
    }
  }
  return (imax == -1) ? 0. : getFacetAngle(imax);
}

/**
  Return area of facet. If xyarea is not null, it will contain area
  projected onto xy plane
*/
double
Stone::getFacetArea(int i, double *xyarea)
{
  Facet *f = &facetpool[i];
  double axy, ayz, azx;
  axy = 0.;
  ayz = 0.;
  azx = 0.;
  Vertex *v0, *v1;
  v0 = &vertpool[f->vinds[f->nvind - 1]];
  for (int i = 0; i < f->nvind; i++) {
    v1 = &vertpool[f->vinds[i]];
    axy += (v0->x * v1->y)-(v1->x * v0->y);
    ayz += (v0->y * v1->z)-(v1->y * v0->z);
    azx += (v0->z * v1->x)-(v1->z * v0->x);
    v0 = v1;
  }
  if (xyarea != NULL)
    *xyarea = axy/2.;
  return sqrt(axy*axy + ayz*ayz + azx*azx)/2.;
}

