/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2008 Audacity Team.
   License: GPL v2.  See License.txt.

   DirManager.cpp

   Dominic Mazzoni
   Matt Brubeck
   Michael Chinen
   James Crook
   Al Dimond
   Brian Gunlogson
   Josh Haberman
   Vaughan Johnson
   Leland Lucius
   Monty
   Markus Meyer

*******************************************************************//*!

\class DirManager
\brief Creates and manages BlockFile objects.

  This class manages the files that a project uses to store most
  of its data.  It creates new BlockFile objects, which can
  be used to store any type of data.  BlockFiles support all of
  the common file operations, but they also support reference
  counting, so two different parts of a project can point to
  the same block of data.

  For example, a track might contain 10 blocks of data representing
  its audio.  If you copy the last 5 blocks and paste at the
  end of the file, no new blocks need to be created - we just store
  pointers to new ones.  When part of a track is deleted, the
  affected blocks decrement their reference counts, and when they
  reach zero they are deleted.  This same mechanism is also used
  to implement Undo.

  The DirManager, besides mapping filenames to absolute paths,
  also hashes all of the block names used in a project, so that
  when reading a project from disk, multiple copies of the
  same block still get mapped to the same BlockFile object.

*//*******************************************************************/


#include "Audacity.h"

#include <wx/defs.h>
#include <wx/app.h>
#include <wx/dir.h>
#include <wx/log.h>
#include <wx/filefn.h>
#include <wx/hash.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/timer.h>
#include <wx/intl.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/object.h>

// chmod
#ifdef __UNIX__
#include <sys/types.h>
#include <sys/stat.h>
#endif

// seed random number generator
#include <time.h>

#include "AudacityApp.h"
#include "BlockFile.h"
#include "blockfile/LegacyBlockFile.h"
#include "blockfile/LegacyAliasBlockFile.h"
#include "blockfile/SimpleBlockFile.h"
#include "blockfile/SilentBlockFile.h"
#include "blockfile/PCMAliasBlockFile.h"
#include "blockfile/ODPCMAliasBlockFile.h"
#include "blockfile/ODDecodeBlockFile.h"
#include "DirManager.h"
#include "Internat.h"
#include "Project.h"
#include "Prefs.h"
#include "widgets/Warning.h"
#include "widgets/MultiDialog.h"

#include "prefs/PrefsDialog.h"
#include "ondemand/ODManager.h"

#if defined(__WXMAC__)
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#endif

// Static class variables

int DirManager::numDirManagers = 0;
bool DirManager::dontDeleteTempFiles = false;

wxString DirManager::globaltemp;

// Methods

DirManager::DirManager()
{
   wxLogDebug(wxT("DirManager: Created new instance"));

   mRef = 1; // MM: Initial refcount is 1 by convention
   
   // this need not be strictly uniform or random, but it should give
   // unclustered numbers
   srand(time(0));

   // Set up local temp subdir
   // Previously, Audacity just named project temp directories "project0",
   // "project1" and so on. But with the advent of recovery code, we need a 
   // unique name even after a crash. So we create a random project index
   // and make sure it is not used already. This will not pose any performance
   // penalties as long as the number of open Audacity projects is much
   // lower than RAND_MAX.
   do {
      mytemp = globaltemp + wxFILE_SEP_PATH +
               wxString::Format(wxT("project%d"), rand());
   } while (wxDirExists(mytemp));
   
   numDirManagers++;

   projPath = wxT("");
   projName = wxT("");

   mLoadingTarget = NULL;
   mMaxSamples = -1;

   // toplevel pool hash is fully populated to begin 
   {
      int i;
      for(i=0; i< 256; i++) dirTopPool[i]=0;
   }

   //create a temporary null log to capture the log dialog
   //that wxGetDiskSpace creates.  
   //JKC: Please explain why.
   // Vaughan, 2010-07-06: No explanation forthcoming and Nestify is long gone.
   //    wxWidgets manual says, "rarely the best way to" suppress wxWidgets log messages, so don't do it.
   // wxLogNull logNo; 

   // Make sure there is plenty of space for temp files
   wxLongLong freeSpace = 0;
   if (wxGetDiskSpace(globaltemp, NULL, &freeSpace)) {
      if (freeSpace < wxLongLong(wxLL(100 * 1048576))) {
         ShowWarningDialog(NULL, wxT("DiskSpaceWarning"),
                           _("There is very little free disk space left on this volume.\nPlease select another temporary directory in Preferences."));
      }
   }
}

DirManager::~DirManager()
{
   wxASSERT(mRef == 0); // MM: Otherwise, we shouldn't delete it

   numDirManagers--;
   if (numDirManagers == 0) {
      CleanTempDir();
      //::wxRmdir(temp);
   }
}

// Behavior of RecursivelyEnumerate is tailored to our uses and not
// entirely straightforward.  It recurs depth-first from the passed-
// in directory into its subdirs according to optional dirspec
// pattern, building a list of directories and (optionally) files 
// in the listed order.  The dirspec is not applied to
// subdirs of subdirs. Files in the passed-in directory will not be
// enumerated.  Also, the passed-in directory is the last entry added
// to the list.
static int RecursivelyEnumerate(wxString dirPath, 
                                  wxArrayString& flist, 
                                  wxString dirspec,
                                  bool bFiles, bool bDirs,
                                  int progress_count = 0, 
                                  int progress_bias = 0,
                                  ProgressDialog* progress = NULL)
{
   int count=0;
   bool cont;

   wxDir dir(dirPath);
   if(dir.IsOpened()){
      wxString name;

      if (bFiles){
         cont= dir.GetFirst(&name, dirspec, wxDIR_FILES);
         while ( cont ){
            wxString filepath = dirPath + wxFILE_SEP_PATH + name;
            
            count++;
            flist.Add(filepath);
            
            cont = dir.GetNext(&name);
            
            if (progress)
               progress->Update(count + progress_bias,
                                progress_count);
         }
      }

      cont= dir.GetFirst(&name, dirspec, wxDIR_DIRS);
      while ( cont ){
         wxString subdirPath = dirPath + wxFILE_SEP_PATH + name;
         count += RecursivelyEnumerate(
                     subdirPath, flist, wxEmptyString,
                     bFiles, bDirs, 
                     progress_count, count + progress_bias, 
                     progress);  
         cont = dir.GetNext(&name);
      }
   }
   
   if (bDirs) {
      flist.Add(dirPath);
      count++;
   }

   return count;
}


static int RecursivelyEnumerateWithProgress(wxString dirPath,
                                             wxArrayString &flist, 
                                             wxString dirspec,
                                             bool bFiles, bool bDirs,
                                             int progress_count,
                                             const wxChar* message)
{
   ProgressDialog *progress = NULL;

   if (message)
      progress = new ProgressDialog(_("Progress"), message);

   int count = RecursivelyEnumerate(
                  dirPath, flist, dirspec, 
                  bFiles, bDirs,
                  progress_count, 0,
                  progress);

   if (progress)
      delete progress;

   return count;
}

