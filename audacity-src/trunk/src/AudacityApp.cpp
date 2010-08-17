/**********************************************************************

  Audacity: A Digital Audio Editor

  AudacityApp.cpp

  Dominic Mazzoni

******************************************************************//**

\class AudacityApp
\brief AudacityApp is the 'main' class for Audacity 

It handles initialization and termination by subclassing wxApp.

*//*******************************************************************/

#if 0
// This may be used to debug memory leaks.
// See: Visual Leak Dectector @ http://dmoulding.googlepages.com/vld
#include <vld.h>
#endif

#include "Audacity.h" // This should always be included first

#include <wx/defs.h>
#include <wx/app.h>
#include <wx/docview.h>
#include <wx/event.h>
#include <wx/ipc.h>
#include <wx/log.h>
#include <wx/window.h>
#include <wx/intl.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/snglinst.h>
#include <wx/sysopt.h>
#include <wx/fontmap.h>

#include <wx/fs_zip.h>
#include <wx/image.h>

#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filename.h>

#ifdef __WXGTK__
#include <unistd.h>
#endif

// chmod, lstat, geteuid
#ifdef __UNIX__
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "AudacityApp.h"

#include "AboutDialog.h"
#include "AColor.h"
#include "AudioIO.h"
#include "Benchmark.h"
#include "DirManager.h"
#include "commands/CommandHandler.h"
#include "commands/AppCommandEvent.h"
#include "effects/LoadEffects.h"
#include "effects/Contrast.h"
#include "effects/VST/VSTEffect.h"
#include "FFmpeg.h"
#include "GStreamerLoader.h"
#include "Internat.h"
#include "LangChoice.h"
#include "Languages.h"
#include "Prefs.h"
#include "Project.h"
#include "Screenshot.h"
#include "Sequence.h"
#include "WaveTrack.h"
#include "Internat.h"
#include "prefs/PrefsDialog.h"
#include "Theme.h"
#include "PlatformCompatibility.h"
#include "FileNames.h"
#include "AutoRecovery.h"
#include "SplashDialog.h"
#include "FFT.h"
#include "BlockFile.h"
#include "ondemand/ODManager.h"
#include "commands/Keyboard.h"
//temporarilly commented out till it is added to all projects
//#include "Profiler.h"

#include "LoadModules.h"

#include "import/Import.h"

#ifdef _DEBUG
    #ifdef _MSC_VER
        #undef THIS_FILE
        static char*THIS_FILE= __FILE__;
        #define new new(_NORMAL_BLOCK, THIS_FILE, __LINE__)
    #endif
#endif

// Windows specific linker control...only needed once so
// this is a good place (unless we want to add another file).
#if defined(__WXMSW__)

// These lines ensure that Audacity gets WindowsXP themes.
// Without them we get the old-style Windows98/2000 look under XP.
#  if !defined(__WXWINCE__)
#     pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='X86' publicKeyToken='6595b64144ccf1df'\"")
#  endif

// These lines allows conditional inclusion of the various libraries
// that Audacity can use.

#  if defined(USE_LIBFLAC)
#     pragma comment(lib, "libflac++")
#     pragma comment(lib, "libflac")
#  endif

#  if defined(USE_LIBID3TAG)
#     pragma comment(lib, "libid3tag")
#  endif

#  if defined(USE_LIBLRDF)
#     pragma comment(lib, "liblrdf")
#     pragma comment(lib, "raptor")
#  endif

#  if defined(USE_LIBMAD)
#     pragma comment(lib, "libmad")
#  endif

#  if defined(USE_LIBRESAMPLE)
#     pragma comment(lib, "libresample")
#  endif

#  if defined(USE_LIBSAMPLERATE)
#     pragma comment(lib, "libsamplerate")
#  endif

#  if defined(USE_LIBTWOLAME)
#     pragma comment(lib, "twolame")
#  endif

#  if defined(USE_LIBVORBIS)
#     pragma comment(lib, "libogg")
#     pragma comment(lib, "libvorbis")
#  endif

#  if defined(USE_MIDI)
#     pragma comment(lib, "portsmf")
#     endif

#  if defined(EXPERIMENTAL_MIDI_OUT)
#     pragma comment(lib, "portmidi")
#  endif

#  if defined(EXPERIMENTAL_SCOREALIGN)
#     pragma comment(lib, "libscorealign")
#  endif

#  if defined(USE_NYQUIST)
#     pragma comment(lib, "libnyquist")
#  endif

#  if defined(USE_PORTMIXER)
#     pragma comment(lib, "portmixer")
#  endif

#  if defined(USE_SLV2)
#     pragma comment(lib, "slv2")
#     pragma comment(lib, "librdf")
#     pragma comment(lib, "raptor")
#     pragma comment(lib, "rasqal")
#  endif

#  if defined(USE_SBSMS)
#     pragma comment(lib, "sbsms")
#  endif

#  if defined(USE_SOUNDTOUCH)
#     pragma comment(lib, "soundtouch")
#  endif

#  if defined(USE_VAMP)
#     pragma comment(lib, "libvamp")
#  endif

#endif //(__WXMSW__)

////////////////////////////////////////////////////////////
/// Custom events
////////////////////////////////////////////////////////////

DEFINE_EVENT_TYPE(EVT_OPEN_AUDIO_FILE);
DEFINE_EVENT_TYPE(EVT_CAPTURE_KEYBOARD);
DEFINE_EVENT_TYPE(EVT_RELEASE_KEYBOARD);
DEFINE_EVENT_TYPE(EVT_CAPTURE_KEY);

#ifdef __WXGTK__
void wxOnAssert(const wxChar *fileName, int lineNumber, const wxChar *msg)
{
   if (msg)
      printf("ASSERTION FAILED: %s\n%s: %d\n", (const char *)wxString(msg).mb_str(), (const char *)wxString(fileName).mb_str(), lineNumber);
   else
      printf("ASSERTION FAILED!\n%s: %d\n", (const char *)wxString(fileName).mb_str(), lineNumber);

   // Force core dump
   int *i = 0;
   if (*i)
      exit(1);

   exit(0);
}
#endif

static wxFrame *gParentFrame = NULL;

bool gInited = false;
bool gIsQuitting = false;

void QuitAudacity(bool bForce)
{                                          
   if (gIsQuitting)
      return;

   gIsQuitting = true;

   // Try to close each open window.  If the user hits Cancel
   // in a Save Changes dialog, don't continue.
   // BG: unless force is true

   // BG: Are there any projects open?
   //-   if (!gAudacityProjects.IsEmpty())
/*start+*/
   if (gAudacityProjects.IsEmpty())
   {
#ifdef __WXMAC__
      AudacityProject::DeleteClipboard();
#endif
   }
   else
/*end+*/
   {
      SaveWindowSize();
      while (gAudacityProjects.Count())
      {
         if (bForce)
         {
            gAudacityProjects[0]->Close(true);
         }
         else
         {
            if (!gAudacityProjects[0]->Close())
            {
               gIsQuitting = false;
               return;
            }
         }
      }
   }

   ModuleManager::Dispatch(AppQuiting);

   wxLogWindow *lw = wxGetApp().mLogger;
   if (lw)
   {
      lw->EnableLogging(false);
      lw->SetActiveTarget(NULL);
      delete lw;
      wxGetApp().mLogger = NULL;
   }

   if (gParentFrame)
      gParentFrame->Destroy();
   gParentFrame = NULL;

   CloseContrastDialog();
   CloseScreenshotTools();
   
   //release ODManager Threads
   ODManager::Quit();

   //print out profile if we have one by deleting it
   //temporarilly commented out till it is added to all projects
   //delete Profiler::Instance();
   
   //delete the static lock for audacity projects
   AudacityProject::DeleteAllProjectsDeleteLock();

   if (bForce)
   {
      wxExit();
   }
}

