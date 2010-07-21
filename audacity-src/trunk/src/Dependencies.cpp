/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2008 Audacity Team.
   License: GPL v2.  See License.txt.
  
   Dependencies.cpp

   Dominic Mazzoni
   Leland Lucius
   Markus Meyer
   LRN
   Michael Chinen
   Vaughan Johnson
  
   The primary function provided in this source file is
   ShowDependencyDialogIfNeeded.  It checks a project to see if
   any of its WaveTracks contain AliasBlockFiles; if so it
   presents a dialog to the user and lets them copy those block
   files into the project, making it self-contained.

**********************************************************************/

#include <wx/defs.h>
#include <wx/dialog.h>
#include <wx/filename.h>
#include <wx/hashmap.h>
#include <wx/progdlg.h>
#include <wx/choice.h>

#include "BlockFile.h"
#include "Dependencies.h"
#include "DirManager.h"
#include "Internat.h"
#include "Prefs.h"
#include "Project.h"
#include "ShuttleGui.h"
#include "Track.h"


class AliasedFile 
{
public:
   AliasedFile(wxFileName fileName, wxLongLong byteCount, bool bOriginalExists) 
   {
      mFileName = fileName;
      mByteCount = byteCount;
      mbOriginalExists = bOriginalExists;
   };
   wxFileName  mFileName;
   wxLongLong  mByteCount; // if stored as current default sample format
   bool        mbOriginalExists;
};

WX_DECLARE_OBJARRAY(AliasedFile, AliasedFileArray);

// Note, this #include must occur here, not up with the others!
// It must be between the WX_DECLARE_OBJARRAY and WX_DEFINE_OBJARRAY. 
#include <wx/arrimpl.cpp> 

WX_DEFINE_OBJARRAY( AliasedFileArray );

WX_DECLARE_HASH_MAP(wxString, AliasedFile *,
		               wxStringHash, wxStringEqual, AliasedFileHash);

WX_DECLARE_HASH_MAP(BlockFile *, BlockFile *,
		               wxPointerHash, wxPointerEqual, ReplacedBlockFileHash);

WX_DECLARE_HASH_MAP(BlockFile *, bool,
		               wxPointerHash, wxPointerEqual, BoolBlockFileHash);

// Given a project, returns a single array of all SeqBlocks
// in the current set of tracks.  Enumerating that array allows
// you to process all block files in the current set.
void GetAllSeqBlocks(AudacityProject *project,
                     BlockArray *outBlocks)
{
   TrackList *tracks = project->GetTracks();
   TrackListIterator iter(tracks);
   Track *t = iter.First();
   while (t) {
      if (t->GetKind() == Track::Wave) {
         WaveTrack *waveTrack = (WaveTrack *)t;
         WaveClipList::compatibility_iterator node = waveTrack->GetClipIterator();
         while(node) {
            WaveClip *clip = node->GetData();
            Sequence *sequence = clip->GetSequence();
            BlockArray *blocks = sequence->GetBlockArray();
            int i;
            for (i = 0; i < (int)blocks->GetCount(); i++)
               outBlocks->Add(blocks->Item(i));
            node = node->GetNext();
         }
      }
      t = iter.Next();
   }
}

// Given an Audacity project and a hash mapping aliased block
// files to un-aliased block files, walk through all of the
// tracks and replace each aliased block file with its replacement.
// Note that this code respects reference-counting and thus the
// process of making a project self-contained is actually undoable.
void ReplaceBlockFiles(AudacityProject *project,
		                 ReplacedBlockFileHash &hash)
{
   DirManager *dirManager = project->GetDirManager();
   BlockArray blocks;
   GetAllSeqBlocks(project, &blocks);

   int i;
   for (i = 0; i < (int)blocks.GetCount(); i++) {
      if (hash.count(blocks[i]->f) > 0) {
         BlockFile *src = blocks[i]->f;
         BlockFile *dst = hash[src];

         dirManager->Deref(src);
         dirManager->Ref(dst);

         blocks[i]->f = dst;
      }
   }
}