static int RecursivelyCountSubdirs(wxString dirPath)
{
   bool bContinue;
   int nCount = 0;
   wxDir dir(dirPath);
   if (dir.IsOpened() && dir.HasSubDirs()) 
   {
      wxString name;
      bContinue = dir.GetFirst(&name, wxEmptyString, wxDIR_DIRS);
      while (bContinue) 
      {
         nCount++;
         wxString subdirPath = dirPath + wxFILE_SEP_PATH + name;
         nCount += RecursivelyCountSubdirs(subdirPath);  
         bContinue = dir.GetNext(&name);
      }
   }
   return nCount;
}

static int RecursivelyRemoveEmptyDirs(wxString dirPath, 
                                       int nDirCount = 0, 
                                       ProgressDialog* pProgress = NULL)
{
   bool bContinue;
   wxDir dir(dirPath);
   int nCount = 0;
   if (dir.IsOpened()) 
   {
      if (dir.HasSubDirs()) 
      {
         wxString name;
         bContinue = dir.GetFirst(&name, wxEmptyString, wxDIR_DIRS);
         while (bContinue) 
         {
            wxString subdirPath = dirPath + wxFILE_SEP_PATH + name;
            nCount += RecursivelyRemoveEmptyDirs(subdirPath, nDirCount, pProgress);  
            bContinue = dir.GetNext(&name);
         }
      }
      // Have to recheck dir.HasSubDirs() again, in case they all were deleted in recursive calls.
      if (!dir.HasSubDirs() && !dir.HasFiles())
      {
         // No subdirs or files. It's empty so delete it. 
         // Vaughan, 2010-07-07: 
         // Note that, per http://src.chromium.org/svn/trunk/src/base/file_util_win.cc, among others, 
         // "Some versions of Windows return ERROR_FILE_NOT_FOUND (0x2) when deleting
         // an empty directory..." Supposedly fixed in Vista and up.
         // I discovered this on WinXP. I tried several other Windows SDK functions (e.g., _rmdir 
         // and RemoveDirectory), and they all give same results. 
         // I noticed dirs get deleted in RecursivelyRemove, maybe because it doesn't 
         // consider whether the path is a directory or a file and wxRemoveFile()'s it first.
         // Tried it here on WinXP, but no joy. Leave the code in case it works on other Win systems. 
         #ifdef __WXMSW__
            ::wxRemoveFile(dirPath);
         #endif
         ::wxRmdir(dirPath);
      }
      nCount++; // Count dirPath in progress.
      if (pProgress)
         pProgress->Update(nCount, nDirCount);
   }
   return nCount;
}

static void RecursivelyRemove(wxArrayString &fList, int count, 
                              bool bFiles, bool bDirs,
                              const wxChar* message = NULL)
{
   ProgressDialog *progress = NULL;

   if (message)
      progress = new ProgressDialog(_("Progress"), message);

   for (int i = 0; i < count; i++) {
      const wxChar *file = fList[i].c_str();
      if (bFiles)
         ::wxRemoveFile(file);
      if (bDirs)
         ::wxRmdir(file); // See note above about wxRmdir sometimes incorrectly failing on Windows.
      if (progress)
         progress->Update(i, count);
   }
   
   if (progress)
      delete progress;
}

// static
void DirManager::CleanTempDir()
{
   if (dontDeleteTempFiles)
      return; // do nothing
      
   wxArrayString flist;

   // Subtract 1 because we don't want to delete the global temp directory, 
   // which this will find and list last.
   int count = 
      RecursivelyEnumerate(globaltemp, flist, wxT("project*"), true, true) - 1;
   if (count == 0) 
      return;

   RecursivelyRemove(flist, count, true, true, _("Cleaning up temporary files"));
}

bool DirManager::SetProject(wxString & projPath, wxString & projName,
                            bool create)
{
   wxString oldPath = this->projPath;
   wxString oldName = this->projName;
   wxString oldFull = projFull;
   wxString oldLoc = projFull;
   if (oldLoc == wxT(""))
      oldLoc = mytemp;

   if (projPath == wxT(""))
      projPath = ::wxGetCwd();

   this->projPath = projPath;
   this->projName = projName;
   this->projFull = projPath + wxFILE_SEP_PATH + projName;

   wxString cleanupLoc1=oldLoc;
   wxString cleanupLoc2=projFull;

   if (create) {
      if (!wxDirExists(projFull))
         if (!wxMkdir(projFull))
            return false;

      #ifdef __UNIX__
      chmod(OSFILENAME(projFull), 0775);
      #endif

      #ifdef __WXMAC__
      chmod(OSFILENAME(projFull), 0775);
      #endif

   } else {
      #ifndef __WXMAC__
      if (!wxDirExists(projFull))
         return false;
      #endif
   }

   /* Move all files into this new directory.  Files which are
      "locked" get copied instead of moved.  (This happens when
      we perform a Save As - the files which belonged to the last
      saved version of the old project must not be moved,
      otherwise the old project would not be safe.) */

   ProgressDialog *progress = new ProgressDialog(_("Progress"),
                                                 _("Saving project data files"));

   int total = mBlockFileHash.size();
   int count=0;

   BlockHash::iterator i = mBlockFileHash.begin();
   bool success = true;
   while(i != mBlockFileHash.end() && success) {
      BlockFile *b = i->second;
      
      if (b->IsLocked())
         success = CopyToNewProjectDirectory(b);
      else{
         success = MoveToNewProjectDirectory(b);
      }

      progress->Update(count, total);

      i++;
      count++;
   }

   if (!success) {
      // If the move failed, we try to move/copy as many files
      // back as possible so that no damage was done.  (No sense
      // in checking for errors this time around - there's nothing
      // that could be done about it.)
      // Likely causes: directory was not writeable, disk was full

      projFull = oldLoc;

      BlockHash::iterator i = mBlockFileHash.begin();
      while(i != mBlockFileHash.end()) {
         BlockFile *b = i->second;
         MoveToNewProjectDirectory(b);

         if (count>=0)
            progress->Update(count, total);

         i++;
         count--;
      }

      this->projFull = oldFull;
      this->projPath = oldPath;
      this->projName = oldName;

      delete progress;

      return false;
   }

   delete progress;

   // Some subtlety; SetProject is used both to move a temp project
   // into a permanent home as well as just set up path variables when
   // loading a project; in this latter case, the movement code does
   // nothing because SetProject is called before there are any
   // blockfiles.  Cleanup code trigger is the same
   if (mBlockFileHash.size()>0){
      // Clean up after ourselves; look for empty directories in the old
      // and new project directories.  The easiest way to do this is to
      // recurse depth-first and rmdir every directory seen in old and
      // new; rmdir will fail on non-empty dirs.
      
      wxArrayString dirlist;
      count = RecursivelyEnumerate(cleanupLoc1, dirlist, wxEmptyString, false, true);

      //This destroys the empty dirs of the OD block files, which are yet to come. 
      //Dont know if this will make the project dirty, but I doubt it. (mchinen)
      //      count += RecursivelyEnumerate(cleanupLoc2, dirlist, wxEmptyString, false, true);
      
      if (count > 0)
         RecursivelyRemove(dirlist, count, false, true, _("Cleaning up cache directories"));
   }
   return true;
}

wxString DirManager::GetProjectDataDir()
{
   return projFull;
}
  
wxString DirManager::GetProjectName()
{
   return projName;
}