void QuitAudacity()
{   
   QuitAudacity(false);
}

void SaveWindowSize()
{
   if (wxGetApp().GetWindowRectAlreadySaved())
   {
      return;
   }
   bool validWindowForSaveWindowSize = FALSE;
   AudacityProject * validProject = NULL;
   bool foundIconizedProject = FALSE;
   size_t numProjects = gAudacityProjects.Count();
   for (size_t i = 0; i < numProjects; i++)
   {
      if (!gAudacityProjects[i]->IsIconized()) {
         validWindowForSaveWindowSize = TRUE;
         validProject = gAudacityProjects[i];
         i = numProjects;
      }
      else
         foundIconizedProject =  TRUE;

   }
   if (validWindowForSaveWindowSize)
   {
      wxRect windowRect = validProject->GetRect();
      wxRect normalRect = validProject->GetNormalizedWindowState();
      bool wndMaximized = validProject->IsMaximized();
      gPrefs->Write(wxT("/Window/X"), windowRect.GetX());
      gPrefs->Write(wxT("/Window/Y"), windowRect.GetY());
      gPrefs->Write(wxT("/Window/Width"), windowRect.GetWidth());
      gPrefs->Write(wxT("/Window/Height"), windowRect.GetHeight());
      gPrefs->Write(wxT("/Window/Maximized"), wndMaximized);
      gPrefs->Write(wxT("/Window/Normal_X"), normalRect.GetX());
      gPrefs->Write(wxT("/Window/Normal_Y"), normalRect.GetY());
      gPrefs->Write(wxT("/Window/Normal_Width"), normalRect.GetWidth());
      gPrefs->Write(wxT("/Window/Normal_Height"), normalRect.GetHeight());
      gPrefs->Write(wxT("/Window/Iconized"), FALSE);
   }
   else
   {
      if (foundIconizedProject) {
         validProject = gAudacityProjects[0];
         bool wndMaximized = validProject->IsMaximized();
         wxRect normalRect = validProject->GetNormalizedWindowState();
         // store only the normal rectangle because the itemized rectangle
         // makes no sense for an opening project window
         gPrefs->Write(wxT("/Window/X"), normalRect.GetX());
         gPrefs->Write(wxT("/Window/Y"), normalRect.GetY());
         gPrefs->Write(wxT("/Window/Width"), normalRect.GetWidth());
         gPrefs->Write(wxT("/Window/Height"), normalRect.GetHeight());
         gPrefs->Write(wxT("/Window/Maximized"), wndMaximized);
         gPrefs->Write(wxT("/Window/Normal_X"), normalRect.GetX());
         gPrefs->Write(wxT("/Window/Normal_Y"), normalRect.GetY());
         gPrefs->Write(wxT("/Window/Normal_Width"), normalRect.GetWidth());
         gPrefs->Write(wxT("/Window/Normal_Height"), normalRect.GetHeight());
         gPrefs->Write(wxT("/Window/Iconized"), TRUE);
      }
      else {
         // this would be a very strange case that might possibly occur on the Mac
         // Audacity would have to be running with no projects open
         // in this case we are going to write only the default values
         wxRect defWndRect;
         GetDefaultWindowRect(&defWndRect);
         gPrefs->Write(wxT("/Window/X"), defWndRect.GetX());
         gPrefs->Write(wxT("/Window/Y"), defWndRect.GetY());
         gPrefs->Write(wxT("/Window/Width"), defWndRect.GetWidth());
         gPrefs->Write(wxT("/Window/Height"), defWndRect.GetHeight());
         gPrefs->Write(wxT("/Window/Maximized"), FALSE);
         gPrefs->Write(wxT("/Window/Normal_X"), defWndRect.GetX());
         gPrefs->Write(wxT("/Window/Normal_Y"), defWndRect.GetY());
         gPrefs->Write(wxT("/Window/Normal_Width"), defWndRect.GetWidth());
         gPrefs->Write(wxT("/Window/Normal_Height"), defWndRect.GetHeight());
         gPrefs->Write(wxT("/Window/Iconized"), FALSE);
      }
   }
   wxGetApp().SetWindowRectAlreadySaved(TRUE);
}

#if defined(__WXGTK__) && defined(HAVE_GTK)

///////////////////////////////////////////////////////////////////////////////
// Provide the ability to receive notification from the session manager
// when the user is logging out or shutting down.
//
// Most of this was taken from nsNativeAppSupportUnix.cpp from Mozilla.
///////////////////////////////////////////////////////////////////////////////

#include <dlfcn.h>
/* There is a conflict between the type names used in Glib >= 2.21 and those in
 * wxGTK (http://trac.wxwidgets.org/ticket/10883)
 * Happily we can avoid the hack, as we only need some of the headers, not
 * the full GTK headers
 */
#include <glib/gtypes.h>
#include <glib-object.h>

typedef struct _GnomeProgram GnomeProgram;
typedef struct _GnomeModuleInfo GnomeModuleInfo;
typedef struct _GnomeClient GnomeClient;

typedef enum
{
  GNOME_SAVE_GLOBAL,
  GNOME_SAVE_LOCAL,
  GNOME_SAVE_BOTH
} GnomeSaveStyle;

typedef enum
{
  GNOME_INTERACT_NONE,
  GNOME_INTERACT_ERRORS,
  GNOME_INTERACT_ANY
} GnomeInteractStyle;

typedef enum
{
  GNOME_DIALOG_ERROR,
  GNOME_DIALOG_NORMAL
} GnomeDialogType;

typedef GnomeProgram * (*_gnome_program_init_fn)(const char *,
                                                 const char *,
                                                 const GnomeModuleInfo *,
                                                 int,
                                                 char **,
                                                 const char *,
                                                 ...);
typedef const GnomeModuleInfo * (*_libgnomeui_module_info_get_fn)();
typedef GnomeClient * (*_gnome_master_client_fn)(void);
typedef void (*GnomeInteractFunction)(GnomeClient *,
                                      gint,
                                      GnomeDialogType,
                                      gpointer);
typedef void (*_gnome_client_request_interaction_fn)(GnomeClient *,
                                                     GnomeDialogType,
                                                     GnomeInteractFunction,
                                                     gpointer);
typedef void (*_gnome_interaction_key_return_fn)(gint, gboolean);

static _gnome_client_request_interaction_fn gnome_client_request_interaction;
static _gnome_interaction_key_return_fn gnome_interaction_key_return;

void interact_cb(GnomeClient *client,
                 gint key,
                 GnomeDialogType type,
                 gpointer data)
{
   wxCloseEvent e(wxEVT_QUERY_END_SESSION, wxID_ANY);
   e.SetEventObject(&wxGetApp());
   e.SetCanVeto(true);

   wxGetApp().ProcessEvent(e);

   gnome_interaction_key_return(key, e.GetVeto());
}

gboolean save_yourself_cb(GnomeClient *client,
                          gint phase,
                          GnomeSaveStyle style,
                          gboolean shutdown,
                          GnomeInteractStyle interact,
                          gboolean fast,
                          gpointer user_data)
{
   if (!shutdown || interact != GNOME_INTERACT_ANY) {
      return TRUE;
   }

   if (gAudacityProjects.IsEmpty()) {
      return TRUE;
   }

   gnome_client_request_interaction(client,
                                    GNOME_DIALOG_NORMAL,
                                    interact_cb,
                                    NULL);

   return TRUE;
}

