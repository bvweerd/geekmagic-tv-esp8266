import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE
from esphome.components import light, display

DEPENDENCIES = ["display", "network", "web_server_base"]

CONF_BACKLIGHT = "backlight"
CONF_DISPLAY_ID = "display_id"

smartclock_ns = cg.esphome_ns.namespace("smartclock_v2")
SmartClockV2Component = smartclock_ns.class_("SmartClockV2Component", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SmartClockV2Component),
        cv.Optional(CONF_BACKLIGHT): cv.use_id(light.LightState),
        cv.Required(CONF_DISPLAY_ID): cv.use_id(display.Display),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    """Generate C++ code for the component"""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Link display
    display_var = await cg.get_variable(config[CONF_DISPLAY_ID])
    cg.add(var.set_display(display_var))

    # Optionally link backlight
    if CONF_BACKLIGHT in config:
        backlight = await cg.get_variable(config[CONF_BACKLIGHT])
        cg.add(var.set_backlight(backlight))

    # ArduinoJson wordt al toegevoegd door web_server, dus we hoeven het niet meer toe te voegen

    # Add LittleFS support for ESP8266
    if CORE.is_esp8266:
        cg.add_library("LittleFS", None)
        # Configureer flash layout voor LittleFS (1MB app, 3MB filesystem)
        cg.add_platformio_option("board_build.filesystem", "littlefs")
        cg.add_platformio_option("board_build.ldscript", "eagle.flash.4m3m.ld")

    # Add TJpg_Decoder library for JPEG rendering
    cg.add_library("bodmer/TJpg_Decoder", "^1.0.8")

    # Add component directory to include path for SD.h stub
    cg.add_build_flag("-Isrc/esphome/components/smartclock_v2")