wxLongLong DirManager::GetFreeDiskSpace()
{
   wxLongLong freeSpace = -1;
   wxString path = projPath;

   if (projPath == wxT(""))
      path = mytemp;
   {
      // wxLogNull logNo; // See above note on why this is commented out.
      if (!wxGetDiskSpace(path, NULL, &freeSpace))
         freeSpace = -1;
   }
   return freeSpace;
}

wxString DirManager::GetDataFilesDir() const
{
   return projFull != wxT("")? projFull: mytemp;
}

wxFileName DirManager::MakeBlockFilePath(wxString value){
   
   wxFileName dir;
   dir.AssignDir(GetDataFilesDir());
   
   if(value.GetChar(0)==wxT('d')){
      // this file is located in a subdiretory tree 
      int location=value.Find(wxT('b'));
      wxString subdir=value.Mid(0,location);
      dir.AppendDir(subdir);
      
      if(!dir.DirExists())dir.Mkdir();
   }
   
   if(value.GetChar(0)==wxT('e')){
      // this file is located in a new style two-deep subdirectory tree 
      wxString topdir=value.Mid(0,3);
      wxString middir=wxT("d");
      middir.Append(value.Mid(3,2));

      dir.AppendDir(topdir);      
      dir.AppendDir(middir);

      if(!dir.DirExists()) 
      {
         if(!dir.Mkdir(0777,wxPATH_MKDIR_FULL))
         {
            printf("mkdir in dirman failed\n");
         }
      }
      
   }
   return dir;
}

bool DirManager::AssignFile(wxFileName &fileName,
			    wxString value,
                            bool diskcheck){
   wxFileName dir=MakeBlockFilePath(value);

   if(diskcheck){
      // verify that there's no possible collision on disk.  If there
      // is, log the problem and return FALSE so that MakeBlockFileName 
      // can try again

      wxDir checkit(dir.GetFullPath());
      if(!checkit.IsOpened()) return FALSE;
      
      // this code is only valid if 'value' has no extention; that
      // means, effectively, AssignFile may be called with 'diskcheck'
      // set to true only if called from MakeFileBlockName().
      
      wxString filespec;
      filespec.Printf(wxT("%s.*"),value.c_str());
      if(checkit.HasFiles(filespec)){
         // collision with on-disk state!
         wxString collision;
         checkit.GetFirst(&collision,filespec);
         
         wxLogWarning(_("Audacity found an orphaned blockfile %s! \nPlease consider saving and reloading the project to perform a complete project check.\n"),
                      collision.c_str());
         
         return FALSE;
      }
   }
   fileName.Assign(dir.GetFullPath(),value);
   return TRUE;
}

static inline unsigned int hexchar_to_int(unsigned int x)
{
   if(x<48U)return 0;
   if(x<58U)return x-48U;
   if(x<65U)return 10U;
   if(x<71U)return x-55U;
   if(x<97U)return 10U;
   if(x<103U)return x-87U;
   return 15U;
}

int DirManager::BalanceMidAdd(int topnum, int midkey)
{
   // enter the midlevel directory if it doesn't exist

   if(dirMidPool.find(midkey) == dirMidPool.end() &&
         dirMidFull.find(midkey) == dirMidFull.end()){
      dirMidPool[midkey]=0;

      // increment toplevel directory fill
      dirTopPool[topnum]++;
      if(dirTopPool[topnum]>=256){
         // this toplevel is now full; move it to the full hash
         dirTopPool.erase(topnum);
         dirTopFull[topnum]=256;
      }
      return 1;
   }
   return 0;
}

void DirManager::BalanceFileAdd(int midkey)
{
   // increment the midlevel directory usage information
   if(dirMidPool.find(midkey) != dirMidPool.end()){
      dirMidPool[midkey]++;
      if(dirMidPool[midkey]>=256){
         // this middir is now full; move it to the full hash
         dirMidPool.erase(midkey);
         dirMidFull[midkey]=256;
      }
   }else{
      // this case only triggers in absurdly large projects; we still
      // need to track directory fill even if we're over 256/256/256
      dirMidPool[midkey]++;
   }
}

void DirManager::BalanceInfoAdd(wxString file)
{
   const wxChar *s=file.c_str();
   if(s[0]==wxT('e')){
      // this is one of the modern two-deep managed files 
      // convert filename to keys 
      unsigned int topnum = (hexchar_to_int(s[1]) << 4) | 
         hexchar_to_int(s[2]);
      unsigned int midnum = (hexchar_to_int(s[3]) << 4) | 
         hexchar_to_int(s[4]);
      unsigned int midkey=topnum<<8|midnum;

      BalanceMidAdd(topnum,midkey);
      BalanceFileAdd(midkey);
   }
}

// Note that this will try to clean up directories out from under even
// locked blockfiles; this is actually harmless as the rmdir will fail
// on non-empty directories.
void DirManager::BalanceInfoDel(wxString file)
{
   const wxChar *s=file.c_str();
   if(s[0]==wxT('e')){
      // this is one of the modern two-deep managed files 

      unsigned int topnum = (hexchar_to_int(s[1]) << 4) | 
         hexchar_to_int(s[2]);
      unsigned int midnum = (hexchar_to_int(s[3]) << 4) | 
         hexchar_to_int(s[4]);
      unsigned int midkey=topnum<<8|midnum;

      // look for midkey in the mid pool
      if(dirMidFull.find(midkey) != dirMidFull.end()){
         // in the full pool

         if(--dirMidFull[midkey]<256){
            // move out of full into available
            dirMidPool[midkey]=dirMidFull[midkey];
            dirMidFull.erase(midkey);
         }
      }else{
         if(--dirMidPool[midkey]<1){
            // erasing the key here is OK; we have provision to add it
            // back if its needed (unlike the dirTopPool hash)
            dirMidPool.erase(midkey);

            // delete the actual directory
            wxString dir=(projFull != wxT("")? projFull: mytemp);
            dir += wxFILE_SEP_PATH;
            dir += file.Mid(0,3);
            dir += wxFILE_SEP_PATH;
            dir += wxT("d");
            dir += file.Mid(3,2);
            wxFileName::Rmdir(dir);

            // also need to remove from toplevel
            if(dirTopFull.find(topnum) != dirTopFull.end()){
               // in the full pool
               if(--dirTopFull[topnum]<256){
                  // move out of full into available
                  dirTopPool[topnum]=dirTopFull[topnum];
                  dirTopFull.erase(topnum);
               }
            }else{
               if(--dirTopPool[topnum]<1){
                  // do *not* erase the hash entry from dirTopPool
                  // *do* delete the actual directory
                  wxString dir=(projFull != wxT("")? projFull: mytemp);
                  dir += wxFILE_SEP_PATH;
                  dir += file.Mid(0,3);
                  wxFileName::Rmdir(dir);
               }
            }
         }
      }
   }
}

