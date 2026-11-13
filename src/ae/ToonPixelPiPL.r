#include "AEConfig.h"
#include "AE_EffectVers.h"

#ifndef AE_OS_WIN
#include <AE_General.r>
#endif

resource 'PiPL' (16000) {
    {
        Kind {
            AEEffect
        },
        Name {
            "ToonPixel SmartFX"
        },
        Category {
            "ToonPixel"
        },
#ifdef AE_OS_MAC
        CodeMacIntel64 {"EffectMain"},
        CodeMacARM64 {"EffectMain"},
#endif
        AE_PiPL_Version {
            2,
            0
        },
        AE_Effect_Spec_Version {
            PF_PLUG_IN_VERSION,
            PF_PLUG_IN_SUBVERS
        },
        AE_Effect_Version {
            38401
        },
        AE_Effect_Info_Flags {
            0
        },
        AE_Effect_Global_OutFlags {
            0x04108200
        },
        AE_Effect_Global_OutFlags_2 {
            0x00001400
        },
        AE_Effect_Match_Name {
            "ToonPixel SmartFX"
        },
        AE_Reserved_Info {
            0
        }
    }
};
