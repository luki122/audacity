/**********************************************************************

  Audacity: A Digital Audio Editor

  ClickRemoval.cpp

  Craig DeForest

*******************************************************************//**

\class EffectClickRemoval
\brief An Effect.

  Clicks are identified as small regions of high amplitude compared
  to the surrounding chunk of sound.  Anything sufficiently tall compared
  to a large (2048 sample) window around it, and sufficiently narrow,
  is considered to be a click.

  The structure was largely stolen from Domonic Mazzoni's NoiseRemoval
  module, and reworked for the new effect.

  This file is intended to become part of Audacity.  You may modify
  and/or distribute it under the same terms as Audacity itself.

*//****************************************************************//**

\class ClickRemovalDialog
\brief Dialog used with EffectClickRemoval

*//*******************************************************************/


#include "../Audacity.h"

#include <math.h>

#if defined(__WXMSW__) && !defined(__CYGWIN__)
#include <float.h>
#define finite(x) _finite(x)
#endif

#include <wx/msgdlg.h>
#include <wx/textdlg.h>
#include <wx/brush.h>
#include <wx/image.h>
#include <wx/dcmemory.h>
#include <wx/statbox.h>
#include <wx/intl.h>

#include "ClickRemoval.h"
#include "../ShuttleGui.h"
#include "../Envelope.h"
// #include "../FFT.h"
#include "../WaveTrack.h"
#include "../Prefs.h"

#define MIN_THRESHOLD   0
#define MAX_THRESHOLD   900
#define MIN_CLICK_WIDTH 0
#define MAX_CLICK_WIDTH 40

EffectClickRemoval::EffectClickRemoval()
{
   windowSize = 8192;
//   mThresholdLevel = 200;
//   mClickWidth = 20;
   sep=2049;

   Init();
}

EffectClickRemoval::~EffectClickRemoval()
{
}

bool EffectClickRemoval::Init()
{
   mThresholdLevel = gPrefs->Read(wxT("/Effects/ClickRemoval/ClickThresholdLevel"), 200);
   if ((mThresholdLevel < MIN_THRESHOLD) || (mThresholdLevel > MAX_THRESHOLD)) {  // corrupted Prefs?
      mThresholdLevel = 0;  //Off-skip
      gPrefs->Write(wxT("/Effects/ClickRemoval/ClickThresholdLevel"), mThresholdLevel);
   }
   mClickWidth = gPrefs->Read(wxT("/Effects/ClickRemoval/ClickWidth"), 20);
   if ((mClickWidth < MIN_CLICK_WIDTH) || (mClickWidth > MAX_CLICK_WIDTH)) {  // corrupted Prefs?
      mClickWidth = 0;  //Off-skip
      gPrefs->Write(wxT("/Effects/ClickRemoval/ClickWidth"), mClickWidth);
   }
   return gPrefs->Flush();
}

bool EffectClickRemoval::CheckWhetherSkipEffect()
{
   bool rc = ((mClickWidth == 0) || (mThresholdLevel == 0));
   return rc;
}

bool EffectClickRemoval::PromptUser()
{
   ClickRemovalDialog dlog(this, mParent);
   dlog.mThresh = mThresholdLevel;
   dlog.mWidth = mClickWidth;

   dlog.TransferDataToWindow();
   dlog.CentreOnParent();
   dlog.ShowModal();

   if (dlog.GetReturnCode() == wxID_CANCEL)
      return false;

   mThresholdLevel = dlog.mThresh;
   mClickWidth = dlog.mWidth;

   gPrefs->Write(wxT("/Effects/ClickRemoval/ClickThresholdLevel"), mThresholdLevel);
   gPrefs->Write(wxT("/Effects/ClickRemoval/ClickWidth"), mClickWidth);

   return gPrefs->Flush();
}
bool EffectClickRemoval::TransferParameters( Shuttle & shuttle )
{  
   shuttle.TransferInt(wxT("Threshold"),mThresholdLevel,0);
   shuttle.TransferInt(wxT("Width"),mClickWidth,0);
   return true;
}

bool EffectClickRemoval::Process()
{
   this->CopyInputTracks(); // Set up mOutputTracks.
   bool bGoodResult = true;

   SelectedTrackListOfKindIterator iter(Track::Wave, mOutputTracks);
   WaveTrack *track = (WaveTrack *) iter.First();
   int count = 0;
   while (track) {
      double trackStart = track->GetStartTime();
      double trackEnd = track->GetEndTime();
      double t0 = mT0 < trackStart? trackStart: mT0;
      double t1 = mT1 > trackEnd? trackEnd: mT1;

      if (t1 > t0) {
         sampleCount start = track->TimeToLongSamples(t0);
         sampleCount end = track->TimeToLongSamples(t1);
         sampleCount len = (sampleCount)(end - start);

         if (!ProcessOne(count, track, start, len))
         {
            bGoodResult = false;
            break;
         }
      }

      track = (WaveTrack *) iter.Next();
      count++;
   }
   this->ReplaceProcessedTracks(bGoodResult); 
   return bGoodResult;
}

