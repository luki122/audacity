/**********************************************************************

  Audacity: A Digital Audio Editor

  MixerToolBar.cpp

  Dominic Mazzoni
 
*******************************************************************//*!

\class MixerToolBar
\brief A ToolBar that provides the record and playback volume settings.

*//*******************************************************************/


#include "../Audacity.h"

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/choice.h>
#include <wx/event.h>
#include <wx/intl.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/tooltip.h>
#endif

#include "MixerToolBar.h"

#include "../AudacityApp.h"
#include "../AColor.h"
#include "../AllThemeResources.h"
#include "../AudioIO.h"
#include "../ImageManipulation.h"
#include "../Prefs.h"
#include "../Project.h"
#include "../Theme.h"
#include "../widgets/ASlider.h"

IMPLEMENT_CLASS(MixerToolBar, ToolBar);

////////////////////////////////////////////////////////////
/// Methods for MixerToolBar
////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(MixerToolBar, ToolBar)
   EVT_PAINT(MixerToolBar::OnPaint)
   EVT_SLIDER(wxID_ANY, MixerToolBar::SetMixer)
   EVT_CHOICE(wxID_ANY, MixerToolBar::SetMixer)
   EVT_COMMAND(wxID_ANY, EVT_CAPTURE_KEY, MixerToolBar::OnCaptureKey)
END_EVENT_TABLE()

//Standard contructor
MixerToolBar::MixerToolBar()
: ToolBar(MixerBarID, _("Mixer"), wxT("Mixer"))
{
   mInputSourceChoice = NULL;
}

MixerToolBar::~MixerToolBar()
{
   delete mPlayBitmap;
   delete mRecordBitmap;
}

void MixerToolBar::Create(wxWindow *parent)
{
   ToolBar::Create(parent);
}

void MixerToolBar::RecreateTipWindows()
{
   // Hack to make sure they appear on top of other windows
   mInputSlider->RecreateTipWin();
   mOutputSlider->RecreateTipWin();
}

void MixerToolBar::Populate()
{
   mPlayBitmap = new wxBitmap(theTheme.Bitmap(bmpSpeaker));

   Add(new wxStaticBitmap(this,
                          wxID_ANY, 
                          *mPlayBitmap), 0, wxALIGN_CENTER);

   mOutputSlider = new ASlider(this, wxID_ANY, _("Output Volume"),
                               wxDefaultPosition, wxSize(130, 25));
   mOutputSlider->SetName(_("Slider Output"));
   Add(mOutputSlider, 0, wxALIGN_CENTER);

   mRecordBitmap = new wxBitmap(theTheme.Bitmap(bmpMic));

   Add(new wxStaticBitmap(this,
                          wxID_ANY, 
                          *mRecordBitmap), 0, wxALIGN_CENTER);

   mInputSlider = new ASlider(this, wxID_ANY, _("Input Volume"),
                              wxDefaultPosition, wxSize(130, 25));
   mInputSlider->SetName(_("Slider Input"));
   Add(mInputSlider, 0, wxALIGN_CENTER);

   mInputSourceChoice = NULL;

   // this bit taken from SelectionBar::Populate()
   mOutputSlider->Connect(wxEVT_SET_FOCUS,
                 wxFocusEventHandler(MixerToolBar::OnFocus),
                 NULL,
                 this);
   mOutputSlider->Connect(wxEVT_KILL_FOCUS,
                 wxFocusEventHandler(MixerToolBar::OnFocus),
                 NULL,
                 this);
   mInputSlider->Connect(wxEVT_SET_FOCUS,
                 wxFocusEventHandler(MixerToolBar::OnFocus),
                 NULL,
                 this);
   mInputSlider->Connect(wxEVT_KILL_FOCUS,
                 wxFocusEventHandler(MixerToolBar::OnFocus),
                 NULL,
                 this);

#if USE_PORTMIXER
   wxArrayString inputSources = gAudioIO->GetInputSourceNames();

   mInputSourceChoice = new wxChoice(this,
                                     wxID_ANY,
                                     wxDefaultPosition,
                                     wxDefaultSize,
                                     inputSources);
   mInputSourceChoice->SetName(_("Input Source"));
   Add(mInputSourceChoice, 0, wxALIGN_CENTER | wxLEFT, 2);
   mInputSourceChoice->Connect(wxEVT_SET_FOCUS,
                 wxFocusEventHandler(MixerToolBar::OnFocus),
                 NULL,
                 this);

   // Set choice control to default value
   float inputVolume;
   float playbackVolume;
   int inputSource;
   gAudioIO->GetMixer(&inputSource, &inputVolume, &playbackVolume);
   mInputSourceChoice->SetSelection(inputSource);

   // Show or hide the control based on input sources
   mInputSourceChoice->Show( inputSources.GetCount() != 0 );

   // Show or hide the input slider based on whether it works
   mInputSlider->Enable(gAudioIO->InputMixerWorks());

   UpdateControls();

#endif

   // Add a little space
   Add(2, -1);
}

