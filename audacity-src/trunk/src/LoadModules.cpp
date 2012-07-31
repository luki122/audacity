/**********************************************************************

  Audacity: A Digital Audio Editor

  LoadModules.cpp

  Dominic Mazzoni
  James Crook


*******************************************************************//*!

\file LoadModules.cpp
\brief Based on LoadLadspa, this code loads pluggable Audacity 
extension modules.  It also has the code to (a) invoke a script
server and (b) invoke a function returning a replacement window,
i.e. an alternative to the usual interface, for Audacity.

*//*******************************************************************/

#include <wx/dynlib.h>
#include <wx/list.h>
#include <wx/log.h>
#include <wx/string.h>
#include <wx/filename.h>

#include "Audacity.h"
#include "AudacityApp.h"
#include "Internat.h"

#include "commands/ScriptCommandRelay.h"
#include <NonGuiThread.h>  // header from libwidgetextra

#include "Prefs.h"
#include "LoadModules.h"

#define initFnName      "ExtensionModuleInit"
#define versionFnName   "GetVersionString"
#define scriptFnName    "RegScriptServerFunc"
#define mainPanelFnName "MainPanelFunc"

typedef wxWindow * pwxWindow;
typedef int (*tModuleInit)(int);
//typedef wxString (*tVersionFn)();
typedef wxChar * (*tVersionFn)();
typedef pwxWindow (*tPanelFn)(int);

// This variable will hold the address of a subroutine in 
// a DLL that can hijack the normal panel.
tPanelFn pPanelHijack=NULL;

// Next two commented out lines are handy when investigating
// strange DLL behaviour.  Instead of dynamic linking,
// link the library which has the replacement panel statically.
// Give the address of the routine here.
// This is a great help in identifying missing 
// symbols which otherwise cause a dll to unload after loading
// without an explanation as to why!
//extern wxWindow * MainPanelFunc( int i );
//tPanelFn pPanelHijack=&MainPanelFunc;

/// IF pPanelHijack has been found in a module DLL
/// THEN when this function is called we'll go and
/// create that window instead of the normal one.
wxWindow * MakeHijackPanel()
{
   if( pPanelHijack == NULL )
      return NULL;
   return pPanelHijack(0);
}

// This variable will hold the address of a subroutine in a DLL that
// starts a thread and reads script commands.
tpRegScriptServerFunc scriptFn;

bool IsAllowedModule( wxString fname )
{
   bool bLoad = false;
   wxString ShortName = wxFileName( fname ).GetName();
   if( (ShortName.CmpNoCase( wxT("mod-script-pipe")) == 0 ))
   {
      gPrefs->Read(wxT("/Module/mod-script-pipe"), &bLoad, false);
   }
   else if( (ShortName.CmpNoCase( wxT("mod-nyq-bench")) == 0 ))
   {
      gPrefs->Read(wxT("/Module/mod-nyq-bench"), &bLoad, false);
   }
   else if( (ShortName.CmpNoCase( wxT("mod-track-panel")) == 0 ))
   {
      gPrefs->Read(wxT("/Module/mod-track-panel"), &bLoad, false);
   }
   return bLoad;
}

void LoadModule(wxString fname)
{
   if( !IsAllowedModule( fname ) )
      return;

   wxLogDebug(wxT("About to load module %s"), fname.c_str());
   wxLogNull logNo; // Don't show wxWidgets Error if cannot load within this method. (Fix bug 544.)

   tModuleInit mainFn = NULL;

   // As a courtesy to some modules that might be bridges to
   // open other modules, we set the current working
   // directory to be the module's directory.

   wxString saveOldCWD = ::wxGetCwd();
   wxString prefix = ::wxPathOnly(fname);
   ::wxSetWorkingDirectory(prefix);

   wxDynamicLibrary* pDLL = new wxDynamicLibrary();
   if (pDLL && pDLL->Load(fname, wxDL_LAZY)) 
   {
      // We've loaded and initialised OK.
      // So look for special case functions:
      // (a) for scripting.
      if( scriptFn == NULL )
         scriptFn = (tpRegScriptServerFunc)(pDLL->GetSymbol(wxT(scriptFnName)));
      // (b) for hijacking the entire Audacity panel.
      if( pPanelHijack==NULL )
         pPanelHijack = (tPanelFn)(pDLL->GetSymbol(wxT(mainPanelFnName)));
   }

   ::wxSetWorkingDirectory(saveOldCWD);
}