void FindDependencies(AudacityProject *project,
                      AliasedFileArray *outAliasedFiles)
{
   sampleFormat format = project->GetDefaultFormat();

   BlockArray blocks;
   GetAllSeqBlocks(project, &blocks);

   AliasedFileHash aliasedFileHash;
   BoolBlockFileHash blockFileHash;

   int i;
   for (i = 0; i < (int)blocks.GetCount(); i++) {
      BlockFile *f = blocks[i]->f;
      if (f->IsAlias() && (blockFileHash.count(f) == 0)) 
      {
         // f is an alias block we have not yet counted.
         blockFileHash[f] = true; // Don't count the same blockfile twice.
         AliasBlockFile *aliasBlockFile = (AliasBlockFile *)f;
         wxFileName fileName = aliasBlockFile->GetAliasedFile();
         wxString fileNameStr = fileName.GetFullPath();
         int blockBytes = (SAMPLE_SIZE(format) *
                           aliasBlockFile->GetLength());
         if (aliasedFileHash.count(fileNameStr) > 0)
            // Already put this AliasBlockFile in aliasedFileHash. 
            // Update block count.
            aliasedFileHash[fileNameStr]->mByteCount += blockBytes;
         else 
         {
            // Haven't counted this AliasBlockFile yet. 
            // Add to return array and internal hash.
            outAliasedFiles->Add(AliasedFile(fileName, 
                                             blockBytes, 
                                             fileName.FileExists()));
            aliasedFileHash[fileNameStr] =
               &((*outAliasedFiles)[outAliasedFiles->GetCount()-1]);
         }
      }
   } 
}

// Given a project and a list of aliased files that should no
// longer be external dependencies (selected by the user), replace
// all of those alias block files with disk block files.
void RemoveDependencies(AudacityProject *project,
			               AliasedFileArray *aliasedFiles)
{
   DirManager *dirManager = project->GetDirManager();

   ProgressDialog *progress = 
      new ProgressDialog(_("Removing Dependencies"),
                         _("Copying audio data into project..."));
   int updateResult = eProgressSuccess;

   // Hash aliasedFiles based on their full paths and 
   // count total number of bytes to process.
   AliasedFileHash aliasedFileHash;
   wxLongLong totalBytesToProcess = 0;
   unsigned int i;
   for (i = 0; i < aliasedFiles->GetCount(); i++) {
      totalBytesToProcess += aliasedFiles->Item(i).mByteCount;
      wxString fileNameStr = aliasedFiles->Item(i).mFileName.GetFullPath();
      aliasedFileHash[fileNameStr] = &aliasedFiles->Item(i);
   }
   
   BlockArray blocks;
   GetAllSeqBlocks(project, &blocks);

   const sampleFormat format = project->GetDefaultFormat();
   ReplacedBlockFileHash blockFileHash;   
   wxLongLong completedBytes = 0;
   for (i = 0; i < blocks.GetCount(); i++) {
      BlockFile *f = blocks[i]->f;
      if (f->IsAlias() && (blockFileHash.count(f) == 0)) 
      {
         // f is an alias block we have not yet processed.
         AliasBlockFile *aliasBlockFile = (AliasBlockFile *)f;
         wxFileName fileName = aliasBlockFile->GetAliasedFile();
         wxString fileNameStr = fileName.GetFullPath();

         if (aliasedFileHash.count(fileNameStr) == 0)
            // This aliased file was not selected to be replaced. Skip it.
            continue;

         // Convert it from an aliased file to an actual file in the project.
         unsigned int len = aliasBlockFile->GetLength();
         samplePtr buffer = NewSamples(len, format);
         f->ReadData(buffer, format, 0, len);
         BlockFile *newBlockFile =
            dirManager->NewSimpleBlockFile(buffer, len, format);
         DeleteSamples(buffer);

         // Update our hash so we know what block files we've done
         blockFileHash[f] = newBlockFile;

         // Update the progress bar
         completedBytes += SAMPLE_SIZE(format) * len;
         updateResult = progress->Update(completedBytes, totalBytesToProcess);
         if (updateResult != eProgressSuccess)
           break;
      }
   }

   // Above, we created a SimpleBlockFile contained in our project
   // to go with each AliasBlockFile that we wanted to migrate.
   // However, that didn't actually change any references to these
   // blockfiles in the Sequences, so we do that next...
   ReplaceBlockFiles(project, blockFileHash);

   // Subtract one from reference count of new block files; they're
   // now all referenced the proper number of times by the Sequences
   ReplacedBlockFileHash::iterator it;
   for( it = blockFileHash.begin(); it != blockFileHash.end(); ++it )
   {
      BlockFile *f = it->second;
      dirManager->Deref(f);
   }

   delete progress;
}

//
// DependencyDialog
//

class DependencyDialog : public wxDialog
{
public:
   DependencyDialog(wxWindow *parent,
                    wxWindowID id,
                    AudacityProject *project,
                    AliasedFileArray *aliasedFiles,
                    bool isSaving);

private:
   void PopulateList();
   void OnList(wxListEvent &evt);
   void OnCopySelectedFiles(wxCommandEvent &evt);
   void OnNo(wxCommandEvent &evt);
   void OnYes(wxCommandEvent &evt);

   void PopulateOrExchange(ShuttleGui & S);
   void SaveFutureActionChoice();
   
   virtual void OnCancel(wxCommandEvent& evt);
   
