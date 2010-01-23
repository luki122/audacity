/**********************************************************************

  Audacity: A Digital Audio Editor

  SpectrumPrefs.cpp

  Dominic Mazzoni
  James Crook

*******************************************************************//**

\class SpectrumPrefs
\brief A PrefsPanel for spectrum settings.

*//*******************************************************************/

#include "../Audacity.h"

#include <wx/defs.h>
#include <wx/intl.h>
#include <wx/msgdlg.h>

#include "../Prefs.h"
#include "../Project.h"
#include "../ShuttleGui.h"
#include "SpectrumPrefs.h"
#include "../FFT.h"

SpectrumPrefs::SpectrumPrefs(wxWindow * parent)
:  PrefsPanel(parent, _("Spectrograms"))
{
   Populate();
}

SpectrumPrefs::~SpectrumPrefs()
{
}

void SpectrumPrefs::Populate()
{
   mSizeChoices.Add(_("8 - most wideband"));
   mSizeChoices.Add(_("16"));
   mSizeChoices.Add(_("32"));
   mSizeChoices.Add(_("64"));
   mSizeChoices.Add(_("128"));
   mSizeChoices.Add(_("256 - default"));
   mSizeChoices.Add(_("512"));
   mSizeChoices.Add(_("1024"));
   mSizeChoices.Add(_("2048"));
#ifdef EXPERIMENTAL_FIND_NOTES
   mSizeChoices.Add(_("4096"));
   mSizeChoices.Add(_("8192"));
   mSizeChoices.Add(_("16384"));
   mSizeChoices.Add(_("32768 - most narrowband"));
#else
   mSizeChoices.Add(_("4096 - most narrowband"));
#endif //LOGARITHMIC_SPECTRUM

   for (size_t i = 0; i < mSizeChoices.GetCount(); i++) {
      mSizeCodes.Add(1 << (i + 3));
   }

   for (int i = 0; i < NumWindowFuncs(); i++) {
      mTypeChoices.Add(WindowFuncName(i));
      mTypeCodes.Add(i);
   }


   //------------------------- Main section --------------------
   // Now construct the GUI itself.
   // Use 'eIsCreatingFromPrefs' so that the GUI is
   // initialised with values from gPrefs.
   ShuttleGui S(this, eIsCreatingFromPrefs);
   PopulateOrExchange(S);
   // ----------------------- End of main section --------------
}

void SpectrumPrefs::PopulateOrExchange(ShuttleGui & S)
{
   S.SetBorder(2);

   S.StartStatic(_("FFT Window"));
   {
      S.StartMultiColumn(2);
      {
         S.TieChoice(_("Window &size") + wxString(wxT(":")),
                     wxT("/Spectrum/FFTSize"), 
                     256,
                     mSizeChoices,
                     mSizeCodes);
         S.SetSizeHints(mSizeChoices);

         S.TieChoice(_("Window &type") + wxString(wxT(":")),
                     wxT("/Spectrum/WindowType"), 
                     3,
                     mTypeChoices,
                     mTypeCodes);
         S.SetSizeHints(mTypeChoices);
      }
      S.EndMultiColumn();
   }
   S.EndStatic();

#ifdef EXPERIMENTAL_FFT_SKIP_POINTS
   wxArrayString wskipn;
   wxArrayInt wskipv;

   for (size_t i = 0; i < 7; i++) {
      wskipn.Add(wxString::Format(wxT("%d"), (1 << i) - 1));
      wskipv.Add((1 << i) - 1);
   }

   S.StartStatic(_("FFT Skip Points"));
   {
      S.StartMultiColumn(2);
      {
         S.TieChoice(_("Skip Points") + wxString(wxT(":")),
                     wxT("/Spectrum/FFTSkipPoints"),
                     0,
                     wskipn,
                     wskipv);
         S.SetSizeHints(wskipn);
      }
      S.EndMultiColumn();
   }
   S.EndStatic();
#endif //EXPERIMENTAL_FFT_SKIP_POINTS

   S.StartStatic(_("Display"));
   {
      S.StartTwoColumn();
      {
         mMinFreq =
            S.TieTextBox(_("Mi&nimum Frequency (Hz):"),
                         wxT("/Spectrum/MinFreq"),
                         0,
                         12);

         mMaxFreq =
            S.TieTextBox(_("Ma&ximum Frequency (Hz):"),
                         wxT("/Spectrum/MaxFreq"),
                         8000,
                         12);

         mGain =
            S.TieTextBox(_("&Gain (dB):"),
                         wxT("/Spectrum/Gain"),
                         20,
                         8);

         mRange =
            S.TieTextBox(_("&Range (dB):"),
                         wxT("/Spectrum/Range"),
                         80,
                         8);

         mFrequencyGain =
			 S.TieTextBox(_("Frequency g&ain (dB/dec):"),
                    wxT("/Spectrum/FrequencyGain"),
                    0,
                    4);
      }
      S.EndTwoColumn();

      S.TieCheckBox(_("S&how the spectrum using grayscale colors"),
                    wxT("/Spectrum/Grayscale"),
                    false);

#ifdef EXPERIMENTAL_FFT_Y_GRID
      S.TieCheckBox(_("Show a grid along the &Y-axis"),
                    wxT("/Spectrum/FFTYGrid"),
                    false);
#endif //EXPERIMENTAL_FFT_Y_GRID
   }
   S.EndStatic();

#ifdef EXPERIMENTAL_FIND_NOTES
   S.StartStatic(_("FFT Find Notes"));
   {
      S.StartTwoColumn();
      {
         mFindNotesMinA =
            S.TieTextBox(_("Minimum Amplitude (dB):"),
                         wxT("/Spectrum/FindNotesMinA"),
                         -30L,
                         8);

         mFindNotesN =
            S.TieTextBox(_("Max. Number of Notes (1..128):"),
                         wxT("/Spectrum/FindNotesN"),
                         5L,
                         8);
      }
      S.EndTwoColumn();

      S.TieCheckBox(_("&Find Notes"),
                    wxT("/Spectrum/FFTFindNotes"),
                    false);

      S.TieCheckBox(_("&Quantize Notes"),
                    wxT("/Spectrum/FindNotesQuantize"),
                    false);
   }
   S.EndStatic();
#endif //EXPERIMENTAL_FIND_NOTES
}