class GnomeShutdown
{
 public:
   GnomeShutdown()
   {
      mArgv[0] = strdup("Audacity");

      mGnomeui = dlopen("libgnomeui-2.so.0", RTLD_NOW);
      if (!mGnomeui) {
         return;
      }

      mGnome = dlopen("libgnome-2.so.0", RTLD_NOW);
      if (!mGnome) {
         return;
      }

      _gnome_program_init_fn gnome_program_init = (_gnome_program_init_fn)
         dlsym(mGnome, "gnome_program_init");
      _libgnomeui_module_info_get_fn libgnomeui_module_info_get = (_libgnomeui_module_info_get_fn)
         dlsym(mGnomeui, "libgnomeui_module_info_get");
      _gnome_master_client_fn gnome_master_client = (_gnome_master_client_fn)
         dlsym(mGnomeui, "gnome_master_client");

      gnome_client_request_interaction = (_gnome_client_request_interaction_fn)
         dlsym(mGnomeui, "gnome_client_request_interaction");
      gnome_interaction_key_return = (_gnome_interaction_key_return_fn)
         dlsym(mGnomeui, "gnome_interaction_key_return");


      if (!gnome_program_init || !libgnomeui_module_info_get) {
         return;
      }

      gnome_program_init(mArgv[0],
                         "1.0",
                         libgnomeui_module_info_get(),
                         1,
                         mArgv,
                         NULL);

      mClient = gnome_master_client();
      if (mClient == NULL) {
         return;
      }

      g_signal_connect(mClient, "save-yourself", G_CALLBACK(save_yourself_cb), NULL);
   }

   virtual ~GnomeShutdown()
   {
      // Do not dlclose() the libraries here lest you want segfaults...

      free(mArgv[0]);
   }

 private:

   char *mArgv[1];
   void *mGnomeui;
   void *mGnome;
   GnomeClient *mClient;
};

GnomeShutdown GnomeShutdownInstance;

#endif

#if defined(__WXMSW__)

//
// DDE support for opening multiple files with one instance
// of Audacity.
//

#define IPC_APPL wxT("audacity")
#define IPC_TOPIC wxT("System")

class IPCConn : public wxConnection
{
public:
   IPCConn()
   : wxConnection()
   {
   };

   ~IPCConn()
   {
   };

   bool OnExecute(const wxString & topic,
                  wxChar *data,
                  int size,
                  wxIPCFormat format)
   {
      if (!gInited) {
         return false;
      }

      AudacityProject *project = CreateNewAudacityProject();

      wxString cmd(data);

      // We queue a command event to the project responsible for 
      // opening the file since it can be a long process and we
      // only have 5 seconds to return the Execute message to the
      // client.
      if (!cmd.IsEmpty()) {
         wxCommandEvent e(EVT_OPEN_AUDIO_FILE);
         e.SetString(data);
         project->AddPendingEvent(e);
      }

      return true;
   };
};

class IPCServ : public wxServer
{
public:
   IPCServ()
   : wxServer()
   {
      Create(wxT("audacity"));
   };

   ~IPCServ()
   {
   };

   wxConnectionBase *OnAcceptConnection(const wxString & topic)
   {
      if (topic != IPC_TOPIC) {
         return NULL;
      }

      return new IPCConn();
   };
};

#endif //__WXMSW__

IMPLEMENT_APP(AudacityApp)
/* make the application class known to wxWidgets for dynamic construction */

#ifdef __WXMAC__

#include <wx/recguard.h>

// in response of an open-document apple event
void AudacityApp::MacOpenFile(const wxString &fileName)
{
   if (!gInited) {
      return;
   }

   wxCommandEvent e(EVT_OPEN_AUDIO_FILE);
   e.SetString(fileName);
   AddPendingEvent(e);
}

// in response of a print-document apple event
void AudacityApp::MacPrintFile(const wxString &fileName)
{
   if (!gInited) {
      return;
   }

   wxCommandEvent e(EVT_OPEN_AUDIO_FILE);
   e.SetString(fileName);
   AddPendingEvent(e);
}

// in response of a open-application apple event
void AudacityApp::MacNewFile()
{
   if (!gInited)
      return;

   // This method should only be used on the Mac platform
   // when no project windows are open.
 
   if (gAudacityProjects.GetCount() == 0) {
      CreateNewAudacityProject();
   }
}

// in response of a reopen-application apple event
void AudacityApp::MacReopenApp()
{
   // Not sure what to do here...bring it to the foreground???
}

void AudacityApp::OnMacOpenFile(wxCommandEvent & event)
{
   // Add name to queue
   static wxArrayString ofqueue;
   ofqueue.Add(event.GetString());

   // Do not attempt to load more than one file at a time
   static wxRecursionGuardFlag guardflag;
   wxRecursionGuard guard(guardflag);
   if (guard.IsInside()) {
      return;
   }

   // Load each file on the queue
   while (ofqueue.GetCount()) {
      wxString name(ofqueue[0]);
      ofqueue.RemoveAt(0);
      MRUOpen(name);
   }
}
#endif //__WXMAC__

typedef int (AudacityApp::*SPECIALKEYEVENT)(wxKeyEvent&);

BEGIN_EVENT_TABLE(AudacityApp, wxApp)
   EVT_QUERY_END_SESSION(AudacityApp::OnEndSession)

   EVT_KEY_DOWN(AudacityApp::OnKeyDown)
   EVT_CHAR(AudacityApp::OnChar)
   EVT_KEY_UP(AudacityApp::OnKeyUp)
#ifdef __WXMAC__
   EVT_MENU(wxID_NEW, AudacityApp::OnMenuNew)
   EVT_MENU(wxID_OPEN, AudacityApp::OnMenuOpen)
   EVT_MENU(wxID_ABOUT, AudacityApp::OnMenuAbout)
   EVT_MENU(wxID_PREFERENCES, AudacityApp::OnMenuPreferences)
   EVT_MENU(wxID_EXIT, AudacityApp::OnMenuExit)

   EVT_COMMAND(wxID_ANY, EVT_OPEN_AUDIO_FILE, AudacityApp::OnMacOpenFile)
#endif
   // Recent file event handlers.  
   EVT_MENU(wxID_FILE, AudacityApp::OnMRUClear)
   EVT_MENU_RANGE(wxID_FILE1, wxID_FILE9, AudacityApp::OnMRUFile)
// EVT_MENU_RANGE(6050, 6060, AudacityApp::OnMRUProject)

   // Handle AppCommandEvents (usually from a script)
   EVT_APP_COMMAND(wxID_ANY, AudacityApp::OnReceiveCommand)
END_EVENT_TABLE()

// Backend for OnMRUFile and OnMRUProject
bool AudacityApp::MRUOpen(wxString fullPathStr) {
   // Most of the checks below are copied from AudacityProject::OpenFiles.
   // - some rationalisation might be possible.
   
   AudacityProject *proj = GetActiveProject();
   
   if (!fullPathStr.IsEmpty()) 
   {   
      // verify that the file exists 
      if (wxFile::Exists(fullPathStr)) 
      {
         gPrefs->Write(wxT("/DefaultOpenPath"), wxPathOnly(fullPathStr));
         
         // Make sure it isn't already open
         wxFileName newFileName(fullPathStr);
         size_t numProjects = gAudacityProjects.Count();
         for (size_t i = 0; i < numProjects; i++) {
            if (newFileName.SameAs(gAudacityProjects[i]->GetFileName())) {
               wxMessageBox(wxString::Format(_("%s is already open in another window."),
                  newFileName.GetName().c_str()),
                  _("Error opening project"),
                  wxOK | wxCENTRE);
               return(true);
            }
         }
         
         // DMM: If the project is dirty, that means it's been touched at
         // all, and it's not safe to open a new project directly in its
         // place.  Only if the project is brand-new clean and the user
         // hasn't done any action at all is it safe for Open to take place
         // inside the current project.
         //
         // If you try to Open a new project inside the current window when
         // there are no tracks, but there's an Undo history, etc, then
         // bad things can happen, including data files moving to the new
         // project directory, etc.
         if (!proj || proj->GetDirty() || !proj->GetIsEmpty()) {
            proj = CreateNewAudacityProject();
         }
         // This project is clean; it's never been touched.  Therefore
         // all relevant member variables are in their initial state,
         // and it's okay to open a new project inside this window.
         proj->OpenFile(fullPathStr);
      }
      else {
         // File doesn't exist - remove file from history
         wxMessageBox(wxString::Format(_("%s could not be found.\n\nIt has been removed from the list of recent files."), 
                      fullPathStr.c_str()));
         return(false);
      }
   }
   return(true);
}