//Also from SelectionBar;
void MixerToolBar::OnFocus(wxFocusEvent &event)
{
   wxCommandEvent e(EVT_CAPTURE_KEYBOARD);

   if (event.GetEventType() == wxEVT_KILL_FOCUS) {
      e.SetEventType(EVT_RELEASE_KEYBOARD);
   }
   e.SetEventObject(this);
   GetParent()->GetEventHandler()->ProcessEvent(e);

   Refresh(false);

   event.Skip();
}

void MixerToolBar::OnCaptureKey(wxCommandEvent &event)
{
   wxKeyEvent *kevent = (wxKeyEvent *)event.GetEventObject();
   int keyCode = kevent->GetKeyCode();

   // Pass LEFT/RIGHT/UP/DOWN/PAGEUP/PAGEDOWN through for input/output sliders
   if (FindFocus() == mOutputSlider && (keyCode == WXK_LEFT || keyCode == WXK_RIGHT
                                    || keyCode == WXK_UP || keyCode == WXK_DOWN
                                    || keyCode == WXK_PAGEUP || keyCode == WXK_PAGEDOWN)) {
      return;
   }
   if (FindFocus() == mInputSlider && (keyCode == WXK_LEFT || keyCode == WXK_RIGHT
                                    || keyCode == WXK_UP || keyCode == WXK_DOWN
                                    || keyCode == WXK_PAGEUP || keyCode == WXK_PAGEDOWN)) {
      return;
   }
   // Pass LEFT/RIGHT/UP/DOWN through for SourceChoice
   if (FindFocus() == mInputSourceChoice && (keyCode == WXK_LEFT || keyCode == WXK_RIGHT
                                    || keyCode == WXK_UP || keyCode == WXK_DOWN)) {
      return;
   }

   event.Skip();

   return;
}

void MixerToolBar::UpdatePrefs()
{
#if USE_PORTMIXER
   float inputVolume;
   float playbackVolume;
   int inputSource;

   wxArrayString inputSources = gAudioIO->GetInputSourceNames();

   // Repopulate the selections
   mInputSourceChoice->Clear();
   mInputSourceChoice->Append(inputSources);

   // Reset the selected source
   gAudioIO->GetMixer(&inputSource, &inputVolume, &playbackVolume);
   mInputSourceChoice->SetSelection(inputSource);

   // Resize the control
   mInputSourceChoice->SetSize(mInputSourceChoice->GetEffectiveMinSize());

   // Show or hide the control based on input sources
   mInputSourceChoice->Show( inputSources.GetCount() != 0 );
   
   // Show or hide the input slider based on whether it works
   mInputSlider->Enable(gAudioIO->InputMixerWorks());

   // Layout the toolbar
   Layout();

   // Resize the toolbar to fit the contents
   Fit();

   // And make that size the minimum
   SetMinSize( wxWindow::GetSizer()->GetMinSize() );
   SetSize( GetMinSize() );

   // Notify someone that we've changed our size
   Updated();
#endif

   // Set label to pull in language change
   SetLabel(_("Mixer"));

   // Give base class a chance
   ToolBar::UpdatePrefs();
}