bool SpectrumPrefs::Validate()
{
   long maxFreq;
   if (!mMaxFreq->GetValue().ToLong(&maxFreq)) {
      wxMessageBox(_("The maximum frequency must be an integer"));
      return false;
   }
   if (maxFreq < 100 || maxFreq > 100000) {
      wxMessageBox(_("Maximum frequency must be in the range 100 Hz - 100,000 Hz"));
      return false;
   }

   long minFreq;
   if (!mMinFreq->GetValue().ToLong(&minFreq)) {
      wxMessageBox(_("The minimum frequency must be an integer"));
      return false;
   }
   if (minFreq < 0) {
      wxMessageBox(_("Minimum frequency must be at least 0 Hz"));
      return false;
   }

   if (maxFreq < minFreq) {
      wxMessageBox(_("Minimum frequency must be less than maximum frequency"));
      return false;
   }

   long gain;
   if (!mGain->GetValue().ToLong(&gain)) {
      wxMessageBox(_("The gain must be an integer"));
      return false;
   }
   long range;
   if (!mRange->GetValue().ToLong(&range)) {
      wxMessageBox(_("The range must be a positive integer"));
      return false;
   }
   if (range <= 0) {
      wxMessageBox(_("The range must be at least 1 dB"));
      return false;
   }

   long frequencygain;
   if (!mFrequencyGain->GetValue().ToLong(&frequencygain)) {
      wxMessageBox(_("The frequency gain must be an integer"));
      return false;
   }
   if (frequencygain < 0) {
      wxMessageBox(_("The frequency gain cannot be negative"));
      return false;
   }
   if (frequencygain > 60) {
      wxMessageBox(_("The frequency gain must be no more than 60 dB/dec"));
      return false;
   }
#ifdef EXPERIMENTAL_FIND_NOTES
   long findNotesMinA;
   if (!mFindNotesMinA->GetValue().ToLong(&findNotesMinA)) {
      wxMessageBox(_("The minimum amplitude (dB) must be an integer"));
      return false;
   }

   long findNotesN;
   if (!mFindNotesN->GetValue().ToLong(&findNotesN)) {
      wxMessageBox(_("The maximum number of notes must be an integer"));
      return false;
   }
   if (findNotesN < 1 || findNotesN > 128) {
      wxMessageBox(_("The maximum number of notes must be in the range 1..128"));
      return false;
   }
#endif //EXPERIMENTAL_FIND_NOTES

   return true;
}

bool SpectrumPrefs::Apply()
{
   ShuttleGui S(this, eIsSavingToPrefs);
   PopulateOrExchange(S);

   return true;
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
// arch-tag: 54d8e954-f415-40e9-afa4-9d53ab37770d

