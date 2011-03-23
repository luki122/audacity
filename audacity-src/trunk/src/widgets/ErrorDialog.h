/**********************************************************************

  Audacity: A Digital Audio Editor

  ErrorDialog.h

  Jimmy Johnson
  James Crook

**********************************************************************/

#ifndef __AUDACITY_ERRORDIALOG__
#define __AUDACITY_ERRORDIALOG__

#include "../Audacity.h"
#include <wx/defs.h>
#include <wx/window.h>

/// Displays an error dialog with a button that offers help
void ShowErrorDialog(wxWindow *parent,
                     const wxString &dlogTitle,
                     const wxString &message, 
                     const wxString &helpURL,
                     bool Close = true);

/// Displays a modeless error dialog with a button that offers help
void ShowModelessErrorDialog(wxWindow *parent,
                     const wxString &dlogTitle,
                     const wxString &message, 
                     const wxString &helpURL,
                     bool Close = true);

/// Displays a custom modeless error dialog for aliased file errors
void ShowAliasMissingDialog(wxWindow *parent,
                     const wxString &dlogTitle,
                     const wxString &message, 
                     const wxString &helpURL,
                     const bool Close = true);

/// Displays cutable information in a text ctrl, with an OK button.
void ShowInfoDialog( wxWindow *parent,
                     const wxString &dlogTitle,
                     const wxString &shortMsg,
                     const wxString &message, 
                     const int xSize, const int ySize);

/// Displays a new window with wxHTML help.
void ShowHtmlText( wxWindow * pParent, 
                  const wxString &Title, 
                  const wxString &HtmlText );

/// Displays a file in your browser, if it's available locally,
/// OR else links to the internet.
void ShowHelpDialog(wxWindow *parent,
                     const wxString &localFileName,
                     const wxString &remoteURL);



#endif // __AUDACITY_ERRORDIALOG__

// Indentation settings for Vim and Emacs and unique identifier for Arch, a
// version control system. Please do not modify past this point.
//
// Local Variables:
// c-basic-offset: 3
// indent-tabs-mode: nil
// End:
//
// vim: et sts=3 sw=3
// arch-tag: 2b69f33b-2dc8-4b9f-99a1-65d57f554133
