/**********************************************************************

  Audacity: A Digital Audio Editor

  WarningsPrefs.cpp

  Brian Gunlogson
  Joshua Haberman
  Dominic Mazzoni
  James Crook


*******************************************************************//**

\class WarningsPrefs
\brief A PrefsPanel to enable/disable certain warning messages.

*//*******************************************************************/

#include "../Audacity.h"

#include <wx/defs.h>

#include "../ShuttleGui.h"

#include "WarningsPrefs.h"

////////////////////////////////////////////////////////////////////////////////

WarningsPrefs::WarningsPrefs(wxWindow * parent)
:  PrefsPanel(parent, _("Warnings"))
{
   Populate();
}

WarningsPrefs::~WarningsPrefs()
{
}

void WarningsPrefs::Populate()
{
   //------------------------- Main section --------------------
   // Now construct the GUI itself.
   // Use 'eIsCreatingFromPrefs' so that the GUI is 
   // initialised with values from gPrefs.
   ShuttleGui S(this, eIsCreatingFromPrefs);
   PopulateOrExchange(S);
   // ----------------------- End of main section --------------
}

void WarningsPrefs::PopulateOrExchange(ShuttleGui & S)
{
   S.SetBorder(2);

   S.StartStatic(_("Show Warnings/Prompts for"));
   {
      S.TieCheckBox(_("Saving &projects"),
                    wxT("/Warnings/FirstProjectSave"),
                    true);
      S.TieCheckBox(_("Saving &empty project"),    
                    wxT("/GUI/EmptyCanBeDirty"),
                    true);
      S.TieCheckBox(_("&Low disk space at program start up"),
                    wxT("/Warnings/DiskSpaceWarning"),
                    true);
      S.TieCheckBox(_("Mixing down to &stereo during export"),
                    wxT("/Warnings/MixStereo"),
                    true);
      S.TieCheckBox(_("Mixing down to &mono during export"),
                    wxT("/Warnings/MixMono"),
                    true);
   }
   S.EndStatic();
}

bool WarningsPrefs::Apply()
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
// arch-tag: 7e997d04-6b94-4abb-b3d6-748400f86598
