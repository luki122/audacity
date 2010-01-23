/**********************************************************************

  Audacity: A Digital Audio Editor

  LabelDialog.h

  Dominic Mazzoni

**********************************************************************/

#ifndef __AUDACITY_LABELDIALOG__
#define __AUDACITY_LABELDIALOG__

#include <wx/defs.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/grid.h>
#include <wx/string.h>

#include "Internat.h"
#include "widgets/Grid.h"

class DirManager;
class TrackList;
class RowData;
class EmptyLabelRenderer;
class LabelTrack;
struct ViewInfo;

WX_DEFINE_ARRAY(RowData *, RowDataArray);

class LabelDialog:public wxDialog
{
 public:

   LabelDialog(wxWindow *parent,
               DirManager *dirmanager,
               TrackList *tracks,
               ViewInfo &viewinfo,
               double rate);
   ~LabelDialog();

    virtual bool Show(bool show = true);

 private:

   bool TransferDataToWindow();
   bool TransferDataFromWindow();
   bool Validate();
   void FindAllLabels();
   void AddLabels(LabelTrack *t);
   wxString TrackName(int & index, wxString dflt = _("Label Track"));

   void OnUpdate(wxCommandEvent &event);
   void OnInsert(wxCommandEvent &event);
   void OnRemove(wxCommandEvent &event);
   void OnImport(wxCommandEvent &event);
   void OnExport(wxCommandEvent &event);
   void OnSelectCell(wxGridEvent &event);
   void OnCellChange(wxGridEvent &event);
   void OnChangeTrack(wxGridEvent &event, int row, RowData *rd);
   void OnChangeLabel(wxGridEvent &event, int row, RowData *rd);
   void OnChangeStime(wxGridEvent &event, int row, RowData *rd);
   void OnChangeEtime(wxGridEvent &event, int row, RowData *rd);
   void OnOK(wxCommandEvent &event);
   void OnCancel(wxCommandEvent &event);

 private:

   Grid *mGrid;
   ChoiceEditor *mChoiceEditor;
   TimeEditor *mTimeEditor;

   RowDataArray mData;
   
   DirManager *mDirManager;
   TrackList *mTracks;
   ViewInfo *mViewInfo;
   wxArrayString mTrackNames;
   wxString mFormat;
   double mRate;

   int mInitialRow;

   DECLARE_EVENT_TABLE();
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
// arch-tag: 94f72c32-970b-4f4e-bbf3-3880fce7b965
