/**********************************************************************

  Audacity: A Digital Audio Editor

  EffectEqualization.cpp

  Mitch Golden
  Vaughan Johnson (Preview)
  Martyn Shaw (FIR filters, response curve, graphic EQ)

*******************************************************************//**

\file Equalization.cpp
\brief Implements EffectEqualiztaion, EqualizationDialog,
EqualizationPanel, EQCurve and EQPoint.

*//****************************************************************//**


\class EffectEqualization
\brief An Effect.

  Performs filtering, using an FFT to do a FIR filter.
  It lets the user draw an arbitrary envelope (using the same
  envelope editing code that is used to edit the track's
  amplitude envelope).

  Also allows the curve to be specified with a series of 'graphic EQ'
  sliders.

  The filter is applied using overlap/add of Hanning windows.

  Has presets of certain specific equalization curves, suitable
  for old recordings.

  Clone of the FFT Filter effect, no longer part of Audacity.

*//****************************************************************//**

\class EqualizationDialog
\brief Dialog used with EffectEqualization

*//****************************************************************//**

\class EqualizationPanel
\brief EqualizationPanel is used with EqualizationDialog and controls
a graph for EffectEqualization.  We should look at amalgamating the
various graphing code, such as provided by FreqWindow and FilterPanel.

*//****************************************************************//**

\class EQCurve
\brief EQCurve is used with EffectEqualization.

*//****************************************************************//**

\class EQPoint
\brief EQPoint is used with EQCurve and hence EffectEqualization.

*//*******************************************************************/

#include "../Audacity.h"
#include "Equalization.h"
#include "../AColor.h"
#include "../ShuttleGui.h"
#include "../PlatformCompatibility.h"
#include "../FileNames.h"
#include "../Envelope.h"
#include "../FFT.h"
#include "../Prefs.h"
#include "../Project.h"
#include "../WaveTrack.h"
#include "../widgets/Ruler.h"
#include "../xml/XMLFileReader.h"
#include "../Theme.h"
#include "../AllThemeResources.h"
#include "../WaveTrack.h"
#include "float_cast.h"
#include <vector>

#include <wx/bitmap.h>
#include <wx/button.h>
#include <wx/msgdlg.h>
#include <wx/brush.h>
#include <wx/dcmemory.h>
#include <wx/event.h>
#include <wx/image.h>
#include <wx/intl.h>
#include <wx/choice.h>
#include <wx/radiobut.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textdlg.h>
#include <wx/ffile.h>
#include <wx/filedlg.h>
#include <wx/filefn.h>
#include <wx/stdpaths.h>
#include <wx/settings.h>
#include <wx/checkbox.h>

#if wxUSE_TOOLTIPS
#include <wx/tooltip.h>
#endif
#include <wx/utils.h>

#include <math.h>

#include <wx/arrimpl.cpp>

WX_DEFINE_OBJARRAY( EQPointArray );
WX_DEFINE_OBJARRAY( EQCurveArray );

const double EqualizationDialog::thirdOct[] =
{
   20., 25., 31., 40., 50., 63., 80., 100., 125., 160., 200.,
   250., 315., 400., 500., 630., 800., 1000., 1250., 1600., 2000.,
   2500., 3150., 4000., 5000., 6300., 8000., 10000., 12500., 16000., 20000.,
};

const float EffectEqualization::curvex[] =
  {
   30., 31., 50., 63., 70., 100., 125., 200., 250., 300.,
   400., 500., 600., 700., 800., 900., 1000., 2000., 3000., 4000.,
   5000., 6000., 7000., 8000., 9000., 10000., 15000., 16000.
};

// Don't want to be warned about double->float precision loss here
// when compiling with MSVC.
#if defined(__WXMSW__)
#pragma warning( disable: 4305 )
#endif
const float EffectEqualization::curvey[][nCurvePoints] =
{
   {
   // amradio
//    30   31   50   63   70   100  125  200  250  300  400  500  600  700
      -20.,-20.,-20.,-20.,-20.,-20.,-16.,-12., -8., -4., 0.0, 0.0, 0.0, 0.0,
//  800  900  1000 2000 3000 4000 5000 6000 7000 8000 9000 10000 15000 16000.
         0.0, 0.0, 0.0, -4., -8.,-12.,-16.,-20.0,-20.,-20.,-20.,-20., -20., -20.0
   },
   {
   // acoustic (see p 52)
      -20.0, -20.0, -20.0,   5.0,   4.4,   3.3,   2.5,   1.7,   0.0,   0.0,
      0.0,     0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,  -1.5,  -2.5,
      -3.3,   -4.0,  -4.5,  -5.0, -10.0, -15.0, -20.0, -20.0
   },
   {
   // NAB
      20.0,   20.0,  16.0,  15.6,  15.5,  13.8,  12.0,   8.0,   6.2,   5.0,
      3.0,     1.7,   1.0,   0.0,  -0.5,  -1.0,  -1.3,  -4.2,  -6.5,  -8.5,
      -10.2, -12.0, -13.0, -14.0, -15.0, -16.0, -20.0, -20.0
   },
   {
   // LP
      13.5,   13.2,  13.0,  12.8,  12.5,  11.0,  10.5,   8.0,   7.0,   6.0,
      3.5,     2.5,   1.5,   1.0,   0.5,  -0.5,  -1.0,  -3.5,  -6.0,  -8.0,
      -10.0, -11.5, -12.5, -13.5, -14.5, -16.0, -21.2, -22.0
   },
   {
   // AES
      22.5,   22.5,  18.0,  16.0,  15.0,  12.0,  10.0,   6.5,   5.2,   4.5,
      3.0,     2.0,   1.5,   1.0,   0.5,   0.0,   0.0,  -2.2,  -4.0,  -5.5,
      -6.7,   -8.0,  -9.0, -10.0, -11.0, -12.0, -15.5, -16.0
   },
   {
   // Decca FFRR Micro
      14.0,   14.0,  14.0,  13.8,  13.5,  12.5,  11.5,   8.5,   7.2,   6.0,
      4.0,     2.5,   1.5,   1.0,   0.5,   0.0,   0.0,  -1.5,  -3.0,  -4.5,
      -6.0,   -7.0,  -8.0,  -8.5,  -9.0, -10.0, -12.6, -13.0
   },
   {
   // Decca FFRR 78
      22.0,   21.5,  14.0,  11.2,   9.8,   6.0,   2,0,   1.5,   1.0,   0.5,
      0.0,     0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0   -0.5,
      -1.0,   -2.0,  -2.5,  -3.5,  -4.0,  -4.5,  -7.0,  -7.5
   },
   {
   // RIAA
      18.6,   18.5,  16.9,  15.9,  15.3,  13.1,  11.6,   8.2,   6.7,   5.5,
      3.8,     2.6,   1.8,   1.2,   0.8,   0.3,   0.0,  -2.6,  -4.7,  -6.6,
      -8.2,   -9.6, -10.8, -11.9, -12.9, -13.7, -17.2, -17.7
   },
   {
   // Col 78
      16.0,   16.0,  16.0,  14.0,  12.5,  10.0,   8.5,   5.0,   4.0,   3.0,
      2.0,     1.0,   0.5,   0.2,   0.0,  -0.5,  -1.0,  -3.5,  -6.0,  -8.0,
      -10.0, -11.5, -12.5, -13.5, -14.5, -16.0, -21.2, -22.0
   },
   {
   // Decca FFRR LP
      17.5,   17.2,  14.0,  12.0,  11.5,   9.0,   7.5,   5.0,   4.0,   3.0,
      2.0,     1.5,   1.0,   0.7,   0.2,   0.0,   0.0,  -4.0,  -6.7,  -8.5,
      -10.0, -11.0, -12.0, -13.0, -13.2, -14.0, -16.0, -16.0
   },
   {
   // EMI 78
      14.0,   14.0,  14.0,  12.0,  11.0,   8.0,   7.0,   4.0,   3.0,   2.0,
      1.0,     0.5,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,
      0.0,     0.0,   0.0,   0.0,   0.0,   0.0,  -5.0,  -5.0
   },
   {
   // RCA Victor 1938
      24.0,   24.0,  24.0,  21.8,  20.0,  16.0,  13.0,   9.0,   7.5,   6.0,
      4.0,     3.0,   2.0,   1.5,   1.0,   0.5,   0.0,  -2.5,  -5.0,  -6.5,
      -7.5,   -8.0,  -7.7,  -7.5,  -7.5,  -7.5,  -7.5,  -7.5
   },
   {
   // RCA Victor 1947
      24.0,   24.0,  24.0,  21.8,  20.0,  16.0,  13.0,   9.0,   7.5,   6.0,
      4.0,     3.0,   2.0,   1.5,   1.0,   0.5,   0.0,  -2.5,  -5.0,  -6.5,
      -8.0,  -10.0, -11.5, -12.0, -12.5, -12.5, -12.5, -12.5
   }
  };
#if defined(__WXMSW__)
#pragma warning( default: 4305 )
#endif

// LLL:  This seem to have a strange interaction with wxWidgets 2.8.9.  It's
//       possible that the wxSlider was fixed as it seems to work ok for me
//       under Ubuntu 8.10 with self build wxWidgets 2.8.9.
//
//       Leaving here for now...just disabling
#ifdef __WXGTK__disabled
/*
 * wxSlider exhibits strange behaviour on wxGTK when wxSL_INVERSE and/or
 * negative scale values are used. This affects at least SUSE 9.3 with
 * self-compiled wxWidgets 2.6.2 and Kubuntu 6.06. This class is used
 * to transparently work around those problems. Use wxSliderBugfix wherever
 * you would have used a slider with wxSL_INVERSE and/or negative scale
 * values.
 *
 * The equalization class uses wxSliderBugfix everywhere even when not
 * strictly needed. This is important because we override some wxSlider
 * functions which are not virtual, and so we need to cast to wxSliderBugfix
 * instead of to wxSlider to allow the right function to be called statically.
 */
class wxSliderBugfix : public wxSlider
{
public:
   wxSliderBugfix(wxWindow* parent, wxWindowID id, int value,
                  int minValue, int maxValue,
                  const wxPoint& point = wxDefaultPosition,
                  const wxSize& size = wxDefaultSize,
                  long style = wxSL_HORIZONTAL,
                  const wxValidator& validator = wxDefaultValidator,
                  const wxString& name = wxT("slider")) :
         wxSlider(parent, id, 0, 0, maxValue - minValue, point,
                  size, style & ~wxSL_INVERSE, validator, name) {
      isInverse = (style & wxSL_INVERSE) != 0;
      originalMinValue = minValue;
      SetValue(value);
   }

   int GetMin() const {
      return originalMinValue;
   }

   int GetMax() const {
      return wxSlider::GetMax() + originalMinValue;
   }

   int GetValue() const {
      if (isInverse)
         return wxSlider::GetMax() - wxSlider::GetValue() + originalMinValue;
      else
         return wxSlider::GetValue() + originalMinValue;
   }

   void SetValue(int value) {
      if (isInverse)
         wxSlider::SetValue(wxSlider::GetMax() - value + originalMinValue);
      else
         wxSlider::SetValue(value - originalMinValue);
   }

private:
   bool isInverse;
   int originalMinValue;
};

#else

/*
 * On platforms other than wxGTK, the workaround is not needed
 */
#define wxSliderBugfix wxSlider

#endif // ifdef __WXGTK__

const wxChar * EffectEqualization::curveNames[] =
  {
    wxT("amradio"),
    wxT("acoustic"),
    wxT("NAB"),
    wxT("Columbia LP"),
    wxT("AES"),
    wxT("Decca FFRR Micro"),
    wxT("Decca FFRR 78"),
    wxT("RIAA"),
    wxT("Columbia 78"),
    wxT("Decca FFRR LP"),
    wxT("EMI 78"),
    wxT("RCA Victor 1938"),
    wxT("RCA Victor 1947")
  };

#define NUM_INTERP_CHOICES 3
static wxString interpChoiceStrings[NUM_INTERP_CHOICES];

void EffectEqualization::ReadPrefs()
{

   gPrefs->Read(wxT("/CsPresets/EQFilterLength"),
         &mM, 4001);
   if ((mM < 21) || (mM > 8191)) {  // corrupted Prefs?
      mM = 4001;  //default
      gPrefs->Write(wxT("/CsPresets/EQFilterLength"), mM);
   }
   gPrefs->Read(wxT("/CsPresets/EQCurveName"), &mCurveName, wxT("custom"));
   gPrefs->Read(wxT("/CsPresets/EQLin"), &mLin, false);
   gPrefs->Read(wxT("/CsPresets/EQdBMin"), &mdBMin, -30.0);
   if ((mdBMin < -120) || (mdBMin > -10)) {  // corrupted Prefs?
      mdBMin = -30;  //default
      gPrefs->Write(wxT("/CsPresets/EQFilterLength"), mdBMin);
   }
   gPrefs->Read(wxT("/CsPresets/EQdBMax"), &mdBMax, 30.);
   if ((mdBMax < 0) || (mdBMax > 60)) {  // corrupted Prefs?
      mdBMax = 30;  //default
      gPrefs->Write(wxT("/CsPresets/EQFilterLength"), mdBMax);
   }
   gPrefs->Read(wxT("/CsPresets/EQDrawMode"), &mDrawMode, true);
   gPrefs->Read(wxT("/CsPresets/EQInterp"), &mInterp, 0);
   gPrefs->Read(wxT("/CsPresets/EQDrawGrid"), &mDrawGrid, true);
}

