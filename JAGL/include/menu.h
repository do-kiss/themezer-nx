#pragma once
#include "shapes.h"

/*
Make a menu out of a ShapeLinker_t list. Must contain at least 1 listview or button. Must contain proper typing for the elements.
@param in The list of shapes/elements to render and control
@param buttonHandler This function will get ran when any other button than the touchscreen, A button or dpad/joysticks are pressed. Function needs to be in format "int (funcname)(Context_t *ctx)". You can also leave this NULL for no extra button handling
*/
Context_t MakeMenu(ShapeLinker_t *in, func_ptr buttonHandler, func_ptr runEveryFrame);
void InitHid();