bool EffectClickRemoval::ProcessOne(int count, WaveTrack * track,
                                    sampleCount start, sampleCount len)
{
   bool rc = true;
   sampleCount s = 0;
   sampleCount idealBlockLen = track->GetMaxBlockSize() * 4;

   if (idealBlockLen % windowSize != 0)
      idealBlockLen += (windowSize - (idealBlockLen % windowSize));

   float *buffer = new float[idealBlockLen];

   float *datawindow = new float[windowSize];

   int i;

   while((s < len)&&((len-s)>(windowSize/2))) {
      sampleCount block = idealBlockLen;
      if (s + block > len)
         block = len - s;

      track->Get((samplePtr) buffer, floatSample, start + s, block);

      for(i=0; i<(block-windowSize/2); i+=windowSize/2) {
         int wcopy = windowSize;
         if (i + wcopy > block)
            wcopy = block - i;

         int j;
         for(j=0; j<wcopy; j++)
            datawindow[j] = buffer[i+j];
         for(j=wcopy; j<windowSize; j++)
            datawindow[j] = 0;

         RemoveClicks(windowSize, datawindow);

         for(j=0; j<wcopy; j++)
           buffer[i+j] = datawindow[j];
      }

      track->Set((samplePtr) buffer, floatSample, start + s, block);

      s += block;

      if (TrackProgress(count, s / (double) len)) {
         rc = false;
         break;
      }
   }

   delete[] buffer;
   delete[] datawindow;

   return rc;
}

void EffectClickRemoval::RemoveClicks(sampleCount len, float *buffer)
{
   int i;
   int j;
   int left = 0;

   float msw;
   int ww;
   int s2 = sep/2;
   float *ms_seq = new float[len];
   float *b2 = new float[len];

   for( i=0; i<len; i++)
      b2[i] = buffer[i]*buffer[i];

   /* Shortcut for rms - multiple passes through b2, accumulating
    * as we go.
    */
   for(i=0;i<len;i++)
      ms_seq[i]=b2[i];

   for(i=1; i < sep; i *= 2) {
      for(j=0;j<len-i; j++)
         ms_seq[j] += ms_seq[j+i];
      }
   /* Cheat by truncating sep to next-lower power of two... */
      sep = i;

      for( i=0; i<len-sep; i++ ) {
         ms_seq[i] /= sep;
      }
      /* ww runs from about 4 to mClickWidth.  wrc is the reciprocal;
       * chosen so that integer roundoff doesn't clobber us.
       */
      int wrc;
      for(wrc=mClickWidth/4; wrc>=1; wrc /= 2) {
         ww = mClickWidth/wrc;

         for( i=0; i<len-sep; i++ ){
            msw = 0;
            for( j=0; j<ww; j++) {
               msw += b2[i+s2+j];
            }
            msw /= ww;

            if(msw >= mThresholdLevel * ms_seq[i]/10) {
               if( left == 0 ) {
                  left = i+s2;
               }
            } else {
               if(left != 0 && i-left+s2 <= ww*2) {
                  float lv = buffer[left];
                  float rv = buffer[i+ww+s2];
                  for(j=left; j<i+ww+s2; j++) {
                     buffer[j]= (rv*(j-left) + lv*(i+ww+s2-j))/(float)(i+ww+s2-left);
                     b2[j] = buffer[j]*buffer[j];
                  }
                  left=0;
               } else if(left != 0) {
               left = 0;
            }
         }
      }
   }
   delete[] ms_seq;
   delete[] b2;
}


// WDR: class implementations

//----------------------------------------------------------------------------
// ClickRemovalDialog
//----------------------------------------------------------------------------

const static wxChar *numbers[] =
{
   wxT("0"), wxT("1"), wxT("2"), wxT("3"), wxT("4"),
   wxT("5"), wxT("6"), wxT("7"), wxT("8"), wxT("9")
};

// Declare window functions                                                                                                         
                                                                                                                                    
#define ID_THRESH_TEXT     10001
#define ID_THRESH_SLIDER   10002
#define ID_WIDTH_TEXT      10003
#define ID_WIDTH_SLIDER    10004

// Declare ranges

BEGIN_EVENT_TABLE(ClickRemovalDialog, EffectDialog)
    EVT_SLIDER(ID_THRESH_SLIDER, ClickRemovalDialog::OnThreshSlider)
    EVT_SLIDER(ID_WIDTH_SLIDER, ClickRemovalDialog::OnWidthSlider)
    EVT_TEXT(ID_THRESH_TEXT, ClickRemovalDialog::OnThreshText)
    EVT_TEXT(ID_WIDTH_TEXT, ClickRemovalDialog::OnWidthText)
    EVT_BUTTON(ID_EFFECT_PREVIEW, ClickRemovalDialog::OnPreview)
END_EVENT_TABLE()

ClickRemovalDialog::ClickRemovalDialog(EffectClickRemoval *effect,
                                       wxWindow *parent)