// only determines appropriate filename and subdir balance; does not
// perform maintainence
wxFileName DirManager::MakeBlockFileName()
{
   wxFileName ret;
   wxString baseFileName;

   unsigned int filenum,midnum,topnum,midkey;

   while(1){

      /* blockfiles are divided up into heirarchical directories.
         Each toplevel directory is represented by "e" + two unique
         hexadecimal digits, for a total possible number of 256
         toplevels.  Each toplevel contains up to 256 subdirs named
         "d" + two hex digits.  Each subdir contains 'a number' of
         files.  */

      filenum=0;
      midnum=0;
      topnum=0;

      // first action: if there is no available two-level directory in
      // the available pool, try to make one

      if(dirMidPool.empty()){
         
         // is there a toplevel directory with space for a new subdir?

         if(!dirTopPool.empty()){

            // there's still a toplevel with room for a subdir

            DirHash::iterator i = dirTopPool.begin();
            int newcount        = 0;
            topnum              = i->first;
            

            // search for unused midlevels; linear search adequate
            // add 32 new topnum/midnum dirs full of  prospective filenames to midpool
            for(midnum=0;midnum<256;midnum++){
               midkey=(topnum<<8)+midnum;
               if(BalanceMidAdd(topnum,midkey)){
                  newcount++;
                  if(newcount>=32)break;
               }
            }

            if(dirMidPool.empty()){
               // all the midlevels in this toplevel are in use yet the
               // toplevel claims some are free; this implies multiple
               // internal logic faults, but simply giving up and going
               // into an infinite loop isn't acceptible.  Just in case,
               // for some reason, we get here, dynamite this toplevel so
               // we don't just fail.
               
               // this is 'wrong', but the best we can do given that
               // something else is also wrong.  It will contain the
               // problem so we can keep going without worry.
               dirTopPool.erase(topnum);
               dirTopFull[topnum]=256;
            }
            continue;
         }
      }

      if(dirMidPool.empty()){
         // still empty, thus an absurdly large project; all dirs are
         // full to 256/256/256; keep working, but fall back to 'big
         // filenames' and randomized placement

         filenum = rand();
         midnum  = (int)(256.*rand()/(RAND_MAX+1.));
         topnum  = (int)(256.*rand()/(RAND_MAX+1.));
         midkey=(topnum<<8)+midnum;

            
      }else{
         
         DirHash::iterator i = dirMidPool.begin();
         midkey              = i->first;

         // split the retrieved 16 bit directory key into two 8 bit numbers
         topnum = midkey >> 8;
         midnum = midkey & 0xff;
         filenum = (int)(4096.*rand()/(RAND_MAX+1.));

      }

      baseFileName.Printf(wxT("e%02x%02x%03x"),topnum,midnum,filenum);

      if (mBlockFileHash.find(baseFileName) == mBlockFileHash.end()){
         // not in the hash, good. 
         if(AssignFile(ret,baseFileName,TRUE)==FALSE){
            
            // this indicates an on-disk collision, likely due to an
            // orphaned blockfile.  We should try again, but first
            // alert the balancing info there's a phantom file here;
            // if the directory is nearly full of orphans we neither
            // want performance to suffer nor potentially get into an
            // infinite loop if all possible filenames are taken by
            // orphans (unlikely but possible)
            BalanceFileAdd(midkey);
 
         }else break;
      }
   }
   // FIX-ME: Might we get here without midkey having been set?
   BalanceFileAdd(midkey);
   return ret;
}

BlockFile *DirManager::NewSimpleBlockFile(
                                 samplePtr sampleData, sampleCount sampleLen,
                                 sampleFormat format,
                                 bool allowDeferredWrite)
{
   wxFileName fileName = MakeBlockFileName();

   BlockFile *newBlockFile =
       new SimpleBlockFile(fileName, sampleData, sampleLen, format,
                           allowDeferredWrite);

   mBlockFileHash[fileName.GetName()]=newBlockFile;

   return newBlockFile;
}

BlockFile *DirManager::NewAliasBlockFile(
                                 wxString aliasedFile, sampleCount aliasStart,
                                 sampleCount aliasLen, int aliasChannel)
{
   wxFileName fileName = MakeBlockFileName();

   BlockFile *newBlockFile =
       new PCMAliasBlockFile(fileName,
                             aliasedFile, aliasStart, aliasLen, aliasChannel);

   mBlockFileHash[fileName.GetName()]=newBlockFile;
   aliasList.Add(aliasedFile);

   return newBlockFile;
}

BlockFile *DirManager::NewODAliasBlockFile(
                                 wxString aliasedFile, sampleCount aliasStart,
                                 sampleCount aliasLen, int aliasChannel)
{
   wxFileName fileName = MakeBlockFileName();

   BlockFile *newBlockFile =
       new ODPCMAliasBlockFile(fileName,
                             aliasedFile, aliasStart, aliasLen, aliasChannel);

   mBlockFileHash[fileName.GetName()]=newBlockFile;
   aliasList.Add(aliasedFile);

   return newBlockFile;
}

BlockFile *DirManager::NewODDecodeBlockFile(
                                 wxString aliasedFile, sampleCount aliasStart,
                                 sampleCount aliasLen, int aliasChannel, int decodeType)
{
   wxFileName fileName = MakeBlockFileName();

   BlockFile *newBlockFile =
       new ODDecodeBlockFile(fileName,
                             aliasedFile, aliasStart, aliasLen, aliasChannel, decodeType);

   mBlockFileHash[fileName.GetName()]=newBlockFile;
   aliasList.Add(aliasedFile); //OD TODO: check to see if we need to remove this when done decoding.
                               //I don't immediately see a place where alias files remove when a file is closed.

   return newBlockFile;
}

// Adds one to the reference count of the block file,
// UNLESS it is "locked", then it makes a new copy of
// the BlockFile.
BlockFile *DirManager::CopyBlockFile(BlockFile *b)
{
   if (!b->IsLocked()) {
      b->Ref();
		//mchinen:July 13 2009 - not sure about this, but it needs to be added to the hash to be able to save if not locked.
		//note that this shouldn't hurt mBlockFileHash's that already contain the filename, since it should just overwrite.
		//but it's something to watch out for.
      //
      // LLL: Except for silent block files which have no filename.
      if (!b->GetFileName().GetName().IsEmpty()) {
         mBlockFileHash[b->GetFileName().GetName()]=b;
      }
      return b;
   }

   // Copy the blockfile
   BlockFile *b2;
   if (b->GetFileName().GetName().IsEmpty()) {
      // Block files with no filename (i.e. SilentBlockFile) just need an
      // in-memory copy
      b2 = b->Copy(wxFileName());
   }
   else
   {
      wxFileName newFile = MakeBlockFileName();

      // We assume that the new file should have the same extension
      // as the existing file
      newFile.SetExt(b->GetFileName().GetExt());

      //some block files such as ODPCMAliasBlockFIle don't always have
      //a summary file, so we should check before we copy.
      if(b->IsSummaryAvailable())
      {
         if( !wxCopyFile(b->GetFileName().GetFullPath(),
                  newFile.GetFullPath()) )
            return NULL;
      }

      b2 = b->Copy(newFile);

      if (b2 == NULL)
         return NULL;

      mBlockFileHash[newFile.GetName()]=b2;
      aliasList.Add(newFile.GetFullPath());
   }

   return b2;
}