void LoadModules(CommandHandler &cmdHandler)
{
   wxArrayString audacityPathList = wxGetApp().audacityPathList;
   wxArrayString pathList;
   wxArrayString files;
   wxString pathVar;
   unsigned int i;

#if 0
   // Code from LoadLadspa that might be useful in load modules.
   pathVar = wxGetenv(wxT("AUDACITY_MODULES_PATH"));
   if (pathVar != wxT(""))
      wxGetApp().AddMultiPathsToPathList(pathVar, pathList);

   #ifdef __WXGTK__
   wxGetApp().AddUniquePathToPathList(INSTALL_PREFIX wxT("/modules"), pathList);
   wxGetApp().AddUniquePathToPathList(wxT("/usr/local/lib/modules"), pathList);
   wxGetApp().AddUniquePathToPathList(wxT("/usr/lib/modules"), pathList);
   #endif
#endif

   for(i=0; i<audacityPathList.GetCount(); i++) {
      wxString prefix = audacityPathList[i] + wxFILE_SEP_PATH;
      wxGetApp().AddUniquePathToPathList(prefix + wxT("modules"),
                                         pathList);
   }

   #ifdef __WXMSW__
   wxGetApp().FindFilesInPathList(wxT("*.dll"), pathList, files);   
   #else
   wxGetApp().FindFilesInPathList(wxT("*.so"), pathList, files);
   #endif

   for(i=0; i<files.GetCount(); i++)
      LoadModule(files[i]);
   // After loading all the modules, we may have a registered scripting function.
   if(scriptFn)
   {
      ScriptCommandRelay::SetCommandHandler(cmdHandler);
      ScriptCommandRelay::SetRegScriptServerFunc(scriptFn);
      NonGuiThread::StartChild(&ScriptCommandRelay::Run);
   }
}

Module::Module(const wxString & name)
{
   mName = name;
   mLib = new wxDynamicLibrary();
   mDispatch = NULL;
}

Module::~Module()
{
   delete mLib;
}

bool Module::Load()
{
   wxLogNull logNo;

   if (mLib->IsLoaded()) {
      if (mDispatch) {
         return true;
      }
      return false;
   }

   if (!mLib->Load(mName, wxDL_LAZY)) {
      return false;
   }

   // Check version string matches.  (For now, they must match exactly)
   tVersionFn versionFn = (tVersionFn)(mLib->GetSymbol(wxT(versionFnName)));
   if (versionFn == NULL){
      wxLogError(wxT("The module %s does not provide a version string. It will not be loaded."), mName.c_str());
      return false;
   }

   wxString moduleVersion = versionFn();
   if( !moduleVersion.IsSameAs(AUDACITY_VERSION_STRING)) {
      wxLogError(wxT("The module %s is designed to work with Audacity version %s; it will not be loaded."), mName.c_str(), moduleVersion.c_str());
      return false;
   }

   mDispatch = (fnModuleDispatch) mLib->GetSymbol(wxT(ModuleDispatchName));
   if (!mDispatch) {
      // Module does not provide a dispacth function...
      return false;
   }

   bool res = ((mDispatch(ModuleInitialize))!=0);
   if (res) {
      return true;
   }

   mDispatch = NULL;
   return false;
}

void Module::Unload()
{
   if (mLib->IsLoaded()) {
      mDispatch(ModuleTerminate);
   }

   mLib->Unload();
}

int Module::Dispatch(ModuleDispatchTypes type)
{
   if (mLib->IsLoaded()) {
      return mDispatch(type);
   }

   return 0;
}

//
// Module Manager (using wxPluginManager would be MUCH better)
//
ModuleManager *ModuleManager::mInstance;

bool ModuleManager::OnInit()
{
   mInstance = this;

   return true;
}

void ModuleManager::OnExit()
{
   size_t cnt = mModules.GetCount();

   for (size_t ndx = 0; ndx < cnt; ndx++) {
      delete (Module *) mModules[ndx];
   }
   mModules.Clear();
}

void ModuleManager::Initialize()
{
   wxArrayString audacityPathList = wxGetApp().audacityPathList;
   wxArrayString pathList;
   wxArrayString files;
   wxString pathVar;
   size_t i;

   // JKC: Is this code duplicating LoadModules() ????
   // Code from LoadLadspa that might be useful in load modules.
   pathVar = wxGetenv(wxT("AUDACITY_MODULES_PATH"));
   if (pathVar != wxT("")) {
      wxGetApp().AddMultiPathsToPathList(pathVar, pathList);
   }

   for (i = 0; i < audacityPathList.GetCount(); i++) {
      wxString prefix = audacityPathList[i] + wxFILE_SEP_PATH;
      wxGetApp().AddUniquePathToPathList(prefix + wxT("modules"),
                                         pathList);
   }

   #if defined(__WXMSW__)
   wxGetApp().FindFilesInPathList(wxT("*.dll"), pathList, files);   
//   #elif defined(__WXMAC__)
//   wxGetApp().FindFilesInPathList(wxT("*.dylib"), pathList, files);
   #else
   wxGetApp().FindFilesInPathList(wxT("*.so"), pathList, files);
   #endif

   for (i = 0; i < files.GetCount(); i++) {
      if( IsAllowedModule( files[i] ) )
      {
         Module *module = new Module(files[i]);

         if (module->Load()) {
            mInstance->mModules.Add(module);
         }
         else {
            delete module;
         }
      }
   }
}

int ModuleManager::Dispatch(ModuleDispatchTypes type)
{
   size_t cnt = mInstance->mModules.GetCount();

   for (size_t ndx = 0; ndx < cnt; ndx++) {
      Module *module = (Module *)mInstance->mModules[ndx];

      module->Dispatch(type);
   }
   return 0;
}

IMPLEMENT_DYNAMIC_CLASS(ModuleManager, wxModule);
// Indentation settings for Vim and Emacs and unique identifier for Arch, a
// version control system. Please do not modify past this point.
//
// Local Variables:
// c-basic-offset: 3
// indent-tabs-mode: nil
// End:
//
// vim: et sts=3 sw=3