EffectEqualization::EffectEqualization()
{
   hFFT = InitializeFFT(windowSize);
   mFFTBuffer = new float[windowSize];
   mFilterFuncR = new float[windowSize];
   mFilterFuncI = new float[windowSize];
   ReadPrefs();

   mPrompting = false;

   interpChoiceStrings[0] = _("B-spline");
   interpChoiceStrings[1] = _("Cosine");
   interpChoiceStrings[2] = _("Cubic");
}


EffectEqualization::~EffectEqualization()
{
   if(hFFT) 
      EndFFT(hFFT);
   hFFT = NULL;
   if(mFFTBuffer)
      delete[] mFFTBuffer;
   mFFTBuffer = NULL;
   if(mFilterFuncR)
      delete[] mFilterFuncR;
   if(mFilterFuncI)
      delete[] mFilterFuncI;
   mFilterFuncR = NULL;
   mFilterFuncI = NULL;
}

bool EffectEqualization::Init()
{
   return(true);
}

bool EffectEqualization::PromptUser()
{
   // Detect whether we are editing a batch chain by checking the parent window
   mEditingBatchParams = (mParent != GetActiveProject());
   if (!mEditingBatchParams)
   {
      ReadPrefs();
   }

   TrackListOfKindIterator iter(Track::Wave, mTracks);
   WaveTrack *t = (WaveTrack *) iter.First();
   float hiFreq;
   if (t)
      hiFreq = ((float)(t->GetRate())/2.);
   else
      hiFreq = ((float)(GetActiveProject()->GetRate())/2.);


   EqualizationDialog dlog(this, ((double)loFreqI), hiFreq, mFilterFuncR, mFilterFuncI,
                           windowSize, mCurveName, mEditingBatchParams, mParent, -1, _("Equalization"));

   dlog.M = mM;
   dlog.curveName = mCurveName;
   dlog.linCheck = mLin;
   dlog.dBMin = mdBMin;
   dlog.dBMax = mdBMax;
   dlog.drawMode = mDrawMode;
   dlog.interp = mInterp;
   dlog.drawGrid = mDrawGrid;
   dlog.CentreOnParent();

   mPrompting = true;   // true when previewing, false in batch
   dlog.ShowModal();
   mPrompting = false;

   if (!dlog.GetReturnCode())
      return false;

   mM = dlog.M;
   mCurveName = dlog.curveName;
   mLin = dlog.linCheck;
   mdBMin = dlog.dBMin;
   mdBMax = dlog.dBMax;
   mDrawMode = dlog.drawMode;
   mInterp = dlog.interp;
   mDrawGrid = dlog.drawGrid;

   if (!mEditingBatchParams)
   {
      gPrefs->Write(wxT("/CsPresets/EQFilterLength"),mM);
      gPrefs->Write(wxT("/CsPresets/EQCurveName"),mCurveName);
      gPrefs->Write(wxT("/CsPresets/EQLin"),mLin);
      gPrefs->Write(wxT("/CsPresets/EQdBMin"),mdBMin);
      gPrefs->Write(wxT("/CsPresets/EQdBMax"),mdBMax);
      gPrefs->Write(wxT("/CsPresets/EQDrawMode"),mDrawMode);
      gPrefs->Write(wxT("/CsPresets/EQInterp"), mInterp);
      gPrefs->Write(wxT("/CsPresets/EQDrawGrid"), mDrawGrid);
   }

   return true;
}

bool EffectEqualization::DontPromptUser()
{
   TrackListOfKindIterator iter(Track::Wave, mTracks);
   WaveTrack *t = (WaveTrack *) iter.First();
   float hiFreq;
   if (t)
      hiFreq = ((float)(t->GetRate())/2.);
   else
      hiFreq = ((float)(GetActiveProject()->GetRate())/2.);

   EqualizationDialog dlog(this, ((double)loFreqI), hiFreq, mFilterFuncR, mFilterFuncI,
                           windowSize, mCurveName, false, NULL, -1, _("Equalization"));

   dlog.M = mM;
   dlog.curveName = mCurveName;
   dlog.linCheck = false;
   dlog.interp = mInterp;
   // IS required here - no call to ShowModal!
   dlog.TransferDataToWindow();
   dlog.CalcFilter();   // is needed here - the one done due to TransferDataToWindow->OnLinFreq isn't reliable

   return true;
}

bool EffectEqualization::TransferParameters( Shuttle & shuttle )
{
   shuttle.TransferInt(wxT("FilterLength"),mM,4001);
   shuttle.TransferString(wxT("CurveName"),mCurveName,wxT("eric"));
   shuttle.TransferBool(wxT("InterpolateLin"),mLin,false);
   shuttle.TransferEnum(wxT("interpolationMethod"),mInterp, NUM_INTERP_CHOICES,interpChoiceStrings);

   if(!mPrompting)
      DontPromptUser();   // not previewing, ie batch mode or initial setup
   return true;
}

bool EffectEqualization::Process()
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


bool EffectEqualization::ProcessOne(int count, WaveTrack * t,
                                 sampleCount start, sampleCount len)
{
   // create a new WaveTrack to hold all of the output, including 'tails' each end
   AudacityProject *p = GetActiveProject();
   WaveTrack *output = p->GetTrackFactory()->NewWaveTrack(floatSample, t->GetRate());

   int L = windowSize - (mM - 1);   //Process L samples at a go
   sampleCount s = start;
   sampleCount idealBlockLen = t->GetMaxBlockSize() * 4;
   if (idealBlockLen % L != 0)
      idealBlockLen += (L - (idealBlockLen % L));

   float *buffer = new float[idealBlockLen];

   float *window1 = new float[windowSize];
   float *window2 = new float[windowSize];
   float *thisWindow = window1;
   float *lastWindow = window2;

   sampleCount originalLen = len;

   int i,j;
   for(i=0; i<windowSize; i++)
      lastWindow[i] = 0;

   TrackProgress(count, 0.);
	bool bLoopSuccess = true;
   int wcopy = 0;
   int offset = (mM - 1)/2;

   while(len)
   {
      sampleCount block = idealBlockLen;
      if (block > len)
         block = len;

      t->Get((samplePtr)buffer, floatSample, s, block);

      for(i=0; i<block; i+=L)   //go through block in lumps of length L
      {
         wcopy = L;
         if (i + wcopy > block)   //if last lump would exceed block
            wcopy = block - i;   //shorten it
         for(j=0; j<wcopy; j++)
            thisWindow[j] = buffer[i+j];   //copy the L (or remaining) samples
         for(j=wcopy; j<windowSize; j++)
            thisWindow[j] = 0;   //this includes the padding

         Filter(windowSize, thisWindow);

         // Overlap - Add
         for(j=0; (j<mM-1) && (j<wcopy); j++)
            buffer[i+j] = thisWindow[j] + lastWindow[L + j];
         for(j=mM-1; j<wcopy; j++)
            buffer[i+j] = thisWindow[j];

         float *tempP = thisWindow;
         thisWindow = lastWindow;
         lastWindow = tempP;
      }  //next i, lump of this block

      output->Append((samplePtr)buffer, floatSample, block);
      len -= block;
      s += block;

      if (TrackProgress(count, (s-start)/(double)originalLen))
      {
			bLoopSuccess = false;
			break;
		}
   }

   if(bLoopSuccess)
   {
      // mM-1 samples of 'tail' left in lastWindow, get them now
      if(wcopy < (mM-1)) {
         // Still have some overlap left to process
         // (note that lastWindow and thisWindow have been exchanged at this point
         //  so that 'thisWindow' is really the window prior to 'lastWindow')
         for(j=0; j<mM-1-wcopy; j++)
            buffer[j] = lastWindow[wcopy + j] + thisWindow[L + wcopy + j];
         // And fill in the remainder after the overlap
         for( ; j<mM-1; j++)
            buffer[j] = lastWindow[wcopy + j];
      } else {
         for(j=0; j<mM-1; j++)
            buffer[j] = lastWindow[wcopy + j];
      }
      output->Append((samplePtr)buffer, floatSample, mM-1);
      output->Flush();

      // now move the appropriate bit of the output back to the track
      // (this could be enhanced in the future to use the tails)
      double offsetT0 = t->LongSamplesToTime((sampleCount)offset);
      double lenT = t->LongSamplesToTime(originalLen);
      // 'start' is the sample offset in 't', the passed in track
      // 'startT' is the equivalent time value
      // 'output' starts at zero
      double startT = t->LongSamplesToTime(start);
      
      //output has one waveclip for the total length, even though 
      //t might have whitespace seperating multiple clips
      //we want to maintain the original clip structure, so
      //only paste the intersections of the new clip.
      
      //Find the bits of clips that need replacing
      std::vector<std::pair<double, double> > clipStartEndTimes;
      std::vector<std::pair<double, double> > clipRealStartEndTimes; //the above may be truncated due to a clip being partially selected
      for (WaveClipList::compatibility_iterator it=t->GetClipIterator(); it; it=it->GetNext())
      {
         WaveClip *clip;
         double clipStartT;
         double clipEndT;
         
         clip = it->GetData();
         clipStartT = clip->GetStartTime();
         clipEndT = clip->GetEndTime();
         if( clipEndT <= startT )
            continue;   // clip is not within selection
         if( clipStartT >= startT + lenT )
            continue;   // clip is not within selection
            
         //save the actual clip start/end so that we can rejoin them after we paste.
         clipRealStartEndTimes.push_back(std::pair<double,double>(clipStartT,clipEndT));            
            
         if( clipStartT < startT )  // does selection cover the whole clip?
            clipStartT = startT; // don't copy all the new clip
         if( clipEndT > startT + lenT )  // does selection cover the whole clip?
            clipEndT = startT + lenT; // don't copy all the new clip
         
         //save them
         clipStartEndTimes.push_back(std::pair<double,double>(clipStartT,clipEndT));
      }
      //now go thru and replace the old clips with new
      for(unsigned int i=0;i<clipStartEndTimes.size();i++)
      {
         Track *toClipOutput;
         //remove the old audio and get the new
         t->Clear(clipStartEndTimes[i].first,clipStartEndTimes[i].second);
         output->Copy(clipStartEndTimes[i].first-startT+offsetT0,clipStartEndTimes[i].second-startT+offsetT0, &toClipOutput);   
         if(toClipOutput)
         {
            //put the processed audio in
            t->Paste(clipStartEndTimes[i].first,toClipOutput);
            //if the clip was only partially selected, the Paste will have created a split line.  Join is needed to take care of this
            //This is not true when the selection is fully contained within one clip (second half of conditional)
            if( (clipRealStartEndTimes[i].first  != clipStartEndTimes[i].first || 
                 clipRealStartEndTimes[i].second != clipStartEndTimes[i].second) &&
                 !(clipRealStartEndTimes[i].first <= startT &&  
                 clipRealStartEndTimes[i].second >= startT+lenT) )
               t->Join(clipRealStartEndTimes[i].first,clipRealStartEndTimes[i].second);
            delete toClipOutput;
         }
      }
   }

   delete[] buffer;
   delete[] window1;
   delete[] window2;
   delete output;

   return bLoopSuccess;
}

void EffectEqualization::Filter(sampleCount len,
         float *buffer)
{
   int i;
   float re,im;
   // Apply FFT
   RealFFTf(buffer, hFFT);
   //FFT(len, false, inr, NULL, outr, outi);

   // Apply filter
   // DC component is purely real
   mFFTBuffer[0] = buffer[0] * mFilterFuncR[0];
   for(i=1; i<(len/2); i++)
   {
      re=buffer[hFFT->BitReversed[i]  ];
      im=buffer[hFFT->BitReversed[i]+1];
      mFFTBuffer[2*i  ] = re*mFilterFuncR[i] - im*mFilterFuncI[i];
      mFFTBuffer[2*i+1] = re*mFilterFuncI[i] + im*mFilterFuncR[i];
   }
   // Fs/2 component is purely real
   mFFTBuffer[1] = buffer[1] * mFilterFuncR[len/2];

   // Inverse FFT and normalization
   InverseRealFFTf(mFFTBuffer, hFFT);
   ReorderToTime(hFFT, mFFTBuffer, buffer);
}


//----------------------------------------------------------------------------
// EqualizationPanel
//----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(EqualizationPanel, wxPanel)
    EVT_PAINT(EqualizationPanel::OnPaint)
    EVT_MOUSE_EVENTS(EqualizationPanel::OnMouseEvent)
    EVT_MOUSE_CAPTURE_LOST(EqualizationPanel::OnCaptureLost)
    EVT_SIZE(EqualizationPanel::OnSize)
END_EVENT_TABLE()

EqualizationPanel::EqualizationPanel( double loFreq, double hiFreq,
                     Envelope *env,
                     EqualizationDialog *parent,
                     float *filterFuncR, float *filterFuncI, long windowSize,
                     wxWindowID id, const wxPoint& pos, const wxSize& size):
   wxPanel(parent, id, pos, size)
{
   mOutr = NULL;
   mOuti = NULL;

   mBitmap = NULL;
   mWidth = 0;
   mHeight = 0;
   mLoFreq = loFreq;
   mHiFreq = hiFreq;
   M = 21;  // should be odd
   mWindowSize = windowSize;
   mFilterFuncR = filterFuncR;
   mFilterFuncI = filterFuncI;
   mParent = parent;

   mEnvelope = env;
   mEnvelope->Flatten(0.);
   mEnvelope->Mirror(false);
   mEnvelope->SetTrackLen(1.0);

   RecalcRequired = true;
}