void AudacityApp::OnMRUClear(wxCommandEvent& event)
{
   mRecentFiles->Clear();
}

void AudacityApp::OnMRUFile(wxCommandEvent& event) {
   int n = event.GetId() - wxID_FILE1;
   wxString fullPathStr = mRecentFiles->GetHistoryFile(n);

   bool opened = MRUOpen(fullPathStr);
   if(!opened) {
      mRecentFiles->RemoveFileFromHistory(n);
   }
}

#if 0
//FIX-ME: Was this OnMRUProject lost in an edit??  Should we have it back?
//vvvvv I think it was removed on purpose, but I don't know why it's still here. 
// Basically, anything from Recent Files is treated as a .aup, until proven otherwise, 
// then it tries to Import(). Very questionable handling, imo. 
void AudacityApp::OnMRUProject(wxCommandEvent& event) {
   AudacityProject *proj = GetActiveProject();

   int n = event.GetId() - 6050;//FIX-ME: Use correct ID name.
   wxString fullPathStr = proj->GetRecentProjects()->GetHistoryFile(n);

   bool opened = MRUOpen(fullPathStr);
   if(!opened) {
      proj->GetRecentProjects()->RemoveFileFromHistory(n);
      gPrefs->SetPath("/RecentProjects");
      proj->GetRecentProjects()->Save(*gPrefs);
      gPrefs->SetPath("..");
   }
}
#endif


void AudacityApp::InitLang( const wxString & lang )
{
   if( mLocale )
      delete mLocale;

   if (lang != wxT("en")) {
      wxLogNull nolog;

// LL: I do not know why loading translations fail on the Mac if LANG is not
//     set, but for some reason it does.  So wrap the creation of wxLocale
//     with the default translation.
#if defined(__WXMAC__)
      wxString oldval;
      bool existed;
      
      existed = wxGetEnv(wxT("LANG"), &oldval);
      wxSetEnv(wxT("LANG"), wxT("en_US"));
#endif

      mLocale = new wxLocale(wxT(""), lang, wxT(""), true, true);

#if defined(__WXMAC__)
      if (existed) {
         wxSetEnv(wxT("LANG"), oldval);
      }
      else {
         wxUnsetEnv(wxT("LANG"));
      }
#endif

      for(unsigned int i=0; i<audacityPathList.GetCount(); i++)
         mLocale->AddCatalogLookupPathPrefix(audacityPathList[i]);

#ifdef AUDACITY_NAME
      mLocale->AddCatalog(wxT(AUDACITY_NAME));
#else
      mLocale->AddCatalog(wxT("audacity"));
#endif
   } else
      mLocale = NULL;

   // Initialize internationalisation (number formats etc.)
   //
   // This must go _after_ creating the wxLocale instance because
   // creating the wxLocale instance sets the application-wide locale.
   Internat::Init();
}

// Only used when checking plugins
void AudacityApp::OnFatalException()
{
   exit(-1);
}