   AudacityProject  *mProject;
   AliasedFileArray *mAliasedFiles;
   bool              mIsSaving;

   wxListCtrl       *mFileList;
   wxButton         *mCopySelectedFilesButton;
   wxChoice         *mFutureActionChoice;
   
public:
   DECLARE_EVENT_TABLE()
};

enum {
   FileListID = 6000,
   CopySelectedFilesButtonID,
   FutureActionChoiceID
};

BEGIN_EVENT_TABLE(DependencyDialog, wxDialog)
   EVT_LIST_ITEM_SELECTED(FileListID, DependencyDialog::OnList)
   EVT_LIST_ITEM_DESELECTED(FileListID, DependencyDialog::OnList)
   EVT_BUTTON(CopySelectedFilesButtonID, DependencyDialog::OnCopySelectedFiles)
   EVT_BUTTON(wxID_NO, DependencyDialog::OnNo)
   EVT_BUTTON(wxID_YES, DependencyDialog::OnYes)
   EVT_BUTTON(wxID_CANCEL, DependencyDialog::OnCancel) // seems to be needed
END_EVENT_TABLE()

DependencyDialog::DependencyDialog(wxWindow *parent,
                                   wxWindowID id,
                                   AudacityProject *project,
                                   AliasedFileArray *aliasedFiles,
                                   bool isSaving):
   wxDialog(parent, id, _("Project depends on other audio files"),
            wxDefaultPosition, wxDefaultSize,
            isSaving ? wxDEFAULT_DIALOG_STYLE & (~wxCLOSE_BOX) :
                       wxDEFAULT_DIALOG_STYLE), // no close box when saving
   mProject(project),
   mAliasedFiles(aliasedFiles),
   mIsSaving(isSaving),
   mFileList(NULL),
   mCopySelectedFilesButton(NULL), 
   mFutureActionChoice(NULL)
{
   ShuttleGui S(this, eIsCreating);
   PopulateOrExchange(S);
}

wxString kDependencyDialogMessage =
_("Copying these files into your project will remove this dependency.\
\nThis needs more disk space, but is safer.\
\n\nFiles shown in italics have been moved or deleted and cannot be copied.\
\nRestore them to their original location to be able to copy into project."); 

void DependencyDialog::PopulateOrExchange(ShuttleGui& S)
{
   S.SetBorder(5);
   S.StartVerticalLay();
   {
      S.AddVariableText(kDependencyDialogMessage, false);

      S.StartStatic(_("Project Dependencies"));
      {  
         mFileList = S.Id(FileListID).AddListControlReportMode();
         mFileList->InsertColumn(0, _("Audio file"));
         mFileList->SetColumnWidth(0, 220);
         mFileList->InsertColumn(1, _("Disk space"));
         mFileList->SetColumnWidth(1, 120);
         PopulateList();

         mCopySelectedFilesButton = 
            S.Id(CopySelectedFilesButtonID).AddButton(
               _("Copy Selected Audio Into Project"), 
               wxALIGN_LEFT);
      }
      S.EndStatic();

      S.StartHorizontalLay(wxALIGN_CENTRE);
      {
         if (mIsSaving) {
            S.Id(wxID_CANCEL).AddButton(_("Cancel Save"));
         }
         S.Id(wxID_NO).AddButton(_("Do Not Copy Any Audio"));

         S.Id(wxID_YES).AddButton(_("Copy All Audio into Project (Safer)"));
      }
      S.EndHorizontalLay();
      
      if (mIsSaving)
      {
         S.StartHorizontalLay(wxALIGN_LEFT);
         {
            wxArrayString choices;
            choices.Add(_("Ask me"));
            choices.Add(_("Always copy all audio (safest)"));
            choices.Add(_("Never copy any audio"));
            mFutureActionChoice = S.Id(FutureActionChoiceID).AddChoice(
               _("Whenever a project depends on other files:"),
               _("Ask me"), &choices);
         }
         S.EndHorizontalLay();
      } else
      {
         mFutureActionChoice = NULL;
      }
   }
   S.EndVerticalLay();
   Layout();
   Fit();
   SetMinSize(GetSize());
   Center();
}

void DependencyDialog::PopulateList()
{
   mFileList->DeleteAllItems();

   unsigned int i;
   for (i = 0; i < mAliasedFiles->GetCount(); i++) {
      wxFileName fileName = mAliasedFiles->Item(i).mFileName;
      wxLongLong byteCount = (mAliasedFiles->Item(i).mByteCount * 124) / 100;
      bool bOriginalExists = mAliasedFiles->Item(i).mbOriginalExists;

      mFileList->InsertItem(i, fileName.GetFullPath());
      mFileList->SetItem(i, 1, Internat::FormatSize(byteCount));
      mFileList->SetItemData(i, long(bOriginalExists));
      if (bOriginalExists)
         mFileList->SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
      else 
      {
         mFileList->SetItemState(i, 0, wxLIST_STATE_SELECTED); // Deselect.

         //mFileList->SetItemBackgroundColour(i, *wxRED);
         //mFileList->SetItemTextColour(i, *wxWHITE);
         mFileList->SetItemTextColour(i, *wxRED);

         mFileList->SetItemFont(i, *wxITALIC_FONT);
      }
   }
}

