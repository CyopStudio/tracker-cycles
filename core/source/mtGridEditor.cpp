#include "mtGridEditor.h"
#include "mtStructs.h"

mtGridEditor gridEditor;
extern strGrid mtGrid;
constexpr uint8_t GRID_ROW_SIZE = 12;

void mtGridEditor::open()
{
    selectedPad = 0;
    isConfirmedPad = false;
}

void mtGridEditor::close()
{
    selectedPad = 0;
    isConfirmedPad = false;
}

void mtGridEditor::changeSelectedPad(enChangePadDirection dir)
{
    switch (dir)
    {
        case enChangePadDirectionUp:
            if (selectedPad >= GRID_ROW_SIZE)
                selectedPad -= GRID_ROW_SIZE;
            break;
        case enChangePadDirectionDown:
            if (selectedPad < (3 * GRID_ROW_SIZE))
                selectedPad += GRID_ROW_SIZE;
            break;
        case enChangePadDirectionLeft:
            if (selectedPad % 12 != 0)
                selectedPad--;
            break;
        case enChangePadDirectionRight:
            if (selectedPad % 12 != 11)
                selectedPad++;
            break;
        default:
            break;
    }
}
void mtGridEditor::changeSelectedPad(uint8_t pad)
{
    selectedPad = pad;
}

void mtGridEditor::confirmSelectedPad()
{
    isConfirmedPad = true;
}
void mtGridEditor::cancelSelectedPad()
{
    isConfirmedPad = false;
}
void mtGridEditor::changeNote(int16_t val)
{
    //	if(!isConfirmedPad) return;

    if (mtGrid.pad[selectedPad].note + val > MAX_NOTE)
        mtGrid.pad[selectedPad].note = MAX_NOTE;
    else if (mtGrid.pad[selectedPad].note + val < MIN_NOTE)
        mtGrid.pad[selectedPad].note = MIN_NOTE;
    else
        mtGrid.pad[selectedPad].note += val;
}
void mtGridEditor::changeMicrotune(int16_t val)
{
    //	if(!isConfirmedPad) return;

    if (mtGrid.pad[selectedPad].microtune + val > MAX_INSTRUMENT_FINETUNE)
        mtGrid.pad[selectedPad].microtune = MAX_INSTRUMENT_FINETUNE;
    else if (mtGrid.pad[selectedPad].microtune + val < MIN_INSTRUMENT_FINETUNE)
        mtGrid.pad[selectedPad].microtune = MIN_INSTRUMENT_FINETUNE;
    else
        mtGrid.pad[selectedPad].microtune += val;
}
void mtGridEditor::changeLedEnable(int16_t val)
{
    //	if(!isConfirmedPad) return;

    if (val > 0)
        mtGrid.pad[selectedPad].ledEnable = true;
    else if (val < 0)
        mtGrid.pad[selectedPad].ledEnable = false;
}

bool mtGridEditor::getIsConfirmedPad()
{
    return isConfirmedPad;
}
uint8_t mtGridEditor::getSelectedPad()
{
    return selectedPad;
}
