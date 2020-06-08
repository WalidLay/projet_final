#include "matrix.h"
#include <mbed.h>

DigitalOut RST(PC_3);
DigitalOut SB(PC_5);
DigitalOut LAT(PC_4);
DigitalOut SCK(PB_1);
DigitalOut SDA(PA_4);
DigitalOut ROW0(PB_2);
DigitalOut ROW1(PA_15);
DigitalOut ROW2(PA_2);
DigitalOut ROW3(PA_7);
DigitalOut ROW4(PA_6);
DigitalOut ROW5(PA_5);
DigitalOut ROW6(PB_0);
DigitalOut ROW7(PA_3);


/*Génération de pulse pulse_SCK : état bas; attente; état haut; attente; état bas; attente */
static void pulse_SCK(){
    SCK = 0;
    //for (int i = 0; i<2000; i++){
    //     asm volatile("nop");
    //}

    SCK = 1 ;
    //for (int i = 0; i<2000; i++){
    //    asm volatile("nop");
    //}
    
    SCK = 0;
}

/*pulse_LAT qui effectue un pulse négatif (état haut; attente; état bas; attente; état haut; attente)*/
static void pulse_LAT(){
    LAT = 1;
    //for (int i = 0; i<2000; i++){
    //    asm volatile("nop");
    //}
    
    
    LAT = 0;
    // for (int i = 0; i<2000; i++){
    //     asm volatile("nop");
    // }
    
    LAT = 1;
}

/*Contrôle des lignes

    Écrivez; à l'aide des macros ROW0 à ROW7; une fonction void deactivate_rows() qui éteint toutes les lignes.
    Écrivez; à l'aide des macros ROW0 à ROW7; une fonction void activate_row(int row) qui active la ligne dont le numéro est passé en argument.
*/
static void deactivate_rows(){
    ROW0 = 0;
    ROW1 = 0;
    ROW2 = 0;
    ROW3 = 0;
    ROW4 = 0;
    ROW5 = 0;
    ROW6 = 0;
    ROW7 = 0;
}

static void activate_row(int row){
    switch(row){
        case 0 :
            ROW0 = 1;
            break;
        case 1 :
            ROW1 = 1;
            break;
        case 2 : 
            ROW2 = 1;
            break;
        case 3 :
            ROW3 = 1;
            break;
        case 4 :
            ROW4 = 1;
            break;
        case 5 : 
            ROW5 = 1;
            break;
        case 6 :
            ROW6 = 1;
            break;
        case 7 :
            ROW7 = 1;
            break;
        default :
            deactivate_rows();
    }
}

/*Contrôle du DM163

    Écrivez une fonction void send_byte(uint8_t val; int bank) qui; à l'aide des macros SB; 
    pulse_SCK et SDA; envoie 8 bits consécutifs au bank spécifié par le paramètre bank (0 ou 1) 
    du DM163 (dans l'ordre attendu par celui-ci).

        on commence par sélectionner le registre à décalage qu'on veut mettre à jour à l'aide du signal SB : 
        0 pour BANK0; 1 pour BANK1
    on envoie sur SDA le bit de poids fort de la dernière LED (led23[7] si on met à jour le BANK1; led23[5] 
    si on met à jour le BANK0);
    puis on fait un pulse positif sur SCK (front montant puis front descendant)
    et on recommence en 2 jusqu'à ce que tous les bits aient été envoyés
    enfin; on fait un pulse négatif sur LAT; ce qui transfère le contenu du registre à décalage dans le 
    BANK choisi. Les sorties du DM163 sont alors mises à jour instantanément.

    Définissez le type rgb_color comme une structure représentant la couleur d'une case de la matrice :

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} rgb_color;

*/
static void send_byte(uint8_t val,int bank){
SB = bank;
    for (int i = 7 ; i >= 0; i--){
        SDA = val & (1<<i);
        pulse_SCK();
    }
}

// /* Écrivez une fonction void init_bank0() qui à l'aide de send_byte et pulse_LAT met tous les bits du BANK0 à 1.
// Faites en sorte que cette fonction soit appelée par matrix_init.*/

static void init_bank0(){
    for (int i = 0 ; i < 18 ; i++){
        send_byte((uint8_t)0xff,0);
    }
    pulse_LAT();
}


//Initialisation des broches puis du module d'activation des lEDs
void matrix_init(){
    /*Initialisation des broches*/


    //positionner les sorties suivantes à une valeur initiale acceptable
    RST = 0;
    LAT = 1;
    SB = 1 ;
    SCK = 0;
    SDA = 0;

    //C0 à C7 : 0 (éteint toutes les lignes)
    deactivate_rows();
        

    //Attendre au moins 100ms que le DM163 soit initialisé; puis passe RST à l'état haut
    /*F=80MHz avec réalisation de 4 instructions par période --> T = 1/F puis multiplication par 4
    pour savoir le nombre d'instructions réalisées par période = 5x10⁻⁵. On veut attendre 100ms donc 1.10⁻³ / 5.10⁻⁵ = 20000 */
    /*for (int i = 0; i<2000; i++){
        asm volatile("nop");
    }*/
    wait(0.1);
    //passer le RST à l'état haut
    RST = 1;

    //appel de init_bank0()
    init_bank0();

}


/*
Écrivez, à l'aide de send_byte et activate_row, une fonction void mat_set_row(int row, const rgb_color *val) qui :

    prend en argument un tableau val de 8 pixels
    à l'aide de send_byte envoie ces 8 pixels au BANK1 du DM163 dans le bon ordre (B7, G7, R7, B6, G6, 
    R6, ..., B0, G0, R0)
    puis à l'aide de activate_row et pulse_LAT active la rangée passée en paramètre et les sorties du DM163.
*/

static void mat_set_row(int row, const rgb_color *val){

    for (int i =7; i>=0; i--){
        send_byte(val[i].b,1);
        send_byte(val[i].g,1);
        send_byte(val[i].r,1);
    }
    deactivate_rows();
    /*for (int i =0; i < 200 ; i++) {
        asm volatile("nop");
    }*/
    wait_ns(50);
    pulse_LAT();
    activate_row(row);
    
}

// Test basique

// Écrivez une fonction void test_pixels() qui teste votre affichage, 
//par exemple en allumant successivement chaque ligne avec un dégradé de bleu, 
//puis de vert puis de rouge. Cette fonction devra être appelée depuis le main, 
//de façon à ce que nous n'ayons qu'à compiler (en tapant make) puis à loader votre programme
// pour voir votre programme de test fonctionner.
// Committez tout ça avec le tag TEST_MATRIX.

void test_pixels(){

    rgb_color color;
        color.b = 255;
        color.g = 255;
        color.r = 0;    
    
    rgb_color color2;
        color2.b = 0;
        color2.g = 0;
        color2.r = 255;    
    
    rgb_color ligne[8] = {color,color,color,color,color,color,color,color2};

    for (int row=7;row>=0;row--){
        mat_set_row(row,ligne);
    }
    //print_image(ligne);      
}

void print_image(rgb_color *image){
    for (int row=0;row<8;row++){
        mat_set_row(row,image+row*8);
    }
}