void DependencyDialog::OnList(wxListEvent &evt)
{
   if (!mCopySelectedFilesButton || !mFileList)
      return;

   wxString itemStr = evt.GetText();
   if (evt.GetData() == 0)
      // This list item is one of mAliasedFiles for which 
      // the original is missing, i.e., moved or deleted.
      // wxListCtrl does not provide for items that are not 
      // allowed to be selected, so always deselect these items. 
      mFileList->SetItemState(evt.GetIndex(), 0, wxLIST_STATE_SELECTED); // Deselect.

   int selectedCount = mFileList->GetSelectedItemCount();
   mCopySelectedFilesButton->Enable(selectedCount > 0);
}

void DependencyDialog::OnNo(wxCommandEvent &evt)
{
   SaveFutureActionChoice();
   EndModal(wxID_NO);
}

void DependencyDialog::OnYes(wxCommandEvent &evt)
{
   SaveFutureActionChoice();
   EndModal(wxID_YES);
}

void DependencyDialog::OnCopySelectedFiles(wxCommandEvent &evt)
{
   AliasedFileArray aliasedFilesToDelete;

   int i;
   // Count backwards so we can remove as we go
   for(i=(int)mAliasedFiles->GetCount()-1; i>=0; i--) {
      if (mFileList->GetItemState(i, wxLIST_STATE_SELECTED)) {
         aliasedFilesToDelete.Add(mAliasedFiles->Item(i));
         mAliasedFiles->RemoveAt(i);
      }
   }  

   RemoveDependencies(mProject, &aliasedFilesToDelete);
   PopulateList();

   if (mAliasedFiles->GetCount() == 0) {
      SaveFutureActionChoice();
      EndModal(wxID_NO);  // Don't need to remove dependencies
   }
}

void DependencyDialog::OnCancel(wxCommandEvent& evt)
{
   if (mIsSaving)
   {
      int ret = wxMessageBox(
         _("If you proceed, your project will not be saved to disk. Is this what you want?"),
         _("Cancel Save"), wxICON_QUESTION | wxYES_NO | wxNO_DEFAULT, this);
      if (ret != wxYES)
         return;
   }
   
   EndModal(wxID_CANCEL);
}

void DependencyDialog::SaveFutureActionChoice()
{
   if (mFutureActionChoice)
   {
      wxString savePref;   
      int sel = mFutureActionChoice->GetSelection();
      switch (sel)
      {
      case 1: savePref = wxT("copy"); break;
      case 2: savePref = wxT("never"); break;
      default: savePref = wxT("ask");
      }
      gPrefs->Write(wxT("/FileFormats/SaveProjectWithDependencies"),
                    savePref);
   }
}

// Checks for alias block files, modifies the project if the
// user requests it, and returns true if the user continues.
// Returns false only if the user clicks Cancel.
bool ShowDependencyDialogIfNeeded(AudacityProject *project,
                                  bool isSaving)
{
   AliasedFileArray aliasedFiles;
   FindDependencies(project, &aliasedFiles);

   if (aliasedFiles.GetCount() == 0) {
      if (!isSaving) 
         wxMessageBox(
_("Your project is currently self-contained; it does not depend on any external audio files. \
\n\nIf you Undo back to a state where it has external dependencies on imported files, it will no \
longer be self-contained. If you then Save without copying those files in, you may lose data."),
                      _("Dependency check"),
                      wxOK | wxICON_INFORMATION,
                      project);
      return true; // Nothing to do.
   }
   
   if (isSaving)
   {
      wxString action = 
         gPrefs->Read(
            wxT("/FileFormats/SaveProjectWithDependencies"), 
            wxT("ask"));
      if (action == wxT("copy"))
      {
         // User always wants to remove dependencies
         RemoveDependencies(project, &aliasedFiles);
         return true;
      }
      if (action == wxT("never"))
         // User never wants to remove dependencies
         return true;
   }

   DependencyDialog dlog(project, -1, project, &aliasedFiles, isSaving);
   int returnCode = dlog.ShowModal();
   if (returnCode == wxID_CANCEL)
      return false;
   else if (returnCode == wxID_YES) 
      RemoveDependencies(project, &aliasedFiles);

   return true;
}

