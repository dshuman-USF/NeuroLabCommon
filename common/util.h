#ifndef UTIL_H
#define UTIL_H

typedef struct
{
  double x;
  double y;
  double z;
} XYZ;

static inline XYZ
sub (XYZ a, XYZ b)
{
  return ((XYZ) {a.x - b.x, a.y - b.y, a.z - b.z});
}

static inline XYZ
add (XYZ a, XYZ b)
{
  return ((XYZ) {a.x + b.x, a.y + b.y, a.z + b.z});
}

static inline XYZ
smul (double a, XYZ b)
{
  return ((XYZ) {a * b.x, a * b.y, a * b.z});
}

static inline double
dot (XYZ a, XYZ b)
{
  return (a.x * b.x + a.y * b.y + a.z * b.z);
}

static inline XYZ
cross (XYZ a, XYZ b)
{
  return ((XYZ) {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x});
}

static inline double
len (XYZ b)
{
  return sqrt (b.x * b.x + b.y * b.y + b.z * b.z);
}

static inline XYZ
norm (XYZ b)
{
  return smul (1 / len (b), b);
}

static inline XYZ
vmmul (XYZ p, float *m)
{
  float x = p.x * m[0] + p.y * m[4] + p.z * m[8];
  float y = p.x * m[1] + p.y * m[5] + p.z * m[9];
  float z = p.x * m[2] + p.y * m[6] + p.z * m[10];
  return (XYZ){x, y, z};
}

#endif