:  EffectDialog(parent, _("Click Removal"), PROCESS_EFFECT),
   mEffect(effect)
{
   Init();
}

void ClickRemovalDialog::PopulateOrExchange(ShuttleGui & S)
{
   wxTextValidator vld(wxFILTER_INCLUDE_CHAR_LIST);
   vld.SetIncludes(wxArrayString(10, numbers));

   S.StartHorizontalLay(wxCENTER, false);
   {
      S.AddTitle(_("Click and Pop Removal by Craig DeForest"));
   }
   S.EndHorizontalLay();

   S.StartHorizontalLay(wxCENTER, false);
   {
      // Add a little space
   }
   S.EndHorizontalLay();

   S.StartMultiColumn(3, wxEXPAND);
   S.SetStretchyCol(2);
   {
      // Threshold
      mThreshT = S.Id(ID_THRESH_TEXT).AddTextBox(_("Select threshold (lower is more sensitive):"),
                                                  wxT(""),
                                                  10);
      mThreshT->SetValidator(vld);

      S.SetStyle(wxSL_HORIZONTAL);
      mThreshS = S.Id(ID_THRESH_SLIDER).AddSlider(wxT(""),
                                                  0,
                                                  MAX_THRESHOLD);
      mThreshS->SetName(_("Select threshold"));
      mThreshS->SetRange(MIN_THRESHOLD, MAX_THRESHOLD);
#if defined(__WXGTK__)
      // Force a minimum size since wxGTK allows it to go to zero
      mThreshS->SetMinSize(wxSize(100, -1));
#endif

      // Click width
      mWidthT = S.Id(ID_WIDTH_TEXT).AddTextBox(_("Max spike width (higher is more sensitive):"),
                                               wxT(""),
                                               10);
      mWidthT->SetValidator(vld);

      S.SetStyle(wxSL_HORIZONTAL);
      mWidthS = S.Id(ID_WIDTH_SLIDER).AddSlider(wxT(""),
                                                0,
                                                MAX_CLICK_WIDTH);
      mWidthS->SetName(_("Max spike width"));
      mWidthS->SetRange(MIN_CLICK_WIDTH, MAX_CLICK_WIDTH);
#if defined(__WXGTK__)
      // Force a minimum size since wxGTK allows it to go to zero
      mWidthS->SetMinSize(wxSize(100, -1));
#endif
   }
   S.EndMultiColumn();
   return;
}

bool ClickRemovalDialog::TransferDataToWindow()
{
   mWidthS->SetValue(mWidth);
   mThreshS->SetValue(mThresh);

   mWidthT->SetValue(wxString::Format(wxT("%d"), mWidth));
   mThreshT->SetValue(wxString::Format(wxT("%d"), mThresh));

   return true;
}

bool ClickRemovalDialog::TransferDataFromWindow()
{
   mWidth = TrapLong(mWidthS->GetValue(), MIN_CLICK_WIDTH, MAX_CLICK_WIDTH);
   mThresh = TrapLong(mThreshS->GetValue(), MIN_THRESHOLD, MAX_THRESHOLD);

   return true;
}

// WDR: handler implementations for ClickRemovalDialog

void ClickRemovalDialog::OnWidthText(wxCommandEvent & event)
{
   long val;

   mWidthT->GetValue().ToLong(&val);
   mWidthS->SetValue(TrapLong(val, MIN_CLICK_WIDTH, MAX_CLICK_WIDTH));
}

void ClickRemovalDialog::OnThreshText(wxCommandEvent & event)
{
   long val;

   mThreshT->GetValue().ToLong(&val);
   mThreshS->SetValue(TrapLong(val, MIN_THRESHOLD, MAX_THRESHOLD));
}

void ClickRemovalDialog::OnWidthSlider(wxCommandEvent & event)
{
   mWidthT->SetValue(wxString::Format(wxT("%d"), mWidthS->GetValue()));
}

void ClickRemovalDialog::OnThreshSlider(wxCommandEvent & event)
{
   mThreshT->SetValue(wxString::Format(wxT("%d"), mThreshS->GetValue()));
}

void ClickRemovalDialog::OnPreview(wxCommandEvent & event)
{
   TransferDataFromWindow();
   mEffect->mThresholdLevel = mThresh;
   mEffect->mClickWidth = mWidth;
   mEffect->Preview();
}

// WDR: handler implementations for NoiseRemovalDialog
/*
void ClickRemovalDialog::OnPreview(wxCommandEvent &event)
{
  // Save & restore parameters around Preview, because we didn't do OK.
  int oldLevel = mEffect->mThresholdLevel;
  int oldWidth = mEffect->mClickWidth;
  int oldSep = mEffect->sep;

  mEffect->mThresholdLevel = m_pSlider->GetValue();
  mEffect->mClickWidth = m_pSlider_width->GetValue();
  //  mEffect->sep = m_pSlider_sep->GetValue();

  mEffect->Preview();

  mEffect->sep   = oldSep;
  mEffect->mClickWidth = oldWidth;
  mEffect->mThresholdLevel = oldLevel;
}
*/