// The `main program' equivalent, creating the windows and returning the
// main frame
bool AudacityApp::OnInit()
{
#if defined(__WXGTK__)
   // Workaround for bug 154 -- initialize to false
   inKbdHandler = false;
#endif

#if defined(__WXMAC__)
   // Disable window animation
   wxSystemOptions::SetOption( wxMAC_WINDOW_PLAIN_TRANSITION, 1 );
#endif

//MERGE:
//Everything now uses Audacity name for preferences.
//(Audacity and CleanSpeech the same program and use
//the same preferences file).
//
// LL: Moved here from InitPreferences() to ensure VST effect
//     discovery writes configuration to the correct directory
//     on OSX with case-sensitive file systems.
#ifdef AUDACITY_NAME
   wxString appName = wxT(AUDACITY_NAME);
   wxString vendorName = wxT(AUDACITY_NAME);
#else
   wxString vendorName = wxT("Audacity");
   wxString appName = wxT("Audacity");
#endif

   wxTheApp->SetVendorName(vendorName);
   wxTheApp->SetAppName(appName);

#ifdef USE_VST // if no VST support, answer is always no
   // Have we been started to check a plugin?
   if (argc == 3 && wxStrcmp(argv[1], VSTCMDKEY) == 0) {
      wxHandleFatalExceptions();

      VSTEffect::Check(argv[2]);
      return false;
   }
#endif

   mLogger = NULL;

   // Unused strings that we want to be translated, even though
   // we're not using them yet...
   wxString future1 = _("Master Gain Control");
   wxString future2 = _("Input Meter");
   wxString future3 = _("Output Meter");

   ::wxInitAllImageHandlers();

   wxFileSystem::AddHandler(new wxZipFSHandler);

   InitPreferences();

   #if defined(__WXMSW__) && !defined(__WXUNIVERSAL__) && !defined(__CYGWIN__)
      this->AssociateFileTypes(); 
   #endif

   // TODO - read the number of files to store in history from preferences
   mRecentFiles = new FileHistory(/* number of files */);
   mRecentFiles->Load(*gPrefs, wxT("RecentFiles"));

   //
   // Paths: set search path and temp dir path
   //

   wxString home = wxGetHomeDir();
   mAppHomeDir = home;
   theTheme.EnsureInitialised();

   // AColor depends on theTheme.
   AColor::Init(); 

   /* On Unix systems, the default temp dir is in /tmp. */
   /* Search path (for plug-ins, translations etc) is (in this order):
      * The AUDACITY_PATH environment variable
      * The current directory
      * The user's .audacity-files directory in their home directory
      * The "share" and "share/doc" directories in their install path */
   #ifdef __WXGTK__
   defaultTempDir.Printf(wxT("/tmp/audacity-%s"), wxGetUserId().c_str());
   
   wxString pathVar = wxGetenv(wxT("AUDACITY_PATH"));
   if (pathVar != wxT(""))
      AddMultiPathsToPathList(pathVar, audacityPathList);
   AddUniquePathToPathList(::wxGetCwd(), audacityPathList);
   AddUniquePathToPathList(wxString::Format(wxT("%s/.audacity-files"),
                                            home.c_str()),
                           audacityPathList);
   #ifdef AUDACITY_NAME
      AddUniquePathToPathList(wxString::Format(wxT("%s/share/%s"),
                                               wxT(INSTALL_PREFIX), wxT(AUDACITY_NAME)),
                              audacityPathList);
      AddUniquePathToPathList(wxString::Format(wxT("%s/share/doc/%s"),
                                               wxT(INSTALL_PREFIX), wxT(AUDACITY_NAME)),
                              audacityPathList);
   #else //AUDACITY_NAME
      AddUniquePathToPathList(wxString::Format(wxT("%s/share/audacity"),
                                               wxT(INSTALL_PREFIX)),
                              audacityPathList);
      AddUniquePathToPathList(wxString::Format(wxT("%s/share/doc/audacity"),
                                               wxT(INSTALL_PREFIX)),
                              audacityPathList);
   #endif //AUDACITY_NAME

   AddUniquePathToPathList(wxString::Format(wxT("%s/share/locale"),
                                            wxT(INSTALL_PREFIX)),
                           audacityPathList);

   #endif //__WXGTK__

   wxFileName tmpFile;
   tmpFile.AssignTempFileName(wxT("nn"));
   wxString tmpDirLoc = tmpFile.GetPath(wxPATH_GET_VOLUME);
   ::wxRemoveFile(tmpFile.GetFullPath());

   // On Mac and Windows systems, use the directory which contains Audacity.
   #ifdef __WXMSW__
   // On Windows, the path to the Audacity program is in argv[0]
   wxString progPath = wxPathOnly(argv[0]);
   AddUniquePathToPathList(progPath, audacityPathList);
   AddUniquePathToPathList(progPath+wxT("\\Languages"), audacityPathList);
   
   defaultTempDir.Printf(wxT("%s\\audacity_temp"), 
                         tmpDirLoc.c_str());
   #endif //__WXWSW__

   #ifdef __WXMAC__
   // On Mac OS X, the path to the Audacity program is in argv[0]
   wxString progPath = wxPathOnly(argv[0]);

   AddUniquePathToPathList(progPath, audacityPathList);
   // If Audacity is a "bundle" package, then the root directory is
   // the great-great-grandparent of the directory containing the executable.
   AddUniquePathToPathList(progPath+wxT("/../../../"), audacityPathList);

   AddUniquePathToPathList(progPath+wxT("/Languages"), audacityPathList);
   AddUniquePathToPathList(progPath+wxT("/../../../Languages"), audacityPathList);
   defaultTempDir.Printf(wxT("%s/audacity-%s"), 
                         tmpDirLoc.c_str(),
                         wxGetUserId().c_str());
   #endif //__WXMAC__

   // BG: Create a temporary window to set as the top window
   wxFrame *temporarywindow = new wxFrame(NULL, -1, wxT("temporarytopwindow"));
   SetTopWindow(temporarywindow);

   // Initialize the ModuleManager
   ModuleManager::Initialize();

   // Initialize the CommandHandler
   InitCommandHandler();

   // load audacity plug-in modules
   LoadModules(*mCmdHandler);

   // Locale
   // wxWidgets 2.3 has a much nicer wxLocale API.  We can make this code much
   // better once we move to wx 2.3/2.4.

   wxString lang = gPrefs->Read(wxT("/Locale/Language"), wxT(""));

   if (lang == wxT(""))
      lang = GetSystemLanguageCode();

#ifdef NOT_RQD
//TIDY-ME: (CleanSpeech) Language prompt??
// The prompt for language only happens ONCE on a system.
// I don't think we should disable it JKC
   wxString lang = gPrefs->Read(wxT("/Locale/Language"), "en");  //lda

// Pop up a dialog the first time the program is run
//lda   if (lang == "")
//lda      lang = ChooseLanguage(NULL);
#endif

   mLocale = NULL;
   InitLang( lang );

   // Init DirManager, which initializes the temp directory
   // If this fails, we must exit the program.

   if (!InitTempDir()) {
      FinishPreferences();
      return false;
   }

   // More initialization
   InitCleanSpeech();

   InitDitherers();
   InitAudioIO();

   LoadEffects();


#ifdef __WXMAC__

   // On the Mac, users don't expect a program to quit when you close the last window.
   // Create a menubar that will show when all project windows are closed.

   wxMenu *fileMenu = new wxMenu();
   wxMenu *recentMenu = new wxMenu();
   fileMenu->Append(wxID_NEW, wxString(_("&New")) + wxT("\tCtrl+N"));
   fileMenu->Append(wxID_OPEN, wxString(_("&Open...")) + wxT("\tCtrl+O"));
   fileMenu->AppendSubMenu(recentMenu, _("Open &Recent..."));
   fileMenu->Append(wxID_ABOUT, _("&About Audacity..."));
   fileMenu->Append(wxID_PREFERENCES, wxString(_("&Preferences...")) + wxT("\tCtrl+,"));

   wxMenuBar *menuBar = new wxMenuBar();
   menuBar->Append(fileMenu, wxT("&File"));

   wxMenuBar::MacSetCommonMenuBar(menuBar);

   mRecentFiles->UseMenu(recentMenu);
   mRecentFiles->AddFilesToMenu(recentMenu);

   // This invisibale frame will be the "root" of all other frames and will
   // become the active frame when no projects are open.
   gParentFrame = new wxFrame(NULL, -1, wxEmptyString, wxPoint(0, 0), wxSize(0, 0), 0);

#endif //__WXMAC__

   SetExitOnFrameDelete(true);

   AudacityProject *project = CreateNewAudacityProject();
   mCmdHandler->SetProject(project);

   wxWindow * pWnd = MakeHijackPanel() ;
   if( pWnd )
   {
      project->Show( false );
      SetTopWindow(pWnd);
      pWnd->Show( true );
   }

   delete temporarywindow;
   
   if( project->mShowSplashScreen )
      project->OnHelpWelcome();

   // JKC 10-Sep-2007: Enable monitoring from the start.
   // (recommended by lprod.org).
   // Monitoring stops again after any 
   // PLAY or RECORD completes.  
   // So we also call StartMonitoring when STOP is called.
   project->MayStartMonitoring();

   mLogger = new wxLogWindow(NULL, wxT("Audacity Log"), false, false);
   mLogger->SetActiveTarget(mLogger);
   mLogger->EnableLogging(true);
   mLogger->SetLogLevel(wxLOG_Max);

   #ifdef USE_FFMPEG
   FFmpegStartup();
   #endif

   #ifdef USE_GSTREAMER
   GStreamerStartup();
   #endif

   mImporter = new Importer;

   //
   // Auto-recovery
   //
   bool didRecoverAnything = false;
   if (!ShowAutoRecoveryDialogIfNeeded(&project, &didRecoverAnything))
   {
      // Important: Prevent deleting any temporary files!
      DirManager::SetDontDeleteTempFiles();
      QuitAudacity(true);
   }

   //
   // Command-line parsing, but only if we didn't recover
   //

#if !defined(__CYGWIN__)

   // Parse command-line arguments
   if (argc > 1 && !didRecoverAnything) {
      for (int option = 1; option < argc; option++) {
         if (!argv[option])
            continue;
         bool handled = false;

         if (!wxString(wxT("-help")).CmpNoCase(argv[option])) {
            PrintCommandLineHelp(); // print the help message out
            exit(0);
         }

         if (option < argc - 1 &&
             argv[option + 1] &&
             !wxString(wxT("-blocksize")).CmpNoCase(argv[option])) {
            long theBlockSize;
            if (wxString(argv[option + 1]).ToLong(&theBlockSize)) {
               if (theBlockSize >= 256 && theBlockSize < 100000000) {
                  wxFprintf(stderr, _("Using block size of %ld\n"),
                          theBlockSize);
                  Sequence::SetMaxDiskBlockSize(theBlockSize);
               }
            }
            option++;
            handled = true;
         }

         if (!handled && !wxString(wxT("-test")).CmpNoCase(argv[option])) {
            RunBenchmark(NULL);
            exit(0);
         }

         if (!handled && !wxString(wxT("-version")).CmpNoCase(argv[option])) {
            wxPrintf(wxT("Audacity v%s (%s)\n"),
                     AUDACITY_VERSION_STRING,
                     (wxUSE_UNICODE ? wxT("Unicode") : wxT("ANSI")));
            exit(0);
         }

         if (argv[option][0] == wxT('-') && !handled) {
            wxPrintf(_("Unknown command line option: %s\n"), argv[option]);
            exit(0);
         }

         if (!handled)
         {
            if (!project)
            {
               // Create new window for project
               project = CreateNewAudacityProject();
            }
            // Always open files with an absolute path
            wxFileName fn(argv[option]);
            fn.MakeAbsolute();
            project->OpenFile(fn.GetFullPath());
            project = NULL; // don't reuse this project for other file
         }

      }                         // for option...
   }                            // if (argc>1)

#else //__CYGWIN__
   
   // Cygwin command line parser (by Dave Fancella)
   if (argc > 1 && !didRecoverAnything) {
      int optionstart = 1;
      bool startAtOffset = false;
      
      // Scan command line arguments looking for trouble
      for (int option = 1; option < argc; option++) {
         if (!argv[option])
            continue;
         // Check to see if argv[0] is copied across other arguments.
         // This is the reason Cygwin gets its own command line parser.
         if (wxString(argv[option]).Lower().Contains(wxString(wxT("audacity.exe")))) {
            startAtOffset = true;
            optionstart = option + 1;
         }
      }
      
      for (int option = optionstart; option < argc; option++) {
         if (!argv[option])
            continue;
         bool handled = false;
         bool openThisFile = false;
         wxString fileToOpen;
         
         if (!wxString(wxT("-help")).CmpNoCase(argv[option])) {
            PrintCommandLineHelp(); // print the help message out
            exit(0);
         }

         if (option < argc - 1 &&
             argv[option + 1] &&
             !wxString(wxT("-blocksize")).CmpNoCase(argv[option])) {
            long theBlockSize;
            if (wxString(argv[option + 1]).ToLong(&theBlockSize)) {
               if (theBlockSize >= 256 && theBlockSize < 100000000) {
                  wxFprintf(stderr, _("Using block size of %ld\n"),
                          theBlockSize);
                  Sequence::SetMaxDiskBlockSize(theBlockSize);
               }
            }
            option++;
            handled = true;
         }

         if (!handled && !wxString(wxT("-test")).CmpNoCase(argv[option])) {
            RunBenchmark(NULL);
            exit(0);
         }

         if (argv[option][0] == wxT('-') && !handled) {
            wxPrintf(_("Unknown command line option: %s\n"), argv[option]);
            exit(0);
         }
         
         if(handled)
            fileToOpen.Clear();
         
         if (!handled)
            fileToOpen = fileToOpen + wxT(" ") + argv[option];
         if(wxString(argv[option]).Lower().Contains(wxT(".aup")))
            openThisFile = true;
         if(openThisFile) {
            openThisFile = false;
            if (!project)
            {
               // Create new window for project
               project = CreateNewAudacityProject();
            }
            project->OpenFile(fileToOpen);
            project = NULL; // don't reuse this project for other file
         }

      }                         // for option...
   }                            // if (argc>1)

#endif // __CYGWIN__ (Cygwin command-line parser)

   gInited = true;
   
   ModuleManager::Dispatch(AppInitialized);

   mWindowRectAlreadySaved = FALSE;

   return TRUE;
}

