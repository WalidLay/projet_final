#ifndef I2C_H
#define I2C_H


extern void loop(void);
void i2c_init(void);
int answer_read_request(const char *data, size_t len);
int get_write_request_paramaters(char *data, size_t len);
void handle_command(char);

#endif
