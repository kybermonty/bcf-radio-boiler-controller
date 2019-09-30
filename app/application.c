#include <application.h>

#define TEMPERATURE_PUB_NO_CHANGE_INTEVAL (15 * 60 * 1000)
#define TEMPERATURE_PUB_VALUE_CHANGE 0.2f
#define TEMPERATURE_UPDATE_INTERVAL (10 * 1000)

#define FLOOD_DETECTOR_NO_CHANGE_INTEVAL (15 * 60 * 1000)
#define FLOOD_DETECTOR_UPDATE_INTERVAL  (5 * 1000)

// LED instance
bc_led_t led;

// Thermometer instance
bc_tmp112_t tmp112;
static event_param_t temperature_event_param = { .next_pub = 0 };

// Flood detector instance
bc_flood_detector_t flood_detector;
event_param_t flood_detector_event_param = { .next_pub = 0 };

void bc_radio_node_on_state_get(uint64_t *id, uint8_t state_id)
{
    (void) id;

    if (state_id == BC_RADIO_NODE_STATE_POWER_MODULE_RELAY)
    {
        bool state = bc_module_power_relay_get_state();

        bc_radio_pub_state(BC_RADIO_PUB_STATE_POWER_MODULE_RELAY, &state);
    }
}

void bc_radio_node_on_state_set(uint64_t *id, uint8_t state_id, bool *state)
{
    (void) id;

    if (state_id == BC_RADIO_NODE_STATE_POWER_MODULE_RELAY)
    {
        bc_module_power_relay_set_state(*state);

        bc_radio_pub_state(BC_RADIO_PUB_STATE_POWER_MODULE_RELAY, state);
    }
}

void tmp112_event_handler(bc_tmp112_t *self, bc_tmp112_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event == BC_TMP112_EVENT_UPDATE)
    {
        if (bc_tmp112_get_temperature_celsius(self, &value))
        {
            if ((fabsf(value - param->value) >= TEMPERATURE_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
            {
                bc_radio_pub_temperature(param->channel, &value);

                param->value = value;
                param->next_pub = bc_scheduler_get_spin_tick() + TEMPERATURE_PUB_NO_CHANGE_INTEVAL;
            }
        }
    }
}

void flood_detector_event_handler(bc_flood_detector_t *self, bc_flood_detector_event_t event, void *event_param)
{
    bool is_alarm;
    event_param_t *param = (event_param_t *)event_param;


    if (event == BC_FLOOD_DETECTOR_EVENT_UPDATE)
    {
       is_alarm = bc_flood_detector_is_alarm(self);

       if ((is_alarm != param->value) || (param->next_pub < bc_scheduler_get_spin_tick()))
       {
           bc_radio_pub_bool("flood-detector/a/alarm", &is_alarm);

           param->value = is_alarm;
           param->next_pub = bc_scheduler_get_spin_tick() + FLOOD_DETECTOR_NO_CHANGE_INTEVAL;
       }
    }
}

void application_init(void)
{
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_OFF);

    bc_radio_init(BC_RADIO_MODE_NODE_LISTENING);

    bc_module_power_init();

    // Initialize thermometer sensor on core module
    temperature_event_param.channel = BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE;
    bc_tmp112_init(&tmp112, BC_I2C_I2C0, 0x49);
    bc_tmp112_set_event_handler(&tmp112, tmp112_event_handler, &temperature_event_param);
    bc_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_INTERVAL);

    // Initialize flood detector
    bc_flood_detector_init(&flood_detector, BC_FLOOD_DETECTOR_TYPE_LD_81_SENSOR_MODULE_CHANNEL_A);
    bc_flood_detector_set_event_handler(&flood_detector, flood_detector_event_handler, &flood_detector_event_param);
    bc_flood_detector_set_update_interval(&flood_detector, FLOOD_DETECTOR_UPDATE_INTERVAL);

    bc_radio_pairing_request("boiler-controller", VERSION);

    bc_led_pulse(&led, 2000);
}
