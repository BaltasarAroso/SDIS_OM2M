#include "max30100.h"
#define CHECK_RD_PTR                                         \
  {                                                          \
    max30100_read_byte(MAX30100_REG_FIFO_READ, &aux_rd_ptr); \
    if (aux_rd_ptr != read_data)                             \
    {                                                        \
      i--;                                                   \
      continue;                                              \
    }                                                        \
  }

float red_dc = DC_REMOVER_ALPHA, ir_dc = DC_REMOVER_ALPHA;

float get_ir_dc();
float get_red_dc();
void max30100_set_led_width(uint8_t led_pulse_width);
void max30100_set_sampling(uint8_t sampling_rate);
void max30100_set_highres(uint8_t enabled);
esp_err_t max30100_check_sample(uint16_t *ir_data, uint16_t *red_data,
                                size_t data_len);
float step(float dcw, float x);
void max30100_test_conf();
esp_err_t max30100_set_mode(uint8_t mode);
esp_err_t max30100_set_leds(uint8_t red_c, uint8_t ir_c);
int max30100_hr_ready();
esp_err_t max30100_reset_fifo();

/*
 * Public functions
 */
esp_err_t max30100_init()
{
  i2c_port_t i2c_master_port = MAX30100_NUM;
  i2c_config_t conf;
  conf.mode = I2C_MODE_MASTER;
  conf.sda_io_num = I2C_SDA;
  conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
  conf.scl_io_num = I2C_SCL;
  conf.scl_pullup_en = GPIO_PULLUP_DISABLE;

  // Install drivers
  ESP_LOGI("MAX30100", "Installing drivers\n");
  ESP_ERROR_CHECK(i2c_driver_install(i2c_master_port, conf.mode));
  ESP_ERROR_CHECK(i2c_param_config(i2c_master_port, &conf));

#if defined(_DEBUG_) && defined(DEBUG_TEST)
  max30100_test_conf();
#endif
  // Configure sensor
  printf("Configuring sensor..\n");

#if defined(_DEBUG_) && defined(DEBUG_INIT)
  printf("Setting configuration: %x\n", MODE);
#endif

  max30100_set_mode(MODE);
  max30100_set_led_width(LED_PULSE_WIDTH);
  max30100_set_sampling(SAMPLING_RATE);

#if defined(_DEBUG_) && defined(DEBUG_INIT)
  printf("Setting leds currents\tIR: %x\tRED: %x -> %x\n", IR_LED_CURRENT,
         RED_LED_CURRENT, RED_LED_CURRENT << 4 | IR_LED_CURRENT);
#endif

  max30100_set_leds(RED_LED_CURRENT, IR_LED_CURRENT);

#if defined(_DEBUG_) && defined(DEBUG_INIT)
  uint8_t leds;
  esp_err_t ret = max30100_read_byte(MAX30100_REG_LED_CONFIGURATION, &leds);
  printf("Leds set: %x -> %d\n", leds, ret);
#endif

  max30100_set_highres(1);
  max30100_write_byte(MAX30100_REG_INTERRUPT_ENABLE, MAX30100_IE_ENB_A_FULL);
  max30100_write_byte(MAX30100_REG_INTERRUPT_ENABLE, MAX30100_IE_ENB_HR_RDY);
  max30100_write_byte(MAX30100_REG_INTERRUPT_ENABLE, MAX30100_IE_ENB_TEMP_RDY);

#if defined(_DEBUG_) && defined(DEBUG_INIT)
  printf("Reseting fifo\n");
#endif
  max30100_reset_fifo();

  ESP_LOGI("MAX30100", "Initialization done\n");

  return ESP_OK;
}

i2c_cmd_handle_t max30100_start(uint8_t reg_address, uint8_t mode)
{
  i2c_cmd_handle_t cmd;
  esp_err_t ret;

#if defined(_DEBUG_) && defined(DEBUG_I2C)
  printf("Starting link\n");
#endif

  cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);

