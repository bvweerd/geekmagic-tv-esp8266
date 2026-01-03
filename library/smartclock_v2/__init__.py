"""ESPHome SmartClock V2 - Using AsyncWebServer"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE

DEPENDENCIES = ["network", "web_server_base"]

smartclock_ns = cg.esphome_ns.namespace("smartclock_v2")
SmartClockV2Component = smartclock_ns.class_("SmartClockV2Component", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SmartClockV2Component),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    """Generate C++ code for the component"""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Add ArduinoJson library
    cg.add_library("bblanchon/ArduinoJson", "^7.0.0")

    # Add LittleFS support for ESP8266
    if CORE.is_esp8266:
        cg.add_library("LittleFS", None)
