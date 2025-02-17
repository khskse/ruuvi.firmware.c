/**
 * Ruuvi Firmware 3.x code. Reads the sensors onboard RuuviTag and broadcasts the sensor data in a manufacturer specific format.
 *
 * License: BSD-3
 * Author: Otso Jousimaa <otso@ojousima.net>
 **/

#include "application_config.h"
#include "ruuvi_interface_log.h"
#include "ruuvi_interface_scheduler.h"
#include "ruuvi_interface_watchdog.h"
#include "ruuvi_interface_yield.h"
#include "ruuvi_boards.h"
#include "task_acceleration.h"
#include "task_adc.h"
#include "task_advertisement.h"
#include "task_button.h"
#include "task_environmental.h"
#include "task_flash.h"
#include "task_gatt.h"
#include "task_gpio.h"
#include "task_led.h"
#include "task_i2c.h"
#include "task_nfc.h"
#include "task_power.h"
#include "task_pressure.h"
#include "task_rtc.h"
#include "task_scheduler.h"
#include "task_spi.h"
#include "task_timer.h"
#include "test_sensor.h"
#include "test_acceleration.h"
#include "test_adc.h"
#include "test_environmental.h"
#include "test_library.h"

#include <stdio.h>

/** Run tests which rely only on MCU. 
 *  These tests require relevant peripherals being uninitialized
 *  before tests and leave the peripherals uninitialized.
 *  Production firmware should not run these tests. 
 */
static void run_mcu_tests()
{
  #if RUUVI_RUN_TESTS
  test_adc_run();
  test_library_run();
  #endif
}

/*
 * Initialize MCU peripherals. 
 */
static void init_mcu(void)
{
// Init logging
  ruuvi_driver_status_t status = RUUVI_DRIVER_SUCCESS;
  status |= ruuvi_interface_log_init(APPLICATION_LOG_LEVEL);
  RUUVI_DRIVER_ERROR_CHECK(status, RUUVI_DRIVER_SUCCESS);
  ruuvi_interface_log(RUUVI_INTERFACE_LOG_INFO, "Program start \r\n");
  // Init yield
  status = ruuvi_interface_yield_init();
  RUUVI_DRIVER_ERROR_CHECK(status, RUUVI_DRIVER_SUCCESS);
  // Init GPIO
  status = task_gpio_init();
  // Initialize LED gpio pins
  status |= task_led_init();
  RUUVI_DRIVER_ERROR_CHECK(status, RUUVI_DRIVER_SUCCESS);
  // Initialize SPI, I2C
  status |= task_spi_init();
  status |= task_i2c_init();
  RUUVI_DRIVER_ERROR_CHECK(status, RUUVI_DRIVER_SUCCESS);
  // Initialize RTC, timer and scheduler. Enable low-power sleep.
  status |= task_rtc_init();
  status |= task_timer_init();
  status |= task_scheduler_init();
  status |= ruuvi_interface_yield_low_power_enable(true);
  RUUVI_DRIVER_ERROR_CHECK(status, RUUVI_DRIVER_SUCCESS);
  // Initialize power
  status |= task_power_dcdc_init();
  RUUVI_DRIVER_ERROR_CHECK(status, RUUVI_DRIVER_SUCCESS);
  // Initialize ADC
  status |= task_adc_init();
  RUUVI_DRIVER_ERROR_CHECK(status, RUUVI_DRIVER_SUCCESS);
  // Initialize flash
  status = task_flash_init();
  status |= task_flash_demo();
  RUUVI_DRIVER_ERROR_CHECK(status, RUUVI_DRIVER_SUCCESS);
}

/*
 * Run series of selftests which verify the underlying drivers, libraries etc.
 */