#if defined(_DEBUG_) && defined(DEBUG_I2C)
  printf("Sending MAX30100 addr + mode: ");
#endif

  ret = i2c_master_write_byte(cmd, MAX30100_ADDR << 1 | mode, ACK_CHECK_EN);

#if defined(_DEBUG_) && defined(DEBUG_I2C)
  printf("%d\n", ret);
  printf("Sending reg_address: ");
#endif

  ret = i2c_master_write_byte(cmd, reg_address, ACK_CHECK_EN);

#if defined(_DEBUG_) && defined(DEBUG_I2C)
  printf("%d\n", ret);
#endif

  return cmd;
}
esp_err_t max30100_stop(i2c_cmd_handle_t cmd)
{
  esp_err_t ret;
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(MAX30100_NUM, cmd, 1000 / portTICK_RATE_MS);
  if(ret < 0) return ESP_FAIL;
  ESP_ERROR_CHECK(ret);

#if defined(_DEBUG_) && defined(DEBUG_I2C)
  printf("Sent queued commands: %d\n", ret);
#endif

  i2c_cmd_link_delete(cmd);

  return ret;
}

esp_err_t max30100_repeat_start(i2c_cmd_handle_t *cmd_)
{
  i2c_cmd_handle_t cmd = *cmd_;
  esp_err_t ret;

  // Terminate previous start
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(MAX30100_NUM, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);

  if (ret != ESP_OK)
    return ret;

  // repeat start to read fifo
  cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, MAX30100_ADDR << 1 | READ_BIT, ACK_CHECK_EN);

  *cmd_ = cmd;

  return ESP_OK;
}

esp_err_t max30100_read_byte(uint8_t read_reg, uint8_t *data)
{
  i2c_cmd_handle_t cmd;
  esp_err_t ret;
  cmd = max30100_start(read_reg, WRITE_BIT);
  ret = max30100_repeat_start(&cmd);
  if (ret < 0)
    return ESP_FAIL;
#if defined(_DEBUG_) && defined(DEBUG_I2C)
  printf("Read byte repeat start: %d\n", ret);
#endif

  ret = i2c_master_read(cmd, data, 1, LAST_NACK_VAL);

#if defined(_DEBUG_) && defined(DEBUG_I2C)
  printf("Read byte master read: %d -> %d\n", *data, ret);
#endif

  max30100_stop(cmd);

  return ret;
}

esp_err_t max30100_read_burst(uint8_t read_reg, uint8_t *data, uint8_t len)
{
  i2c_cmd_handle_t cmd;
  esp_err_t ret;
  cmd = max30100_start(read_reg, WRITE_BIT);
  ret = max30100_repeat_start(&cmd);

  ret = i2c_master_read(cmd, data, len, LAST_NACK_VAL);

  max30100_stop(cmd);

  return ret;
}

esp_err_t max30100_write_byte(uint8_t write_reg, uint8_t data)
{
  return max30100_write(write_reg, &data, 1);
}

esp_err_t max30100_write(uint8_t write_reg, uint8_t *data, size_t data_len)
{
  esp_err_t ret;
  i2c_cmd_handle_t cmd = max30100_start(write_reg, WRITE_BIT);

#if defined(_DEBUG_) && defined(DEBUG_I2C)
  printf("Writing data: ");
#endif

  ret = i2c_master_write(cmd, data, data_len, LAST_NACK_VAL);

#if defined(_DEBUG_) && defined(DEBUG_I2C)
  printf("%d\n", ret);
  printf("Stopping\n");
#endif

  ret = max30100_stop(cmd);
  return ret;
}

esp_err_t max30100_reset_fifo()
{
  max30100_write_byte(MAX30100_REG_FIFO_WRITE, 0);
  max30100_write_byte(MAX30100_REG_FIFO_OVERFLOW_COUNTER, 0);
  max30100_write_byte(MAX30100_REG_FIFO_READ, 0);

  return ESP_OK;
}

