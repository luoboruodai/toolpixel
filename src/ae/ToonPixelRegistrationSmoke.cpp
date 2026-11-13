#include <CoreServices/CoreServices.h>
#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_EffectVers.h"
#include "SPBasic.h"

#include <dlfcn.h>
#include <cstdio>
#include <string>

namespace {

using RegistrationCallback = A_Err (*)(
    void*,
    const A_u_char*,
    const A_u_char*,
    const A_u_char*,
    const A_u_char*,
    A_long,
    A_long,
    A_long,
    A_long,
    const A_u_char*);

using RegistrationEntry = A_Err (*)(
    void*,
    RegistrationCallback,
    SPBasicSuite*,
    const char*,
    const char*);

struct RegistrationRecord {
    int calls = 0;
    void* opaque = nullptr;
    std::string name;
    std::string match_name;
    std::string category;
    std::string entry_point;
    A_long kind = 0;
    A_long major = 0;
    A_long minor = 0;
    A_long reserved = 0;
    std::string support_url;
};

RegistrationRecord record;

A_Err registration_callback(
    void* opaque,
    const A_u_char* name,
    const A_u_char* match_name,
    const A_u_char* category,
    const A_u_char* entry_point,
    A_long kind,
    A_long major,
    A_long minor,
    A_long reserved,
    const A_u_char* support_url) {
    record.calls += 1;
    record.opaque = opaque;
    record.name = reinterpret_cast<const char*>(name);
    record.match_name = reinterpret_cast<const char*>(match_name);
    record.category = reinterpret_cast<const char*>(category);
    record.entry_point = reinterpret_cast<const char*>(entry_point);
    record.kind = kind;
    record.major = major;
    record.minor = minor;
    record.reserved = reserved;
    record.support_url = reinterpret_cast<const char*>(support_url);
    return 0;
}

}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: toon_registration_smoke <plugin-binary> <compiled-rsrc>\n");
        return 1;
    }
    std::string resource_text;
    const std::string command = "/usr/bin/DeRez -useDF -only PiPL '" + std::string(argv[2]) + "' 2>&1";
    if (FILE* pipe = popen(command.c_str(), "r")) {
        char buffer[4096];
        while (std::fgets(buffer, sizeof(buffer), pipe)) {
            resource_text += buffer;
        }
        const int status = pclose(pipe);
        if (status != 0) {
            std::fprintf(stderr, "DeRez failed: %s\n", resource_text.c_str());
            return 1;
        }
    } else {
        return 1;
    }
    std::string compact;
    for (const char c : resource_text) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            compact += c;
        }
    }
    const bool resource_valid = compact.find("00009601") != std::string::npos &&
        compact.find("04108200") != std::string::npos;
    if (!resource_valid) {
        std::fprintf(stderr, "compiled PiPL version/flags mismatch\n%s\n", resource_text.c_str());
        return 1;
    }
    std::printf("compiled_pipl_version=00009601 flags=04108200\n");
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) {
        std::fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 1;
    }
    const auto registration = reinterpret_cast<RegistrationEntry>(dlsym(library, "PluginDataEntryFunction2"));
    if (!registration) {
        std::fprintf(stderr, "dlsym PluginDataEntryFunction2 failed: %s\n", dlerror());
        dlclose(library);
        return 1;
    }
    SPBasicSuite basic{};
    const A_Err error = registration(&record, registration_callback, &basic, "After Effects", "26.5x89");
    const bool valid = error == 0 &&
        record.calls == 1 &&
        record.opaque == &record &&
        record.name == "ToonPixel SmartFX" &&
        record.match_name == "ToonPixel SmartFX" &&
        record.category == "ToonPixel" &&
        record.entry_point == "EffectMain" &&
        record.kind == 0x65464b54L &&
        record.major == PF_PLUG_IN_VERSION &&
        record.minor == PF_PLUG_IN_SUBVERS &&
        record.reserved == 8 &&
        record.support_url.empty() &&
        dlsym(library, "EffectMain") != nullptr;
    std::printf(
        "registration_error=%ld calls=%d kind=%lx spec=%ld.%ld reserved=%ld entry=%s\n",
        static_cast<long>(error), record.calls, static_cast<unsigned long>(record.kind),
        static_cast<long>(record.major), static_cast<long>(record.minor),
        static_cast<long>(record.reserved), record.entry_point.c_str());
    if (!valid) {
        std::fprintf(stderr, "registration metadata mismatch\n");
    }
    dlclose(library);
    return valid ? 0 : 1;
}
