import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import mqtt
from esphome.const import CONF_ID
from esphome.core import CORE

DEPENDENCIES = ["wifi", "mqtt", "web_server"]


def AUTO_LOAD():
    # ESP32 uses web_server_idf, which has no Digest auth; p1_setup implements it with MD5.
    return ["md5"] if CORE.is_esp32 else []

p1_setup_ns = cg.esphome_ns.namespace("p1_setup")
P1Setup = p1_setup_ns.class_("P1Setup", cg.Component)

CONF_MQTT_ID = "mqtt_id"
CONF_DEVICE_NAME = "device_name"
CONF_LOGIN_USERNAME = "login_username"
CONF_LOGIN_PASSWORD = "login_password"


def validate_optional_setup_password(value):
    value = cv.string(value)
    if value == "":
        return value
    value = cv.Length(min=8, max=63)(value)
    if not re.fullmatch(r"[\x20-\x7e]+", value):
        raise cv.Invalid("Setup password must contain only printable ASCII characters")
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(P1Setup),
        cv.GenerateID(CONF_MQTT_ID): cv.use_id(mqtt.MQTTClientComponent),
        cv.Required(CONF_LOGIN_USERNAME): cv.All(cv.string, cv.Length(min=1, max=32)),
        cv.Optional(CONF_LOGIN_PASSWORD, default=""): validate_optional_setup_password,
        cv.Optional(CONF_DEVICE_NAME, default="ESPHome Device"): cv.All(cv.string, cv.Length(min=1, max=64)),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_mqtt_client(await cg.get_variable(config[CONF_MQTT_ID])))
    cg.add(var.set_device_name(config[CONF_DEVICE_NAME]))
    cg.add(var.set_login_credentials(config[CONF_LOGIN_USERNAME], config[CONF_LOGIN_PASSWORD]))
    cg.add_define("USE_WIFI_MANUAL_IP")
    cg.add_library("DNSServer", None)
    await cg.register_component(var, config)