esp_err_t max30100_read_fifo(uint16_t *ir_data, uint16_t *red_data,
                             size_t *data_len)
{
  uint8_t NUM_SAMPLES_TO_READ;
  uint8_t read_data, write_data;

  *data_len = 0;
  max30100_read_byte(MAX30100_REG_FIFO_WRITE, &write_data);
  max30100_read_byte(MAX30100_REG_FIFO_READ, &read_data);
  NUM_SAMPLES_TO_READ = (write_data - read_data) & (MAX30100_FIFO_DEPTH - 1);

#if defined(_DEBUG_) && defined(DEBUG_FIFO)
  printf("Write\\read fifo:%d \t%d\n", write_data, read_data);
  printf("NUM SAMPLES: %d\n", NUM_SAMPLES_TO_READ);
#endif

  if (NUM_SAMPLES_TO_READ <= 0)
  {
    max30100_write_byte(MAX30100_REG_FIFO_READ, 0);
    return ESP_OK;
  }

  i2c_cmd_handle_t cmd;
  esp_err_t ret;
  int i;

#if defined(_DEBUG_) && defined(DEBUG_FIFO)
  printf("Reading FIFO\n");
#endif

  int read = 0;
  // IR[15:8] IR[7:0] RED[15:8] RED[7:0] -> One sample
  for (i = 0; i < NUM_SAMPLES_TO_READ; i++)
  {

    uint8_t data;
    uint8_t aux_rd_ptr;
    max30100_read_byte(MAX30100_REG_FIFO_READ, &read_data);
    //printf("Before RD PTR: %d\n", read_data);

    CHECK_RD_PTR
    max30100_read_byte(MAX30100_REG_FIFO_DATA, &data); // Read IR[15:8]
    ir_data[i] = (uint16_t)data << 8;                  // Save IR[15:8]
    //printf("data 1: %d\n", data);

    CHECK_RD_PTR
    max30100_read_byte(MAX30100_REG_FIFO_DATA, &data); // Read IR[7:0]
    ir_data[i] |= (uint16_t)data;                      // Save IR[7:0]
    //printf("data 2: %d\n", data);

    CHECK_RD_PTR
    max30100_read_byte(MAX30100_REG_FIFO_DATA, &data); // Read RED[15:8]
    red_data[i] = (uint16_t)data << 8;                 // Save RED[15:8]
    //printf("data 3: %d\n", data);

    CHECK_RD_PTR
    max30100_read_byte(MAX30100_REG_FIFO_DATA, &data); // Read RED[7:0]
    red_data[i] |= (uint16_t)data;                     // Save IR[7:0]
    //printf("data 4: %d\n", data);

    read++;
    max30100_read_byte(MAX30100_REG_FIFO_READ, &read_data);
    //printf("After RD PTR: %d\n", read_data);

#if defined(_DEBUG_) && defined(DEBUG_FIFO)
    //printf("IR[%d]: %d\tRED[%d]: %d\n", i, ir_data[i], i, red_data[i]);
#endif
  }

#if defined(_DEBUG_) && defined(DEBUG_FIFO)
  printf("Read fifo data length: %d\n", read);
#endif
  /*
  i2c_master_stop(cmd);
  i2c_cmd_link_delete(cmd);*/
  //max30100_stop(cmd);

  *data_len = read;
  /*
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(MAX30100_NUM, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);*/
  return ESP_OK;
}

esp_err_t max30100_update(uint16_t *ir_data, uint16_t *red_data,
                          size_t *data_len)
{

  //int ret = max30100_hr_ready();
  //printf("HR ready: %d\n",ret);
  //if(!max30100_hr_ready()) return ESP_OK;
  max30100_read_fifo(ir_data, red_data, data_len);

#if defined(_DEBUG_) && defined(DEBUG_FIFO)
  printf("Updated data length: %d\n", *data_len);
#endif
  max30100_check_sample(ir_data, red_data, *data_len);
  return ESP_OK;
}

