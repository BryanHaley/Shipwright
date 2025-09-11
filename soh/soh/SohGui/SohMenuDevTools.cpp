#include "SohMenu.h"

#define BYTE_DEFINED 1
#include "src/overlays/actors/ovl_player_actor/hlmov_bridge.h"

namespace SohGui {

extern std::shared_ptr<SohMenu> mSohMenu;
using namespace UIWidgets;

static const std::unordered_map<int32_t, const char*> logLevels = {
    { DEBUG_LOG_TRACE, "Trace" }, { DEBUG_LOG_DEBUG, "Debug" }, { DEBUG_LOG_INFO, "Info" },
    { DEBUG_LOG_WARN, "Warn" },   { DEBUG_LOG_ERROR, "Error" }, { DEBUG_LOG_CRITICAL, "Critical" },
    { DEBUG_LOG_OFF, "Off" },
};

static const std::unordered_map<int32_t, const char*> debugSaveFileModes = {
    { 0, "Off" },
    { 1, "Vanilla" },
    { 2, "Maxed" },
};

void SohMenu::AddMenuDevTools() {
    // Add Dev Tools Menu
    AddMenuEntry("Dev Tools", CVAR_SETTING("Menu.DevToolsSidebarSection"));

    // General
    AddSidebarEntry("Dev Tools", "General", 3);
    WidgetPath path = { "Dev Tools", "General", SECTION_COLUMN_1 };

    AddWidget(path, "Popout Menu", WIDGET_CVAR_CHECKBOX)
        .CVar("gSettings.Menu.Popout")
        .Options(CheckboxOptions().Tooltip("Changes the menu display from overlay to windowed."));
    AddWidget(path, "Debug Mode", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_DEVELOPER_TOOLS("DebugEnabled"))
        .Options(
            CheckboxOptions().Tooltip("Enables Debug Mode, allowing you to select maps with L + R + Z, noclip "
                                      "with L + D-pad Right, and open the debug menu with L on the pause screen."));
    AddWidget(path, "Boot To Debug Warp Screen", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_DEVELOPER_TOOLS("BootToDebugWarpScreen"))
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger(CVAR_DEVELOPER_TOOLS("DebugEnabled"), 0); })
        .Options(
            CheckboxOptions().Tooltip("Automatically shows Debug Warp Screen when starting or resetting the game.\n"
                                      "This option takes precedence over \"Boot Sequence\" option."));
    AddWidget(path, "OoT Registry Editor", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_DEVELOPER_TOOLS("RegEditEnabled"))
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger(CVAR_DEVELOPER_TOOLS("DebugEnabled"), 0); })
        .Options(CheckboxOptions().Tooltip("Enables the registry editor."));
    AddWidget(path, "Debug Save File Mode", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_DEVELOPER_TOOLS("DebugSaveFileMode"))
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger(CVAR_DEVELOPER_TOOLS("DebugEnabled"), 0); })
        .Options(ComboboxOptions()
                     .Tooltip("Changes the behavior of debug file select creation (creating a save file on slot 1 "
                              "with debug mode on):\n"
                              "- Off: The debug save file will be a normal savefile.\n"
                              "- Vanilla: The debug save file will be the debug save file from the original game.\n"
                              "- Maxed: The debug save file will be a save file with all of the items & upgrades.")
                     .ComboMap(debugSaveFileModes));
    AddWidget(path, "OoT Skulltula Debug", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_DEVELOPER_TOOLS("SkulltulaDebugEnabled"))
        .Options(CheckboxOptions().Tooltip("Enables Skulltula Debug, when moving the cursor in the menu above various "
                                           "map icons (boss key, compass, map screen locations, etc.) will set the GS "
                                           "bits in that area.\nUSE WITH CAUTION AS IT DOES NOT UPDATE THE GS COUNT!"));
    AddWidget(path, "Better Debug Warp Screen", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_DEVELOPER_TOOLS("BetterDebugWarpScreen"))
        .Options(CheckboxOptions()
                     .Tooltip("Optimized Debug Warp Screen, with the added ability to chose entrances and time of day.")
                     .DefaultValue(true));
    AddWidget(path, "Debug Warp Screen Translation", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_DEVELOPER_TOOLS("DebugWarpScreenTranslation"))
        .Options(CheckboxOptions()
                     .Tooltip("Translate the Debug Warp Screen based on the game language.")
                     .DefaultValue(true));
    AddWidget(path, "Resource logging", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_DEVELOPER_TOOLS("ResourceLogging"))
        .Options(CheckboxOptions().Tooltip("Logs some resources as XML when they're loaded in binary format."));

    AddWidget(path, "Frame Advance", WIDGET_CHECKBOX)
        .Options(CheckboxOptions().Tooltip(
            "This allows you to advance through the game one frame at a time on command. "
            "To advance a frame, hold Z and tap R on the second controller. Holding Z "
            "and R will advance a frame every half second. You can also use the buttons below."))
        .PreFunc([](WidgetInfo& info) {
            info.isHidden = mSohMenu->disabledMap.at(DISABLE_FOR_NULL_PLAY_STATE).active ||
                            mSohMenu->disabledMap.at(DISABLE_FOR_DEBUG_MODE_OFF).active;
            if (gPlayState != nullptr) {
                info.valuePointer = (bool*)&gPlayState->frameAdvCtx.enabled;
            } else {
                info.valuePointer = (bool*)nullptr;
            }
        });
    AddWidget(path, "Advance 1", WIDGET_BUTTON)
        .Options(ButtonOptions().Tooltip("Advance 1 frame.").Size(Sizes::Inline))
        .Callback([](WidgetInfo& info) { CVarSetInteger(CVAR_DEVELOPER_TOOLS("FrameAdvanceTick"), 1); })
        .PreFunc([](WidgetInfo& info) {
            info.isHidden = mSohMenu->disabledMap.at(DISABLE_FOR_FRAME_ADVANCE_OFF).active ||
                            mSohMenu->disabledMap.at(DISABLE_FOR_DEBUG_MODE_OFF).active;
        });
    AddWidget(path, "Advance (Hold)", WIDGET_BUTTON)
        .Options(ButtonOptions().Tooltip("Advance frames while the button is held.").Size(Sizes::Inline))
        .PreFunc([](WidgetInfo& info) {
            info.isHidden = mSohMenu->disabledMap.at(DISABLE_FOR_FRAME_ADVANCE_OFF).active ||
                            mSohMenu->disabledMap.at(DISABLE_FOR_DEBUG_MODE_OFF).active;
        })
        .PostFunc([](WidgetInfo& info) {
            if (ImGui::IsItemActive()) {
                CVarSetInteger(CVAR_DEVELOPER_TOOLS("FrameAdvanceTick"), 1);
            }
        })
        .SameLine(true);
    AddWidget(path, "Log Level", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_DEVELOPER_TOOLS("LogLevel"))
        .Options(ComboboxOptions()
                     .Tooltip("The log level determines which messages are printed to the console."
                              " This does not affect the log file output")
                     .ComboMap(logLevels))
        .Callback([](WidgetInfo& info) {
            Ship::Context::GetInstance()->GetLogger()->set_level(
                (spdlog::level::level_enum)CVarGetInteger(CVAR_DEVELOPER_TOOLS("LogLevel"), DEBUG_LOG_DEBUG));
        })
        .PreFunc([](WidgetInfo& info) { info.isHidden = mSohMenu->disabledMap.at(DISABLE_FOR_DEBUG_MODE_OFF).active; });

    // Stats
    path.sidebarName = "Stats";
    AddSidebarEntry("Dev Tools", path.sidebarName, 1);
    AddWidget(path, "Popout Stats Window", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("SohStats"))
        .RaceDisable(false)
        .WindowName("Stats##Soh")
        .Options(WindowButtonOptions().Tooltip("Enables the separate Stats Window."));

    // Console
    path.sidebarName = "Console";
    AddSidebarEntry("Dev Tools", path.sidebarName, 1);
    AddWidget(path, "Popout Console", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("SohConsole"))
        .WindowName("Console##SoH")
        .Options(WindowButtonOptions().Tooltip("Enables the separate Console Window."));

    // Save Editor
    path.sidebarName = "Save Editor";
    AddSidebarEntry("Dev Tools", path.sidebarName, 1);
    AddWidget(path, "Popout Save Editor", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("SaveEditor"))
        .WindowName("Save Editor")
        .Options(WindowButtonOptions().Tooltip("Enables the separate Save Editor Window."));

    // Hook Debugger
    path.sidebarName = "Hook Debugger";
    AddSidebarEntry("Dev Tools", path.sidebarName, 1);
    AddWidget(path, "Popout Hook Debugger", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("HookDebugger"))
        .WindowName("Hook Debugger")
        .Options(WindowButtonOptions().Tooltip("Enables the separate Hook Debugger Window."));

    // Collision Viewer
    path.sidebarName = "Collision Viewer";
    AddSidebarEntry("Dev Tools", path.sidebarName, 2);
    AddWidget(path, "Popout Collision Viewer", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("CollisionViewer"))
        .WindowName("Collision Viewer")
        .Options(WindowButtonOptions().Tooltip("Enables the separate Collision Viewer Window."));

    // Actor Viewer
    path.sidebarName = "Actor Viewer";
    AddSidebarEntry("Dev Tools", path.sidebarName, 2);
    AddWidget(path, "Popout Actor Viewer", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("ActorViewer"))
        .WindowName("Actor Viewer")
        .Options(WindowButtonOptions().Tooltip("Enables the separate Actor Viewer Window."));

    // Display List Viewer
    path.sidebarName = "DList Viewer";
    AddSidebarEntry("Dev Tools", path.sidebarName, 2);
    AddWidget(path, "Popout Display List Viewer", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("DisplayListViewer"))
        .WindowName("Display List Viewer")
        .Options(WindowButtonOptions().Tooltip("Enables the separate Display List Viewer Window."));

    // Value Viewer
    path.sidebarName = "Value Viewer";
    AddSidebarEntry("Dev Tools", path.sidebarName, 2);
    AddWidget(path, "Popout Value Viewer", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("ValueViewer"))
        .WindowName("Value Viewer")
        .Options(WindowButtonOptions().Tooltip("Enables the separate Value Viewer Window."));

    // Message Viewer
    path.sidebarName = "Message Viewer";
    AddSidebarEntry("Dev Tools", path.sidebarName, 2);
    AddWidget(path, "Popout Message Viewer", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("MessageViewer"))
        .WindowName("Message Viewer")
        .Options(WindowButtonOptions().Tooltip("Enables the separate Message Viewer Window."));

    // Gfx Debugger
    path.sidebarName = "Gfx Debugger";
    AddSidebarEntry("Dev Tools", path.sidebarName, 1);
    AddWidget(path, "Popout Gfx Debugger", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("SohGfxDebugger"))
        .WindowName("GfxDebugger##SoH")
        .Options(WindowButtonOptions().Tooltip("Enables the separate Gfx Debugger Window."));
}

