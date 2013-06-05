/**********************************************************************

  Audacity: A Digital Audio Editor

  LoadModules.h

  Dominic Mazzoni
  James Crook

**********************************************************************/

#ifndef __AUDACITY_LOADMODULES_H__
#define __AUDACITY_LOADMODULES_H__

#include <wx/dynlib.h>
#include <wx/module.h>

class CommandHandler;

wxWindow *  MakeHijackPanel();

//
// Module Manager
//
// wxPluginManager would be MUCH better, but it's an "undocumented" framework.
//
#define ModuleDispatchName "ModuleDispatch"

typedef enum
{
   ModuleInitialize,
   ModuleTerminate,
   AppInitialized,
   AppQuiting,
   ProjectInitialized,
   ProjectClosing,
   MenusRebuilt
} ModuleDispatchTypes;

typedef int (*fnModuleDispatch)(ModuleDispatchTypes type);

class Module
{
public:
   Module(const wxString & name);
   virtual ~Module();

   bool Load();
   void Unload();
   int Dispatch(ModuleDispatchTypes type);
   void * GetSymbol(wxString name);

private:
   wxString mName;
   wxDynamicLibrary *mLib;
   fnModuleDispatch mDispatch;
};

class ModuleManager:public wxModule
{
public:
   ModuleManager() {};
   virtual ~ModuleManager() {};

   virtual bool OnInit();
   virtual void OnExit();

   static void Initialize(CommandHandler &cmdHandler);
   static int Dispatch(ModuleDispatchTypes type);

private:
   static ModuleManager *mInstance;

   wxArrayPtrVoid mModules;

   DECLARE_DYNAMIC_CLASS(ModuleManager);
};

#endif /* __AUDACITY_LOADMODULES_H__ */
