#include "application_config.h"
#include "ruuvi_boards.h"
#include "ruuvi_driver_error.h"
#include "ruuvi_driver_sensor.h"
#include "ruuvi_interface_bme280.h"
#include "ruuvi_interface_shtcx.h"
#include "ruuvi_interface_environmental_mcu.h"
#include "ruuvi_interface_log.h"
#include "test_sensor.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#define RETURN_ON_ERROR(status) if(RUUVI_DRIVER_SUCCESS != status) { return status; }

// Use own file for each test run to track the source of any errors.
static ruuvi_driver_status_t test_run(ruuvi_driver_sensor_init_fp init, ruuvi_driver_bus_t bus, uint8_t handle)
{
  ruuvi_driver_status_t err_code = RUUVI_DRIVER_SUCCESS;
  #ifdef RUUVI_RUN_TESTS
    err_code = test_sensor_init(init, bus, handle);
    RUUVI_DRIVER_ERROR_CHECK(err_code, RUUVI_DRIVER_ERROR_SELFTEST | RUUVI_DRIVER_ERROR_NOT_FOUND);
    RETURN_ON_ERROR(err_code);

    err_code = test_sensor_setup(init, bus, handle);
    RUUVI_DRIVER_ERROR_CHECK(err_code, RUUVI_DRIVER_ERROR_SELFTEST);

    err_code = test_sensor_modes(init, bus, handle);
    RUUVI_DRIVER_ERROR_CHECK(err_code, RUUVI_DRIVER_ERROR_SELFTEST);
  #endif
  return err_code;
}

ruuvi_driver_status_t test_environmental_run(void)
{
  ruuvi_driver_status_t err_code = RUUVI_DRIVER_SUCCESS;
  ruuvi_driver_bus_t bus = RUUVI_DRIVER_BUS_NONE;
  uint8_t handle = 0;

  #if RUUVI_BOARD_ENVIRONMENTAL_BME280_PRESENT
    err_code = RUUVI_DRIVER_SUCCESS;
    
    #if RUUVI_BOARD_ENVIRONMENTAL_BME280_SPI_USE
    bus = RUUVI_DRIVER_BUS_SPI;
    handle = RUUVI_BOARD_SPI_SS_ENVIRONMENTAL_PIN;
    err_code = test_run(ruuvi_interface_bme280_init, bus, handle);
    RUUVI_DRIVER_ERROR_CHECK(err_code, RUUVI_DRIVER_ERROR_SELFTEST | RUUVI_DRIVER_ERROR_NOT_FOUND);
    #endif

    #if RUUVI_BOARD_ENVIRONMENTAL_BME280_I2C_USE
    bus = RUUVI_DRIVER_BUS_I2C;
    handle = RUUVI_BOARD_BME280_I2C_ADDRESS;
    err_code = test_run(ruuvi_interface_bme280_init, bus, handle);
    RUUVI_DRIVER_ERROR_CHECK(err_code, RUUVI_DRIVER_ERROR_SELFTEST);
    #endif 
  #endif

  #if RUUVI_BOARD_ENVIRONMENTAL_SHTCX_PRESENT
    err_code = RUUVI_DRIVER_SUCCESS;
    bus = RUUVI_DRIVER_BUS_I2C;
    handle = RUUVI_BOARD_SHTCX_I2C_ADDRESS;
    err_code = test_run(ruuvi_interface_shtcx_init, bus, handle);
    RUUVI_DRIVER_ERROR_CHECK(err_code, RUUVI_DRIVER_ERROR_SELFTEST | RUUVI_DRIVER_ERROR_NOT_FOUND);
  #endif

  #if RUUVI_BOARD_ENVIRONMENTAL_MCU_PRESENT
    bus = RUUVI_DRIVER_BUS_NONE;
    handle = 0;
    err_code = test_run(ruuvi_interface_environmental_mcu_init, bus, handle);
    RUUVI_DRIVER_ERROR_CHECK(err_code, RUUVI_DRIVER_ERROR_SELFTEST);
  #endif
  // Return error if usable environmental sensor was not found.
  return err_code;
}