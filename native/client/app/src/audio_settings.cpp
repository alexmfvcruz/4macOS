#include "StdAfx.h"
#include "TClient.h"
#include <fourstory_audio_settings.h>
#include "../../audio/backend.h"
BYTE fourstory_spatial_audio_option=FALSE;
void fourstory_spatial_audio_refresh() {
    fourstory_spatial_audio_option=fourstory_audio::spatial_enabled()?TRUE:FALSE;
}
void fourstory_spatial_audio_apply() {
    if(!fourstory_audio::set_spatial(fourstory_spatial_audio_option!=FALSE))
        fourstory_spatial_audio_option=FALSE;
    theApp.WriteProfileInt("Settings","SpatialAudio",fourstory_spatial_audio_option);
}