void AudacityApp::InitCommandHandler()
{
   mCmdHandler = new CommandHandler(*this);
   //SetNextHandler(mCmdHandler);
}

void AudacityApp::DeInitCommandHandler()
{
   wxASSERT(NULL != mCmdHandler);
   delete mCmdHandler;
   mCmdHandler = NULL;
}

// AppCommandEvent callback - just pass the event on to the CommandHandler
void AudacityApp::OnReceiveCommand(AppCommandEvent &event)
{
   wxASSERT(NULL != mCmdHandler);
   mCmdHandler->OnReceiveCommand(event);
}

bool AudacityApp::InitCleanSpeech()
{
   wxString userdatadir = FileNames::DataDir();
   wxString presetsFromPrefs = gPrefs->Read(wxT("/Directories/PresetsDir"), wxT(""));
   wxString presets = wxT("");

   #ifdef __WXGTK__
   if (presetsFromPrefs.Length() > 0 && presetsFromPrefs[0] != wxT('/'))
      presetsFromPrefs = wxT("");
   #endif //__WXGTK__

   wxString presetsDefaultLoc =
      wxFileName(userdatadir, wxT("presets")).GetFullPath();

   // Stop wxWidgets from printing its own error messages (not used ... does this really do anything?)
   wxLogNull logNo;
   
   // Try temp dir that was stored in prefs first
   if (presetsFromPrefs != wxT("")) {
      if (wxDirExists(presetsFromPrefs))
         presets = presetsFromPrefs;
      else if (wxMkdir(presetsFromPrefs))
         presets = presetsFromPrefs;
   }

   // If that didn't work, try the default location
   if ((presets == wxT("")) && (presetsDefaultLoc != wxT(""))) {
      if (wxDirExists(presetsDefaultLoc))
         presets = presetsDefaultLoc;
      else if (wxMkdir(presetsDefaultLoc))
         presets = presetsDefaultLoc;
   }

   if (presets == wxT("")) {
      // Failed
      wxMessageBox(wxT("Audacity could not find a place to store\n.csp CleanSpeech preset files\nAudacity is now going to exit. \nInstallation may be corrupt."));
      return false;
   }

   // The permissions don't always seem to be set on
   // some platforms.  Hopefully this fixes it...
   #ifdef __UNIX__
   chmod(OSFILENAME(presets), 0755);
   #endif

   gPrefs->Write(wxT("/Directories/PresetsDir"), presets);
   return true;
}

bool AudacityApp::InitTempDir()
{
   // We need to find a temp directory location.

   wxString tempFromPrefs = gPrefs->Read(wxT("/Directories/TempDir"), wxT(""));
   wxString tempDefaultLoc = wxGetApp().defaultTempDir;

   wxString temp = wxT("");

   #ifdef __WXGTK__
   if (tempFromPrefs.Length() > 0 && tempFromPrefs[0] != wxT('/'))
      tempFromPrefs = wxT("");
   #endif

   // Stop wxWidgets from printing its own error messages

   wxLogNull logNo;

   // Try temp dir that was stored in prefs first

   if (tempFromPrefs != wxT("")) {
      if (wxDirExists(tempFromPrefs))
         temp = tempFromPrefs;
      else if (wxMkdir(tempFromPrefs, 0755))
         temp = tempFromPrefs;
   }

   // If that didn't work, try the default location

   if (temp==wxT("") && tempDefaultLoc != wxT("")) {
      if (wxDirExists(tempDefaultLoc))
         temp = tempDefaultLoc;
      else if (wxMkdir(tempDefaultLoc, 0755))
         temp = tempDefaultLoc;
   }

   // Check temp directory ownership on *nix systems only
   #ifdef __UNIX__
   struct stat tempStatBuf;
   if ( lstat(temp.mb_str(), &tempStatBuf) != 0 ) {
      temp.clear();
   }
   else {
      if ( geteuid() != tempStatBuf.st_uid ) {
         temp.clear();
      }
   }
   #endif

   if (temp == wxT("")) {
      // Failed
      wxMessageBox(_("Audacity could not find a place to store temporary files.\nPlease enter an appropriate directory in the preferences dialog."));

      PrefsDialog dialog(NULL);
      dialog.ShowTempDirPage();
      dialog.ShowModal();

      wxMessageBox(_("Audacity is now going to exit. Please launch Audacity again to use the new temporary directory."));
      return false;
   }

   // The permissions don't always seem to be set on
   // some platforms.  Hopefully this fixes it...
   #ifdef __UNIX__
   chmod(OSFILENAME(temp), 0755);
   #endif

   gPrefs->Write(wxT("/Directories/TempDir"), temp);
   DirManager::SetTempDir(temp);

   // Make sure the temp dir isn't locked by another process.
   if (!CreateSingleInstanceChecker(temp))
      return false;

   return true;
}

// Return true if there are no other instances of Audacity running,
// false otherwise.
//
// Use "dir" for creating lockfiles (on OS X and Unix).