EqualizationPanel::~EqualizationPanel()
{
   if (mBitmap)
      delete mBitmap;
   if (mOuti)
      delete [] mOuti;
   if (mOutr)
      delete [] mOutr;
}

void EqualizationPanel::Recalc()
{
   if (mOutr)
      delete [] mOutr;
   mOutr = new float[mWindowSize];

   if (mOuti)
      delete [] mOuti;
   mOuti = new float[mWindowSize];

   mParent->CalcFilter();   //to calculate the actual response
#ifdef EXPERIMENTAL_USE_REALFFTF
   InverseRealFFT(mWindowSize, mFilterFuncR, mFilterFuncI, mOutr);
#else
   FFT(mWindowSize,true,mFilterFuncR,mFilterFuncI,mOutr,mOuti);   //work out FIR response - note mOuti will be all zeros
#endif // EXPERIMENTAL_USE_REALFFTF
}

void EqualizationPanel::OnSize(wxSizeEvent & evt)
{
   Refresh( false );
}

void EqualizationPanel::OnPaint(wxPaintEvent & evt)
{
   wxPaintDC dc(this);
   if(RecalcRequired) {
      Recalc();
      RecalcRequired = false;
   }
   int width, height;
   GetSize(&width, &height);

   if (!mBitmap || mWidth!=width || mHeight!=height)
   {
      if (mBitmap)
         delete mBitmap;

      mWidth = width;
      mHeight = height;
      mBitmap = new wxBitmap(mWidth, mHeight);
   }

   wxBrush bkgndBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));

   wxMemoryDC memDC;
   memDC.SelectObject(*mBitmap);

   wxRect bkgndRect;
   bkgndRect.x = 0;
   bkgndRect.y = 0;
   bkgndRect.width = mWidth;
   bkgndRect.height = mHeight;
   memDC.SetBrush(bkgndBrush);
   memDC.SetPen(*wxTRANSPARENT_PEN);
   memDC.DrawRectangle(bkgndRect);

   bkgndRect.y = mHeight;
   memDC.DrawRectangle(bkgndRect);

   wxRect border;
   border.x = 0;
   border.y = 0;
   border.width = mWidth;
   border.height = mHeight;

   memDC.SetBrush(*wxWHITE_BRUSH);
   memDC.SetPen(*wxBLACK_PEN);
   memDC.DrawRectangle(border);

   mEnvRect = border;
   mEnvRect.Deflate(2, 2);

   // Pure blue x-axis line
   memDC.SetPen(wxPen(theTheme.Colour( clrGraphLines ), 1, wxSOLID));
   int center = (int) (mEnvRect.height * dBMax/(dBMax-dBMin) + .5);
   AColor::Line(memDC,
                mEnvRect.GetLeft(), mEnvRect.y + center,
                mEnvRect.GetRight(), mEnvRect.y + center);

   // Med-blue envelope line
   memDC.SetPen(wxPen(theTheme.Colour( clrGraphLines ), 3, wxSOLID));

   // Draw envelope
   double *values = new double[mEnvRect.width];
   mEnvelope->GetValues(values, mEnvRect.width, 0.0, 1.0/mEnvRect.width);
   int x, y, xlast = 0, ylast = 0;
   bool off = false, off1 = false;
   for(int i=0; i<mEnvRect.width; i++)
   {
      x = mEnvRect.x + i;
      y = lrint(mEnvRect.height*((dBMax-values[i])/(dBMax-dBMin)) + .25 ); //needs more optimising, along with'what you get'?
      if( y >= mEnvRect.height)
      {
         y = mEnvRect.height - 1;
         off = true;
      }
      else
      {
         off = false;
         off1 = false;
      }
      if ( (i != 0) & (!off1) )
      {
         AColor::Line(memDC, xlast, ylast,
                      x, mEnvRect.y + y);
      }
      off1 = off;
      xlast = x;
      ylast = mEnvRect.y + y;
   }
   delete[] values;

   //Now draw the actual response that you will get.
   //mFilterFunc has a linear scale, window has a log one so we have to fiddle about
   memDC.SetPen(wxPen(theTheme.Colour( clrResponseLines ), 1, wxSOLID));
   double scale = (double)mEnvRect.height/(dBMax-dBMin);   //pixels per dB
   double yF;   //gain at this freq
   double delta = mHiFreq/(((double)mWindowSize/2.));   //size of each freq bin

   bool lin = mParent->mFaderOrDraw[0]->GetValue() & mParent->mLinFreq->IsChecked();   // log or lin scale?

   double loLog = log10(mLoFreq);
   double step = lin ? mHiFreq : (log10(mHiFreq) - loLog);
   step /= ((double)mEnvRect.width-1.);
   double freq;   //actual freq corresponding to x position
   int halfM = (M-1)/2;
   int n;   //index to mFreqFunc
   for(int i=0; i<mEnvRect.width; i++)
   {
      x = mEnvRect.x + i;
      freq = lin ? step*i : pow(10., loLog + i*step);   //Hz
      if( ( lin ? step : (pow(10., loLog + (i+1)*step)-freq) ) < delta)
      {   //not enough resolution in FFT
         freq = M_PI*freq/mHiFreq;   //radians, normalized
         yF = 0.;
         for(int j=0;j<halfM;j++)
         {
            yF += 2. * mOutr[j] * cos(freq*(halfM-j));
         }
         yF += mOutr[halfM];
         yF = fabs(yF);
         if(yF!=0.)
            yF = 20.0*log10(yF);   //20 here as an amplitude
         else
            yF = dBMin;
      }
      else
      {   //use FFT, it has enough resolution
         n = (int)(freq/delta + .5);
         if(pow(mFilterFuncR[n],2)+pow(mFilterFuncI[n],2)!=0.)
            yF = 10.0*log10(pow(mFilterFuncR[n],2)+pow(mFilterFuncI[n],2));   //10 here, a power
         else
            yF = dBMin;
      }
      if(yF < dBMin)
         yF = dBMin;
      yF = center-scale*yF;
      if(yF>mEnvRect.height)
         yF = mEnvRect.height - 1;
      if(yF<0.)
         yF=0.;
      y = (int)(yF+.5);

      if (i != 0)
      {
         AColor::Line(memDC, xlast, ylast, x, mEnvRect.y + y);
      }
      xlast = x;
      ylast = mEnvRect.y + y;
   }

   memDC.SetPen(*wxBLACK_PEN);
   if( mParent->mFaderOrDraw[0]->GetValue() )
      mEnvelope->DrawPoints(memDC, mEnvRect, 0.0, mEnvRect.width, false, dBMin, dBMax);

   if( mParent->drawGrid )
   {
      mParent->freqRuler->ruler.DrawGrid(memDC, mEnvRect.height+2, true, true, 0, 1);
      mParent->dBRuler->ruler.DrawGrid(memDC, mEnvRect.width+2, true, true, 1, 2);
   }

   dc.Blit(0, 0, mWidth, mHeight,
           &memDC, 0, 0, wxCOPY, FALSE);
}

void EqualizationPanel::OnMouseEvent(wxMouseEvent & event)
{
   if (event.ButtonDown() && !HasCapture())
   {
      CaptureMouse();
   }

   if (mEnvelope->MouseEvent(event, mEnvRect, 0.0, mEnvRect.width, false,
            dBMin, dBMax, dBMin, dBMax))
   {
      mParent->EnvelopeUpdated();
      RecalcRequired = true;
      Refresh(false);
   }

   if (event.ButtonUp() && HasCapture())
   {
      ReleaseMouse();
   }
}

void EqualizationPanel::OnCaptureLost(wxMouseCaptureLostEvent & event)
{
   if (HasCapture())
   {
      ReleaseMouse();
   }
}


// WDR: class implementations

//----------------------------------------------------------------------------
// EqualizationDialog
//----------------------------------------------------------------------------

// WDR: event table for EqualizationDialog

BEGIN_EVENT_TABLE(EqualizationDialog,wxDialog)
   EVT_SIZE( EqualizationDialog::OnSize )
   EVT_PAINT( EqualizationDialog::OnPaint )
   EVT_ERASE_BACKGROUND( EqualizationDialog::OnErase )

   EVT_SLIDER( ID_LENGTH, EqualizationDialog::OnSliderM )
   EVT_SLIDER( ID_DBMAX, EqualizationDialog::OnSliderDBMAX )
   EVT_SLIDER( ID_DBMIN, EqualizationDialog::OnSliderDBMIN )
   EVT_COMMAND_RANGE( ID_SLIDER,
                      ID_SLIDER + NUMBER_OF_BANDS - 1,
                      wxEVT_COMMAND_SLIDER_UPDATED,
                      EqualizationDialog::OnSlider)
   EVT_CHOICE( ID_INTERP, EqualizationDialog::OnInterp )

   EVT_CHOICE( ID_CURVE, EqualizationDialog::OnCurve )
   EVT_BUTTON( ID_SAVEAS, EqualizationDialog::OnSaveAs )
   EVT_BUTTON( ID_DELETE, EqualizationDialog::OnDelete )
   EVT_BUTTON( ID_CLEAR, EqualizationDialog::OnClear )
   EVT_BUTTON( ID_INVERT, EqualizationDialog::OnInvert )

   EVT_BUTTON( ID_EFFECT_PREVIEW, EqualizationDialog::OnPreview )
   EVT_BUTTON( wxID_OK, EqualizationDialog::OnOk )
   EVT_BUTTON( wxID_CANCEL, EqualizationDialog::OnCancel )
   EVT_RADIOBUTTON(drawRadioID, EqualizationDialog::OnDrawRadio)
   EVT_RADIOBUTTON(sliderRadioID, EqualizationDialog::OnSliderRadio)
   EVT_CHECKBOX(ID_LIN_FREQ, EqualizationDialog::OnLinFreq)
   EVT_CHECKBOX(GridOnOffID, EqualizationDialog::OnGridOnOff)
END_EVENT_TABLE()

EqualizationDialog::EqualizationDialog(EffectEqualization * effect,
                     double loFreq, double hiFreq,
                     float *filterFuncR,
                     float *filterFuncI,
                     long windowSize,
                     wxString curveName,
                     bool disallowCustom,
                     wxWindow *parent, wxWindowID id,
                     const wxString &title,
                     const wxPoint &position,
                     const wxSize& size,
                     long style):
   wxDialog( parent, id, title, position, size, style | wxRESIZE_BORDER | wxMAXIMIZE_BOX ),
   mDisallowCustom(disallowCustom),
   mCustomBackup(wxT("custom"))
{
   m_pEffect = effect;

   M = 4001;
   linCheck = false;
   dBMin = -30.;
   dBMax = 30;

#if wxUSE_TOOLTIPS
   wxToolTip::Enable(true);
#endif

   mLogEnvelope = new Envelope();
   mLogEnvelope->SetInterpolateDB(false);
   mLogEnvelope->Mirror(false);

   mLinEnvelope = new Envelope();
   mLinEnvelope->SetInterpolateDB(false);
   mLinEnvelope->Mirror(false);

   mLoFreq = loFreq;
   mHiFreq = hiFreq;

   mFilterFuncR = filterFuncR;
   mFilterFuncI = filterFuncI;
   mWindowSize = windowSize;

   mCurve = NULL;
   mDirty = false;

   // Load the EQ curves
   LoadCurves();
   if (mDisallowCustom)
   {
      mCustomBackup.Name = wxT("custom");
      EQCurve &realCustom = mCurves[mCurves.GetCount()-1];
      wxASSERT(realCustom.Name.IsSameAs(wxT("custom")));
      mCustomBackup.points = realCustom.points;
   }

   // Create the dialog
   MakeEqualizationDialog();

   // Note: initial curve is set in TransferDataToWindow

   bandsInUse = NUMBER_OF_BANDS;
   //double loLog = log10(mLoFreq);
   //double stepLog = (log10(mHiFreq) - loLog)/((double)NUM_PTS-1.);
   for(int i=0; i<NUM_PTS-1; i++)
      whens[i] = (double)i/(NUM_PTS-1.);
   whens[NUM_PTS-1] = 1.;
   whenSliders[NUMBER_OF_BANDS] = 1.;
   m_EQVals[NUMBER_OF_BANDS] = 0.;
}

EqualizationDialog::~EqualizationDialog()
{
   delete mLogEnvelope;
   delete mLinEnvelope;
}

//
// Load external curves with fallback to internal
//
void EqualizationDialog::LoadCurves()
{
   // Construct default curve filename
   //
   // LLL:  Wouldn't you know that as of WX 2.6.2, there is a conflict
   //       between wxStandardPaths and wxConfig under Linux.  The latter
   //       creates a normal file as "$HOME/.audacity", while the former
   //       expects the ".audacity" portion to be a directory.
   wxFileName fn( FileNames::DataDir(), wxT("EQCurves.xml") );

   // If it doesn't exist...
   if( !fn.FileExists() )
   {
      // Fallback to internal
      LoadDefaultCurves();
      return;
   }

   // Start from scratch
   mCurves.Clear();

   // Load the curves
   XMLFileReader reader;
   if( !reader.Parse( this, fn.GetFullPath() ) )
   {
      // Inform user of load failure
      wxMessageBox( reader.GetErrorStr(),
                    _("Error loading EQ curve"),
                    wxOK | wxCENTRE,
                    this );

      // Fallback to internal
      LoadDefaultCurves();
   }

   return;
}