bool DirManager::HandleXMLTag(const wxChar *tag, const wxChar **attrs)
{
   if( mLoadingTarget == NULL )
      return false;

   BlockFile* pBlockFile = NULL;

   if( !wxStricmp(tag, wxT("silentblockfile")) ) {
      // Silent blocks don't actually have a file associated, so
      // we don't need to worry about the hash table at all
      *mLoadingTarget = SilentBlockFile::BuildFromXML(*this, attrs);
      return true;
   }
   else if ( !wxStricmp(tag, wxT("simpleblockfile")) )
      pBlockFile = SimpleBlockFile::BuildFromXML(*this, attrs);
   else if( !wxStricmp(tag, wxT("pcmaliasblockfile")) )
      pBlockFile = PCMAliasBlockFile::BuildFromXML(*this, attrs);
   else if( !wxStricmp(tag, wxT("odpcmaliasblockfile")) )
   {
      pBlockFile = ODPCMAliasBlockFile::BuildFromXML(*this, attrs);
      //in the case of loading an OD file, we need to schedule the ODManager to begin OD computing of summary
      //However, because we don't have access to the track or even the Sequence from this call, we mark a flag
      //in the ODMan and check it later.
      ODManager::MarkLoadedODFlag();
   }
   else if( !wxStricmp(tag, wxT("oddecodeblockfile")) )
   {
      pBlockFile = ODDecodeBlockFile::BuildFromXML(*this, attrs);
      ODManager::MarkLoadedODFlag();
   }
   else if( !wxStricmp(tag, wxT("blockfile")) ||
            !wxStricmp(tag, wxT("legacyblockfile")) ) {
      // Support Audacity version 1.1.1 project files

      int i=0;
      bool alias = false;

      while(attrs[i]) {
         if (!wxStricmp(attrs[i], wxT("alias"))) {
            if (wxAtoi(attrs[i+1])==1)
               alias = true;
         }
         i++;
         if (attrs[i])
            i++;
      }

      if (alias)
         pBlockFile = LegacyAliasBlockFile::BuildFromXML(projFull, attrs);
      else      
         pBlockFile = LegacyBlockFile::BuildFromXML(projFull, attrs,
                                                         mLoadingBlockLen,
                                                         mLoadingFormat);
   }
   else
      return false;
      
   if ((pBlockFile == NULL) || 
         // Check the length here so we don't have to do it in each BuildFromXML method.
         ((mMaxSamples > -1) && // is initialized
            (pBlockFile->GetLength() > mMaxSamples)))
   {
      delete pBlockFile;
      return false;
   }
   else 
      *mLoadingTarget = pBlockFile;

   //
   // If the block we loaded is already in the hash table, then the
   // object we just loaded is a duplicate, so we delete it and
   // return a reference to the existing object instead.
   //

   wxString name = (*mLoadingTarget)->GetFileName().GetName();    
   BlockFile *retrieved = mBlockFileHash[name];
   if (retrieved) {
      // Lock it in order to delete it safely, i.e. without having
      // it delete the file, too...
      (*mLoadingTarget)->Lock();
      delete (*mLoadingTarget);

      Ref(retrieved); // Add one to its reference count
      *mLoadingTarget = retrieved;
      return true;
   }

   // This is a new object
   mBlockFileHash[name]=*mLoadingTarget;
   // MakeBlockFileName wasn't used so we must add the directory
   // balancing information
   BalanceInfoAdd(name);

   return true;
}

bool DirManager::MoveToNewProjectDirectory(BlockFile *f)
{
   // Check that this BlockFile corresponds to a file on disk
   if (f->GetFileName().GetName().IsEmpty()) {
      return true;
   }

   wxFileName newFileName;
   wxFileName oldFileName=f->GetFileName();
   AssignFile(newFileName,f->GetFileName().GetFullName(),FALSE); 

   if ( !(newFileName == f->GetFileName()) ) {
      bool ok = f->IsSummaryAvailable() && wxRenameFile(f->GetFileName().GetFullPath(),
                             newFileName.GetFullPath());

      if (ok)
         f->SetFileName(newFileName);
      else {
         
         //check to see that summary exists before we copy.
         bool summaryExisted =  f->IsSummaryAvailable();
         if( summaryExisted)
         {
            if(!wxRenameFile(f->GetFileName().GetFullPath(),
                             newFileName.GetFullPath()))
                             /*wxCopyFile(f->GetFileName().GetFullPath(),
                         newFileName.GetFullPath()))*/
               return false;
//            wxRemoveFile(f->GetFileName().GetFullPath());
         }
         f->SetFileName(newFileName);
            
         //there is a small chance that the summary has begun to be computed on a different thread with the
         //original filename.  we need to catch this case by waiting for it to finish and then copy.
         if(!summaryExisted && (f->IsSummaryAvailable()||f->IsSummaryBeingComputed()))
         {
            //block to make sure OD files don't get written while we are changing file names.
            //(It is important that OD files set this lock while computing their summary files.)
            while(f->IsSummaryBeingComputed() && !f->IsSummaryAvailable())
               ::wxMilliSleep(50);
            
            //check to make sure the oldfile exists.  
            //if it doesn't, we can assume it was written to the new name, which is fine.
            if(wxFileExists(oldFileName.GetFullPath()))
            {
               ok = wxCopyFile(oldFileName.GetFullPath(),
                        newFileName.GetFullPath());
               if(ok)
                  wxRemoveFile(f->GetFileName().GetFullPath());
            }
         }
      }
   }

   return true;
}

bool DirManager::CopyToNewProjectDirectory(BlockFile *f)
{
   // Check that this BlockFile corresponds to a file on disk
   if (f->GetFileName().GetName().IsEmpty()) {
      return true;
   }
   wxFileName newFileName;
   wxFileName oldFileName=f->GetFileName();
   AssignFile(newFileName,f->GetFileName().GetFullName(),FALSE); 

   //mchinen:5/31/08:adding OD support 
   //But also I'm also wondering if we need to delete the copied file here, while i'm reimplementing.
   //see original code below - I don't see where that file will ever get delted or used again.
   if ( !(newFileName == f->GetFileName()) ) {
      bool ok=true;
      bool summaryExisted =  f->IsSummaryAvailable();
      
      if( summaryExisted)
      {   
        if(!wxCopyFile(f->GetFileName().GetFullPath(),
                        newFileName.GetFullPath()))
               return false;
         //TODO:make sure we shouldn't delete               
         //   wxRemoveFile(f->mFileName.GetFullPath());

      }  
        
      f->SetFileName(newFileName);
            
      //there is a small chance that the summary has begun to be computed on a different thread with the
      //original filename.  we need to catch this case by waiting for it to finish and then copy.
      if(!summaryExisted && (f->IsSummaryAvailable()||f->IsSummaryBeingComputed()))
      {
         //block to make sure OD files don't get written while we are changing file names.
         //(It is important that OD files set this lock while computing their summary files.)
         while(f->IsSummaryBeingComputed() && !f->IsSummaryAvailable())
            ::wxMilliSleep(50);
            
         //check to make sure the oldfile exists.  
         //if it doesn't, we can assume it was written to the new name, which is fine.
         if(wxFileExists(oldFileName.GetFullPath()))
         {
            ok = wxCopyFile(oldFileName.GetFullPath(),
                     newFileName.GetFullPath());
         //     if(ok)
         //      wxRemoveFile(f->mFileName.GetFullPath());
         }
      }
   }
   return true;
//
//   
//   if ( !(newFileName == f->mFileName) ) {
//      bool ok = wxCopyFile(f->mFileName.GetFullPath(),
//                           newFileName.GetFullPath());
//      if (ok) {
//         f->mFileName = newFileName;
//      }
//      else
//         return false;
//   }
//
//   return true;
}