bool AudacityApp::CreateSingleInstanceChecker(wxString dir)
{
   wxLogNull dontLog;

   wxString name = wxString::Format(wxT("audacity-lock-%s"), wxGetUserId().c_str());
   mChecker = new wxSingleInstanceChecker();

   wxString runningTwoCopiesStr = _("Running two copies of Audacity simultaneously may cause\ndata loss or cause your system to crash.\n\n");

   if (!mChecker->Create(name, dir)) {
      // Error initializing the wxSingleInstanceChecker.  We don't know
      // whether there is another instance running or not.

      wxString prompt =
         _("Audacity was not able to lock the temporary files directory.\nThis folder may be in use by another copy of Audacity.\n") +
         runningTwoCopiesStr +
         _("Do you still want to start Audacity?");
      int action = wxMessageBox(prompt,
                                _("Error locking temporary folder"),
                                wxYES_NO | wxICON_EXCLAMATION,
                                NULL);
      if (action == wxNO) {
         delete mChecker;
         return false;
      }
   }
   else if ( mChecker->IsAnotherRunning() ) {
#if defined(__WXMSW__)
      //
      // On Windows (and possibly Linux?), we attempt to make
      // a DDE connection to an already active Audacity.  If
      // successful, we send the first command line argument
      // (the audio file name) to that Audacity for processing.
      wxClient client;
      wxConnectionBase *conn;

      // We try up to 10 times since there's a small window
      // where the DDE server may not have been fully initialized
      // yet.
      for (int i = 0; i < 10; i++) {
         conn = client.MakeConnection(wxEmptyString,
                                      IPC_APPL,
                                      IPC_TOPIC);
         if (conn) {
            if (conn->Execute(argv[1])) {
               // Command was successfully queued so exit quietly
               delete mChecker;
               return false;
            }
         }
         wxMilliSleep(100);
      }
#endif
      // There is another copy of Audacity running.  Force quit.
      
      wxString prompt =
         _("The system has detected that another copy of Audacity is running.\n") +
         runningTwoCopiesStr +
         _("Use the New or Open commands in the currently running Audacity\nprocess to open multiple projects simultaneously.\n");
      wxMessageBox(prompt, _("Audacity is already running"),
            wxOK | wxICON_ERROR);
      delete mChecker;
      return false;
   }

#if defined(__WXMSW__)
   // Create the DDE server
   mIPCServ = new IPCServ();
#endif

   return true;
}

void  AudacityApp::PrintCommandLineHelp(void)
{
            wxPrintf(wxT("%s\n%s\n%s\n%s\n%s\n\n%s\n"),
                   _("Command-line options supported:"),
                   /*i18n-hint: '-help' is the option and needs to stay in
                    * English. This displays a list of available options */
                   _("\t-help (this message)"),
                   /*i18n-hint '-version' needs to stay in English. */
                   _("\t-version (display Audacity version)"),
                   /*i18n-hint '-test' is the option and needs to stay in
                    * English. This runs a set of automatic tests on audacity
                    * itself */
                   _("\t-test (run self diagnostics)"),
                   /*i18n-hint '-blocksize' is the option and needs to stay in
                    * English. 'nnn' is any integer number. This controls the
                    * size pieces that audacity uses when writing files to the
                    * disk */
                   _("\t-blocksize nnn (set max disk block size in bytes)"),
                   _("In addition, specify the name of an audio file or Audacity project to open it."));

}

// static
void AudacityApp::AddUniquePathToPathList(wxString path,
                                          wxArrayString &pathList)
{
   wxFileName pathNorm = path;
   pathNorm.Normalize();
   path = pathNorm.GetFullPath();

   for(unsigned int i=0; i<pathList.GetCount(); i++) {
      if (wxFileName(path) == wxFileName(pathList[i]))
         return;
   }

   pathList.Add(path);
}

// static
void AudacityApp::AddMultiPathsToPathList(wxString multiPathString,
                                          wxArrayString &pathList)
{
   while (multiPathString != wxT("")) {
      wxString onePath = multiPathString.BeforeFirst(wxPATH_SEP[0]);
      multiPathString = multiPathString.AfterFirst(wxPATH_SEP[0]);
      AddUniquePathToPathList(onePath, pathList);
   }
}

// static
void AudacityApp::FindFilesInPathList(const wxString & pattern,
                                      const wxArrayString & pathList,
                                      wxArrayString & results,
                                      int flags)
{
   wxLogNull nolog;

   if (pattern == wxT("")) {
      return;
   }

   wxFileName f;

   for(size_t i = 0; i < pathList.GetCount(); i++) {
      f = pathList[i] + wxFILE_SEP_PATH + pattern;
      wxDir::GetAllFiles(f.GetPath(), &results, f.GetFullName(), flags);
   }
}

void AudacityApp::OnEndSession(wxCloseEvent & event)
{
   bool force = !event.CanVeto();

   // Try to close each open window.  If the user hits Cancel
   // in a Save Changes dialog, don't continue.
   if (!gAudacityProjects.IsEmpty()) {
      while (gAudacityProjects.Count()) {
         if (force) {
            gAudacityProjects[0]->Close(true);
         }
         else if (!gAudacityProjects[0]->Close()) {
            gIsQuitting = false;
            event.Veto();
            break;
         }
      }
   }
}

void AudacityApp::OnKeyDown(wxKeyEvent & event)
{
   // Not handled
   event.Skip(true);

   // Make sure this event is destined for a project window
   AudacityProject *prj = GetActiveProject();

   // TODO: I don't know how it can happen, but it did on 2006-07-06.
   // I was switching between apps fairly quickly so maybe that has something
   // to do with it.
   if (!prj)
      return;

   if (prj != wxGetTopLevelParent(wxWindow::FindFocus()))
      return;

   if (prj->HandleKeyDown(event))
      event.Skip(false);
}

void AudacityApp::OnChar(wxKeyEvent & event)
{
   // Not handled
   event.Skip(true);

   // Make sure this event is destined for a project window
   AudacityProject *prj = GetActiveProject();

   // TODO: I don't know how it can happen, but it did on 2006-07-06.
   // I was switching between apps fairly quickly so maybe that has something
   // to do with it.
   if (!prj)
      return;

   if (prj != wxGetTopLevelParent(wxWindow::FindFocus()))
      return;

   if (prj->HandleChar(event))
      event.Skip(false);
}

void AudacityApp::OnKeyUp(wxKeyEvent & event)
{
   // Not handled
   event.Skip(true);

   // Make sure this event is destined for a project window
   AudacityProject *prj = GetActiveProject();

   // TODO: I don't know how it can happen, but it did on 2006-07-06.
   // I was switching between apps fairly quickly so maybe that has something
   // to do with it.
   if (!prj)
      return;

   if (prj != wxGetTopLevelParent(wxWindow::FindFocus()))
      return;

   if (prj->HandleKeyUp(event))
      event.Skip(false);
}

void AudacityApp::AddFileToHistory(const wxString & name)
{
   mRecentFiles->AddFileToHistory(name);
}

int AudacityApp::OnExit()
{
   gIsQuitting = true;
   while(Pending())
   {
      Dispatch();
   }

#if defined(__WXMSW__)
   delete mIPCServ;
#endif

   if (mImporter)
      delete mImporter;

   if(gPrefs)
   {
      bool bFalse = false;
      //Should we change the commands.cfg location next startup?
      if(gPrefs->Read(wxT("/QDeleteCmdCfgLocation"), &bFalse))
      {
         gPrefs->DeleteEntry(wxT("/QDeleteCmdCfgLocation"));
         gPrefs->Write(wxT("/DeleteCmdCfgLocation"), true);
      }
   }

   DeInitCommandHandler();

   mRecentFiles->Save(*gPrefs, wxT("RecentFiles"));
   delete mRecentFiles;

   FinishPreferences();

   #ifdef USE_FFMPEG
   DropFFmpegLibs();
   #endif

   UnloadEffects();

   DeinitFFT();
   BlockFile::Deinit();

   DeinitAudioIO();

   if (mLocale)
      delete mLocale;
   delete mChecker;

   return 0;
}