//
// Load the internal curves
//
void EqualizationDialog::LoadDefaultCurves()
{
   int curve;
   int point;

   // Start from scratch
   mCurves.Clear();

   // For all internally defined curves...
   for( curve = 0; curve < EffectEqualization::nCurveTypes; curve++ )
   {
      // Create and name a new curve class
      mCurves.Add( wxString( EffectEqualization::curveNames[ curve ] ) );

      // For all points within this curve...
      for( point = 0; point < EffectEqualization::nCurvePoints; point++ )
      {
         // Extract the frequency and dB values
         double freq = EffectEqualization::curvex[ point ];
         double db   = EffectEqualization::curvey[ curve ][ point ];

         // Add it to the list of points within the curve
         mCurves[ curve ].points.Add( EQPoint( freq, db ) );
      }
   }
   mCurves.Add( _("custom") );

   SaveCurves();

   return;
}

//
// Save curves to external file
//
void EqualizationDialog::SaveCurves()
{
   if (mDisallowCustom)
      RevertCustom();

   // Construct default curve filename
   //
   // LLL:  Wouldn't you know that as of WX 2.6.2, there is a conflict
   //       between wxStandardPaths and wxConfig under Linux.  The latter
   //       creates a normal file as "$HOME/.audacity", while the former
   //       expects the ".audacity" portion to be a directory.
   wxFileName fn( FileNames::DataDir(), wxT("EQCurves.xml") );

   // If the directory doesn't exist...
   if( !fn.DirExists() )
   {
      // Attempt to create it
      if( !fn.Mkdir( fn.GetPath(), 511, wxPATH_MKDIR_FULL ) )
      {
         // MkDir() will emit message
         return;
      }
   }

   // Create/Open the file
   XMLFileWriter eqFile;

   try
   {
      eqFile.Open( fn.GetFullPath(), wxT("wb") );

      // Write the curves
      WriteXML( eqFile );

      // Close the file
      eqFile.Close();
   }
   catch (XMLFileWriterException* pException)
   {
      wxMessageBox(wxString::Format(
         _("Couldn't write to file \"%s\": %s"),
         fn.GetFullPath().c_str(), pException->GetMessage().c_str()),
         _("Error saving equalization curves"), wxICON_ERROR, this);

      delete pException;
   }
}

//
// Create the Equalization dialog
//
void EqualizationDialog::MakeEqualizationDialog()
{
   wxStaticText *txt;
   wxButton *btn;

   // Create the base sizer
   szrV = new wxBoxSizer( wxVERTICAL );

   // -------------------------------------------------------------------
   // ROW 1: Banner
   // -------------------------------------------------------------------
   txt = new wxStaticText(this, wxID_ANY,
         /* i18n-hint: && in here is an escape character to get a single & 
          * on screen, so keep it as is */
                          _("Equalization, by Martyn Shaw && Mitch Golden"));
   szrV->Add( txt, 0, wxALIGN_CENTRE|wxALL, 4 );

   // -------------------------------------------------------------------
   // ROW 2: EQ panel and sliders for vertical scale
   // -------------------------------------------------------------------
   szr1 = new wxFlexGridSizer( 4, 0, 0 );
   szr1->AddGrowableCol( 2, 1 );
   szr1->AddGrowableRow( 0, 1 );
   szr1->SetFlexibleDirection( wxBOTH );

   szr2 = new wxBoxSizer( wxVERTICAL );
   dBMaxSlider = new wxSliderBugfix(this, ID_DBMAX, 30, 0, 60,
                           wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL|wxSL_INVERSE);
   szr2->Add( dBMaxSlider, 1, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL, 4 );
   dBMinSlider = new wxSliderBugfix(this, ID_DBMIN, -30, -120, -10,
                           wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL|wxSL_INVERSE);
   szr2->Add( dBMinSlider, 1, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL, 4 );
   szr1->Add( szr2, 0, wxEXPAND|wxALIGN_CENTRE|wxALL, 4 );

#if wxUSE_ACCESSIBILITY
   dBMaxSlider->SetName(_("Max dB"));
   dBMaxSlider->SetAccessible(new SliderAx(dBMaxSlider, wxString(wxT("%d ")) + _("dB")));
   dBMinSlider->SetName(_("Min dB"));
   dBMinSlider->SetAccessible(new SliderAx(dBMinSlider, wxString(wxT("%d ")) + _("dB")));
#endif

   dBRuler = new RulerPanel(this, wxID_ANY);
   dBRuler->ruler.SetBounds(0, 0, 100, 100); // Ruler can't handle small sizes
   dBRuler->ruler.SetOrientation(wxVERTICAL);
   dBRuler->ruler.SetRange(60.0, -120.0);
   dBRuler->ruler.SetFormat(Ruler::LinearDBFormat);
   dBRuler->ruler.SetUnits(_("dB"));
   dBRuler->ruler.SetLabelEdges(true);
   int w, h;
   dBRuler->ruler.GetMaxSize(&w, NULL);
   dBRuler->SetSize(wxSize(w, 150));  // height needed for wxGTK

   szr4 = new wxBoxSizer( wxVERTICAL );
   szr4->AddSpacer(2); // vertical space for panel border and thickness of line
   szr4->Add( dBRuler, 1, wxEXPAND|wxALIGN_LEFT|wxALL );
   szr4->AddSpacer(1); // vertical space for thickness of line
   szr1->Add( szr4, 0, wxEXPAND|wxALIGN_LEFT|wxALL );

   mPanel = new EqualizationPanel( mLoFreq, mHiFreq,
                                   mLogEnvelope,
                                   this,
                                   mFilterFuncR, mFilterFuncI, mWindowSize,
                                   ID_FILTERPANEL);
   szr1->Add( mPanel, 1, wxEXPAND|wxALIGN_CENTRE|wxRIGHT, 4);
   szr3 = new wxBoxSizer( wxVERTICAL );
   szr1->Add( szr3, 0, wxALIGN_CENTRE|wxRIGHT, 0);   //spacer for last EQ

   /// Next row of wxFlexGridSizer
   szr1->Add(1, 1); // horizontal spacer
   szr1->Add(1, 1); // horizontal spacer

   freqRuler  = new RulerPanel(this, wxID_ANY);
   freqRuler->ruler.SetBounds(0, 0, 100, 100); // Ruler can't handle small sizes
   freqRuler->ruler.SetOrientation(wxHORIZONTAL);
   freqRuler->ruler.SetLog(true);
   freqRuler->ruler.SetRange(mLoFreq, mHiFreq);
   freqRuler->ruler.SetFormat(Ruler::IntFormat);
   freqRuler->ruler.SetUnits(_("Hz"));
   freqRuler->ruler.SetFlip(true);
   freqRuler->ruler.SetLabelEdges(true);
   freqRuler->ruler.GetMaxSize(NULL, &h);
   freqRuler->SetMinSize(wxSize(-1, h));
   szr1->Add( freqRuler, 0, wxEXPAND|wxALIGN_LEFT|wxRIGHT, 4 );

   szrV->Add( szr1, 1, wxEXPAND|wxALIGN_CENTER|wxALL, 0 );

   // -------------------------------------------------------------------
   // ROW 3: Graphic EQ - this gets laid out horizontally in onSize
   // -------------------------------------------------------------------

   szrG = new wxBoxSizer( wxHORIZONTAL  );
   szrG->Add(0, 0, 0); // horizontal spacer, will be used to position LH EQ slider
   for (int i = 0; thirdOct[i] <= mHiFreq; ++i)
   {
      if( i == NUMBER_OF_BANDS )
         break;
      m_sliders[i] = new wxSliderBugfix(this, ID_SLIDER + i, 0, -20, +20,
                        wxDefaultPosition, wxSize(20, 124), wxSL_VERTICAL|
                         wxSL_INVERSE);
      szrG->Add( m_sliders[i], 0, wxEXPAND|wxALIGN_CENTER );
      szrG->Add(0, 0, 0); // horizontal spacer - used to put EQ sliders in correct position
      m_sliders[i]->Connect(wxEVT_ERASE_BACKGROUND,wxEraseEventHandler(EqualizationDialog::OnErase));
      m_sliders_old[i] = 0;
      m_EQVals[i] = 0.;

#if wxUSE_ACCESSIBILITY
      wxString name;
      if( thirdOct[i] < 1000.)
         name.Printf(wxString(wxT("%d ")) + _("Hz"), (int)thirdOct[i]);
      else
         name.Printf(wxString(wxT("%g ")) + _("kHz"), thirdOct[i]/1000.);
      m_sliders[i]->SetName(name);
      m_sliders[i]->SetAccessible(new SliderAx(m_sliders[i], wxString(wxT("%d ")) + _("dB")));
#endif
   }
   szrV->Add( szrG, 0, wxEXPAND|wxALIGN_LEFT|wxALL, 0 );

   wxSizerItem *EQslider = szrG->GetItem((size_t)1);
   wxSize EQsliderSize = EQslider->GetSize();   //size of the sliders
   szr3->SetMinSize(EQsliderSize.x/2, -1);   //extra gap for last slider

   // -------------------------------------------------------------------
   // ROW 4a: Graphic or curve drawing?
   // -------------------------------------------------------------------
   szrH = new wxBoxSizer( wxHORIZONTAL );

   mFaderOrDraw[0] = new wxRadioButton(
         this, drawRadioID, _("Draw curves"),
         wxDefaultPosition, wxDefaultSize, wxRB_GROUP );
   szrH->Add( mFaderOrDraw[0], 0, wxRIGHT, 10 );

   mFaderOrDraw[1] = new wxRadioButton(
         this, sliderRadioID, _("Graphic EQ"),
         wxDefaultPosition, wxDefaultSize, 0 );
   szrH->Add( mFaderOrDraw[1], 0, wxRIGHT, 4 );

   mInterpChoice = new wxChoice(this, ID_INTERP,
                             wxDefaultPosition, wxDefaultSize,
                             NUM_INTERP_CHOICES, interpChoiceStrings);

   mInterpChoice->SetSelection(0);
   szrI = new wxBoxSizer( wxHORIZONTAL );
   szrI->Add( mInterpChoice, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 40 );
   szrH->Add( szrI );

   szrL = new wxBoxSizer( wxHORIZONTAL );
   mLinFreq = new wxCheckBox(this, ID_LIN_FREQ, _("Linear frequency scale"));
   szrL->Add( mLinFreq, 0 );
   szrH->Add(szrL);  // either szrI or szrL are visible, not both.

   // -------------------------------------------------------------------
   // ROW 4b: Filter length grouping
   // -------------------------------------------------------------------

   // length of filter (M) label
   txt = new wxStaticText(this, wxID_ANY, _("Length of filter:"));
   szrH->Add( txt, 0 );

   // length of filter (M) slider
   MSlider = new wxSliderBugfix(this, ID_LENGTH, (M -1)/2, 10, 4095,
                           wxDefaultPosition, wxSize(200, -1), wxSL_HORIZONTAL);
   szrH->Add( MSlider, 0, wxEXPAND );

   wxString label;
   label.Printf( wxT("%d"), M );
   mMText = new wxStaticText(this, wxID_ANY, label);
   szrH->Add( mMText, 0 );

   // Add the length / graphic / draw grouping
   szrV->Add( szrH, 0, wxALIGN_CENTER | wxALL, 4 );

   // -------------------------------------------------------------------
   // ROW 5: Curve management grouping
   // -------------------------------------------------------------------
   szrC = new wxBoxSizer( wxHORIZONTAL );   //szrC is for the curves bits

   txt = new wxStaticText( this, wxID_ANY, _("Select curve:") );
   szrC->Add( txt, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxLEFT, 4 );

   // Create the choice sizer (helps in recreating choice control)
   mCurveSizer = new wxBoxSizer( wxHORIZONTAL );
   szrC->Add( mCurveSizer, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxLEFT, 4 );

   // Create the choice control
   CreateChoice();

   mSaveAs = new wxButton( this, ID_SAVEAS, _("Save As...") );
   szrC->Add( mSaveAs, 0, wxALIGN_CENTRE|wxLEFT, 4 );

   mDelete = new wxButton( this, ID_DELETE, _("Delete") );
   szrC->Add( mDelete, 0, wxALIGN_CENTRE|wxLEFT, 4 );

   btn = new wxButton( this, ID_CLEAR, _("Flatten"));
   szrC->Add( btn, 0, wxALIGN_CENTRE | wxALL, 4 );
   btn = new wxButton( this, ID_INVERT, _("Invert"));
   szrC->Add( btn, 0, wxALIGN_CENTRE | wxALL, 4 );
   mGridOnOff = new wxCheckBox(this, GridOnOffID, _("Grids"),
                            wxDefaultPosition, wxDefaultSize,
                            wxALIGN_RIGHT);
   mGridOnOff->SetName(_("Grids"));
   szrC->Add( mGridOnOff, 0, wxALIGN_CENTRE | wxALL, 4 );

   szrV->Add( szrC, 0, wxALIGN_CENTER | wxALL, 0 );

   // -------------------------------------------------------------------
   // ROW 6: Preview, OK, & Cancel buttons
   // -------------------------------------------------------------------
   szrV->Add(CreateStdButtonSizer(this, ePreviewButton|eCancelButton|eOkButton), 0, wxEXPAND);

   // -------------------------------------------------------------------
   // Display now
   // -------------------------------------------------------------------
   SetAutoLayout(false);

   szrV->Show(szrG,true);
   szrH->Show(szrI,true);
   szrH->Show(szrL,false);

   SetSizerAndFit( szrV );
   SetSizeHints(GetSize());
   szrL->SetMinSize( szrI->GetSize() );

   szrV->Show(szrC,true);
   szrV->Show(szrG,false);
   szrH->Show(szrI,false);
   szrH->Show(szrL,true);

   return;
}