esp_err_t max30100_start_tmp_sampling()
{
  uint8_t mode;

  ESP_ERROR_CHECK(max30100_read_byte(MAX30100_REG_MODE_CONFIGURATION, &mode));
  mode |= MAX30100_MC_TEMP_EN;

  return max30100_write_byte(MAX30100_REG_MODE_CONFIGURATION, mode);
}

int max30100_tmp_ready()
{
  uint8_t tmp_ready;
  max30100_read_byte(MAX30100_REG_MODE_CONFIGURATION, &tmp_ready);
  printf("tmp ready: %d\n", tmp_ready);

  return !(tmp_ready & MAX30100_MC_TEMP_EN);
}

int max30100_hr_ready()
{
  uint8_t hr_ready;
  max30100_read_byte(MAX30100_REG_INTERRUPT_STATUS, &hr_ready);

  return (hr_ready & MAX30100_IS_HR_RDY) != 0;
}

float max30100_tmp()
{
  uint8_t tmp;
  float temp_frac;

  max30100_read_byte(MAX30100_REG_TEMPERATURE_DATA_INT, &tmp);
  max30100_read_byte(MAX30100_REG_TEMPERATURE_DATA_FRAC, (uint8_t *)&temp_frac);

  return temp_frac * 0.0625 + tmp;
}

esp_err_t max30100_shutdown()
{
  uint8_t mode;
  max30100_read_byte(MAX30100_REG_MODE_CONFIGURATION, &mode);
  mode |= MAX30100_MC_SHDN;

  return max30100_write_byte(MAX30100_REG_MODE_CONFIGURATION, mode);
}

esp_err_t max30100_resume()
{
  uint8_t mode;
  max30100_read_byte(MAX30100_REG_MODE_CONFIGURATION, &mode);
  mode &= ~MAX30100_MC_SHDN;

  return max30100_write_byte(MAX30100_REG_MODE_CONFIGURATION, mode);
}

esp_err_t max30100_get_id(uint8_t *part_id)
{
  return max30100_read_byte(0xff, part_id);
}

void max30100_adjust_current()
{
  // Follower that adjusts the red led current in order to have comparable DC
  // baselines between red and IR leds. The numbers are really magic: the less
  // possible to avoid oscillations
  uint8_t led_conf, red_current;
  max30100_read_byte(MAX30100_REG_LED_CONFIGURATION, &led_conf);
  red_current = led_conf >> 4;
  led_conf &= 0x0f;

  if (ir_dc - red_dc > 70000 && red_current < RED_LED_CURRENT)
  {
    ++red_current;
  }
  else if (red_dc - ir_dc > 70000 && red_current > 0)
  {
    --red_current;
  }
  max30100_write_byte(MAX30100_REG_LED_CONFIGURATION,
                      red_current << 4 | led_conf);

#if defined(DEBUG) && defined(DEBUG_I2C)
  printf("I: %d\n", red_current);
#endif
}

/*
 * Private functions
 */
esp_err_t max30100_check_sample(uint16_t *ir_data, uint16_t *red_data,
                                size_t data_len)
{
  int i;
  for (i = 1; i < data_len; i++)
  {
    red_data[i] = (uint16_t)step(red_data[i], red_data[i - 1]);
    ir_data[i] = (uint16_t)step(ir_data[i], ir_data[i - 1]);
  }
  return ESP_OK;
}
float step(float x, float prev_w)
{
  float oldcw = x + DC_REMOVER_ALPHA * prev_w;
  float dcw = (float)oldcw - prev_w;

  return dcw;
}
void max30100_set_led_width(uint8_t led_pulse_width)
{
#if defined(_DEBUG_) && defined(DEBUG_INIT)
  printf("Setting led pulse width: %d\n", led_pulse_width);
#endif
  uint8_t previous;
  esp_err_t ret;

  ret = max30100_read_byte(MAX30100_REG_SPO2_CONFIGURATION, &previous);

#if defined(_DEBUG_) && defined(DEBUG_INIT)
  printf("Led pulse width read config: previous %x -> return: %d\n", previous, ret);
#endif

  ret = max30100_write_byte(MAX30100_REG_SPO2_CONFIGURATION,
                            (previous & 0xfc) | led_pulse_width);

#if defined(_DEBUG_) && defined(DEBUG_INIT)
  printf("Led pulse width write: %d\n", ret);
  ret = max30100_read_byte(MAX30100_REG_SPO2_CONFIGURATION, &previous);
  printf("Led pulse width written: %x\n", previous);
#endif
}

