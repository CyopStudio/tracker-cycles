#ifndef INTERFACE_CORE_THEMES_H_
#define INTERFACE_CORE_THEMES_H_

#include <stdint.h>

struct Theme
{
    uint32_t backgroundColor;

    // rozne wspolne miedzyekranowe
    uint32_t topBarLabelColors[4];
    uint32_t songTimerLabelColors[4];

    uint32_t activeLabelsColors[4];
    uint32_t inactiveLabelsColors[4];
    uint32_t activeButtonLabelsColors[4];
    uint32_t inactiveButtonLabelsColors[4];
    uint32_t highlightedButtonLabelsColors[4];
    uint32_t redButtonLabelsColors[4];
    uint32_t masterEditTrackNameLabelsColors[4]; // master screen track naming
    uint32_t redBlinkLabelColors[4];
    uint32_t performActiveTrackLabelsColors[4];
    uint32_t performMutedTrackLabelsColors[4];
    uint32_t blankSpaceCoverLabelColors[4];
    uint32_t patchEditSynthModelIconColors[4];

    uint32_t bottomButtonsBgColors[3];

    uint32_t miniBoxMenuColors[7];

    uint32_t imgGridColors[6];

    uint32_t activeBarColors[5];
    uint32_t inactiveBarColors[5];

    uint32_t activeListColors[7];
    uint32_t inactiveListColors[7];
    uint32_t projectActiveListColors[7];
    uint32_t projectInactiveListColors[7];
    uint32_t sampleLoaderActiveListColors[7];
    uint32_t sampleLoaderInactiveListColors[7];

    uint32_t editBoxColors[5];

    uint32_t frameColors[2];
    uint32_t greenFrameColors[2];

    uint32_t keyboardColors[3];

    // popups -----------------------------------------------------------------
    uint32_t stepPopupTitleLabelColor[4];
    uint32_t stepPopupListColor[7]; // activeListColors
    uint32_t stepPopupBgLabelColor[4];
    uint32_t stepPopupDescriptionTextBoxColors[4]; // valueLabelColorsEdit

    uint32_t notePopupColors[3];
    uint32_t noteKeyboardColors[3];

    // popup interface (info, progress)
    uint32_t popupHorizontalBarColors[4]; // progress bar popupu interface
    uint32_t popupTextColors[3];          // czesc tekstowa i tlo popupu interface

    // popup wylaczania / operacji copy-paste song / operacji sample editora
    uint32_t popupWithProgressColors[3];

    uint32_t noSdInfoTextColors[2];

    //--------------------------------------------------------------------------
    // pattern editor
    uint32_t patternTrackerColors[21];
    uint32_t patternFillBgLabelColors[4];

    // song
    uint32_t songPlayerColors[10];

    // config
    uint32_t configItemEditorTextLabelColors[4];
    uint32_t gridEditorColors[3];
    uint32_t configConfirmPopUpLabelColors[4];

    // sample loader
    uint32_t sampleLoadMemBarNormalColors[5];
    uint32_t sampleLoadMemBarFullColors[5];

    // performance
    uint32_t perfFxNameTextLabelColors[4];
    uint32_t perfValueNormalMLabelColors[4];
    uint32_t perfValueEditMLabelColors[4];

    // start screen
    uint32_t startScreenColors[2];

    // waveform
    uint32_t waveformColors[2];
    uint32_t waveformPointsColors[4];
    uint32_t waveformProgressColors[2];
    uint32_t granularProgressColors[2];
    uint32_t waveTablePositionColors[2];
    uint32_t sliceColors[3];

    // battery
    uint32_t batteryIconColors[3];

    // domyslne kolory dla roznych tekstowych kontrolek
    uint32_t defaultTextColors[4];

    // bars
    uint32_t white;
    uint32_t red;
    uint32_t orange;
    uint32_t yellow;
    uint32_t green;
    uint32_t cyan;
    uint32_t grey_22;

    uint32_t bottomScreenShadow;
    uint32_t barShadow;
    uint32_t patternEditorPlayheadShadow;
    uint32_t patternEditorBottomButtonsLines;
    uint32_t moreButtonColors[4];
};

extern const Theme themeDefault;
extern const Theme themeWhiteout;
extern const Theme themeAnniversary;
extern Theme themeCustom;

#endif /* INTERFACE_CORE_THEMES_H_ */