static void run_sensor_tests(void)
{
  #if RUUVI_RUN_TESTS
  // Tests will initialize and uninitialize the sensors, run this before using them in application
  ruuvi_interface_log(RUUVI_INTERFACE_LOG_INFO,
                      "Running extended self-tests, this might take a while\r\n");
  test_acceleration_run();
  test_environmental_run();


  // Print unit test status, activate tests by building in DEBUG configuration under SES
  size_t tests_run, tests_passed;
  test_sensor_status(&tests_run, &tests_passed);
  char message[128] = {0};
  snprintf(message, sizeof(message), "Tests ran: %u, passed: %u\r\n", tests_run,
           tests_passed);
  ruuvi_interface_log(RUUVI_INTERFACE_LOG_INFO, message);
  // Init watchdog after tests. Normally init at the start of the program
  #endif
  ruuvi_interface_watchdog_init(APPLICATION_WATCHDOG_INTERVAL_MS);
}

static void init_sensors(void)
{
  ruuvi_driver_status_t status = RUUVI_DRIVER_SUCCESS;
  // Initialize environmental- nRF52 will return ERROR NOT SUPPORTED on RuuviTag basic
  // if DSP was configured, log warning
  status = task_environmental_init();
  ruuvi_interface_environmental_data_t data;
  task_environmental_data_get(&data);
  RUUVI_DRIVER_ERROR_CHECK(status, RUUVI_DRIVER_ERROR_NOT_SUPPORTED);
  // Allow NOT FOUND in case we're running on basic model
  status = task_acceleration_init();
  RUUVI_DRIVER_ERROR_CHECK(status, RUUVI_DRIVER_ERROR_NOT_FOUND);
}

static void init_comms(void)
{
  ruuvi_driver_status_t status = RUUVI_DRIVER_SUCCESS;
  #if APPLICATION_BUTTON_ENABLED
  // Initialize button with on_button task
  status = task_button_init(RUUVI_INTERFACE_GPIO_SLOPE_HITOLO, task_button_on_press);
  RUUVI_DRIVER_ERROR_CHECK(status, RUUVI_DRIVER_SUCCESS);
  #endif

  #if APPLICATION_COMMUNICATION_NFC_ENABLED
  // Initialize nfc
  status |= task_nfc_init();
  RUUVI_DRIVER_ERROR_CHECK(status, RUUVI_DRIVER_SUCCESS);
  #endif

  #if APPLICATION_COMMUNICATION_BLUETOOTH_ENABLED
  // Initialize BLE - and start advertising.
  status = task_advertisement_init();
  status |= task_gatt_init();
  status |= task_advertisement_start();
  RUUVI_DRIVER_ERROR_CHECK(status, RUUVI_DRIVER_SUCCESS);
  #endif
}

int main(void)
{
  run_mcu_tests();  // Runs tests which do not rely on MCU peripherals being initialized
  init_mcu();       // Initialize MCU peripherals, except for communication with users. 

  task_led_write(RUUVI_BOARD_LED_ACTIVITY, TASK_LED_ON); // Turn activity led on
  run_sensor_tests(); // Run tests which rely on MCU peripherals, for example sensor drivers
  init_sensors();     // Initializes sensors with application-defined default mode.
   // Initialize communication with outside world - BLE, NFC, Buttons
   /* IMPORTANT! After this point pausing the program flow asserts,
    * since softdevice asserts on missed timer. This includes debugger.
    */
  // run comms tests - TODO
  init_comms();    

  // Turn activity led off. Turn status_ok led on if no errors occured
  task_led_write(RUUVI_BOARD_LED_ACTIVITY, TASK_LED_OFF);
  task_led_activity_led_set(RUUVI_BOARD_LED_ACTIVITY);
  if(RUUVI_DRIVER_SUCCESS == ruuvi_driver_errors_clear())
  {
    task_led_write(RUUVI_BOARD_LED_STATUS_OK, TASK_LED_ON);
    task_led_activity_led_set(RUUVI_BOARD_LED_STATUS_OK);
    ruuvi_interface_delay_ms(1000);
  }

  // Turn LEDs off
  task_led_write(RUUVI_BOARD_LED_STATUS_OK, TASK_LED_OFF);
  // Configure activity indication
  ruuvi_interface_yield_indication_set(task_led_activity_indicate);
  //RUUVI_DRIVER_ERROR_CHECK(1, RUUVI_DRIVER_SUCCESS);
  while(1)
  {
    // Sleep
    ruuvi_interface_yield();
    // Execute scheduled tasks
    ruuvi_interface_scheduler_execute();
  }
}