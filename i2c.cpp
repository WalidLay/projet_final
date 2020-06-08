#include <mbed.h>
#include <stdint.h>
#include "events/mbed_events.h"
#include "matrix.h"
#include "i2c.h"

static char SLAVE_ADDR = 0x2A;
static char scratchpad[4];
extern DigitalOut led1(LED1);
extern DigitalOut led2(LED2);
extern DigitalOut led3(LED3);
extern InterruptIn lat;
extern Queue <rgb_color,2> matrix_full;
extern Queue <rgb_color,2> matrix_empty;
extern char nb_interrupt;
Thread i2c_thread;


I2CSlave slave(I2C_SDA,I2C_SCL);

//Configuration initiales GPIO
void i2c_init (){
    printf("Appel de la fonction I2C_init\n");
    slave.address(SLAVE_ADDR<<1);

    //Au début, le scratchpad doit être initié à 0
    for (int i = 0 ; i < 4 ; i ++){
        scratchpad[i] = 0 ;
    }
    i2c_thread.start(callback(loop));
}

static int wait_for_request(){
    int req;
    printf("Appel de la fonction wait_for_request\n");

    //attend une transaction I2C
    //renvoie 0 si requête en lecture ou 1 en écriture
    while(1) {
        req = slave.receive();
        switch (req) {
            case I2CSlave::ReadAddressed:
                return 0;
            case I2CSlave::WriteGeneral:
                return 1;
            case I2CSlave::WriteAddressed:
                return 1;
        }
    }
}

int answer_read_request(const char *data, size_t len){
    int res;
    int recv;
    printf("Appel de la fonction answer_read_request\n");

    //attend une requête en lecture (en utilisant wait_for_request)
    recv = wait_for_request();
    
    //Si une requête en écriture arrive à la place
    //Ou si une erreur d'écriture de data, return 1
    //return 0 si tout s'est bien passé
    if (recv == 1){
        return 1;
    }
    else {
        res = slave.write(data, len);
        return res;
    }
}

int get_write_request_paramaters(char *data, size_t len){
    int res;
    int recv;
    //attend une requête en écriture(idem précédement)
    recv = wait_for_request();
    
    //Si une requête en écriture s'est bien passé, return 0.
    //Sinon (requête lecture ou erreur de lecture de data), return 1;
    if (recv == 0){
        return 1;
    }
    else {
        res = slave.read(data, len);
        return res;
    }
}

void loop(){
    int res ;
    char cmd;

    //attend une transation en écriture sur l'I2C (en utilisant wait_for_request)
    while(1){
        res = wait_for_request();
        switch (res){
            //Traite la commande entrante en appelant une fct handle_command
            case 1 :
                slave.read(&cmd,1);
                handle_command(cmd);
            break;
            case 0:
                printf("transaction en lecture, on continue la loop \n");
            break;
        }
    }
    
}

void handle_command(char cmd){
    char res;
    char data;
    rgb_color *image;
    osEvent event;
    

    switch(cmd){
        //Contenu à écrire dans le scratchpad
        case 0x02:
            res = get_write_request_paramaters(scratchpad,4);
            //mettre un warning si res diff de 0, alors erreur
            if(res != 0){
                printf("Erreur sur la lecture/ecriture\n ");
            }
        break;
        //Le bit 0 (poids faible) est écrit dans LED1, le bit 1 dans LED2 et le bit 2 dans LED3
        case 0x06:
            res = get_write_request_paramaters(&data, 1);
            led1 = ((data & 0x01)!=0);
            led2 = ((data & 0x02)!=0);
            led3 = ((data & 0x04)!=0);
            if(res != 0){
                printf("Erreur sur la lecture/ecriture\n ");
            }
        break;
        //Image à afficher sur la matrice de leds
        case 0x10:
            do{
                event = matrix_empty.get();
            }while(event.status != (osStatus)osEventMessage);

            image = (rgb_color*)event.value.p;

            res = get_write_request_paramaters((char*)image, 192);

            matrix_full.put(image);
            if(res != 0){
                printf("Erreur sur la lecture/ecriture\n ");
            }

        break;
        //Nombre d'appui sur le bouton BUTTON1 depuis la dernière lecture
        case 0x80:
            res = answer_read_request(&nb_interrupt,1);
            if(res != 0){
                printf("Erreur sur la lecture/ecriture\n ");
            }
            
        break;
        //Contenu du scratchpad
        case 0x82:
            res = answer_read_request(scratchpad,4);
            if(res != 0){
                printf("Erreur sur la lecture/ecriture\n ");
            }

        break;
        //Adresse I2C de l'esclave sur 7bits
        case 0X83:
            res = SLAVE_ADDR;
            if(res != 0){
                printf("Erreur sur la lecture/ecriture\n ");
            }

        break;

    
    
    
    }

}
