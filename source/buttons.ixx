module;

#include <common.hxx>
#include <string>

export module buttons;

import common;
import settings;
import natives;

class Buttons
{
private:
    static inline std::vector<std::string> btnPrefix = {
        "", //XBOX360
        "XBONE_",
        "PS3_",
        "PS4_",
        "PS5_",
        "SWITCH_",
        "SD_",
    };

    static inline std::vector<std::string> buttons = {
        "UP_ARROW", "DOWN_ARROW", "LEFT_ARROW", "RIGHT_ARROW", "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT", "DPAD_NONE",
        "DPAD_ALL", "DPAD_UPDOWN", "DPAD_LEFTRIGHT", "LSTICK_UP", "LSTICK_DOWN", "LSTICK_LEFT", "LSTICK_RIGHT", "LSTICK_NONE",
        "LSTICK_ALL", "LSTICK_UPDOWN", "LSTICK_LEFTRIGHT", "RSTICK_UP", "RSTICK_DOWN", "RSTICK_LEFT", "RSTICK_RIGHT", "RSTICK_NONE",
        "RSTICK_ALL", "RSTICK_UPDOWN", "RSTICK_LEFTRIGHT", "A_BUTT", "B_BUTT", "X_BUTT", "Y_BUTT", "LB_BUTT", "LT_BUTT", "RB_BUTT",
        "RT_BUTT", "START_BUTT", "BACK_BUTT", "A_BUTT", "B_BUTT"
    };

    static inline std::vector<void*> controllerTexPtrs;
    static inline std::vector<std::vector<void*>> buttonTexPtrs;
    static inline void** gameButtonPtrs = nullptr;
    static inline void** gameControllerPtrs = nullptr;
    static void ButtonsCallback()
    {
        auto prefvalue = FusionFixSettings("PREF_BUTTONS");
        auto prefvalueindex = prefvalue - FusionFixSettings.ButtonsText.eXbox360;
        auto bNeedsReset = false;
        while (std::any_of(buttonTexPtrs[prefvalueindex].begin(), buttonTexPtrs[prefvalueindex].end(), [](auto i) { return i == nullptr; }))
        {
            prefvalue++;

            if (prefvalue > FusionFixSettings.ButtonsText.eSteamDeck)
                prefvalue = FusionFixSettings.ButtonsText.eXbox360;

            prefvalueindex = prefvalue - FusionFixSettings.ButtonsText.eXbox360;
            bNeedsReset = true;
        }

        if (bNeedsReset)
            FusionFixSettings.Set("PREF_BUTTONS", prefvalue);

        if (gameButtonPtrs)
        {
            for (auto b = buttons.begin(); b < buttons.end(); b++)
            {
                auto i = std::distance(std::begin(buttons), b);
                auto ptr = buttonTexPtrs[prefvalueindex][i];
                if (ptr)
                    gameButtonPtrs[i] = ptr;
            }
        }

        if (gameControllerPtrs && controllerTexPtrs[prefvalueindex])
            gameControllerPtrs[0] = controllerTexPtrs[prefvalueindex];
    }

    static inline injector::hook_back<void(__fastcall*)(void*, void*, const char*)> CTxdStore__LoadTexture;
    static void __fastcall LoadCustomButtons(void* dst, void* edx, const char* name)
    {
        CTxdStore__LoadTexture.fun(dst, edx, name);

        buttonTexPtrs.clear();
        buttonTexPtrs.resize(btnPrefix.size());
        controllerTexPtrs.clear();
        controllerTexPtrs.resize(btnPrefix.size());

        for (auto& v : buttonTexPtrs)
            v.resize(buttons.size());

        for (auto prefix = btnPrefix.begin(); prefix < btnPrefix.end(); prefix++)
        {
            for (auto b = buttons.begin(); b < buttons.end(); b++)
            {
                auto texName = *prefix + *b;
                CTxdStore__LoadTexture.fun(&buttonTexPtrs[std::distance(std::begin(btnPrefix), prefix)][std::distance(std::begin(buttons), b)], edx, texName.c_str());
            }

            auto texName = *prefix + "CONTROLLER";
            CTxdStore__LoadTexture.fun(&controllerTexPtrs[std::distance(std::begin(btnPrefix), prefix)], edx, texName.c_str());
        }

        ButtonsCallback();
    }

    static void __fastcall ControllerTextureCallback(void* dst, void* edx, const char* name)
    {
        CTxdStore__LoadTexture.fun(dst, edx, name);
        controllerTexPtrs[0] = *(void**)dst;
        ButtonsCallback();
    }

    static inline injector::hook_back<decltype(&Natives::GetTexture)> hbNATIVE_GET_TEXTURE;
    static Texture __cdecl NATIVE_GET_TEXTURE(TextureDict dictionary, const char* textureName)
    {
        auto texName = std::string(textureName);
        if (iequals(texName, "LT_BUTT") || iequals(texName, "RT_BUTT"))
        {
            auto prefvalue = FusionFixSettings("PREF_BUTTONS");
            auto prefvalueindex = prefvalue - FusionFixSettings.ButtonsText.eXbox360;
            texName = btnPrefix[prefvalueindex] + texName;
            auto result = hbNATIVE_GET_TEXTURE.fun(dictionary, texName.c_str());
            if (result)
                return result;
        }
        return hbNATIVE_GET_TEXTURE.fun(dictionary, textureName);
    }

public:
    Buttons()
    {
        FusionFix::onInitEvent() += []()
        {
            auto pattern = hook::pattern("83 C4 14 B9 ? ? ? ? 68 ? ? ? ? E8");
            if (!pattern.empty())
                gameButtonPtrs = *pattern.get_first<void**>(4);
            else
            {
                pattern = hook::pattern("83 C4 14 68 ? ? ? ? B9");
                gameButtonPtrs = *pattern.get_first<void**>(9);
            }

            pattern = hook::pattern("B9 ? ? ? ? E8 ? ? ? ? 68 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? 68 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? 68 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? 68 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? E8");
            gameControllerPtrs = *pattern.get_first<void**>(1);
            CTxdStore__LoadTexture.fun = injector::MakeCALL(pattern.get_first(5), ControllerTextureCallback).get();

            pattern = find_pattern("E8 ? ? ? ? 6A FF E8 ? ? ? ? C7 05", "E8 ? ? ? ? 55 E8 ? ? ? ? C7 05");
            CTxdStore__LoadTexture.fun = injector::MakeCALL(pattern.get_first(), LoadCustomButtons).get();

            FusionFixSettings.SetCallback("PREF_BUTTONS", [](int32_t value)
            {
                ButtonsCallback();
            });

            // Script
            {
                auto hash_GET_TEXTURE = std::to_underlying(Natives::NativeHashes::GET_TEXTURE);
                pattern = hook::pattern(pattern_str(0x68, to_bytes(hash_GET_TEXTURE))); // push 0x...
                auto addr = *pattern.get_first<uintptr_t>(-4);
                auto range = hook::range_pattern(addr, addr + 30, "E8 ? ? ? ? 8B 0E 83");
                hbNATIVE_GET_TEXTURE.fun = injector::MakeCALL(range.get_first(0), NATIVE_GET_TEXTURE, true).get();
            }
        };
    }
} Buttons;