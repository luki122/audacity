/**********************************************************************

  Audacity: A Digital Audio Editor

  AColor.h

  Dominic Mazzoni

  Manages color brushes and pens and provides utility
  drawing functions

**********************************************************************/

#ifndef __AUDACITY_COLOR__
#define __AUDACITY_COLOR__

#include <wx/brush.h>
#include <wx/pen.h>

class wxDC;
class wxRect;

class AColor {
 public:
   static void Init();
   static void ReInit();

   static void Arrow(wxDC & dc, wxCoord x, wxCoord y, int width, bool down = true);
   static void Line(wxDC & dc, wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2);
   static void DrawFocus(wxDC & dc, wxRect & r);
   static void Bevel(wxDC & dc, bool up, wxRect & r);
   static void BevelTrackInfo(wxDC & dc, bool up, wxRect & r);
   static wxColour Blend(const wxColour & c1, const wxColour & c2);

   static void UseThemeColour( wxDC * dc, int iIndex );
   static void TrackPanelBackground(wxDC * dc, bool selected);

   static void Light(wxDC * dc, bool selected);
   static void Medium(wxDC * dc, bool selected);
   static void MediumTrackInfo(wxDC * dc, bool selected);
   static void Dark(wxDC * dc, bool selected);

   static void CursorColor(wxDC * dc);
   static void IndicatorColor(wxDC * dc, bool bIsNotRecording);
   static void PlayRegionColor(wxDC * dc, bool locked);

   static void Mute(wxDC * dc, bool on, bool selected, bool soloing);
   static void Solo(wxDC * dc, bool on, bool selected);

   static void MIDIChannel(wxDC * dc, int channel /* 1 - 16 */ );
   static void LightMIDIChannel(wxDC * dc, int channel /* 1 - 16 */ );
   static void DarkMIDIChannel(wxDC * dc, int channel /* 1 - 16 */ );

   static void TrackFocusPen(wxDC * dc, int level /* 0 - 2 */);
   static void SnapGuidePen(wxDC * dc);

   // Member variables

   static wxBrush lightBrush[2];
   static wxBrush mediumBrush[2];
   static wxBrush darkBrush[2];
   static wxPen lightPen[2];
   static wxPen mediumPen[2];
   static wxPen darkPen[2];

   static wxPen cursorPen;
   static wxPen indicatorPen[2];
   static wxBrush indicatorBrush[2];
   static wxPen playRegionPen[2];
   static wxBrush playRegionBrush[2];

   static wxBrush muteBrush[2];
   static wxBrush soloBrush;

   static wxPen clippingPen;

   static wxPen envelopePen;
   static wxPen WideEnvelopePen;
   static wxBrush envelopeBrush;

   static wxBrush labelTextNormalBrush;
   static wxBrush labelTextEditBrush;
   static wxBrush labelUnselectedBrush;
   static wxBrush labelSelectedBrush;
   static wxPen labelUnselectedPen;
   static wxPen labelSelectedPen;
   static wxPen labelSurroundPen;

   static wxPen trackFocusPens[3];
   static wxPen snapGuidePen;

   static wxBrush tooltipBrush;

 private:
   static wxPen sparePen;
   static wxBrush spareBrush;
   static bool inited;

};

void GetColorGradient(float value,
                      bool selected,
                      bool grayscale,
                      unsigned char *red,
                      unsigned char *green, unsigned char *blue);

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
// arch-tag: b8a9d878-fa18-4cba-a5ce-3c61b5d77f0e

