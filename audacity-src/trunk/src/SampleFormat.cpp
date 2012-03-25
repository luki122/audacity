/**********************************************************************

  Audacity: A Digital Audio Editor

  SampleFormat.h

  Dominic Mazzoni

*******************************************************************//*!

\file SampleFormat.cpp
\brief Functions that work with Dither and initialise it.


  This file handles converting between all of the different
  sample formats that Audacity supports, such as 16-bit,
  24-bit (packed into a 32-bit int), and 32-bit float.

  Floating-point samples use the range -1.0...1.0, inclusive.
  Integer formats use the full signed range of their data type,
  for example 16-bit samples use the range -32768...32767.
  This means that reading in a wav file and writing it out again
  ('round tripping'), via floats, is lossless; -32768 equates to -1.0f 
  and 32767 equates to +1.0f - (a little bit).
  It also means (unfortunatly) that writing out +1.0f leads to
  clipping by 1 LSB.  This creates some distortion, but I (MJS) have
  not been able to measure it, it's so small.  Zero is preserved.

  http://limpet.net/audacity/bugzilla/show_bug.cgi?id=200
  leads to some of the discussions that were held about this.

   Note: These things are now handled by the Dither class, which
         also replaces the CopySamples() method (msmeyer)

*//*******************************************************************/

#include <wx/intl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SampleFormat.h"
#include "Prefs.h"
#include "Dither.h"

Dither::DitherType gLowQualityDither = Dither::none;
Dither::DitherType gHighQualityDither = Dither::none;
Dither gDitherAlgorithm;

void InitDitherers()
{
   // Read dither preferences
   gLowQualityDither = (Dither::DitherType)
   gPrefs->Read(wxT("/Quality/DitherAlgorithm"), (long)Dither::none);

   gHighQualityDither = (Dither::DitherType)
   gPrefs->Read(wxT("/Quality/HQDitherAlgorithm"), (long)Dither::shaped);
}

const wxChar *GetSampleFormatStr(sampleFormat format)
{
   switch(format) {
   case int16Sample:
      /* i18n-hint: Audio data bit depth (precision): 16-bit integers */
      return _("16-bit PCM");
   case int24Sample:
      /* i18n-hint: Audio data bit depth (precision): 24-bit integers */
      return _("24-bit PCM");
   case floatSample:
      /* i18n-hint: Audio data bit depth (precision): 32-bit floating point */
      return _("32-bit float");
   }
   return wxT("Unknown format"); // compiler food
}

AUDACITY_DLL_API samplePtr NewSamples(int count, sampleFormat format)
{
   return (samplePtr)malloc(count * SAMPLE_SIZE(format));
}

AUDACITY_DLL_API void DeleteSamples(samplePtr p)
{
   free(p);
}

// TODO: Risky?  Assumes 0.0f is represented by 0x00000000;
void ClearSamples(samplePtr src, sampleFormat format,
                  int start, int len)
{
   int size = SAMPLE_SIZE(format);
   memset(src + start*size, 0, len*size);
}

void CopySamples(samplePtr src, sampleFormat srcFormat,
                 samplePtr dst, sampleFormat dstFormat,
                 unsigned int len,
                 bool highQuality, /* = true */
                 unsigned int stride /* = 1 */)
{
   gDitherAlgorithm.Apply(
      highQuality ? gHighQualityDither : gLowQualityDither,
      src, srcFormat, dst, dstFormat, len, stride);
}

void CopySamplesNoDither(samplePtr src, sampleFormat srcFormat,
                 samplePtr dst, sampleFormat dstFormat,
                 unsigned int len,
                 unsigned int stride /* = 1 */)
{
   gDitherAlgorithm.Apply(
      Dither::none,
      src, srcFormat, dst, dstFormat, len, stride);
}

// Indentation settings for Vim and Emacs and unique identifier for Arch, a
// version control system. Please do not modify past this point.
//
// Local Variables:
// c-basic-offset: 3
// indent-tabs-mode: nil
// End:
//
// vim: et sts=3 sw=3
// arch-tag: 1dacb18a-a027-463b-b558-73b6d24995d6
