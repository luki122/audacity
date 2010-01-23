/**********************************************************************

  Audacity: A Digital Audio Editor

  PrefsDialog.cpp

  Joshua Haberman
  James Crook

*******************************************************************//**

\class PrefsDialog
\brief Dialog that shows the current PrefsPanel in a tabbed divider.

*//*******************************************************************/

#include "../Audacity.h"

#include <wx/defs.h>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/font.h>
#include <wx/gdicmn.h>
#include <wx/intl.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>

#include <wx/listbook.h>

#if wxCHECK_VERSION(2, 8, 4)
#include <wx/treebook.h>
#else
#include "../widgets/treebook.h"
#endif

#include "../Experimental.h"
#include "../Project.h"
#include "../Prefs.h"

#include "PrefsDialog.h"
#include "PrefsPanel.h"

#include "BatchPrefs.h"
#include "DevicePrefs.h"
#include "DirectoriesPrefs.h"
#include "EffectsPrefs.h"
#include "GUIPrefs.h"
#include "ImportExportPrefs.h"
#include "KeyConfigPrefs.h"
#include "LibraryPrefs.h"
#include "MousePrefs.h"
#include "PlaybackPrefs.h"
#include "ProjectsPrefs.h"
#include "QualityPrefs.h"
#include "RecordingPrefs.h"
#include "SpectrumPrefs.h"
#include "ThemePrefs.h"
#include "TracksPrefs.h"
#include "WarningsPrefs.h"

#ifdef EXPERIMENTAL_MIDI_OUT
#include "MidiIOPrefs.h"
#endif

BEGIN_EVENT_TABLE(PrefsDialog, wxDialog)
   EVT_BUTTON(wxID_OK, PrefsDialog::OnOK)
   EVT_BUTTON(wxID_CANCEL, PrefsDialog::OnCancel)
   EVT_TREE_KEY_DOWN(wxID_ANY, PrefsDialog::OnTreeKeyDown) // Handles key events when tree has focus
END_EVENT_TABLE()

PrefsDialog::PrefsDialog(wxWindow * parent)
:  wxDialog(parent, wxID_ANY, wxString(_("Audacity Preferences")),
            wxDefaultPosition,
            wxDefaultSize,
            wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
   ShuttleGui S(this, eIsCreating);

   S.StartVerticalLay(true);
   {
      S.StartHorizontalLay(wxALIGN_LEFT | wxEXPAND, true);
      {
         mCategories = new wxTreebook(this, wxID_ANY);
         S.Prop(1);
         S.AddWindow(mCategories, wxEXPAND);

         wxWindow *w;
         // Parameters are: AppPage( page, name, IsSelected, imageId)
         w = new DevicePrefs(mCategories);      mCategories->AddPage(w, w->GetName(), false, 0);
         w = new PlaybackPrefs(mCategories);    mCategories->AddPage(w, w->GetName(), false, 0);
         w = new RecordingPrefs(mCategories);   mCategories->AddPage(w, w->GetName(), false, 0);
#ifdef EXPERIMENTAL_MIDI_OUT
         w = new MidiIOPrefs(mCategories);      mCategories->AddPage(w, w->GetName(), false, 0);
#endif
         w = new QualityPrefs(mCategories);     mCategories->AddPage(w, w->GetName(), false, 0);
         w = new GUIPrefs(mCategories);         mCategories->AddPage(w, w->GetName(), false, 0);
         w = new TracksPrefs(mCategories);      mCategories->AddPage(w, w->GetName(), false, 0);
         w = new ImportExportPrefs(mCategories);mCategories->AddPage(w, w->GetName(), false, 0);
         w = new ProjectsPrefs(mCategories);    mCategories->AddPage(w, w->GetName(), false, 0);
         w = new LibraryPrefs(mCategories);     mCategories->AddPage(w, w->GetName(), false, 0);
         w = new SpectrumPrefs(mCategories);    mCategories->AddPage(w, w->GetName(), false, 0);
         w = new DirectoriesPrefs(mCategories); mCategories->AddPage(w, w->GetName(), false, 0);
         w = new WarningsPrefs(mCategories);    mCategories->AddPage(w, w->GetName(), false, 0);
         w = new EffectsPrefs(mCategories);     mCategories->AddPage(w, w->GetName(), false, 0);

#ifdef EXPERIMENTAL_THEME_PREFS
         w = new ThemePrefs(mCategories);       mCategories->AddPage(w, w->GetName(), false, 0);
#endif

//       w = new BatchPrefs(mCategories);       mCategories->AddPage(w, w->GetName(), false, 0);
         w = new KeyConfigPrefs(mCategories);   mCategories->AddPage(w, w->GetName(), false, 0);
         w = new MousePrefs(mCategories);       mCategories->AddPage(w, w->GetName(), false, 0);
      }
      S.EndHorizontalLay();
   }
   S.EndVerticalLay();

   S.AddStandardButtons(eOkButton | eCancelButton);

   /* long is signed, size_t is unsigned. On some platforms they are different
    * lengths as well. So we must check that the stored category is both > 0
    * and within the possible range of categories, making the first check on the
    * _signed_ value to avoid issues when converting an unsigned one.
    */
   size_t selected;
   long prefscat = gPrefs->Read(wxT("/Prefs/PrefsCategory"), 0L);
   if (prefscat > 0L )
      selected = prefscat; // only assign if number will fit
   else
      selected = 0;  // use 0 if value can't be assigned

   if (selected >= mCategories->GetPageCount())
      selected = 0;  // clamp to available range of tabs

   mCategories->SetSelection(selected);

#if defined(__WXGTK__)
   mCategories->GetTreeCtrl()->EnsureVisible(mCategories->GetTreeCtrl()->GetRootItem());
#endif

//   mCategories->SetSizeHints(-1, -1, 790, 600);  // 790 = 800 - (border * 2)
   Layout();
   Fit();
   wxSize sz = GetSize();

   wxASSERT_MSG(sz.x <= 800 && sz.y <= 600, wxT("Preferences dialog exceeds max size"));

   if (sz.x > 800) {
      sz.x = 800;
   }

   if (sz.y > 600) {
      sz.y = 600;
   }

   SetSizeHints(sz.x, sz.y, 800, 600);

   // Center after all that resizing, but make sure it doesn't end up
   // off-screen
   CentreOnParent();
}