//
// (Re)Create the choice control
//
void EqualizationDialog::CreateChoice()
{
   wxChoice *choice;
   int i;
   int numCurves = mCurves.GetCount();
   wxString *curveNames = new wxString[ numCurves ];

   // Create an array of names
   for( i = 0; i < numCurves; i++ )
   {
     curveNames[ i ] = mCurves[ i ].Name;
   }

   // Create the control
   choice = new wxChoice( this, ID_CURVE,
                          wxDefaultPosition, wxDefaultSize,
                          numCurves, curveNames );

   // Have an existing control?
   if( mCurve )
   {
      // Then detach it from its sizer and delete it
      mCurveSizer->Detach( mCurve );
      delete mCurve;
   }

   // Save control ptr and add to its sizer
   mCurve = choice;
   mCurveSizer->Add( mCurve, 0 );

   // Delete the array of names
   delete [] curveNames;
}

//
// Validate data
//
bool EqualizationDialog::Validate()
{
   // If editing a batch chain, we don't want to be using the custom curve so
   // we offer to save it.
   while (mDisallowCustom && curveName.IsSameAs(wxT("custom")))
   {
      wxMessageBox(_("To use this EQ curve in a batch chain, please choose a new name for it."),
            _("EQ Curve needs a different name"),
            wxOK | wxCENTRE,
            this);

      if (!SaveAs())
         return false;
   }
   return true;
}

//
// Populate the window with relevant variables
//
bool EqualizationDialog::TransferDataToWindow()
{
   // Set log or lin freq scale (affects interpolation as well)
   mLinFreq->SetValue( linCheck );
   wxCommandEvent dummyEvent;
   OnLinFreq(dummyEvent);  // causes a CalcFilter

   mGridOnOff->SetValue( drawGrid ); // checks/unchecks the box on the interface

   MSlider->SetValue((M-1)/2);
   M = 0;                        // force refresh in TransferDataFromWindow()

   dBMinSlider->SetValue((int)dBMin);
   dBMin = 0;                     // force refresh in TransferDataFromWindow()

   dBMaxSlider->SetValue((int)dBMax);
   dBMax = 0;                    // force refresh in TransferDataFromWindow()

   // Set initial curve
   setCurve( curveName );

   // Set graphic interpolation mode
   mInterpChoice->SetSelection(interp);

   // Set Graphic (Fader) or Draw mode
   if(drawMode)
   {
      mFaderOrDraw[0]->SetValue(true);
   }
   else
   {
      mFaderOrDraw[1]->SetValue(true);
      UpdateGraphic();
   }

   return TransferDataFromWindow();
}

//
// Retrieve data from the window
//
bool EqualizationDialog::TransferDataFromWindow()
{   // Read the sliders and send to the panel
   wxString tip;

   bool rr = false;
   int dB = dBMinSlider->GetValue();
   if (dB != dBMin) {
      rr = true;
      dBMin = dB;
      mPanel->dBMin = dBMin;
#if wxUSE_TOOLTIPS
      tip.Printf(wxString(wxT("%d ")) + _("dB"),(int)dBMin);
      dBMinSlider->SetToolTip(tip);
#endif
   }

   dB = dBMaxSlider->GetValue();
   if (dB != dBMax) {
      rr = true;
      dBMax = dB;
      mPanel->dBMax = dBMax;
#if wxUSE_TOOLTIPS
      tip.Printf(wxString(wxT("%d ")) + _("dB"),(int)dBMax);
      dBMaxSlider->SetToolTip(tip);
#endif
   }

   // Refresh ruler if values have changed
   if (rr) {
      int w1, w2, h;
      dBRuler->ruler.GetMaxSize(&w1, &h);
      dBRuler->ruler.SetRange(dBMax, dBMin);
      dBRuler->ruler.GetMaxSize(&w2, &h);
      if( w1 != w2 )   // Reduces flicker
      {
         dBRuler->SetSize(wxSize(w2,h));
         szr1->Layout();
         LayoutEQSliders();
         szrG->Layout();
         freqRuler->Refresh(false);
      }
      dBRuler->Refresh(false);
   }

   int m = 2 * MSlider->GetValue() + 1;   // odd numbers only
   if (m != M) {
      M = m;
      mPanel->M = M;
      mMText->SetLabel(wxString::Format(wxT("%d"), M));
#if wxUSE_TOOLTIPS
      tip.Printf(wxT("%d"), M);
      MSlider->SetToolTip(tip);
#endif
   }

   mPanel->Refresh(false);

   return true;
}

bool EqualizationDialog::CalcFilter()
{
   double loLog = log10(mLoFreq);
   double hiLog = log10(mHiFreq);
   double denom = hiLog - loLog;

   double delta = mHiFreq / ((double)(mWindowSize/2.));
   double val0;
   double val1;

   bool lin = mFaderOrDraw[0]->GetValue() & mLinFreq->IsChecked();

   if( lin )
   {
      val0 = mLinEnvelope->GetValue(0.0);   //no scaling required - saved as dB
      val1 = mLinEnvelope->GetValue(1.0);
   }
   else
   {
      val0 = mLogEnvelope->GetValue(0.0);   //no scaling required - saved as dB
      val1 = mLogEnvelope->GetValue(1.0);
   }
   mFilterFuncR[0] = val0;
   double freq = delta;

   int i;
   for(i=1; i<=mWindowSize/2; i++)
   {
      double when;
      if( lin )
         when = freq/mHiFreq;
      else
         when = (log10(freq) - loLog)/denom;
      if(when < 0.)
      {
         mFilterFuncR[i] = val0;
      }
      else  if(when > 1.0)
            {
               mFilterFuncR[i] = val1;
            }
            else
            {
               if( lin )
                  mFilterFuncR[i] = mLinEnvelope->GetValue(when);
               else
                  mFilterFuncR[i] = mLogEnvelope->GetValue(when);
            }
      freq += delta;
   }
   mFilterFuncR[mWindowSize/2] = val1;

   mFilterFuncR[0] = (float)(pow(10., mFilterFuncR[0]/20.));
   for(i=1;i<mWindowSize/2;i++)
   {
      mFilterFuncR[i] = (float)(pow(10., mFilterFuncR[i]/20.));
      mFilterFuncR[mWindowSize-i]=mFilterFuncR[i];   //Fill entire array
   }
   mFilterFuncR[i] = (float)(pow(10., mFilterFuncR[i]/20.));   //do last one

   //transfer to time domain to do the padding and windowing
   float *outr = new float[mWindowSize];
   float *outi = new float[mWindowSize];
#ifdef EXPERIMENTAL_USE_REALFFTF
   InverseRealFFT(mWindowSize, mFilterFuncR, NULL, outr); // To time domain
#else
   FFT(mWindowSize,true,mFilterFuncR,NULL,outr,outi);   //To time domain
#endif

   for(i=0;i<=(M-1)/2;i++)
   {   //Windowing - could give a choice, fixed for now - MJS
//      double mult=0.54-0.46*cos(2*M_PI*(i+(M-1)/2.0)/(M-1));   //Hamming
//Blackman
      double mult=0.42-0.5*cos(2*M_PI*(i+(M-1)/2.0)/(M-1))+.08*cos(4*M_PI*(i+(M-1)/2.0)/(M-1));
      outr[i]*=mult;
      if(i!=0){
         outr[mWindowSize-i]*=mult;
      }
   }
   for(;i<=mWindowSize/2;i++)
   {   //Padding
      outr[i]=0;
      outr[mWindowSize-i]=0;
   }
   float *tempr = new float[M];
   for(i=0;i<(M-1)/2;i++)
   {   //shift so that padding on right
      tempr[(M-1)/2+i]=outr[i];
      tempr[i]=outr[mWindowSize-(M-1)/2+i];
   }
   tempr[(M-1)/2+i]=outr[i];

   for(i=0;i<M;i++)
   {   //and copy useful values back
      outr[i]=tempr[i];
   }
   for(i=M;i<mWindowSize;i++)
   {   //rest is padding
      outr[i]=0.;
   }

   //Back to the frequency domain so we can use it
   RealFFT(mWindowSize,outr,mFilterFuncR,mFilterFuncI);

   delete[] outr;
   delete[] outi;
   delete[] tempr;

   return TRUE;
}

//
// Make the passed curve index the active one
//
void EqualizationDialog::setCurve(int currentCurve)
{
   // Set current choice
   Select( currentCurve );

   wxASSERT( currentCurve < (int) mCurves.GetCount() );
   bool changed = false;

   if( mLinFreq->IsChecked() )   // linear freq mode?
   {
      Envelope *env = mLinEnvelope;
      env->Flatten(0.);
      env->SetTrackLen(1.0);

      if( mCurves[currentCurve].points.GetCount() )
      {
         double when, value;
         int i;
         int nCurvePoints = mCurves[currentCurve].points.GetCount();
         for(i=0;i<nCurvePoints;i++)
         {
            when = mCurves[currentCurve].points[i].Freq / mHiFreq;
            value = mCurves[currentCurve].points[i].dB;
            if(when <= 1)
               env->Insert(when, value);
            else
               break;
         }
         if ( i != nCurvePoints) // there are more points at higher freqs
         {
            when = 1.;  // set the RH end to the next highest point
            value = mCurves[currentCurve].points[nCurvePoints-1].dB;
            env->Insert(when, value);
            changed = true;
         }
      }
   }
   else
   {
      Envelope *env = mLogEnvelope;
      env->Flatten(0.);
      env->SetTrackLen(1.0);

      if( mCurves[currentCurve].points.GetCount() )
      {
         double when, value;
         double loLog = log10(20.);
         double hiLog = log10(mHiFreq);
         double denom = hiLog - loLog;
         int i;
         int nCurvePoints = mCurves[currentCurve].points.GetCount();

         for(i=0;i<nCurvePoints;i++)
         {
            if( mCurves[currentCurve].points[i].Freq >= 20)
            {
               when = (log10(mCurves[currentCurve].points[i].Freq) - loLog)/denom;
               value = mCurves[currentCurve].points[i].dB;
               if(when <= 1)
                  env->Insert(when, value);
               else
                  break;
            }
            else
            {  //get the first point as close as we can to the last point requested
               changed = true;
               //double f = mCurves[currentCurve].points[i].Freq;
               //double v = mCurves[currentCurve].points[i].dB;
               mLogEnvelope->Insert(0., mCurves[currentCurve].points[i].dB);
            }
         }
         if ( i != nCurvePoints)
         {
            when = 1.;
            value = mCurves[currentCurve].points[nCurvePoints-1].dB;
            env->Insert(when, value);
            changed = true;
         }
      }
   }
   if(changed) // not all points were loaded so switch to custom
      EnvelopeUpdated();
   mPanel->RecalcRequired = true;
   mPanel->Refresh( false );
}
void EqualizationDialog::setCurve()
{
   setCurve((int) mCurves.GetCount()-1);
}

void EqualizationDialog::setCurve(wxString curveName)
{
   unsigned i = 0;
   for( i = 0; i < mCurves.GetCount(); i++ )
      if( curveName == mCurves[ i ].Name )
         break;
   if( i == mCurves.GetCount())
   {
      wxMessageBox( wxT("Requested curve not found, using 'custom'"), wxT("Curve not found"), wxOK|wxICON_ERROR );
      setCurve((int) mCurves.GetCount()-1);
   }
   else
      setCurve( i );
}

//
// Set new curve selection and manage state of delete button
//
void EqualizationDialog::Select( int curve )
{
   // Set current choice
   mCurve->SetSelection( curve );
   curveName = mCurves[ curve ].Name;

   // If the "custom" curve became active
   if( (unsigned int)curve + 1U == mCurve->GetCount() )
   {
      // Prevent focus from being lost
      if( mDelete->FindFocus() == mDelete )
      {
         mCurve->SetFocus();
      }

      // Not allowed to delete the "custom" curve
      mDelete->Disable();
   }
   else
   {
      // Allow the curve to be deleted
      mDelete->Enable();
   }
}

//
// Capture updated envelope
//
void EqualizationDialog::EnvelopeUpdated()
{
   if( mFaderOrDraw[0]->GetValue() & mLinFreq->IsChecked() )
      EnvelopeUpdated(mLinEnvelope, true);
   else
      EnvelopeUpdated(mLogEnvelope, false);
}

