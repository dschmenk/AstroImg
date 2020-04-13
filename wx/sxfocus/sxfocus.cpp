/***************************************************************************\

    Copyright (c) 2001-2020 David Schmenk

    All rights reserved.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the
    "Software"), to deal in the Software without restriction, including
    without limitation the rights to use, copy, modify, merge, publish,
    distribute, and/or sell copies of the Software, and to permit persons
    to whom the Software is furnished to do so, provided that the above
    copyright notice(s) and this permission notice appear in all copies of
    the Software and that both the above copyright notice(s) and this
    permission notice appear in supporting documentation.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
    OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
    OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
    HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
    INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
    FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
    NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
    WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

    Except as contained in this notice, the name of a copyright holder
    shall not be used in advertising or otherwise to promote the sale, use
    or other dealings in this Software without prior written authorization
    of the copyright holder.

\***************************************************************************/
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif
#include <wx/cmdline.h>
#include "sxfocus.h"
#define MIN_ZOOM    -4
#define MAX_ZOOM     4

int ccdModel = SXCCD_MX5;
class FocusApp : public wxApp
{
public:
    virtual void OnInitCmdLine(wxCmdLineParser &parser);
    virtual bool OnCmdLineParsed(wxCmdLineParser &parser);
    virtual bool OnInit();
};
class FocusFrame : public wxFrame
{
public:
    FocusFrame();
private:
    unsigned int ccdFrameX, ccdFrameY, ccdFrameWidth, ccdFrameHeight, ccdFrameDepth, ccdDoubleHeight;
    unsigned int ccdPixelWidth, ccdPixelHeight;
    uint16_t    *ccdFrame;
    int          focusWinWidth, focusWinHeight;
    bool         focusFilter;
    int          focusZoom;
    wxTimer      focusTimer;
    void OnTimer(wxTimerEvent& event);
    void OnFilter(wxCommandEvent& event);
    void OnZoomIn(wxCommandEvent& event);
    void OnZoomOut(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    wxDECLARE_EVENT_TABLE();
};
enum
{
    ID_TIMER = 1,
    ID_FILTER,
    ID_ZOOM_IN,
    ID_ZOOM_OUT
};
wxBEGIN_EVENT_TABLE(FocusFrame, wxFrame)
    EVT_TIMER(ID_TIMER,   FocusFrame::OnTimer)
    EVT_MENU(ID_FILTER,   FocusFrame::OnFilter)
    EVT_MENU(ID_ZOOM_IN,  FocusFrame::OnZoomIn)
    EVT_MENU(ID_ZOOM_OUT, FocusFrame::OnZoomOut)
    EVT_MENU(wxID_ABOUT,  FocusFrame::OnAbout)
    EVT_MENU(wxID_EXIT,   FocusFrame::OnExit)
wxEND_EVENT_TABLE()

wxIMPLEMENT_APP(FocusApp);
void FocusApp::OnInitCmdLine(wxCmdLineParser &parser)
{
    wxApp::OnInitCmdLine(parser);
    parser.AddOption(wxT("m"), wxT("model"), wxT("camera model override"), wxCMD_LINE_VAL_STRING);
}
bool FocusApp::OnCmdLineParsed(wxCmdLineParser &parser)
{
    wxString *modelString = new wxString(' ', 10);
    if (parser.Found(wxT("m"), modelString))
    {
        //wxPrintf("Overriding SX camera model with: %s\n", modelString->c_str());
        //
        // Validate ccdModel
        //
        if (toupper(modelString->GetChar(1)) == 'X')
        {
            switch (toupper(modelString->GetChar(0)))
            {
                case 'H':
                    ccdModel &= ~SXCCD_INTERLEAVE;
                    break;
                case 'M':
                    ccdModel |= SXCCD_INTERLEAVE;
                    break;
                default:
                    printf("Invalid model type designation.\n");
            }
            switch ((char)modelString->GetChar(2))
            {
                case '5':
                case '7':
                case '9':
                    ccdModel = (ccdModel & 0xC0) | (modelString->GetChar(2) - '0');
                    break;
                default:
                    printf("Invalid model number designation.\n");
            }
            if (toupper(modelString->GetChar(3)) == 'C')
                ccdModel |= SXCCD_COLOR;
            else
                ccdModel &= ~SXCCD_COLOR;
        }
        else
            printf("Invalid SX designation.\n");
        printf("SX model now: 0x%02X\n", ccdModel);
    }
    return wxApp::OnCmdLineParsed(parser);
}
bool FocusApp::OnInit()
{
    if (wxApp::OnInit())
    {
        FocusFrame *frame = new FocusFrame();
        frame->Show(true);
        return true;
    }
    return false;
}
FocusFrame::FocusFrame() : wxFrame(NULL, wxID_ANY, "SX Focus"), focusTimer(this, ID_TIMER)
{
    char statusText[40];
    wxMenu *menuFocus = new wxMenu;
    menuFocus->AppendCheckItem(ID_FILTER, wxT("&Red Filter\tR"));
    menuFocus->Append(ID_ZOOM_IN,  wxT("&Zoom\t="));
    menuFocus->Append(ID_ZOOM_OUT, wxT("&Bin\t-"));
    menuFocus->AppendSeparator();
    menuFocus->Append(wxID_EXIT);
    wxMenu *menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);
    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append(menuFocus, wxT("&Focus"));
    menuBar->Append(menuHelp, wxT("&Help"));
    SetMenuBar(menuBar);
    if (sxOpen(ccdModel))
    {
        ccdModel = sxGetModel(0);
        sxGetFrameDimensions(0, &ccdFrameWidth, &ccdFrameHeight, &ccdFrameDepth);
        sxGetPixelDimensions(0, &ccdPixelWidth, &ccdPixelHeight);
        sxClearFrame(0, SXCCD_EXP_FLAGS_FIELD_BOTH);
        ccdFrame        = (uint16_t *)malloc(sizeof(uint16_t) * ccdFrameWidth * ccdFrameHeight);
        ccdDoubleHeight = (ccdModel & SXCCD_INTERLEAVE) ? 2 : 1;
        focusTimer.StartOnce(100);
        sprintf(statusText, "Attached: %cX-%d", ccdModel & SXCCD_INTERLEAVE ? 'M' : 'H', ccdModel & 0x3F);
    }
    else
    {
        ccdModel        = 0;
        ccdFrameWidth   = ccdFrameHeight = 512;
        ccdFrameDepth   = 16;
        ccdPixelWidth   = ccdPixelHeight = 1;
        ccdDoubleHeight = 1;
        ccdFrame        = NULL;
        strcpy(statusText, "Attached: None");
    }
    focusFilter    = false;
    focusZoom      = -2;
    focusWinWidth  = ccdFrameWidth;//  / 4;
    focusWinHeight = ccdFrameHeight * ccdPixelHeight / ccdPixelWidth;// / 4; // Keep aspect ratio
    while (focusWinHeight > 720)
    {
        focusWinWidth  <<= 1;
        focusWinHeight <<= 1;
    }
    CreateStatusBar(4);
    SetStatusText(statusText, 0);
    SetStatusText("Bin: 2x2", 1);
    SetClientSize(focusWinWidth, focusWinHeight);
}
void FocusFrame::OnTimer(wxTimerEvent& event)
{
    int zoomWidth, zoomHeight;
    if (focusZoom < 1)
    {
        zoomWidth  = ccdFrameWidth  >> -focusZoom;
        zoomHeight = ccdFrameHeight >> -focusZoom;
        sxReadPixels(0, // cam idx
                     SXCCD_EXP_FLAGS_FIELD_BOTH, // options
                     0, // xoffset
                     0, // yoffset
                     ccdFrameWidth, // width
                     ccdFrameHeight, // height
                     1 << -focusZoom, // xbin
                     1 << -focusZoom, // ybin
                     (unsigned char *)ccdFrame); //pixbuf
    }
    else
    {
        zoomWidth  = ccdFrameWidth >> focusZoom;
        zoomHeight = ccdFrameHeight >> focusZoom;
        sxReadPixels(0, // cam idx
                     SXCCD_EXP_FLAGS_FIELD_BOTH, // options
                     (ccdFrameWidth  - zoomWidth)  / 2, // xoffset
                     (ccdFrameHeight - zoomHeight) / 2, // yoffset
                     zoomWidth, // width
                     zoomHeight, // height
                     1, // xbin
                     1, // ybin
                     (unsigned char *)ccdFrame); //pixbuf
    }
    /*
     * Convert 16 bit samples to 24 BPP image
     */
    wxImage ccdImage(zoomWidth, zoomHeight);
    unsigned char *rgb    = ccdImage.GetData();
    uint16_t      *m16    = ccdFrame;
    uint16_t       minPix = 0xFFFF;
    uint16_t       maxPix = 0;
    int x, y;
    if (focusFilter)
        for (y = 0; y < zoomHeight; y++)
            for (x = 0; x < zoomWidth; x++)
            {
                if (*m16 < minPix) minPix = *m16;
                if (*m16 > maxPix) maxPix = *m16;
                rgb[0] = (*m16 >> 8);
                rgb[1] = rgb[2] = 0;
                rgb += 3;
                m16++;
            }
    else
        for (y = 0; y < zoomHeight; y++)
            for (x = 0; x < zoomWidth; x++)
            {
                if (*m16 < minPix) minPix = *m16;
                if (*m16 > maxPix) maxPix = *m16;
                rgb[0] = rgb[1] = rgb[2] = (*m16 >> 8);
                rgb += 3;
                m16++;
            }
    GetClientSize(&focusWinWidth, &focusWinHeight);
    if (focusWinWidth > 0 && focusWinHeight > 0)
    {
        wxClientDC dc(this);
        wxBitmap bitmap(ccdImage.Scale(focusWinWidth, focusWinHeight, wxIMAGE_QUALITY_BILINEAR));
        dc.DrawBitmap(bitmap, 0, 0);
    }
    char minmax[20];
    sprintf(minmax, "Min: %d", minPix);
    SetStatusText(minmax, 2);
    sprintf(minmax, "Max: %d", maxPix);
    SetStatusText(minmax, 3);
    /*
     * Prep next frame
     */
    //sxClearFrame(0, SXCCD_EXP_FLAGS_FIELD_BOTH);
    focusTimer.StartOnce(100);
}
void FocusFrame::OnFilter(wxCommandEvent& event)
{
    focusFilter = event.IsChecked();
}
void FocusFrame::OnZoomIn(wxCommandEvent& event)
{
    char statusText[20];

    focusZoom++;
    if (focusZoom > MAX_ZOOM)
        focusZoom = MAX_ZOOM;
    if (focusZoom < 1)
    sprintf(statusText, "Bin: %dX%d", 1 << -focusZoom, 1 << -focusZoom);
else
    sprintf(statusText, "Zoom: %dX", 1 << focusZoom);
    SetStatusText(statusText, 1);
}
void FocusFrame::OnZoomOut(wxCommandEvent& event)
{
    char statusText[20];

    focusZoom--;
    if (focusZoom < MIN_ZOOM)
        focusZoom = MIN_ZOOM;
    if (focusZoom < 1)
        sprintf(statusText, "Bin: %dX%d", 1 << -focusZoom, 1 << -focusZoom);
    else
        sprintf(statusText, "Zoom: %dX", 1 << focusZoom);
    SetStatusText(statusText, 1);
}
void FocusFrame::OnExit(wxCommandEvent& event)
{
    Close(true);
}
void FocusFrame::OnAbout(wxCommandEvent& event)
{
    wxMessageBox("Startlight Xpress Focussing App\nCopyright (c) 2020, David Schmenk", "About SX Focus", wxOK | wxICON_INFORMATION);
}