void DirManager::Ref(BlockFile * f)
{
   f->Ref();
   //printf("Ref(%d): %s\n",
   //       f->mRefCount,
   //       (const char *)f->mFileName.GetFullPath().mb_str());
}

int DirManager::GetRefCount(BlockFile * f)
{
   return f->mRefCount;
}

void DirManager::Deref(BlockFile * f)
{
   wxString theFileName = f->GetFileName().GetName();

   //printf("Deref(%d): %s\n",
   //       f->mRefCount-1,
   //       (const char *)f->mFileName.GetFullPath().mb_str());

   if (f->Deref()) {
      // If Deref() returned true, the reference count reached zero
      // and this block is no longer needed.  Remove it from the hash
      // table.

      mBlockFileHash.erase(theFileName);
      BalanceInfoDel(theFileName);

   }
}

bool DirManager::EnsureSafeFilename(wxFileName fName)
{
   // Quick check: If it's not even in our alias list,
   // then the file name is A-OK.

   if (aliasList.Index(fName.GetFullPath()) == wxNOT_FOUND)
      return true;

   /* i18n-hint: 'old' is part of a filename used when a file is renamed. */
   // Figure out what the new name for the existing file would be.  
   /* i18n-hint: e.g. Try to go from "mysong.wav" to "mysong-old1.wav". */
   // Keep trying until we find a filename that doesn't exist.

   wxFileName renamedFile = fName;
   int i = 0;
   do {
      i++;
      /* i18n-hint: This is the pattern for filenames that are created
         when a file needs to be backed up to a different name.  For
         example, mysong would become mysong-old1, mysong-old2, etc. */
      renamedFile.SetName(wxString::Format(_("%s-old%d"), fName.GetName().c_str(), i));
   } while (wxFileExists(renamedFile.GetFullPath()));

   // Test creating a file by that name to make sure it will
   // be possible to do the rename

   wxFile testFile(renamedFile.GetFullPath(), wxFile::write);
   if (!testFile.IsOpened()) {
      wxLogSysError(_("Unable to open/create test file"),
                    renamedFile.GetFullPath().c_str());
      return false;
   }

   // Close the file prior to renaming.
   testFile.Close();

   if (!wxRemoveFile(renamedFile.GetFullPath())) {
      wxLogSysError(_("Unable to remove '%s'"),
                    renamedFile.GetFullPath().c_str());
      return false;
   }

   wxPrintf(_("Renamed file: %s\n"), renamedFile.GetFullPath().c_str());

   // Go through our block files and see if any indeed point to
   // the file we're concerned about.  If so, point the block file
   // to the renamed file and when we're done, perform the rename.

   bool needToRename = false;
   wxBusyCursor busy;
   BlockHash::iterator it = mBlockFileHash.begin();
   while(it != mBlockFileHash.end()) {
      BlockFile *b = it->second;
      // don't worry, we don't rely on this cast unless IsAlias is true
      AliasBlockFile *ab = (AliasBlockFile*)b;

      if (b->IsAlias() && ab->GetAliasedFile() == fName) {
         needToRename = true;
         
         //ODBlocks access the aliased file on another thread, so we need to pause them before this continues.
         ab->LockRead();
      }
      
      //now for encoded OD blocks  (e.g. flac)
      // don't worry, we don't rely on this cast unless ISDataAvailable is false
      // which means that it still needs to access the file.
      ODDecodeBlockFile *db = (ODDecodeBlockFile*)b;
      if (!b->IsDataAvailable() && db->GetEncodedAudioFilename() == fName) {
         needToRename = true;
         
         //ODBlocks access the aliased file on another thread, so we need to pause them before this continues.
         db->LockRead();
      }

      it++;
   }

   if (needToRename) {
      if (!wxRenameFile(fName.GetFullPath(),
                        renamedFile.GetFullPath())) 
      {
         // ACK!!! The renaming was unsuccessful!!!
         // (This shouldn't happen, since we tried creating a
         // file of this name and then deleted it just a
         // second earlier.)  But we'll handle this scenario
         // just in case!!!

         // Put things back where they were
         BlockHash::iterator it = mBlockFileHash.begin();
         while(it != mBlockFileHash.end()) {
            BlockFile *b = it->second;
            AliasBlockFile *ab = (AliasBlockFile*)b;
            ODDecodeBlockFile *db = (ODDecodeBlockFile*)b;


            if (b->IsAlias() && ab->GetAliasedFile() == fName)
            {
               ab->UnlockRead();
            }
            if (!b->IsDataAvailable() && db->GetEncodedAudioFilename() == fName) {
               db->UnlockRead();
            }
            it++;
         }

         // Print error message and cancel the export
         wxLogSysError(_("Unable to rename '%s' to '%s'"),
                       fName.GetFullPath().c_str(),
                       renamedFile.GetFullPath().c_str());
         return false;
      }
      else
      {
         //point the aliases to the new filename.
         BlockHash::iterator it = mBlockFileHash.begin();
         while(it != mBlockFileHash.end()) {
            BlockFile *b = it->second;
            AliasBlockFile *ab = (AliasBlockFile*)b;
            ODDecodeBlockFile *db = (ODDecodeBlockFile*)b;

            if (b->IsAlias() && ab->GetAliasedFile() == fName)
            {
               ab->ChangeAliasedFile(renamedFile);
               ab->UnlockRead();
               wxPrintf(_("Changed block %s to new alias name\n"), b->GetFileName().GetFullName().c_str());
               
            }
            if (!b->IsDataAvailable() && db->GetEncodedAudioFilename() == fName) {
               db->ChangeAudioFile(renamedFile);
               db->UnlockRead();
            }
            it++;
         }

      }

      aliasList.Remove(fName.GetFullPath());
      aliasList.Add(renamedFile.GetFullPath());
   }

   // Success!!!  Either we successfully renamed the file,
   // or we didn't need to!
   return true;
}

void DirManager::Ref()
{
   wxASSERT(mRef > 0); // MM: If mRef is smaller, it should have been deleted already
   ++mRef;
}

void DirManager::Deref()
{
   wxASSERT(mRef > 0); // MM: If mRef is smaller, it should have been deleted already
   
   --mRef;

   // MM: Automatically delete if refcount reaches zero
   if (mRef == 0)
   {
//      wxLogDebug(wxT("DirManager::Deref: Automatically deleting 'this'"));
      delete this;
   }
}

// check the Blockfiles against the disk state.  Missing Blockfile
// data is regenerated if possible orreplaced with silence, orphaned
// blockfiles are deleted.... but only after user confirm!  Note that
// even blockfiles not referenced by the current savefile (but locked
// by history) will be reflected in the mBlockFileHash, and that's a
// good thing; this is one reason why we use the hash and not the most
// recent savefile.