void EqualizationDialog::EnvelopeUpdated(Envelope *env, bool lin)
{
   // Allocate and populate point arrays
   int numPoints = env->GetNumberOfPoints();
   double *when = new double[ numPoints ];
   double *value = new double[ numPoints ];
   env->GetPoints( when, value, numPoints );

   // Clear the custom curve
   int curve = mCurves.GetCount()-1;
   mCurves[ curve ].points.Clear();

   if(lin)
   {
      // Copy and convert points
      int point;
      for( point = 0; point < numPoints; point++ )
      {
         double freq = when[ point ] * mHiFreq;
         double db = value[ point ];

         // Add it to the curve
         mCurves[ curve ].points.Add( EQPoint( freq, db ) );
      }
   }
   else
   {
      double loLog = log10( 20. );
      double hiLog = log10( mHiFreq );
      double denom = hiLog - loLog;

      // Copy and convert points
      int point;
      for( point = 0; point < numPoints; point++ )
      {
         double freq = pow( 10., ( ( when[ point ] * denom ) + loLog ));
         double db = value[ point ];

         // Add it to the curve
         mCurves[ curve ].points.Add( EQPoint( freq, db ) );
      }
   }
   // Remember that we've updated the custom curve
   mDirty = true;

   // set 'custom' as the selected curve
   Select( (int) mCurves.GetCount()-1 );

   // Clean up
   delete [] when;
   delete [] value;
}

//
// Process XML tags and handle the ones we recognize
//
bool EqualizationDialog::HandleXMLTag(const wxChar *tag, const wxChar **attrs)
{
   // May want to add a version strings...
   if( !wxStrcmp( tag, wxT("equalizationeffect") ) )
   {
      return true;
   }

   // Located a new curve
   if( !wxStrcmp(tag, wxT("curve") ) )
   {
      // Process the attributes
      while( *attrs )
      {
         // Cache attr/value and bump to next
         const wxChar *attr = *attrs++;
         const wxChar *value = *attrs++;

         // Create a new curve and name it
         if( !wxStrcmp( attr, wxT("name") ) )
         {
            const wxString strValue = value;
            if (!XMLValueChecker::IsGoodString(strValue))
               return false;
            mCurves.Add( EQCurve( strValue ) );
         }
      }

      // Tell caller it was processed
      return true;
   }

   // Located a new point
   if( !wxStrcmp( tag, wxT("point") ) )
   {
      // Set defaults in case attributes are missing
      double f = 0.0;
      double d = 0.0;

      // Process the attributes
      double dblValue;
      while( *attrs )
      {   // Cache attr/value and bump to next
         const wxChar *attr = *attrs++;
         const wxChar *value = *attrs++;

         const wxString strValue = value;

         // Get the frequency
         if( !wxStrcmp( attr, wxT("f") ) )
         {
            if (!XMLValueChecker::IsGoodString(strValue) ||
                  !Internat::CompatibleToDouble(strValue, &dblValue))
               return false;
            f = dblValue;
         }
         // Get the dB
         else if( !wxStrcmp( attr, wxT("d") ) )
         {
            if (!XMLValueChecker::IsGoodString(strValue) ||
                  !Internat::CompatibleToDouble(strValue, &dblValue))
               return false;
            d = dblValue;
         }
      }

      // Create a new point
      mCurves[ mCurves.GetCount() - 1 ].points.Add( EQPoint( f, d ) );

      // Tell caller it was processed
      return true;
   }

   // Tell caller we didn't understand the tag
   return false;
}

//
// Return handler for recognized tags
//
XMLTagHandler *EqualizationDialog::HandleXMLChild(const wxChar *tag)
{
   if( !wxStrcmp( tag, wxT("equalizationeffect") ) )
   {
      return this;
   }

   if( !wxStrcmp( tag, wxT("curve") ) )
   {
      return this;
   }

   if( !wxStrcmp( tag, wxT("point") ) )
   {
      return this;
   }

   return NULL;
}

//
// Write all of the curves to the XML file
//
void EqualizationDialog::WriteXML(XMLWriter &xmlFile)
{
   // Start our heirarchy
   xmlFile.StartTag( wxT( "equalizationeffect" ) );

   // Write all curves
   int numCurves = mCurves.GetCount();
   int curve;
   for( curve = 0; curve < numCurves; curve++ )
   {
      // Start a new curve
      xmlFile.StartTag( wxT( "curve" ) );
      xmlFile.WriteAttr( wxT( "name" ), mCurves[ curve ].Name );

      // Write all points
      int numPoints = mCurves[ curve ].points.GetCount();
      int point;
      for( point = 0; point < numPoints; point++ )
      {
         // Write new point
         xmlFile.StartTag( wxT( "point" ) );
         xmlFile.WriteAttr( wxT( "f" ), mCurves[ curve ].points[ point ].Freq, 12 );
         xmlFile.WriteAttr( wxT( "d" ), mCurves[ curve ].points[ point ].dB, 12 );
         xmlFile.EndTag( wxT( "point" ) );
      }

      // Terminate curve
      xmlFile.EndTag( wxT( "curve" ) );
   }

   // Terminate our heirarchy
   xmlFile.EndTag( wxT( "equalizationeffect" ) );
}

// WDR: handler implementations for EqualizationDialog

//
// Graphic EQ slider was adjusted
//

void EqualizationDialog::OnSlider(wxCommandEvent &event)
{
   wxSliderBugfix *s = (wxSliderBugfix *)event.GetEventObject();
   for (int i = 0; thirdOct[i] <= mHiFreq; ++i)
   {
      if( i == NUMBER_OF_BANDS )
         break;
      if( s == m_sliders[i])
      {
         int posn = m_sliders[i]->GetValue();
         if( wxGetKeyState(WXK_SHIFT) )
         {
            if( posn > m_sliders_old[i] )
               m_EQVals[i] += (float).1;
            else
               if( posn < m_sliders_old[i] )
                  m_EQVals[i] -= .1f;
         }
         else
            m_EQVals[i] += (posn - m_sliders_old[i]);
         if( m_EQVals[i] > 20. )
            m_EQVals[i] = 20.;
         if( m_EQVals[i] < -20. )
            m_EQVals[i] = -20.;
         int newPosn = (int)m_EQVals[i];
         m_sliders[i]->SetValue( newPosn );
         m_sliders_old[i] = newPosn;
#if wxUSE_TOOLTIPS
         wxString tip;
         if( thirdOct[i] < 1000.)
            tip.Printf( wxT("%dHz\n%.1fdB"), (int)thirdOct[i], m_EQVals[i] );
         else
            tip.Printf( wxT("%gkHz\n%.1fdB"), thirdOct[i]/1000., m_EQVals[i] );
         s->SetToolTip(tip);
#endif
         break;
      }
   }
   GraphicEQ(mLogEnvelope);
   EnvelopeUpdated();
}

void EqualizationDialog::OnInterp(wxCommandEvent &event)
{
   if(mFaderOrDraw[1]->GetValue())
   {
      GraphicEQ(mLogEnvelope);
      EnvelopeUpdated();
   }
   interp = mInterpChoice->GetSelection();
}

void EqualizationDialog::LayoutEQSliders()
{
   //layout the Graphic EQ sizer here
   wxSize szrGSize = szrG->GetSize();   //total size we have to play with
   wxSize szr2Size = szr2->GetSize();   //size of the dBMax/Min sliders
   wxSizerItem *ruler = szr1->GetItem((size_t)1);
   wxSize rulerSize = ruler->GetSize();   //and the ruler
   wxSizerItem *EQslider = szrG->GetItem((size_t)1);
   wxSize EQsliderSize = EQslider->GetSize();   //size of the sliders

#if defined(__WXMAC__)
   // LL: 2010-01-04 - Don't know why, but on the Mac, the rightmost sliders
   // will wind up off the edge of the window since they get spaced out too
   // much.  Somewhere, there's an extra 2 pixels in slider width that's not
   // being accounted for.  (I guess)
   EQsliderSize.x += 2;
#endif

   int start, w, range, so_far;
   start = szr2Size.x + rulerSize.x + 12;   //inc ruler & mPanel border (4+4 + 4)
   szrG->SetItemMinSize((size_t)0, start - EQsliderSize.x/2, -1);   //set 1st spacer so that 1st slider aligned with ruler
   range = szrGSize.x - EQsliderSize.x/2 - start;
   so_far = start + EQsliderSize.x/2;

   double loLog = log10(mLoFreq);
   double hiLog = log10(mHiFreq);
   double denom = hiLog - loLog;
   for (int i = 1; thirdOct[i] <= mHiFreq; ++i)   //go along the spacers
   {
      if( i == NUMBER_OF_BANDS )
         break;
      float posn = range*(log10(thirdOct[i])-loLog)/denom;   //centre of this slider, from start
      w = start + ((int)(posn+.5)) - EQsliderSize.x/2;   //LH edge of slider, from 0
      w = w - so_far;   //gap needed to put it here
      szrG->SetItemMinSize((size_t)(i*2), w, -1);   //set spacers so that sliders aligned with ruler
      so_far += (w + EQsliderSize.x);
   }

   RefreshRect(wxRect(szrG->GetPosition(),szrGSize));
}

void EqualizationDialog::GraphicEQ(Envelope *env)
{
   double value, dist, span, s;

   env->Flatten(0.);
   env->SetTrackLen(1.0);

   int n = mInterpChoice->GetSelection();
   switch( n )
   {
      case 0:  // B-spline
      {
         int minF = 0;
         for(int i=0; i<NUM_PTS; i++)
         {
            while( (whenSliders[minF] <= whens[i]) & (minF < bandsInUse) )
               minF++;
            minF--;
            if( minF < 0 ) //before first slider
            {
               dist = whens[i] - whenSliders[0];
               span = whenSliders[1] - whenSliders[0];
               s = dist/span;
               if( s < -1.5 )
                  value = 0.;
               else
                  if( s < -.5 )
                     value = m_EQVals[0]*(s + 1.5)*(s + 1.5)/2.;
                  else
                     value = m_EQVals[0]*(.75 - s*s) + m_EQVals[1]*(s + .5)*(s + .5)/2.;
            }
            else
            {
               if( whens[i] > whenSliders[bandsInUse-1] )   //after last fader
               {
                  dist = whens[i] - whenSliders[bandsInUse-1];
                  span = whenSliders[bandsInUse-1] - whenSliders[bandsInUse-2];
                  s = dist/span;
                  if( s > 1.5 )
                     value = 0.;
                  else
                     if( s > .5 )
                        value = m_EQVals[bandsInUse-1]*(s - 1.5)*(s - 1.5)/2.;
                     else
                        value = m_EQVals[bandsInUse-1]*(.75 - s*s) +
                                 m_EQVals[bandsInUse-2]*(s - .5)*(s - .5)/2.;
               }
               else  //normal case
               {
                  dist = whens[i] - whenSliders[minF];
                  span = whenSliders[minF+1] - whenSliders[minF];
                  s = dist/span;
                  if(s < .5 )
                  {
                     value = m_EQVals[minF]*(0.75 - s*s);
                     if( minF+1 < bandsInUse )
                        value += m_EQVals[minF+1]*(s+.5)*(s+.5)/2.;
                     if( minF-1 >= 0 )
                        value += m_EQVals[minF-1]*(s-.5)*(s-.5)/2.;
                  }
                  else
                  {
                     value = m_EQVals[minF]*(s-1.5)*(s-1.5)/2.;
                     if( minF+1 < bandsInUse )
                        value += m_EQVals[minF+1]*(.75-(1.-s)*(1.-s));
                     if( minF+2 < bandsInUse )
                        value += m_EQVals[minF+2]*(s-.5)*(s-.5)/2.;
                  }
               }
            }
            if(whens[i]<=0.)
               env->Move( 0., value );
            env->Insert( whens[i], value );
         }
         env->Move( 1., value );
         break;
      }

      case 1:  // Cosine squared
      {
         int minF = 0;
         for(int i=0; i<NUM_PTS; i++)
         {
            while( (whenSliders[minF] <= whens[i]) & (minF < bandsInUse) )
               minF++;
            minF--;
            if( minF < 0 ) //before first slider
            {
               dist = whenSliders[0] - whens[i];
               span = whenSliders[1] - whenSliders[0];
               if( dist < span )
                  value = m_EQVals[0]*(1. + cos(M_PI*dist/span))/2.;
               else
                  value = 0.;
            }
            else
            {
               if( whens[i] > whenSliders[bandsInUse-1] )   //after last fader
               {
                  span = whenSliders[bandsInUse-1] - whenSliders[bandsInUse-2];
                  dist = whens[i] - whenSliders[bandsInUse-1];
                  if( dist < span )
                     value = m_EQVals[bandsInUse-1]*(1. + cos(M_PI*dist/span))/2.;
                  else
                     value = 0.;
               }
               else  //normal case
               {
                  span = whenSliders[minF+1] - whenSliders[minF];
                  dist = whenSliders[minF+1] - whens[i];
                  value = m_EQVals[minF]*(1. + cos(M_PI*(span-dist)/span))/2. +
                  m_EQVals[minF+1]*(1. + cos(M_PI*dist/span))/2.;
               }
            }
            if(whens[i]<=0.)
               env->Move( 0., value );
            env->Insert( whens[i], value );
         }
         env->Move( 1., value );
         break;
      }

      case 2:  // Cubic Spline
      {
         double y2[NUMBER_OF_BANDS+1];
         m_EQVals[bandsInUse] = m_EQVals[bandsInUse-1];
         spline(whenSliders, m_EQVals, bandsInUse+1, y2);
         for(double xf=0; xf<1.; xf+=1./NUM_PTS)
         {
            env->Insert(xf, splint(whenSliders, m_EQVals, bandsInUse+1, y2, xf));
         }
         break;
      }
   }

   mPanel->RecalcRequired = true;
   mPanel->Refresh( false );
}

