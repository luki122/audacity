/**********************************************************************

  Audacity: A Digital Audio Editor

  ModulePrefs.h

  Brian Gunlogson
  Joshua Haberman
  James Crook

**********************************************************************/

#ifndef __AUDACITY_MODULE_PREFS__
#define __AUDACITY_MODULE_PREFS__

#include <wx/defs.h>

#include <wx/window.h>

#include "../ShuttleGui.h"

#include "PrefsPanel.h"

class ModulePrefs:public PrefsPanel
{
 public:
   ModulePrefs(wxWindow * parent);
   ~ModulePrefs();
   virtual bool Apply();

 private:
   void Populate();
   void PopulateOrExchange(ShuttleGui & S);
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
// arch-tag: 57018e2b-d264-4f93-bfa7-06752ebf631e