// The following five methods are currently only used on Mac OS,
// where it's possible to have a menu bar but no windows open.
// It doesn't hurt any other platforms, though.

// ...That is, as long as you check to see if no windows are open 
// before executing the stuff.
// To fix this, check to see how many project windows are open,
// and skip the event unless none are open (which should only happen
// on the Mac, at least currently.)

void AudacityApp::OnMenuAbout(wxCommandEvent & event)
{
   // This function shadows a similar function
   // in Menus.cpp, but should only be used on the Mac platform
   // when no project windows are open. This check assures that 
   // this happens, and enable the same code to be present on
   // all platforms.
   if(gAudacityProjects.GetCount() == 0) {
      AboutDialog dlog(NULL);
      dlog.ShowModal();
   }
   else
      event.Skip();
}

void AudacityApp::OnMenuNew(wxCommandEvent & event)
{
   // This function shadows a similar function
   // in Menus.cpp, but should only be used on the Mac platform
   // when no project windows are open. This check assures that 
   // this happens, and enable the same code to be present on
   // all platforms.
 
   if(gAudacityProjects.GetCount() == 0)
      CreateNewAudacityProject();
   else
      event.Skip();
}


void AudacityApp::OnMenuOpen(wxCommandEvent & event)
{
   // This function shadows a similar function
   // in Menus.cpp, but should only be used on the Mac platform
   // when no project windows are open. This check assures that 
   // this happens, and enable the same code to be present on
   // all platforms.


   if(gAudacityProjects.GetCount() == 0)
      AudacityProject::OpenFiles(NULL);
   else
      event.Skip();


}

void AudacityApp::OnMenuPreferences(wxCommandEvent & event)
{
   // This function shadows a similar function
   // in Menus.cpp, but should only be used on the Mac platform
   // when no project windows are open. This check assures that 
   // this happens, and enable the same code to be present on
   // all platforms.

   if(gAudacityProjects.GetCount() == 0) {
      PrefsDialog dialog(NULL /* parent */ );
      dialog.ShowModal();
   }
   else
      event.Skip();
   
}

void AudacityApp::OnMenuExit(wxCommandEvent & event)
{
   // This function shadows a similar function
   // in Menus.cpp, but should only be used on the Mac platform
   // when no project windows are open. This check assures that 
   // this happens, and enable the same code to be present on
   // all platforms.

   // LL:  Removed "if" to allow closing based on final project count.
   // if(gAudacityProjects.GetCount() == 0)
      QuitAudacity();
   
   // LL:  Veto quit if projects are still open.  This can happen
   //      if the user selected Cancel in a Save dialog.
   event.Skip(gAudacityProjects.GetCount() == 0);
   
}

//BG: On Windows, associate the aup file type with Audacity
/* We do this in the Windows installer now, 
   to avoid issues where user doesn't have admin privileges, but 
   in case that didn't work, allow the user to decide at startup.

   //v Should encapsulate this & allow access from Prefs, too, 
   //      if people want to manually change associations.
*/
#if defined(__WXMSW__) && !defined(__WXUNIVERSAL__) && !defined(__CYGWIN__)
void AudacityApp::AssociateFileTypes()
{
   wxRegKey associateFileTypes;
   associateFileTypes.SetName(wxT("HKCR\\.AUP"));
   bool bKeyExists = associateFileTypes.Exists();
   if (!bKeyExists) {
      // Not at HKEY_CLASSES_ROOT. Try HKEY_CURRENT_USER.
      associateFileTypes.SetName(wxT("HKCU\\Software\\Classes\\.AUP"));
      bKeyExists = associateFileTypes.Exists();
   }
   if (!bKeyExists) {   
      // File types are not currently associated. 
      // Check pref in case user has already decided against it.
      bool bWantAssociateFiles = true;
      if (!gPrefs->Read(wxT("/WantAssociateFiles"), &bWantAssociateFiles) || 
            bWantAssociateFiles) {
         // Either there's no pref or user does want associations 
         // and they got stepped on, so ask.
         int wantAssoc = 
            wxMessageBox(
               _("Audacity project (.AUP) files are not currently \nassociated with Audacity. \n\nAssociate them, so they open on double-click?"), 
               _("Audacity Project Files"), 
               wxYES_NO | wxICON_QUESTION);
         if (wantAssoc == wxYES) {
            gPrefs->Write(wxT("/WantAssociateFiles"), true);

            wxString root_key;

            root_key = wxT("HKCU\\Software\\Classes\\");
            associateFileTypes.SetName(root_key + wxT(".AUP")); // Start again with HKEY_CLASSES_ROOT.
            if (!associateFileTypes.Create(true)) {
               // Not at HKEY_CLASSES_USER. Try HKEY_CURRENT_ROOT.
               root_key = wxT("HKCR\\");
               associateFileTypes.SetName(root_key + wxT(".AUP"));
               if (!associateFileTypes.Create(true)) { 
                  // Actually, can't create keys. Empty root_key to flag failure.
                  root_key.Empty();
               }
            }
            if (root_key.IsEmpty()) {
               //v Warn that we can't set keys. Ask whether to set pref for no retry?
            } else {
               associateFileTypes = wxT("Audacity.Project"); // Finally set value for .AUP key

               associateFileTypes.SetName(root_key + wxT("Audacity.Project"));
               if(!associateFileTypes.Exists()) {
                  associateFileTypes.Create(true);
                  associateFileTypes = wxT("Audacity Project File");
               }

               associateFileTypes.SetName(root_key + wxT("Audacity.Project\\shell"));
               if(!associateFileTypes.Exists()) {
                  associateFileTypes.Create(true);
                  associateFileTypes = wxT("");
               }

               associateFileTypes.SetName(root_key + wxT("Audacity.Project\\shell\\open"));
               if(!associateFileTypes.Exists()) {
                  associateFileTypes.Create(true);
               }

               associateFileTypes.SetName(root_key + wxT("Audacity.Project\\shell\\open\\command"));
               wxString tmpRegAudPath;
               if(associateFileTypes.Exists()) {
                  tmpRegAudPath = wxString(associateFileTypes).Lower();
               }
               if (!associateFileTypes.Exists() || 
                     (tmpRegAudPath.Find(wxT("audacity.exe")) >= 0)) {
                  associateFileTypes.Create(true);
                  associateFileTypes = (wxString)argv[0] + (wxString)wxT(" \"%1\"");
               }

#if 0
               // These can be use later to support more startup messages
               // like maybe "Import into existing project" or some such.
               // Leaving here for an example...
               associateFileTypes.SetName(root_key + wxT("Audacity.Project\\shell\\open\\ddeexec"));
               if(!associateFileTypes.Exists()) {
                  associateFileTypes.Create(true);
                  associateFileTypes = wxT("%1");
               }

               associateFileTypes.SetName(root_key + wxT("Audacity.Project\\shell\\open\\ddeexec\\Application"));
               if(!associateFileTypes.Exists()) {
                  associateFileTypes.Create(true);
                  associateFileTypes = IPC_APPL;
               }

               associateFileTypes.SetName(root_key + wxT("Audacity.Project\\shell\\open\\ddeexec\\Topic"));
               if(!associateFileTypes.Exists()) {
                  associateFileTypes.Create(true);
                  associateFileTypes = IPC_TOPIC;
               }
#endif
            }
         } else {
            // User said no. Set a pref so we don't keep asking.
            gPrefs->Write(wxT("/WantAssociateFiles"), false);
         }
      }
   }
}
#endif

