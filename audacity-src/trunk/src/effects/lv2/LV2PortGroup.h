/**********************************************************************

  Audacity: A Digital Audio Editor

  LV2PortGroup.h

  Audacity(R) is copyright (c) 1999-2008 Audacity Team.
  License: GPL v2.  See License.txt.

*********************************************************************/

#ifndef LV2PORTGROUP_H
#define LV2PORTGROUP_H

#include <vector>

#include <wx/string.h>


/** A class that contains information about a single LV2 plugin port group,
    such as its children and its name. */
class LV2PortGroup {
 public:
   
   LV2PortGroup(const wxString& name = wxT(""));
   
   /** Add a subgroup of this group. */
   void AddSubGroup(const LV2PortGroup& subgroup);
   
   /** Return a list of all subgroups. */
   const std::vector<LV2PortGroup>& GetSubGroups() const;
   
   /** Add a parameter number (not port number). */
   void AddParameter(uint32_t parameter);
   
   /** Return a list of all parameters in this group. */
   const std::vector<uint32_t>& GetParameters() const;
   
   const wxString& GetName() const;
   
 private:
   
   wxString mName;
   std::vector<LV2PortGroup> mSubgroups;
   std::vector<uint32_t> mParameters;
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
// arch-tag: d8622b59-5c08-4e7f-a170-2502ff8af8e5

