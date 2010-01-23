/**********************************************************************

  Audacity: A Digital Audio Editor

  Normalize.h

  Dominic Mazzoni
  Vaughan Johnson (Preview)

**********************************************************************/

#ifndef __AUDACITY_EFFECT_NORMALIZE__
#define __AUDACITY_EFFECT_NORMALIZE__

#include "Effect.h"

#include <wx/checkbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

class wxString;

class WaveTrack;

class EffectNormalize: public Effect
{
 friend class NormalizeDialog;

 public:
   EffectNormalize();
   
   virtual wxString GetEffectName() {
      return wxString(_("Normalize..."));
   }

   virtual std::set<wxString> GetEffectCategories() {
      std::set<wxString> result;
      result.insert(wxT("http://lv2plug.in/ns/lv2core#UtilityPlugin"));
      result.insert(wxT("http://lv2plug.in/ns/lv2core#AmplifierPlugin"));
      return result;
   }

   // This is just used internally, users should not see it.  Do not translate.
   virtual wxString GetEffectIdentifier() {
      return wxT("Normalize");
   }
   
   virtual wxString GetEffectAction() {
      return wxString(_("Normalizing..."));
   }
   
   virtual wxString GetEffectDescription(); // useful only after parameter values have been set

   virtual bool PromptUser();
   virtual bool TransferParameters( Shuttle & shuttle );

   virtual bool Init();
   virtual void End();
   virtual bool CheckWhetherSkipEffect();
   virtual bool Process();
   
 private:
   bool ProcessOne(WaveTrack * t,
                   sampleCount start, sampleCount end);

   virtual void StartAnalysis();
   virtual void AnalyzeData(float *buffer, sampleCount len);

   virtual void StartProcessing();
   virtual void ProcessData(float *buffer, sampleCount len);

   bool   mGain;
   bool   mDC;
   double mLevel;

   int    mCurTrackNum;
   double mCurRate;
   double mCurT0;
   double mCurT1;
   int    mCurChannel;
   float  mMult;
   float  mOffset;
   float  mMin;
   float  mMax;
   double mSum;
   int    mCount;
};

//----------------------------------------------------------------------------
// NormalizeDialog
//----------------------------------------------------------------------------

class NormalizeDialog: public EffectDialog
{
 public:
   // constructors and destructors
   NormalizeDialog(EffectNormalize *effect, wxWindow * parent);

   // method declarations
   void PopulateOrExchange(ShuttleGui & S);
   bool TransferDataToWindow();
   bool TransferDataFromWindow();

 private:
	// handlers
   void OnUpdateUI(wxCommandEvent& evt);
   void OnPreview(wxCommandEvent &event);

   void UpdateUI();

 private:
   EffectNormalize *mEffect;
   wxCheckBox *mGainCheckBox;
   wxCheckBox *mDCCheckBox;
   wxStaticText *mLevelMinux;
   wxTextCtrl *mLevelTextCtrl;
   wxStaticText *mLeveldB;

   DECLARE_EVENT_TABLE()

 public:   
   bool mGain;
   bool mDC;
   double mLevel;
};

#endif


// Indentation settings for Vim and Emacs and unique identifier for Arch, a
// version control system. Please do not modify past this point.
//
// Local Variables:
// c-basic-offset: 3
// indent-tabs-mode: nil
// End:
//
// vim: et sts=3 sw=3
// arch-tag: 2e3f0feb-9ac1-4bac-ba42-3d7e37007aa8