int DirManager::ProjectFSCK(bool forceerror, bool silentlycorrect, bool bIgnoreNonAUs /*= true*/)
{
      
   // get a rough guess of how many blockfiles will be found/processed
   // at each step by looking at the size of the blockfile hash
   int blockcount = mBlockFileHash.size();
   int ret=0;
   int ndx;

   // enumerate *all* files in the project directory
   wxArrayString fnameList;

   wxArrayString orphanList;
   BlockHash    missingAliasList;
   BlockHash    missingAliasFiles;
   BlockHash    missingSummaryList;
   BlockHash    missingDataList;

   wxString dirPath = (projFull != wxT("") ? projFull: mytemp);
   int count = 
      RecursivelyEnumerateWithProgress(
         dirPath, fnameList, wxEmptyString, 
         true, false, 
         blockcount, _("Inspecting project file data"));
   
   // enumerate orphaned blockfiles
   BlockHash diskFileHash;
   for (ndx = 0; ndx < count; ndx++){
      wxFileName fullname = fnameList[ndx];
      wxString basename=fullname.GetName();
      
      diskFileHash[basename.c_str()]=0; // just needs to be defined
      if ((mBlockFileHash.find(basename) == mBlockFileHash.end()) &&       // is orphaned
            (!bIgnoreNonAUs ||      // check only AU, e.g., not an imported ogg or branding jpg
               fullname.GetExt().IsSameAs(wxT("au"))))
      {
         // the blockfile on disk is orphaned
         orphanList.Add(fullname.GetFullPath());
         if (!silentlycorrect)
            wxLogWarning(_("Orphaned blockfile: (%s)"),
                         fullname.GetFullPath().c_str());
      }
   }
   
   // enumerate missing alias files
   BlockHash::iterator i = mBlockFileHash.begin();
   while(i != mBlockFileHash.end()) {
      wxString key=i->first;
      BlockFile *b=i->second;
      
      if(b->IsAlias()){
         wxFileName aliasfile=((AliasBlockFile *)b)->GetAliasedFile();
         if(aliasfile.GetFullPath()!=wxEmptyString && !wxFileExists(aliasfile.GetFullPath())){
            missingAliasList[key]=b;
            missingAliasFiles[aliasfile.GetFullPath().c_str()]=0; // simply must be defined
         }
      }
      i++;
   }

   if (!silentlycorrect) {
      i=missingAliasFiles.begin();
      while(i != missingAliasFiles.end()) {
         wxString key=i->first;
         wxLogWarning(_("Missing alias file: (%s)"),key.c_str());
         i++;
      }
   }

   // enumerate missing summary blockfiles
   i = mBlockFileHash.begin();
   while(i != mBlockFileHash.end()) {
      wxString key=i->first;
      BlockFile *b=i->second;
      
      if(b->IsAlias() && b->IsSummaryAvailable()){
         /* don't look in hash; that might find files the user moved
            that the Blockfile abstraction can't find itself */
         wxFileName file=MakeBlockFilePath(key);
         file.SetName(key);
         file.SetExt(wxT("auf"));
         if(!wxFileExists(file.GetFullPath().c_str())){
            missingSummaryList[key]=b;
            if (!silentlycorrect)
               wxLogWarning(_("Missing summary file: (%s.auf)"),
                            key.c_str());
         }
      }
      i++;
   }

   // enumerate missing data blockfiles
   i = mBlockFileHash.begin();
   while(i != mBlockFileHash.end()) {
      wxString key=i->first;
      BlockFile *b=i->second;

      if(!b->IsAlias()){
         wxFileName file=MakeBlockFilePath(key);
         file.SetName(key);
         file.SetExt(wxT("au"));
         if(!wxFileExists(file.GetFullPath().c_str())){
            missingDataList[key]=b;
            if (!silentlycorrect)
               wxLogWarning(_("Missing data file: (%s.au)"),
                            key.c_str());
         }
      }
      i++;
   }
   
   // First, pop the log so the user can see what be up.
   if(forceerror ||
      ((!orphanList.IsEmpty() ||
      !missingAliasList.empty() ||
      !missingDataList.empty() ||
      !missingSummaryList.empty()) && !silentlycorrect)){

      wxLogWarning(_("Project check found inconsistencies inspecting the loaded project data;\nclick 'Details' for a complete list of errors, or 'OK' to proceed to more options."));
      
      wxLog::GetActiveTarget()->Flush(); // Flush is both modal
      // (desired) and will clear the log (desired)
   }

   // report, take action
   // If in "silently correct" mode, leave orphaned blockfiles alone
   // (they will be deleted when project is saved the first time)
   if(!orphanList.IsEmpty() && !silentlycorrect){

      wxString promptA =
         _("Project check found %d orphaned blockfile(s). These files are\nunused and probably left over from a crash or some other bug.\nThey should be deleted to avoid disk contention.");
      wxString prompt;
      
      prompt.Printf(promptA,(int)orphanList.GetCount());
      
      const wxChar *buttons[]={_("Delete orphaned files (safe and recommended)"),
                               _("Continue without deleting; ignore the extra files this session"),
                               _("Close project immediately with no changes"),NULL};
      int action = ShowMultiDialog(prompt,
                                   _("Warning"),
                                   buttons);

      if(action==2)return (ret | FSCKstatus_CLOSEREQ);

      if(action==0){
         ret |= FSCKstatus_CHANGED;
         for(ndx=0;ndx<(int)orphanList.GetCount();ndx++){
            wxRemoveFile(orphanList[ndx]);
         }
      }
   }


   // Deal with any missing aliases
   if(!missingAliasList.empty()){
      int action;
      
      if (silentlycorrect)
      {
         // In "silently correct" mode, we always create silent blocks. This
         // makes sure the project is complete even if we open it again.
         action = 0;
      } else
      {
         wxString promptA =
            _("Project check detected %d input file(s) being used in place\n('alias files') are now missing.  There is no way for Audacity\nto recover these files automatically; you may choose to\npermanently fill in silence for the missing files, temporarily\nfill in silence for this session only, or close the project now\nand try to restore the missing files by hand.");
         wxString prompt;
      
         prompt.Printf(promptA,missingAliasFiles.size());
      
         const wxChar *buttons[]={_("Replace missing data with silence (permanent upon save)"),
                                  _("Temporarily replace missing data with silence (this session only)"),
                                  _("Close project immediately with no further changes"),NULL};
         action = ShowMultiDialog(prompt,
                                      _("Warning"),
                                      buttons);

         if(action==2)return (ret | FSCKstatus_CLOSEREQ);
      }

      BlockHash::iterator i=missingAliasList.begin();
      while(i != missingAliasList.end()) {
         AliasBlockFile *b = (AliasBlockFile *)i->second; //this is
         //safe, we checked that it's an alias block file earlier
         
         if(action==0){
            // silence the blockfiles by yanking the filename
            //vvvvv But this causes Check Dependencies to show "MISSING" with no filename.
            //vvvvv Replace with actual SilentBlockFile, as that's what the user commanded.
            //vvvvv Call RemoveDependencies from Dependencies.cpp instead?
            wxFileName dummy;
            dummy.Clear();
            b->ChangeAliasedFile(dummy);
            b->Recover();
            ret |= FSCKstatus_CHANGED;
         }else if(action==1){
            // silence the log for this session
            b->SilenceAliasLog();
         }
         i++;
      }
   }

   // Summary regeneration must happen after alias checking.
   if(!missingSummaryList.empty()){
      int action;
      
      if (silentlycorrect)
      {
         // In "silently correct" mode we just recreate the summary files
         action = 0;
      } else
      {
         wxString promptA =
            _("Project check detected %d missing summary file(s) (.auf).\nAudacity can fully regenerate these summary files from the\noriginal audio data in the project.");
         wxString prompt;
      
         prompt.Printf(promptA,missingSummaryList.size());
      
         const wxChar *buttons[]={_("Regenerate summary files (safe and recommended)"),
                                  _("Fill in silence for missing display data (this session only"),
                                  _("Close project immediately with no further changes"),NULL};
         action = ShowMultiDialog(prompt,
                                      _("Warning"),
                                      buttons);
                                      
         if(action==2)return (ret | FSCKstatus_CLOSEREQ);
      }
      
      BlockHash::iterator i=missingSummaryList.begin();
      while(i != missingSummaryList.end()) {
         BlockFile *b = i->second;
         if(action==0){
            //regenerate from data
            b->Recover();
            ret |= FSCKstatus_CHANGED;
         }else if (action==1){
            b->SilenceLog();
         }
         i++;
      }
   }

   // Deal with any missing SimpleBlockFiles
   if(!missingDataList.empty()){

      int action;
      
      if (silentlycorrect)
      {
         // In "silently correct" mode, we always create silent blocks. This
         // makes sure the project is complete even if we open it again.
         action = 0;
      } else
      {
         wxString promptA =
            _("Project check detected %d missing audio data blockfile(s) (.au), \nprobably due to a bug, system crash or accidental deletion.\nThere is no way for Audacity to recover this lost data\nautomatically; you may choose to permanently fill in silence\nfor the missing data, temporarily fill in silence for this\nsession only, or close the project now and try to restore the\nmissing data by hand.");
         wxString prompt;
      
         prompt.Printf(promptA,missingDataList.size());
      
         const wxChar *buttons[]={_("Replace missing data with silence (permanent immediately)"),
                                  _("Temporarily replace missing data with silence (this session only)"),
                                  _("Close project immediately with no further changes"),NULL};
         action = ShowMultiDialog(prompt,
                                      _("Warning"),
                                      buttons);
      
         if(action==2)return (ret | FSCKstatus_CLOSEREQ);
      }
      
      BlockHash::iterator i=missingDataList.begin();
      while(i != missingDataList.end()) {
         BlockFile *b = i->second;
         if(action==0){
            //regenerate with zeroes
            b->Recover();
            ret |= FSCKstatus_CHANGED;
         }else if(action==1){
            b->SilenceLog();
         }
         i++;
      }
   }

   // Remove any empty directories.
   ProgressDialog* pProgress = 
      new ProgressDialog(_("Progress"), 
                           _("Cleaning up unused directories in project data"));
   // nDirCount is for updating pProgress. +1 because we may delete dirPath.
   int nDirCount = RecursivelyCountSubdirs(dirPath) + 1; 
   RecursivelyRemoveEmptyDirs(dirPath, nDirCount, pProgress);
   delete pProgress;

   return ret;
}

