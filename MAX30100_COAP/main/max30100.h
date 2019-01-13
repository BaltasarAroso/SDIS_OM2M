#ifndef MAX30100_H
#define MAX30100_H

#include "driver/i2c.h"
#include "esp_log.h"

#define I2C_SCL GPIO_NUM_5 /*!< gpio number for I2C master clock */
#define I2C_SDA GPIO_NUM_4 /*!< gpio number for I2C master data  */

#define MAX30100_ADDR 0x57
#define MAX30100_NUM I2C_NUM_0

#define WRITE_BIT I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ   /*!< I2C master read */
#define ACK_CHECK_EN 0x1           /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0      /*!< I2C master will not check ack from slave */
#define ACK_VAL I2C_MASTER_ACK /*!< I2C ack value */
#define NACK_VAL I2C_MASTER_NACK           /*!< I2C nack value */
#define LAST_NACK_VAL I2C_MASTER_LAST_NACK /*!< I2C last_nack value */

#define MAX30100_FIFO_DEPTH 0x10

// FIFO control and data registers
#define MAX30100_REG_FIFO_WRITE 0x02
#define MAX30100_REG_FIFO_OVERFLOW_COUNTER 0x03
#define MAX30100_REG_FIFO_READ 0x04
#define MAX30100_REG_FIFO_DATA 0x05
#define MAX30100_REG_LED_CONFIGURATION 0x09
#define MAX30100_REG_MODE_CONFIGURATION 0x06
#define MAX30100_REG_SPO2_CONFIGURATION 0x07
#define MAX30100_REG_TEMPERATURE_DATA_INT 0x16
#define MAX30100_REG_TEMPERATURE_DATA_FRAC 0x17

#define MAX30100_MC_TEMP_EN (1 << 3)
#define MAX30100_MC_RESET (1 << 6)
#define MAX30100_MC_SHDN (1 << 7)

#define MAX30100_LED_CURR_50MA 0x0f
#define MAX30100_LED_CURR_7_6MA 0x02
#define MAX30100_LED_CURR_27_1MA 0x08
#define MAX30100_SPC_SPO2_HI_RES_EN (1 << 6)

#define LED_PULSE_WIDTH 0x03                      // 1600US 16BITS
#define SAMPLING_RATE 0x01                        // 100Hz
#define RED_LED_CURRENT MAX30100_LED_CURR_27_1MA  // 27mA
#define IR_LED_CURRENT MAX30100_LED_CURR_50MA     // 50mA

#define CURRENT_ADJUSTMENT_PERIOD_MS 500
#define DC_REMOVER_ALPHA 0.95

// Interrupts
#define MAX30100_REG_INTERRUPT_STATUS 0x00
#define MAX30100_IS_PWR_RDY (1 << 0)
#define MAX30100_IS_SPO2_RDY (1 << 4)
#define MAX30100_IS_HR_RDY (1 << 5)
#define MAX30100_IS_TEMP_RDY (1 << 6)
#define MAX30100_IS_A_FULL (1 << 7)

// Enable interrupts
#define MAX30100_REG_INTERRUPT_ENABLE 0x01
#define MAX30100_IE_ENB_SPO2_RDY (1 << 4)
#define MAX30100_IE_ENB_HR_RDY (1 << 5)
#define MAX30100_IE_ENB_TEMP_RDY (1 << 6)
#define MAX30100_IE_ENB_A_FULL (1 << 7)

#define DEBUG_I2C
//#define DEBUG_INIT
//#define DEBUG_TEST
#define DEBUG_FIFO
#define _DEBUG_

#define MAX30100_MODE_HRONLY 0x02   // Heart rate only
#define MAX30100_MODE_SPO2_HR 0x03  // Enable SpO2 monitor

#define MODE MAX30100_MODE_SPO2_HR

esp_err_t max30100_init();
i2c_cmd_handle_t max30100_start(uint8_t reg_addr, uint8_t mode);
esp_err_t max30100_stop(i2c_cmd_handle_t cmd);

esp_err_t max30100_read_fifo(uint16_t *ir_data, uint16_t *red_data,
                             size_t *data_len);
esp_err_t max30100_write(uint8_t write_reg, uint8_t *data, size_t data_len);
esp_err_t max30100_write_byte(uint8_t write_reg, uint8_t data);
esp_err_t max30100_read_byte(uint8_t read_reg, uint8_t *data);

esp_err_t max30100_start_tmp_sampling();
int max30100_tmp_ready();
float max30100_tmp();
esp_err_t max30100_shutdown();
esp_err_t max30100_resume();
esp_err_t max30100_get_id(uint8_t *part_id);
esp_err_t max30100_update(uint16_t *ir_data, uint16_t *red_data,
                          size_t *data_len);
void max30100_adjust_current();

#endif