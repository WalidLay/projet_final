#include <mbed.h>
#include "events/mbed_events.h"
#include "matrix.h"
#include "i2c.h"

/**** Mise en place de l'environnement ****/
EventQueue queue(32* EVENTS_EVENT_SIZE);
static DigitalOut led1(LED1);
static DigitalOut led2(LED2);
static DigitalOut led3(LED3);
Thread t_button;
InterruptIn lat(PC_13);
char nb_interrupt = 0;

void allum_led2(void){
    //wait(1);
    led2 = !led2;
}

void etein_led2(void){
    //wait(0.1);
    led2 = !led2;
}

void button_interrupt(void){
  nb_interrupt++;
}


/**** Gestion de l'affichage ***/

Queue <rgb_color,2> matrix_full;
Queue <rgb_color,2> matrix_empty;

Thread t_affichage;
Thread prep_affichage;

static rgb_color image1[64];
static rgb_color image2[64];

void blink_matrix(void){
  osEvent buf;
  rgb_color *pbuf;

  buf = matrix_empty.get();
  pbuf = (rgb_color*)buf.value.p;

  for (int i = 0 ; i < 64 ; i++){
    pbuf[i].r = 255;
    pbuf[i].b = 0;
    pbuf[i].g = 0;
  }

  matrix_full.put(pbuf);

  wait_ms(500);

  while(buf.status != (osStatus)osEventMessage){
    buf = matrix_empty.get();
  }

  pbuf = (rgb_color*)buf.value.p;
  for (int i = 0 ; i < 64 ; i++){
    pbuf[i].r = 0;
    pbuf[i].b = 255;
    pbuf[i].g = 0;
  }

  matrix_full.put(pbuf);

  wait_ms(500);
  
}

void affichage_thread(){
  matrix_init();

  osEvent current_buffer;
  static rgb_color *im;

  current_buffer = matrix_full.get(osWaitForever);

  do{
    current_buffer = matrix_full.get(osWaitForever);
  } while (current_buffer.status != osEventMessage);

  im = (rgb_color*)current_buffer.value.p;
  
  while(true){
    print_image(im);
    current_buffer = matrix_full.get(0);
    if(current_buffer.status == osEventMessage){
      matrix_empty.put(im);
      im = (rgb_color*)current_buffer.value.p;
    }
  }

}


int main() {

  i2c_init();

  /**** Mise en place de l'environnement
  for (;;) {
    led1 = !led1;
    lat = 1;
    //led2 = !led2;
    wait(1);
  }
    ****/

  /****Gestion des boutons *****/
  //début de l'event_queue 
  
  t_button.start(callback(&queue,&EventQueue::dispatch_forever));
  
  /**** Allumage led2 après event *****
  lat.rise(allum_led2);
  /**** Eteindre led2 ******
  lat.fall(queue.event(etein_led2));
  ***/
  
  lat.rise(button_interrupt);  

  //Eteind matrice de led
  lat.fall(queue.event(affichage_thread));


  /**** test matrix.cpp ****

  rgb_color one_frame[64];
  matrix_init();
  
  for (int i = 0 ; i < 64 ; i++){
    one_frame[i].b = 55;
    one_frame[i].g = 200;
    one_frame[i].r = 50;
  }
  for (;;){
  print_image(one_frame);
  //test_pixels();
  } */


  /**** Gestion de l'affichage **/
  
  matrix_empty.put(image1);
  matrix_empty.put(image2);
   
  t_affichage.start(callback(affichage_thread));
  for(;;){
    blink_matrix();
    //print_image(green_image);
    ThisThread::sleep_for(1000);
  }

}