#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/i2c-dev.h>


int main(int argc, char **argv) {
  int file;
  int addr = 0x2A; /* The I2C address */
  char buf[10];

  /* Open I2C bus */
  file = open("/dev/i2c-2", O_RDWR);
  if (file < 0) {
    perror("open");
    exit(1);
  }

  /* Set I2C client address */
  if (ioctl(file, I2C_SLAVE, addr) < 0) {
    perror("ioctl");
    exit(1);
  }

  /* Transfer WRITE */
  char cmd;
  
  /* Issu de l'exemple
  buf[0] = 0xDE;
  buf[1] = 0xAD;
  buf[2] = 0xBE;
  buf[3] = 0xEF;
  */



  //J'envoie d'abord la commande
  cmd = 06;
  if (write(file, &cmd, 1) != 1) {
    perror("write");
    exit(1);
  }
  //Puis j'envoie mon action
  //allume moi toutes les leds
  buf[0] = 07;
  if (write(file, buf, 1) != 1) {
    perror("write");
    exit(1);
  }

  sleep(5);

  //J'envoie d'abord la commande
  cmd = 0x83;
  if (write(file, &cmd, 1) != 1) {
    perror("write");
    exit(1);
  }
  //Puis j'envoie mon action
  //renvoit moi l'adresse de l'esclave
  if (read(file, buf, 1) != 1) {
    perror("write");
    exit(1);
  } 
  printf("l'adresse de l'esclave est %X \n", buf[0]);



  /**** Modèle à garder pour faire les lectures/écritures vers le slave ***

  if (write(file, buf, 4) != 4) {
    perror("write");
    exit(1);
  }

  /* Transfer READ *
  if (read(file, buf, 1) != 1) {
    perror("read");
    exit(1);
  }
  */

  return 0;
}