void max30100_set_sampling(uint8_t sampling_rate)
{
#if defined(_DEBUG_) && defined(DEBUG_INIT)
  printf("Setting sampling rate: %d\n", sampling_rate);
#endif

  uint8_t previous;
  esp_err_t ret;
  ret = max30100_read_byte(MAX30100_REG_SPO2_CONFIGURATION, &previous);

#if defined(_DEBUG_) && defined(DEBUG_INIT)
  printf("Sampling rate reading conf:%x -> %d\n", previous, ret);
#endif

  ret = max30100_write_byte(MAX30100_REG_SPO2_CONFIGURATION,
                            (previous & 0xe3) | (sampling_rate << 2));

#if defined(_DEBUG_) && defined(DEBUG_INIT)
  printf("Sampling rate write: %d\n", ret);
#endif
}

void max30100_set_highres(uint8_t enabled)
{
#if defined(_DEBUG_) && defined(DEBUG_INIT)
  printf("Setting High resolution mode: %d\n", enabled);
#endif
  uint8_t previous;
  esp_err_t ret;

  ret = max30100_read_byte(MAX30100_REG_SPO2_CONFIGURATION, &previous);
#if defined(_DEBUG_) && defined(DEBUG_INIT)
  printf("Reading previous conf:%x -> %d\n", previous, ret);
#endif

  uint8_t mode =
      enabled ? MAX30100_SPC_SPO2_HI_RES_EN : ~MAX30100_SPC_SPO2_HI_RES_EN;
  ret = max30100_write_byte(MAX30100_REG_SPO2_CONFIGURATION, previous | mode);

#if defined(_DEBUG_) && defined(DEBUG_INIT)
  printf("Setting mode: %x -> %d\n", enabled, ret);
#endif
}

esp_err_t max30100_set_mode(uint8_t mode)
{
  return max30100_write_byte(MAX30100_REG_MODE_CONFIGURATION, mode);
}
esp_err_t max30100_set_leds(uint8_t red_c, uint8_t ir_c)
{
  return max30100_write_byte(MAX30100_REG_LED_CONFIGURATION, red_c << 4 | ir_c);
}
void max30100_test_conf()
{
  printf("Enabling HR/SPO2 mode..\n");
  ESP_ERROR_CHECK(max30100_set_mode(MAX30100_MODE_SPO2_HR));
  printf("done.\n");

  printf("Configuring LEDs biases to 50mA..\n");
  ESP_ERROR_CHECK(
      max30100_set_leds(MAX30100_LED_CURR_50MA, MAX30100_LED_CURR_50MA));
  printf("done.\n");

  printf("Lowering the current to 7.6mA..\n");
  ESP_ERROR_CHECK(
      max30100_set_leds(MAX30100_LED_CURR_7_6MA, MAX30100_LED_CURR_7_6MA));
  printf("done.\n");

  printf("Shutting down..\n");
  ESP_ERROR_CHECK(max30100_shutdown());
  printf("done.\n");

  printf("Resuming normal operation..\n");
  ESP_ERROR_CHECK(max30100_resume());
  printf("done.\n");

  printf("Sampling die temperature..\n");
  ESP_ERROR_CHECK(max30100_start_tmp_sampling());
  while (!max30100_tmp_ready())
    ;
  printf("done.\n");

  float temperature = max30100_tmp();
  printf("done, temp=%fC\n", temperature);

  if (temperature < 5)
  {
    printf("WARNING: Temperature probe reported an odd value\n");
  }
  else
  {
    printf("All test pass.\n");
  }
}
