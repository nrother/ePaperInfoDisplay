import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core, pins
from esphome.components import display, spi
from esphome.const import (
    CONF_BUSY_PIN,
    CONF_DC_PIN,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_PAGES,
    CONF_RESET_PIN,
    CONF_CS_PIN,
    CONF_ENABLE_PIN,
    CONF_TEMPERATURE,
)

DEPENDENCIES = ["spi"]

pd_epaper_ns = cg.esphome_ns.namespace("pervasive_displays_epaper")
PervasiveDisplaysEPaperBase = pd_epaper_ns.class_(
    "PervasiveDisplaysEPaperBase", cg.PollingComponent, spi.SPIDevice, display.DisplayBuffer
)
PervasiveDisplaysEPaper = pd_epaper_ns.class_("PervasiveDisplaysEPaper", PervasiveDisplaysEPaperBase)
#PervasiveDisplaysEPaperBWR = pd_epaper_ns.class_(
#    "PervasiveDisplaysEPaperBWR", PervasiveDisplaysEPaperBase
#)
PervasiveDisplaysEPaper581In = pd_epaper_ns.class_(
    "PervasiveDisplaysEPaper581In", PervasiveDisplaysEPaper
)
PervasiveDisplaysEPaper741In = pd_epaper_ns.class_(
    "PervasiveDisplaysEPaper741In", PervasiveDisplaysEPaper
)

MODELS = {
    "5.81in": PervasiveDisplaysEPaper581In,
    "7.41in": PervasiveDisplaysEPaper741In,
}

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(PervasiveDisplaysEPaperBase),
            cv.Required(CONF_MODEL): cv.one_of(*MODELS, lower=True),
            cv.Required(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_TEMPERATURE, 25): cv.temperature,
        }
    )
    .extend(cv.polling_component_schema("1h"))
    .extend(spi.spi_device_schema()),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


async def to_code(config):
    model = MODELS[config[CONF_MODEL]]

    var = cg.Pvariable(config[CONF_ID], model.new(), model)
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    busy = await cg.gpio_pin_expression(config[CONF_BUSY_PIN])
    cg.add(var.set_busy_pin(busy))
    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))
    reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
    cg.add(var.set_reset_pin(reset))
    enable = await cg.gpio_pin_expression(config[CONF_ENABLE_PIN])
    cg.add(var.set_enable_pin(enable))
    cg.add(var.set_temperature(config[CONF_TEMPERATURE]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