void MixerToolBar::UpdateControls()
{
#if USE_PORTMIXER
   float inputVolume;
   float playbackVolume;
   int inputSource;

   gAudioIO->GetMixer(&inputSource, &inputVolume, &playbackVolume);

   // This causes weird GUI behavior and isn't really essential.
   // We could enable it again later.
   //
   // LL: Re-enabled to keep the Audacity source in sync with
   //     the operating systems if the latter is changed outsize
   //     of Audacity.
   if (inputSource != mInputSourceChoice->GetSelection()) {
       mInputSourceChoice->SetSelection(inputSource);
#if defined(__WXMSW__)
      // LL:  Hack to reset volume to reported level after the input
      //      source is changed outside of Audacity.  Not sure why
      //      the input comes in as if it were at a volume of 1.0,
      //      but this will put it back to where it should be.  This
      //      should be removed in the future...once the problem is
      //      fully understood.
      gAudioIO->SetMixer(inputSource, 1.0, playbackVolume);
      gAudioIO->SetMixer(inputSource, inputVolume, playbackVolume);
#endif
   }

   if (mOutputSlider->Get() != playbackVolume) {
      mOutputSlider->Set(playbackVolume);
   }

   if (mInputSlider->Get() != inputVolume) {
      mInputSlider->Set(inputVolume);
   }
#endif // USE_PORTMIXER
}

void MixerToolBar::SetMixer(wxCommandEvent &event)
{
#if USE_PORTMIXER
   float inputVolume = mInputSlider->Get();
   float outputVolume = mOutputSlider->Get();
   int inputSource = mInputSourceChoice->GetSelection();

   gAudioIO->SetMixer(inputSource, inputVolume, outputVolume);
#endif // USE_PORTMIXER
}

void MixerToolBar::ShowOutputGainDialog()
{
   mOutputSlider->ShowDialog();
   wxCommandEvent e;
   SetMixer(e);
   UpdateControls();
}

void MixerToolBar::ShowInputGainDialog()
{
   mInputSlider->ShowDialog();
   wxCommandEvent e;
   SetMixer(e);
   UpdateControls();
}

void MixerToolBar::ShowInputSourceDialog()
{
   if (!mInputSourceChoice || mInputSourceChoice->GetCount() == 0) {
      wxMessageBox(_("Input source information is not available."));
      return;
   }

#if USE_PORTMIXER
   wxArrayString inputSources = mInputSourceChoice->GetStrings();

   wxDialog dlg(NULL, wxID_ANY, wxString(_("Select Input Source")));
   ShuttleGui S(&dlg, eIsCreating);
   wxChoice *c;

   S.StartVerticalLay(true);
   {
      S.StartHorizontalLay(wxCENTER, false);
      {
         c = S.AddChoice(_("Input Source:"),
                         mInputSourceChoice->GetStringSelection(),
                         &inputSources);
      }
      S.EndHorizontalLay();
      S.AddStandardButtons();
   }
   S.EndVerticalLay();

   dlg.SetSize(dlg.GetSizer()->GetMinSize());
   dlg.Center();

   if (dlg.ShowModal() == wxID_OK)
   {
      // This will fire an event which will invoke SetMixer above.
      mInputSourceChoice->SetSelection(c->GetSelection());
   }
#endif
}

void MixerToolBar::AdjustOutputGain(int adj)
{
   if (adj < 0) {
      mOutputSlider->Decrease(-adj);
   }
   else {
      mOutputSlider->Increase(adj);
   }
   wxCommandEvent e;
   SetMixer(e);
   UpdateControls();
}

void MixerToolBar::AdjustInputGain(int adj)
{
   if (adj < 0) {
      mInputSlider->Decrease(-adj);
   }
   else {
      mInputSlider->Increase(adj);
   }
   wxCommandEvent e;
   SetMixer(e);
   UpdateControls();
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
// arch-tag: 6a50243e-9fc9-4f0f-b344-bd3044dc09ad

