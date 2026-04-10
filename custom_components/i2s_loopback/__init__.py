import esphome.codegen as cg
from esphome.components.esp32 import include_builtin_idf_component
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32"]

i2s_loopback_ns = cg.esphome_ns.namespace("i2s_loopback")
I2SLoopback = i2s_loopback_ns.class_("I2SLoopback", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(I2SLoopback),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    # Properly register ESP-IDF I2S driver dependency
    include_builtin_idf_component("driver")
    include_builtin_idf_component("esp_driver_i2s")
