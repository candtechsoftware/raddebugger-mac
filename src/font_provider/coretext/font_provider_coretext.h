#ifndef FONT_PROVIDER_CORETEXT_H
#define FONT_PROVIDER_CORETEXT_H

#include "mac/mac_framework.h"

////////////////////////////////
//~ Helpers

internal CGFontRef fp_coretext_font_from_handle(FP_Handle handle);
internal FP_Handle fp_coretext_handle_from_font(CGFontRef font);


#endif //FONT_PROVIDER_CORETEXT_H