void EqualizationDialog::spline(double x[], double y[], int n, double y2[])
{
   int i;
   double p, sig, *u = new double[n];

   y2[0] = 0.;  //
   u[0] = 0.;   //'natural' boundary conditions
   for(i=1;i<n-1;i++)
   {
      sig = ( x[i] - x[i-1] ) / ( x[i+1] - x[i-1] );
      p = sig * y2[i-1] + 2.;
      y2[i] = (sig - 1.)/p;
      u[i] = ( y[i+1] - y[i] ) / ( x[i+1] - x[i] ) - ( y[i] - y[i-1] ) / ( x[i] - x[i-1] );
      u[i] = (6.*u[i]/( x[i+1] - x[i-1] ) - sig * u[i-1]) / p;
   }
   y2[n-1] = 0.;
   for(i=n-2;i>=0;i--)
      y2[i] = y2[i]*y2[i+1] + u[i];

   delete [] u;
}

double EqualizationDialog::splint(double x[], double y[], int n, double y2[], double xr)
{
   double a, b, h;
   static double xlast = 0.;   // remember last x value requested
   static int k = 0;           // and which interval we were in

   if( xr < xlast )
      k = 0;                   // gone back to start, (or somewhere to the left)
   xlast = xr;
   while( (x[k] <= xr) && (k < n-1) )
      k++;
   k--;
   h = x[k+1] - x[k];
   a = ( x[k+1] - xr )/h;
   b = (xr - x[k])/h;
   return( a*y[k]+b*y[k+1]+((a*a*a-a)*y2[k]+(b*b*b-b)*y2[k+1])*h*h/6.);
}

void EqualizationDialog::OnDrawRadio(wxCommandEvent &evt)
{
   int numPoints = mLogEnvelope->GetNumberOfPoints();
   double *when = new double[ numPoints ];
   double *value = new double[ numPoints ];
   double deltadB = 0.1;
   double dx, dy, dx1, dy1, err;

   mLogEnvelope->GetPoints( when, value, numPoints );

   // set 'custom' as the selected curve
   EnvelopeUpdated();

   bool flag = true;
   while (flag)
   {
      flag = false;
      int numDeleted = 0;
      mLogEnvelope->GetPoints( when, value, numPoints );
      for(int j=0;j<numPoints-2;j++)
      {
         dx = when[j+2+numDeleted] - when[j+numDeleted];
         dy = value[j+2+numDeleted] - value[j+numDeleted];
         dx1 = when[j+numDeleted+1] - when[j+numDeleted];
         dy1 = dy * dx1 / dx;
         err = fabs(value[j+numDeleted+1] - (value[j+numDeleted] + dy1));
         if( err < deltadB )
         {   // within < deltadB dB?
            mLogEnvelope->Delete(j+1);
            numPoints--;
            numDeleted++;
            flag = true;
         }
      }
   }
   delete [] when;
   delete [] value;

   if(mLinFreq->IsChecked())
   {
      EnvLogToLin();
      mPanel->mEnvelope = mLinEnvelope;
      freqRuler->ruler.SetLog(false);
      freqRuler->ruler.SetRange(0, mHiFreq);
   }

   szrV->Show(szrG,false);
   szrH->Show(szrI,false);
   szrH->Show(szrL,true);
   Layout();
   mPanel->RecalcRequired = true;   // it may have changed slightly due to the deletion of points
   mPanel->Refresh( false );
   drawMode = true;
}

void EqualizationDialog::OnSliderRadio(wxCommandEvent &evt)
{
   UpdateGraphic();
}

void EqualizationDialog::UpdateGraphic()
{
   double loLog = log10(mLoFreq);
   double hiLog = log10(mHiFreq);
   double denom = hiLog - loLog;

   if(mLinFreq->IsChecked())  //going from lin to log freq scale
   {  // add some extra points to the linear envelope for the graphic to follow
      double step = pow(2., 1./12.);   // twelve steps per octave
      double when,value;
      for(double freq=10.; freq<mHiFreq; freq*=step)
      {
         when = freq/mHiFreq;
         value = mLinEnvelope->GetValue(when);
         mLinEnvelope->Insert(when, value);
      }

      EnvLinToLog();
      mPanel->mEnvelope = mLogEnvelope;
      freqRuler->ruler.SetLog(true);
      freqRuler->ruler.SetRange(mLoFreq, mHiFreq);
   }

   bandsInUse = 0;
   while(thirdOct[bandsInUse] <= mHiFreq) {
      bandsInUse++;
      if(bandsInUse == NUMBER_OF_BANDS)
         break;
   }

   for (int i = 0; i<bandsInUse; ++i)
   {
      if( thirdOct[i] == mLoFreq )
         whenSliders[i] = 0.;
      else
         whenSliders[i] = (log10(thirdOct[i])-loLog)/denom;
      m_EQVals[i] = mLogEnvelope->GetValue(whenSliders[i]);    //set initial values of sliders
      if( m_EQVals[i] > 20.)
         m_EQVals[i] = 20.;
      if( m_EQVals[i] < -20.)
         m_EQVals[i] = -20.;
   }
   ErrMin();                  //move sliders to minimise error
   for (int i = 0; i<bandsInUse; ++i)
   {
      m_sliders[i]->SetValue(lrint(m_EQVals[i])); //actually set slider positions
      m_sliders_old[i] = m_sliders[i]->GetValue();
#if wxUSE_TOOLTIPS
      wxString tip;
      if( thirdOct[i] < 1000.)
         tip.Printf( wxT("%dHz\n%.1fdB"), (int)thirdOct[i], m_EQVals[i] );
      else
         tip.Printf( wxT("%gkHz\n%.1fdB"), thirdOct[i]/1000., m_EQVals[i] );
      m_sliders[i]->SetToolTip(tip);
#endif
   }
   szrV->Show(szrG,true);  // eq sliders
   szrH->Show(szrI,true);  // interpolation choice
   szrH->Show(szrL,false); // linear freq checkbox
   Layout();            // Make all sizers get resized first
   LayoutEQSliders();   // Then layout sliders
   Layout();            // And layout again to resize dialog

   wxSize wsz = GetSize();
   wxSize ssz = szrV->GetSize();
   if (ssz.x > wsz.x || ssz.y > wsz.y)
   {
      Fit();
   }
   GraphicEQ(mLogEnvelope);
   drawMode = false;
}

void EqualizationDialog::OnLinFreq(wxCommandEvent &evt)
{
   if(mLinFreq->IsChecked())  //going from log to lin freq scale
   {
      freqRuler->ruler.SetLog(false);
      freqRuler->ruler.SetRange(0, mHiFreq);
      EnvLogToLin();
      mPanel->mEnvelope = mLinEnvelope;
      linCheck = true;
   }
   else  //going from lin to log freq scale
   {
      freqRuler->ruler.SetLog(true);
      freqRuler->ruler.SetRange(mLoFreq, mHiFreq);
      EnvLinToLog();
      mPanel->mEnvelope = mLogEnvelope;
      linCheck = false;
   }
   mPanel->Recalc();
   freqRuler->Refresh(false);
   mPanel->Refresh(false);
}

void EqualizationDialog::EnvLogToLin(void)
{
   int numPoints = mLogEnvelope->GetNumberOfPoints();
   if( numPoints == 0 )
   {
      return;
   }

   double *when = new double[ numPoints ];
   double *value = new double[ numPoints ];

   mLinEnvelope->Flatten(0.);
   mLinEnvelope->SetTrackLen(1.0);
   mLogEnvelope->GetPoints( when, value, numPoints );
   mLinEnvelope->Move(0., value[0]);
   double loLog = log10(20.);
   double hiLog = log10(mHiFreq);
   double denom = hiLog - loLog;

   for( int i=0; i < numPoints; i++)
      mLinEnvelope->Insert(pow( 10., ((when[i] * denom) + loLog))/mHiFreq , value[i]);
   mLinEnvelope->Move(1., value[numPoints-1]);

   delete [] when;
   delete [] value;
}

void EqualizationDialog::EnvLinToLog(void)
{
   int numPoints = mLinEnvelope->GetNumberOfPoints();
   if( numPoints == 0 )
   {
      return;
   }
   
   double *when = new double[ numPoints ];
   double *value = new double[ numPoints ];

   mLogEnvelope->Flatten(0.);
   mLogEnvelope->SetTrackLen(1.0);
   mLinEnvelope->GetPoints( when, value, numPoints );
   mLogEnvelope->Move(0., value[0]);
   double loLog = log10(20.);
   double hiLog = log10(mHiFreq);
   double denom = hiLog - loLog;
   bool changed = false;

   for( int i=0; i < numPoints; i++)
   {
      if( when[i]*mHiFreq >= 20 )
      {
         mLogEnvelope->Insert((log10(when[i]*mHiFreq)-loLog)/denom , value[i]);
      }
      else
      {  //get the first point as close as we can to the last point requested
         changed = true;
         double v = value[i];
         mLogEnvelope->Insert(0., v);
      }
   }
   mLogEnvelope->Move(1., value[numPoints-1]);

   delete [] when;
   delete [] value;

   if(changed)
      EnvelopeUpdated(mLogEnvelope, false);
}

void EqualizationDialog::ErrMin(void)
{
   double vals[NUM_PTS];
   int i;
   double error = 0.0;
   double oldError = 0.0;
   double m_EQValsOld = 0.0;
   double correction = 1.6;
   bool flag;
   int j=0;
   Envelope *testEnvelope;
   testEnvelope = new Envelope();
   testEnvelope->SetInterpolateDB(false);
   testEnvelope->Mirror(false);
   testEnvelope->Flatten(0.);
   testEnvelope->SetTrackLen(1.0);
   testEnvelope->CopyFrom(mLogEnvelope, 0.0, 1.0);

   for(i=0; i < NUM_PTS; i++)
      vals[i] = testEnvelope->GetValue(whens[i]);

   //   Do error minimisation
   error = 0.;
   GraphicEQ(testEnvelope);
   for(i=0; i < NUM_PTS; i++)   //calc initial error
   {
      double err = vals[i] - testEnvelope->GetValue(whens[i]);
      error += err*err;
   }
   oldError = error;
   while( j < bandsInUse*12 )  //loop over the sliders a number of times
   {
      i = j%bandsInUse;       //use this slider
      if( (j > 0) & (i == 0) )   // if we've come back to the first slider again...
      {
         if( correction > 0 )
            correction = -correction;     //go down
         else
            correction = -correction/2.;  //go up half as much
      }
      flag = true;   // check if we've hit the slider limit
      do
      {
         oldError = error;
         m_EQValsOld = m_EQVals[i];
         m_EQVals[i] += correction;    //move fader value
         if( m_EQVals[i] > 20. )
         {
            m_EQVals[i] = 20.;
            flag = false;
         }
         if( m_EQVals[i] < -20. )
         {
            m_EQVals[i] = -20.;
            flag = false;
         }
         GraphicEQ(testEnvelope);         //calculate envelope
         error = 0.;
         for(int k=0; k < NUM_PTS; k++)  //calculate error
         {
            double err = vals[k] - testEnvelope->GetValue(whens[k]);
            error += err*err;
         }
      }
      while( (error < oldError) & flag );
      if( error > oldError )
      {
         m_EQVals[i] = m_EQValsOld;   //last one didn't work
         error = oldError;
      }
      else
         oldError = error;
      if( error < .0025 * bandsInUse)
         break;   // close enuff
      j++;  //try next slider
   }
   if( error > .0025 * bandsInUse ) // not within 0.05dB on each slider, on average
   {
      Select( (int) mCurves.GetCount()-1 );
      EnvelopeUpdated(testEnvelope, false);
   }
   delete testEnvelope;
   testEnvelope = NULL;
}

void EqualizationDialog::OnSliderM(wxCommandEvent &event)
{
   TransferDataFromWindow();
   mPanel->RecalcRequired = true;
}

void EqualizationDialog::OnSliderDBMIN(wxCommandEvent &event)
{
   TransferDataFromWindow();
}

void EqualizationDialog::OnSliderDBMAX(wxCommandEvent &event)
{
   TransferDataFromWindow();
}

//
// New curve was selected
//
void EqualizationDialog::OnCurve(wxCommandEvent &event)
{
   // Select new curve
   #if wxCHECK_VERSION(2, 6, 2) && !defined(__WXX11__)
   setCurve( mCurve->GetCurrentSelection() );
   #else
   setCurve( mCurve->GetSelection() );
   #endif
   if( !drawMode )
      UpdateGraphic();
}

//
// User wants to save a new curve
//
void EqualizationDialog::OnSaveAs(wxCommandEvent &event)
{
   SaveAs();
}