void DirManager::SetLocalTempDir(wxString path)
{
   mytemp = path;
}

// msmeyer: Large parts of this function have been copied from 'ProjectFSCK'.
//          Might want to unify / modularize the approach some time.
void DirManager::RemoveOrphanedBlockfiles()
{
   int i;

   // get a rough guess of how many blockfiles will be found/processed
   // at each step by looking at the size of the blockfile hash
   int blockcount = mBlockFileHash.size();

   // enumerate *all* files in the project directory
   wxArrayString fnameList;

   RecursivelyEnumerateWithProgress(
      (projFull != wxT("") ? projFull: mytemp),
      fnameList, wxEmptyString, 
      true, false, 
      blockcount, _("Inspecting project file data"));

   // enumerate orphaned blockfiles
   wxArrayString orphanList;
   for(i=0;i<(int)fnameList.GetCount();i++){
      wxFileName fullname(fnameList[i]);
      wxString basename=fullname.GetName();
      
      if (mBlockFileHash.find(basename) == mBlockFileHash.end()){
         // the blockfile on disk is orphaned
         orphanList.Add(fullname.GetFullPath());
      }
   }
   
   // remove all orphaned blockfiles
   for(i=0;i<(int)orphanList.GetCount();i++){
      wxRemoveFile(orphanList[i]);
   }
}

void DirManager::FillBlockfilesCache()
{
   bool cacheBlockFiles = false;
   gPrefs->Read(wxT("/Directories/CacheBlockFiles"), &cacheBlockFiles);

   if (!cacheBlockFiles)
      return; // user opted not to cache block files

   int lowMem = gPrefs->Read(wxT("/Directories/CacheLowMem"), 16l);
   if (lowMem < 16) {
      lowMem = 16;
   }
   lowMem <<= 20;

   BlockHash::iterator i;
   int numNeed = 0;

   i = mBlockFileHash.begin();
   while (i != mBlockFileHash.end())
   {
      BlockFile *b = i->second;
      if (b->GetNeedFillCache())
         numNeed++;
      i++;
   }
   
   if (numNeed == 0)
      return;

   ProgressDialog progress(_("Caching audio"),
                           _("Caching audio into memory"));

   i = mBlockFileHash.begin();
   int current = 0;
   while (i != mBlockFileHash.end())
   {
      BlockFile *b = i->second;
      if (b->GetNeedFillCache() && (GetFreeMemory() > lowMem)) {
         b->FillCache();
      }

      if (!progress.Update(current, numNeed))
         break; // user cancelled progress dialog, stop caching
      i++;
      current++;
   }
}

void DirManager::WriteCacheToDisk()
{
   BlockHash::iterator i;
   int numNeed = 0;

   i = mBlockFileHash.begin();
   while (i != mBlockFileHash.end())
   {
      BlockFile *b = i->second;
      if (b->GetNeedWriteCacheToDisk())
         numNeed++;
      i++;
   }
   
   if (numNeed == 0)
      return;

   ProgressDialog progress(_("Saving recorded audio"),
                           _("Saving recorded audio to disk"));

   i = mBlockFileHash.begin();
   int current = 0;
   while (i != mBlockFileHash.end())
   {
      BlockFile *b = i->second;
      if (b->GetNeedWriteCacheToDisk())
      {
         b->WriteCacheToDisk();
         progress.Update(current, numNeed);
      }
      i++;
      current++;
   }
}

wxMemorySize GetFreeMemory()
{
   wxMemorySize avail;

#if defined(__WXMAC__)
   mach_port_t port = mach_host_self();
   mach_msg_type_number_t cnt = HOST_VM_INFO_COUNT;
   vm_statistics_data_t	stats;
   vm_size_t pagesize = 0;
   
   memset(&stats, 0, sizeof(stats));

   host_page_size(port, &pagesize);
   host_statistics(port, HOST_VM_INFO, (host_info_t) &stats, &cnt);
   avail = stats.free_count * pagesize;
#else
   avail = wxGetFreeMemory();
#endif

   return avail;
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
// arch-tag: 192b7dbe-6fef-49a8-b4f4-f11bce51d84f