void SohMenu::AddMenuHLMov() {
    // Add Dev Tools Menu
    AddMenuEntry("HL Mov", CVAR_SETTING("Menu.HLMovSidebarSection"));

    // General
    AddSidebarEntry("HL Mov", "General", 4);
    WidgetPath path = { "HL Mov", "General", SECTION_COLUMN_1 };

    AddWidget(path, "Popout Menu", WIDGET_CVAR_CHECKBOX)
        .CVar("gSettings.Menu.Popout")
        .Options(CheckboxOptions().Tooltip("Changes the menu display from overlay to windowed."));

    AddWidget(path, "HL Movement", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SETTING("hlmov.HLMovEnabled"))
        .Options(
            CheckboxOptions().Tooltip("Enable HL Movement (must have first person movement enabled in camera settings)."));

    AddWidget(path, "Autohop", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_SETTING("hlmov.sv_autohop"))
        .Options(CheckboxOptions().Tooltip(
            "Automatically jump when holding the spacebar down."));

    AddWidget(path, "cl_fps_fov_multiplier", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_SETTING("hlmov.cl_fps_fov_multiplier"))
        .Options(FloatSliderOptions()
                     .DefaultValue(2)
                     .Min(0.1f)
                     .Max(3.0f)
                     .Step(0.1f)
                     .Tooltip("FOV multiplier for FPS camera"));

    AddWidget(path, "cl_forwardspeed", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_SETTING("hlmov.cl_forwardspeed"))
        .Options(FloatSliderOptions()
                     .DefaultValue(80)
                     .Min(1.0f)
                     .Max(500.0f)
                     .Step(1.0f)
                     .Tooltip(
            "Forward movement speed."));
    AddWidget(path, "cl_sidespeed", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_SETTING("hlmov.cl_sidespeed"))
        .Options(FloatSliderOptions()
                     .DefaultValue(80)
                     .Min(1.0f)
                     .Max(500.0f)
                     .Step(1.0f)
                     .Tooltip("Side movement speed."));
    AddWidget(path, "cl_upspeed", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_SETTING("hlmov.cl_upspeed"))
        .Options(FloatSliderOptions()
                     .DefaultValue(70)
                     .Min(1.0f)
                     .Max(500.0f)
                     .Step(1.0f)
                     .Tooltip("Up/down movement speed."));
    AddWidget(path, "sv_maxspeed", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_SETTING("hlmov.sv_maxspeed"))
        .Options(FloatSliderOptions()
                     .DefaultValue(70)
                     .Min(1.0f)
                     .Max(1000.0f)
                     .Step(1.0f)
                     .Tooltip("Max ground speed"));
    AddWidget(path, "sv_jumpspeed", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_SETTING("hlmov.sv_jumpspeed"))
        .Options(FloatSliderOptions()
                     .DefaultValue(35)
                     .Min(1.0f)
                     .Max(200.0f)
                     .Step(1.0f)
                     .Tooltip("Jump velocity."));


    AddWidget(path, "sv_gravity", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_SETTING("hlmov.sv_gravity"))
        .Options(FloatSliderOptions()
                     .DefaultValue(25)
                     .Min(1.0f)
                     .Max(100.0f)
                     .Step(1.0f)
                     .Tooltip("Gravity"));
    AddWidget(path, "sv_stopspeed", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_SETTING("hlmov.sv_stopspeed"))
        .Options(FloatSliderOptions()
                     .DefaultValue(5)
                     .Min(1.0f)
                     .Max(100.0f)
                     .Step(1.0f)
                     .Tooltip("Stop speed"));
    AddWidget(path, "sv_accelerate", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_SETTING("hlmov.sv_accelerate"))
        .Options(FloatSliderOptions()
                     .DefaultValue(2)
                     .Min(0.1f)
                     .Max(10.0f)
                     .Step(0.1f)
                     .Tooltip("Ground acceleration"));
    AddWidget(path, "sv_airaccelerate", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_SETTING("hlmov.sv_airaccelerate"))
        .Options(FloatSliderOptions()
                     .DefaultValue(1)
                     .Min(0.1f)
                     .Max(10.0f)
                     .Step(0.1f)
                     .Tooltip("Air acceleration"));
    AddWidget(path, "sv_friction", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_SETTING("hlmov.sv_friction"))
        .Options(FloatSliderOptions()
                     .DefaultValue(1)
                     .Min(0.1f)
                     .Max(20.0f)
                     .Step(0.1f)
                     .Tooltip("Ground friction"));
    AddWidget(path, "sv_edgefriction", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_SETTING("hlmov.sv_edgefriction"))
        .Options(FloatSliderOptions()
                     .DefaultValue(2)
                     .Min(1.0f)
                     .Max(100.0f)
                     .Step(1.0f)
                     .Tooltip("Edge friction"));
    AddWidget(path, "sv_stepsize", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_SETTING("hlmov.sv_stepsize"))
        .Options(FloatSliderOptions()
                     .DefaultValue(20)
                     .Min(1.0f)
                     .Max(100.0f)
                     .Step(1.0f)
                     .Tooltip("Step size"));
    AddWidget(path, "sv_maxvelocity", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_SETTING("hlmov.sv_maxvelocity"))
        .Options(FloatSliderOptions()
                     .DefaultValue(300)
                     .Min(1.0f)
                     .Max(2000.0f)
                     .Step(1.0f)
                     .Tooltip("Maximum velocity"));

    AddWidget(path, "Reset", WIDGET_BUTTON)
        .Options(ButtonOptions().Tooltip("Reset CVARs").Size(Sizes::Inline))
        .Callback([](WidgetInfo& info) {
            CVarSetInteger(CVAR_SETTING("hlmov.HLMovEnabled"), 1);
            CVarSetInteger(CVAR_SETTING("hlmov.sv_autohop"), 1);
            CVarSetFloat(CVAR_SETTING("hlmov.cl_forwardspeed"), 80);
            CVarSetFloat(CVAR_SETTING("hlmov.cl_sidespeed"), 80);
            CVarSetFloat(CVAR_SETTING("hlmov.cl_upspeed"), 70);
            CVarSetFloat(CVAR_SETTING("hlmov.sv_maxspeed"), 70);
            CVarSetFloat(CVAR_SETTING("hlmov.sv_jumpspeed"), 35);
            CVarSetFloat(CVAR_SETTING("hlmov.sv_gravity"), 25);
            CVarSetFloat(CVAR_SETTING("hlmov.sv_stopspeed"), 5);
            CVarSetFloat(CVAR_SETTING("hlmov.sv_accelerate"), 2);
            CVarSetFloat(CVAR_SETTING("hlmov.sv_airaccelerate"), 1);
            CVarSetFloat(CVAR_SETTING("hlmov.sv_friction"), 1);
            CVarSetFloat(CVAR_SETTING("hlmov.sv_edgefriction"), 2);
            CVarSetFloat(CVAR_SETTING("hlmov.sv_stepsize"), 20);
            CVarSetFloat(CVAR_SETTING("hlmov.sv_maxvelocity"), 300);
        });
}

} // namespace SohGui