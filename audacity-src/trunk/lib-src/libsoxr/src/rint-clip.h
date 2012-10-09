/* SoX Resampler Library      Copyright (c) 2007-12 robs@users.sourceforge.net
 * Licence for this file: LGPL v2.1                  See LICENCE for details. */

#if defined DITHER

#define DITHERING (1./32)*(int)(((ran1>>=3)&31)-((ran2>>=3)&31))
#define DITHER_RAND (seed = 1664525UL * seed + 1013904223UL) >> 3
#define DITHER_VARS unsigned long ran1 = DITHER_RAND, ran2 = DITHER_RAND
#define SEED_ARG , unsigned long * seed0
#define SAVE_SEED *seed0 = seed
#define COPY_SEED unsigned long seed = *seed0;
#define COPY_SEED1 unsigned long seed1 = seed
#define PASS_SEED1 , &seed1
#define PASS_SEED0 , seed0

#else

#define DITHERING 0
#define DITHER_VARS
#define SEED_ARG
#define SAVE_SEED
#define COPY_SEED
#define COPY_SEED1
#define PASS_SEED1
#define PASS_SEED0

#endif



#if defined FE_INVALID && defined FPU_RINT
static void RINT_CLIP(RINT_T * const dest, FLOATX const * const src,
    unsigned stride, size_t i, size_t const n, size_t * const clips SEED_ARG)
{
  COPY_SEED
  DITHER_VARS;
  for (; i < n; ++i) {
    double d = src[i] + DITHERING;
    dest[stride * i] = RINT(d);
    if (fetestexcept(FE_INVALID)) {
      feclearexcept(FE_INVALID);
      dest[stride * i] = d > 0? RINT_MAX : -RINT_MAX - 1;
      ++*clips;
    }
  }
  SAVE_SEED;
}
#endif



static size_t LSX_RINT_CLIP(void * * const dest0, FLOATX const * const src,
    size_t const n SEED_ARG)
{
  size_t i, clips = 0;
  RINT_T * dest = *dest0;
  COPY_SEED
#if defined FE_INVALID && defined FPU_RINT
#define _ dest[i] = RINT(src[i] + DITHERING), ++i,
  feclearexcept(FE_INVALID);
  for (i = 0; i < (n & ~7u);) {
    COPY_SEED1;
    DITHER_VARS;
    _ _ _ _ _ _ _ _ 0;
    if (fetestexcept(FE_INVALID)) {
      feclearexcept(FE_INVALID);
      RINT_CLIP(dest, src, 1, i - 8, i, &clips PASS_SEED1);
    }
  }
  RINT_CLIP(dest, src, 1, i, n, &clips PASS_SEED0);
#else
#define _ d = src[i] + DITHERING, dest[i++] = (RINT_T)(d > N - 1? ++clips, (RINT_T)(N - 1) : d < -N? ++clips, (RINT_T)(-N) : RINT(d)),
  const double N = 1. + RINT_MAX;
  double d;
  for (i = 0; i < (n & ~7u);) {
    DITHER_VARS;
    _ _ _ _ _ _ _ _ 0;
  }
  {
    DITHER_VARS;
    for (; i < n; _ 0);
  }
#endif
  SAVE_SEED;
  *dest0 = dest + n;
  return clips;
}
#undef _



static size_t LSX_RINT_CLIP_2(void * * dest0, FLOATX const * const * srcs,
    unsigned const stride, size_t const n SEED_ARG)
{
  unsigned j;
  size_t i, clips = 0;
  RINT_T * dest = *dest0;
  COPY_SEED
#if defined FE_INVALID && defined FPU_RINT
#define _ dest[stride * i] = RINT(src[i] + DITHERING), ++i,
  feclearexcept(FE_INVALID);
  for (j = 0; j < stride; ++j, ++dest) {
    FLOATX const * const src = srcs[j];
    for (i = 0; i < (n & ~7u);) {
      COPY_SEED1;
      DITHER_VARS;
      _ _ _ _ _ _ _ _ 0;
      if (fetestexcept(FE_INVALID)) {
        feclearexcept(FE_INVALID);
        RINT_CLIP(dest, src, stride, i - 8, i, &clips PASS_SEED1);
      }
    }
    RINT_CLIP(dest, src, stride, i, n, &clips PASS_SEED0);
  }
#else
#define _ d = src[i] + DITHERING, dest[stride * i++] = (RINT_T)(d > N - 1? ++clips, (RINT_T)(N - 1) : d < -N? ++clips, (RINT_T)(-N) : RINT(d)),
  const double N = 1. + RINT_MAX;
  double d;
  for (j = 0; j < stride; ++j, ++dest) {
    FLOATX const * const src = srcs[j];
    for (i = 0; i < (n & ~7u);) {
      DITHER_VARS;
      _ _ _ _ _ _ _ _ 0;
    }
    {
      DITHER_VARS;
      for (; i < n; _ 0);
    }
  }
#endif
  SAVE_SEED;
  *dest0 = dest + stride * (n - 1);
  return clips;
}
#undef _

#undef PASS_SEED0
#undef PASS_SEED1
#undef COPY_SEED1
#undef COPY_SEED
#undef SAVE_SEED
#undef SEED_ARG
#undef DITHER_VARS
#undef DITHERING
#undef DITHER

#undef RINT_MAX
#undef RINT_T
#undef FPU_RINT
#undef RINT
#undef RINT_CLIP
#undef LSX_RINT_CLIP
#undef LSX_RINT_CLIP_2