PrefsDialog::~PrefsDialog()
{
}

void PrefsDialog::OnCancel(wxCommandEvent & event)
{
   for (size_t i = 0; i < mCategories->GetPageCount(); i++) {
      ((PrefsPanel *) mCategories->GetPage(i))->Cancel();
   }

   EndModal(false);
}

void PrefsDialog::OnTreeKeyDown(wxTreeEvent & event)
{
   if(event.GetKeyCode() == WXK_RETURN)
      OnOK(event);
   else
      event.Skip(); // Ensure standard behavior when enter is not pressed
}

void PrefsDialog::OnOK(wxCommandEvent & event)
{
   // Validate all pages first
   for (size_t i = 0; i < mCategories->GetPageCount(); i++) {
      PrefsPanel *panel = (PrefsPanel *) mCategories->GetPage(i);

      // The dialog doesn't end until all the input is valid
      if (!panel->Validate()) {
         mCategories->SetSelection(i);
         return;
      }
   }

   // Now apply the changes
   for (size_t i = 0; i < mCategories->GetPageCount(); i++) {
      PrefsPanel *panel = (PrefsPanel *) mCategories->GetPage(i);

      panel->Apply();
   }

   gPrefs->Write(wxT("/Prefs/PrefsCategory"), (long)mCategories->GetSelection());

#if USE_PORTMIXER
   if (gAudioIO) {
      gAudioIO->HandleDeviceChange();
   }
#endif

   // LL:  wxMac can't handle recreating the menus when this dialog is still active,
   //      so AudacityProject::UpdatePrefs() or any of the routines it calls must
   //      not cause AudacityProject::RebuildMenuBar() to be executed.
   for (size_t i = 0; i < gAudacityProjects.GetCount(); i++) {
      gAudacityProjects[i]->UpdatePrefs();
   }

   EndModal(true);
}

void PrefsDialog::SelectPageByName(wxString pageName)
{
   size_t n = mCategories->GetPageCount();

   for (size_t i = 0; i < n; i++) {
      if (mCategories->GetPageText(i) == pageName) {
         mCategories->SetSelection(i);
         return;
      }
   }
}

void PrefsDialog::ShowTempDirPage()
{
   SelectPageByName(_("Directories"));   
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
// arch-tag: b305b538-1d2e-4acf-a997-95023d10a7bd