bool EqualizationDialog::SaveAs()
{
   wxString name;
   int numCurves = mCurves.GetCount();
   int curve;
   bool overWrite = false;

   // build the dialog
   wxTextEntryDialog dlg( this,
                          _("Enter the desired name of the curve"),
                          _("Save As...") );

   // Setup list of characters that aren't allowed
   wxArrayString exclude;
   exclude.Add( wxT("<") );
   exclude.Add( wxT(">") );
   exclude.Add( wxT("'") );
   exclude.Add( wxT("\"") );

   // And tell the validator about them
   dlg.SetTextValidator( wxFILTER_EXCLUDE_CHAR_LIST );
   wxTextValidator *tv = dlg.GetTextValidator();
   tv->SetExcludes( exclude );

   // Prompt the user until a valid name is enter or cancelled
   while( true )
   {   // Show the dialog and bail if the user cancels
      if( dlg.ShowModal() == wxID_CANCEL )
      {
         return false;
      }

      // Extract the name from the dialog
      name = dlg.GetValue();

      // Search list of curves for a duplicate name
      bool bad = false;
      for( curve = 0; curve < numCurves; curve++ )
      {
         if( name == mCurves[ curve ].Name )
         {
            int answer = wxMessageBox( wxT("Overwrite existing curve?"),
                     wxT("Curve exists"), wxYES_NO );
            if (answer == wxNO)
            {
               bad = true;
               break;
            }
            else
            {
               overWrite = true;
               break;
            }
         }
      }
      if( name == wxT("") || name == wxT("custom") )
         bad = true;

      // An unacceptable duplicate wasn't found so break out of loop
      if( !bad )
      {
         break;
      }
   }

   if( overWrite == false)
   {
      // Create a new entry
      mCurves.Add( EQCurve( wxT("custom") ) );

      // Copy over the points
      mCurves[ numCurves ].points = mCurves[ numCurves - 1 ].points;

      // Give the original custom entry the new name
      mCurves[ numCurves - 1 ].Name = name;

      // Recreate the choice control and layout everything again
      CreateChoice();
      Layout();

      // Select the newly name curve
      Select( numCurves - 1 );
   }
   else
   {
      // Overwrite curve from custom
      mCurves[ curve ].points = mCurves[ numCurves - 1 ].points;

      // Select the overwritten curve
      Select( curve );
   }

   // Save the curves to file - enables the user to set up curves without applying them
   SaveCurves();
   return true;
}

//
// User wants to delete a curve
//
void EqualizationDialog::OnDelete(wxCommandEvent &event)
{
   // Get the one to be deleted
   int sel = mCurve->GetSelection();

   // Create the prompt
   wxString quest;
   quest = wxString(_("Delete")) + wxT(" ") + mCurves[ sel ].Name + wxT("?");

   // Ask for confirmation
   int ans = wxMessageBox( quest,
                           _("Confirm Deletion"),
                           wxYES_NO | wxCENTRE,
                           this );

   // Denied, so bail
   if( ans == wxNO )
   {
      return;
   }

   // Remove the curve from the array and choice control
   mCurves.RemoveAt( sel );
   mCurve->Delete( sel );

   // Save the curves to file - enables the user to delete curves without applying anything
   SaveCurves();

   // set 'custom' as the selected curve
   Select( (int) mCurves.GetCount()-1 );

   // Reselect
   setCurve( mCurves.GetCount()-1 );
}

void EqualizationDialog::OnClear(wxCommandEvent &event)
{
   mLogEnvelope->Flatten(0.);
   mLogEnvelope->SetTrackLen(1.0);
   mLinEnvelope->Flatten(0.);
   mLinEnvelope->SetTrackLen(1.0);
   mPanel->RecalcRequired = true;
   mPanel->Refresh(false);
   if( !drawMode )
   {
      for( int i=0; i< bandsInUse; i++)
      {
         m_sliders[i]->SetValue(0);
         m_sliders_old[i] = 0;
         m_EQVals[i] = 0.;
      }
   }
   EnvelopeUpdated();
}

void EqualizationDialog::OnInvert(wxCommandEvent &event) // Inverts any curve
{
   if(!drawMode)   // Graphic (Slider) mode. Invert the sliders.
   {
      for (int i = 0; thirdOct[i] <= mHiFreq; ++i)
      {
         if( i == NUMBER_OF_BANDS )
            break;
         m_EQVals[i] = -m_EQVals[i];
         int newPosn = (int)m_EQVals[i];
         m_sliders[i]->SetValue( newPosn );
         m_sliders_old[i] = newPosn;
   #if wxUSE_TOOLTIPS
         wxString tip;
         if( thirdOct[i] < 1000.)
            tip.Printf( wxT("%dHz\n%.1fdB"), (int)thirdOct[i], m_EQVals[i] );
         else
            tip.Printf( wxT("%gkHz\n%.1fdB"), thirdOct[i]/1000., m_EQVals[i] );
         m_sliders[i]->SetToolTip(tip);
   #endif
      }
      GraphicEQ(mLogEnvelope);
   }
   else  // Draw mode.  Invert the points.
   {
      bool lin;   // refers to the 'log' or 'lin' of the frequency scale, not the amplitude
      int numPoints; // number of points in the curve/envelope

      // determine if log or lin curve is the current one
      // and find out how many points are in the curve
      if(mLinFreq->IsChecked())  // lin freq scale and so envelope
      {
         lin = true;
         numPoints = mLinEnvelope->GetNumberOfPoints();
      }
      else
      {
         lin = false;
         numPoints = mLogEnvelope->GetNumberOfPoints();
      }

      if( numPoints == 0 )
         return;

      double *when = new double[ numPoints ];
      double *value = new double[ numPoints ];

      if(lin)
         mLinEnvelope->GetPoints( when, value, numPoints );
      else
         mLogEnvelope->GetPoints( when, value, numPoints );

      // invert the curve
      for( int i=0; i < numPoints; i++)
      {
         if(lin)
            mLinEnvelope->Move(when[i] , -value[i]);
         else
            mLogEnvelope->Move(when[i] , -value[i]);
      }

      // copy it back to the other one (just in case)
      if(lin)
         EnvLinToLog();
      else
         EnvLogToLin();
   }

   // and update the display etc
   mPanel->Recalc();
   mPanel->Refresh(false);
   EnvelopeUpdated();
}

void EqualizationDialog::OnErase(wxEraseEvent &event)
{
   // Ignore it
}

void EqualizationDialog::OnPaint(wxPaintEvent &event)
{
   wxPaintDC dc(this);

#if defined(__WXGTK__)
   dc.SetBackground(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE)));
#endif

   dc.Clear();
}

void EqualizationDialog::OnSize(wxSizeEvent &event)
{
   Layout();

   if (mFaderOrDraw[1]->GetValue())
   {
      LayoutEQSliders();
   }

   event.Skip();
}

void EqualizationDialog::OnPreview(wxCommandEvent &event)
{
   TransferDataFromWindow();
   m_pEffect->Preview();
   mPanel->RecalcRequired = true;
   //redraw the window.
   Refresh();
   Update();
   //v Restore previous values?
}

void EqualizationDialog::OnGridOnOff(wxCommandEvent & WXUNUSED(event))
{
   if( mGridOnOff->IsChecked() )
      drawGrid = true;
   else
      drawGrid = false;
   mPanel->Refresh(false);
}

void EqualizationDialog::RevertCustom()
{
   EQCurve &realCustom = mCurves[mCurves.GetCount()-1];
   wxASSERT(realCustom.Name.IsSameAs(wxT("custom")));
   realCustom.points = mCustomBackup.points;
}

void EqualizationDialog::Finish(bool ok)
{
   if(mLogEnvelope)
     delete mLogEnvelope;
   mLogEnvelope = NULL;
   if(mLinEnvelope)
     delete mLinEnvelope;
   mLinEnvelope = NULL;
   mPanel = NULL;

   EndModal(ok);
}

void EqualizationDialog::OnCancel(wxCommandEvent &event)
{
   if (mDisallowCustom)
      RevertCustom();

   Finish(false);
}

void EqualizationDialog::OnOk(wxCommandEvent &event)
{
   TransferDataFromWindow();

   if( Validate() )
   {
      // Update custom curve (so it's there for next time)
      //(done in a hurry, may not be the neatest -MJS)
      if( !mDisallowCustom)
      {
         if ( mDirty && drawMode )
         {
            int i, j;
            int numPoints = mLogEnvelope->GetNumberOfPoints();
            double *when = new double[ numPoints ];
            double *value = new double[ numPoints ];
            mLogEnvelope->GetPoints( when, value, numPoints );
            for(i=0,j=0;j<numPoints-2;i++,j++)
            {
               if( (value[i]<value[i+1]+.05) && (value[i]>value[i+1]-.05) &&
                   (value[i+1]<value[i+2]+.05) && (value[i+1]>value[i+2]-.05) )
               {   // within < 0.05 dB?
                  mLogEnvelope->Delete(j+1);
                  numPoints--;
                  j--;
               }
            }
            delete [] when;
            delete [] value;
            Select( (int) mCurves.GetCount()-1 );
         }
         SaveCurves();
      }
      Finish(true);
   }
   else
   {
      event.Skip(false);
   }
}

#if wxUSE_ACCESSIBILITY

SliderAx::SliderAx( wxWindow * window, wxString fmt ):
   wxWindowAccessible( window )
{
   mParent = window;
   mFmt = fmt;
}

SliderAx::~SliderAx()
{
}

// Retrieves the address of an IDispatch interface for the specified child.
// All objects must support this property.
wxAccStatus SliderAx::GetChild( int childId, wxAccessible** child )
{
   if( childId == wxACC_SELF )
   {
      *child = this;
   }
   else
   {
      *child = NULL;
   }

   return wxACC_OK;
}

// Gets the number of children.
wxAccStatus SliderAx::GetChildCount(int* childCount)
{
   *childCount = 3;

   return wxACC_OK;
}

// Gets the default action for this object (0) or > 0 (the action for a child).
// Return wxACC_OK even if there is no action. actionName is the action, or the empty
// string if there is no action.
// The retrieved string describes the action that is performed on an object,
// not what the object does as a result. For example, a toolbar button that prints
// a document has a default action of "Press" rather than "Prints the current document."
wxAccStatus SliderAx::GetDefaultAction( int childId, wxString *actionName )
{
   actionName->Clear();

   return wxACC_OK;
}

// Returns the description for this object or a child.
wxAccStatus SliderAx::GetDescription( int childId, wxString *description )
{
   description->Clear();

   return wxACC_OK;
}

// Gets the window with the keyboard focus.
// If childId is 0 and child is NULL, no object in
// this subhierarchy has the focus.
// If this object has the focus, child should be 'this'.
wxAccStatus SliderAx::GetFocus(int* childId, wxAccessible** child)
{
   *childId = 0;
   *child = this;

   return wxACC_OK;
}

// Returns help text for this object or a child, similar to tooltip text.
wxAccStatus SliderAx::GetHelpText( int childId, wxString *helpText )
{
   helpText->Clear();

   return wxACC_OK;
}

// Returns the keyboard shortcut for this object or child.
// Return e.g. ALT+K
wxAccStatus SliderAx::GetKeyboardShortcut( int childId, wxString *shortcut )
{
   shortcut->Clear();

   return wxACC_OK;
}

// Returns the rectangle for this object (id = 0) or a child element (id > 0).
// rect is in screen coordinates.
wxAccStatus SliderAx::GetLocation( wxRect& rect, int elementId )
{
   wxSliderBugfix *s = wxDynamicCast( GetWindow(), wxSliderBugfix );

   rect = s->GetRect();
   rect.SetPosition( s->GetParent()->ClientToScreen( rect.GetPosition() ) );

   return wxACC_OK;
}

// Gets the name of the specified object.
wxAccStatus SliderAx::GetName(int childId, wxString* name)
{
   wxSliderBugfix *s = wxDynamicCast( GetWindow(), wxSliderBugfix );

   *name = s->GetName();

   return wxACC_OK;
}

// Returns a role constant.
wxAccStatus SliderAx::GetRole(int childId, wxAccRole* role)
{
   switch( childId )
   {
      case 0:
         *role = wxROLE_SYSTEM_SLIDER;
      break;

      case 1:
      case 3:
         *role = wxROLE_SYSTEM_PUSHBUTTON;
      break;

      case 2:
         *role = wxROLE_SYSTEM_INDICATOR;
      break;
   }

   return wxACC_OK;
}

// Gets a variant representing the selected children
// of this object.
// Acceptable values:
// - a null variant (IsNull() returns TRUE)
// - a list variant (GetType() == wxT("list"))
// - an integer representing the selected child element,
//   or 0 if this object is selected (GetType() == wxT("long"))
// - a "void*" pointer to a wxAccessible child object
wxAccStatus SliderAx::GetSelections( wxVariant *selections )
{
   return wxACC_NOT_IMPLEMENTED;
}

// Returns a state constant.
wxAccStatus SliderAx::GetState(int childId, long* state)
{
   wxSliderBugfix *s = wxDynamicCast( GetWindow(), wxSliderBugfix );

   switch( childId )
   {
      case 0:
         *state = wxACC_STATE_SYSTEM_FOCUSABLE;
      break;

      case 1:
         if( s->GetValue() == s->GetMin() )
         {
            *state = wxACC_STATE_SYSTEM_INVISIBLE;
         }
      break;

      case 3:
         if( s->GetValue() == s->GetMax() )
         {
            *state = wxACC_STATE_SYSTEM_INVISIBLE;
         }
      break;
   }

   // Do not use mSliderIsFocused is not set until after this method
   // is called.
   *state |= ( s == wxWindow::FindFocus() ? wxACC_STATE_SYSTEM_FOCUSED : 0 );

   return wxACC_OK;
}

// Returns a localized string representing the value for the object
// or child.
wxAccStatus SliderAx::GetValue(int childId, wxString* strValue)
{
   wxSliderBugfix *s = wxDynamicCast( GetWindow(), wxSliderBugfix );

   if( childId == 0 )
   {
      strValue->Printf( mFmt, s->GetValue() );

      return wxACC_OK;
   }

   return wxACC_NOT_SUPPORTED;
}

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
// arch-tag: 65b35bfa-632c-46fe-9170-840a158b3c97

