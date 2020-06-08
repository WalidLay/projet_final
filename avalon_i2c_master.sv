// Code de la coquille de contrôleur a compléter

// synthesis translate_off
`default_nettype none
// synthesis translate_on

module avalon_i2c_master (
    input wire clk,
    input wire rst_n,
    input wire [1:0] addr,
    input wire read,
    input wire write,
    input wire [31:0] writedata,
    output logic [31:0] readdata,
    input  wire sda_in,
    input  wire scl_in,
    output logic sda_oe,
    output logic scl_oe,
    output logic intr,
    output wire [15:0] debug 
);


// Les adresses des registres
localparam I2C_START = 2'b00 ;
localparam I2C_FIFO  = 2'b01 ;
localparam I2C_NBD   = 2'b10 ;
localparam I2C_CST   = 2'b11 ;


// Les bits du registre status/controle
logic i2c_busy ;       // bit 0
logic i2c_intr ;       // bit 1
logic i2c_err ;        // bit 2
logic [1:0] i2c_spd ;  // bits 3 et 4
logic i2c_reset ;      // bit 5

// Immplementation du master i2c
logic ena  ;
logic [6:0] slv_addr ;
logic rw ;
logic busy;
wire ack_error ;
wire [7:0] data_to_i2c ;
wire [7:0] data_from_i2c ;

i2c_master i2c_m(
    .clk(clk) ,
    .reset_n(!i2c_reset),
    .ena(ena),
    .addr(slv_addr),
    .rw(rw) ,
    .data_wr(data_to_i2c),
    .speed(i2c_spd),
    .busy(busy)      ,
    .data_rd(data_from_i2c),
    .ack_error(ack_error) ,
    .sda_in(sda_in),
    .scl_in(scl_in),
    .sda_oe(sda_oe),
    .scl_oe(scl_oe)
) ;

// //Implémentation de la FIFO de Lecture
logic [7:0] data_read_in_InputFIFO;
//logic [7:0] data_write_in_InputFIFO;
logic InputFIFO_read;
logic InputFIFO_write;
//Variables locales à connecter
logic InputFIFO_almost_full;
logic InputFIFO_almost_empty;

sync_fifo #(.DEPTH(256)) 
InputFIFO (
    .clk(clk),
    .reset(i2c_reset),
    .wdata(data_from_i2c),
    .write(InputFIFO_write),
    .full(),
    .almost_full(InputFIFO_almost_full),
    .read(InputFIFO_read),
    .rdata(data_read_in_InputFIFO),
    .empty(),
    .almost_empty(InputFIFO_almost_empty)
);

// //Implémentation de la FIFO d'écriture
logic OutputFIFO_almost_empty;
//logic [7:0] data_read_in_OutputFIFO;
logic [7:0] data_write_in_OutputFIFO;
logic OutputFIFO_read;
logic OutputFIFO_write;

sync_fifo  #(.DEPTH(256)) 
OutputFIFO (
    .clk(clk),
    .reset(i2c_reset),
    .wdata(data_write_in_OutputFIFO),
    .write(OutputFIFO_write),
    .full(),
    .almost_full(),
    .read(OutputFIFO_read),
    .rdata(data_to_i2c),
    .empty(),
    .almost_empty(OutputFIFO_almost_empty)
);


//Gestion de reset SOFT
always @ ( posedge clk ) begin
    if (!rst_n)
        i2c_reset<=1'b1;
    else begin
    // AVOUS DE JOUER 
    // Activation du périphérique
    	if(write && (addr == I2C_CST))
	    i2c_reset <= writedata[5];
    end
end

// Détection des changement d'état du signal busy
// en provenance de i2c_master
logic last_busy;
logic busy_rising_edge;
logic busy_falling_edge;
// A VOUS DE JOUER
//Methode 1

always @(posedge clk) begin 
        last_busy<=busy;
end

//methode 2

assign busy_rising_edge = busy && !last_busy;
assign busy_falling_edge = !busy && last_busy; 

//////////////////////////////////////////////////////////////
// AUTOMATE de GESTION DES SEQUENCES DE TRANSFERT 
//////////////////////////////////////////////////////////////

// A VOUS DE JOUER : AJOUTEZ DES ETATS NECESSAIRES
typedef enum {STOP, WAIT, WR, RD, LAST_WR, LAST_RD} machine ; // needed states
machine state;

// Il faudra compter les octets lus ou envoyés
logic [7:0] cmpt_nbd_read;
logic [7:0] cmpt_nbd_write;

always @ (posedge (clk)) begin
    //Gestion du reset SOFT
    if (i2c_reset) begin
        //Initialisation de la machine à état
        state<=STOP;
    end
    else begin
        case(state)
            STOP: // On attend que master_i2c soit prêt
	           if(busy_falling_edge) begin
                       state <=WAIT;	
	           end 
            WAIT: //Déclenchement d'un transfert par écriture dans I2C_START 
	           if(write && (addr==I2C_START)) begin 
		        if(writedata[0]== 0) begin
			       if(OutputFIFO_almost_empty)
			        	state <= LAST_WR ;
		        	else if(!OutputFIFO_almost_empty)
					state <= WR;
			end
			if (writedata[0]==1) begin
				if((cmpt_nbd_read > 1) && busy_falling_edge)
				       state <= RD;
		                else if ((cmpt_nbd_read == 0) && busy_falling_edge)
			 	       state <= LAST_RD;		
			end
                   end
	     
            // A VOUS DE JOUER AJOUTEZ DES ETATS ET DES ENCHAINEMENTS
            WR : //Déclenchement de l'écriture
		    if(OutputFIFO_almost_empty && busy_rising_edge) begin
			state <= LAST_WR;
	            end
            RD :
		    if(cmpt_nbd_read == 2 && busy_falling_edge) begin
			    state <= LAST_RD;
		    end
	    LAST_WR :
		    if(busy_falling_edge && (OutputFIFO_almost_empty) ) begin
		            state <= WAIT;//Je me mets en position d'attente pour reprendre mon travail
		    end
	    LAST_RD :
		    if(busy_falling_edge && cmpt_nbd_read == 1) begin
			    state <= WAIT;
		    end
        endcase
    end
end

// Les signaux suivants pourrons évoluer en fonction des etats

// Gestion des signaux de contrôle de "i2c_master"
// ena, rw, slv_addr 
always @ (posedge (clk)) 
    if (i2c_reset) begin
        //Initialisation master i2c
        ena<=0;
        slv_addr<=0;
        rw<=0;
    end else begin
        // A VOUS DE JOUER
        if (write && (addr == I2C_START)) begin
		slv_addr <= writedata[7:1];
		rw <= writedata[0];
		ena <= 1;
	end else begin 
		if((state == WR)&&(busy_rising_edge))
			ena <= !OutputFIFO_almost_empty;
	end else begin
        if((state == READ || state == LAST_RD) && busy_rising_edge)
            ena <= 0;
    end

// Le compteur d'écritures
always @ (posedge (clk)) 
  if (i2c_reset) 
        cmpt_nbd_write<='0;
    else  begin
        // A VOUS DE JOUER
	if(write && (addr == I2C_START) && writedata[0]==0)
		cmpt_nbd_write <= 0;
	else if( (state == WR) || (state == LAST_WR) && (busy_falling_edge) && (!ack_error))
		cmpt_nbd_write <= cmpt_nbd_write + 1;
    end

// Le compteur de lectures 
always @ (posedge (clk)) 
  if (i2c_reset) 
      cmpt_nbd_read<='0;
  else  begin
	if(write && (addr == I2C_NBD))
		cmpt_nbd_read<=writedata[7:0];
	else if((state == RD) && busy_falling_edge)
		cmpt_nbd_read <= cmpt_nbd_read - 1 ;
  end


// Génération du signal d'écriture dans la FIFO d'entrée
always @ (posedge (clk)) 
  if (i2c_reset) 
      InputFIFO_write<=1'b0 ;
  else begin
        // A VOUS DE JOUER
        InputFIFO_write <= 1'b0;
	//if(write && (addr == I2C_FIFO))
	//	InputFIFO_write <= 1'b1;
        if((state == RD) && (busy_falling_edge))
		InputFIFO_write <= 1'b1;	
  end      

// Génération du signal de lecture dans la FIFO de sortie
always @ (posedge (clk)) 
  if (i2c_reset) 
      OutputFIFO_read<=1'b0 ;
  else begin
	  OutputFIFO_read <= 1'b0;
	  if((state == WR) && busy_rising_edge)
		  OutputFIFO_read <= 1'b1;
	  if((state == LAST_WR) && busy_rising_edge)
		  OutputFIFO_read <= 1'b1;//On est sûr d'avoir couvert tous les cas ainsi
  end

// Génération de l'interrupion de fin de traitement et du flag associé dans 
// le registre de status
always @ (posedge (clk)) 
    if (i2c_reset) 
        i2c_intr<=1'b0;
    else begin
        // A VOUS DE JOUER suite 2804
	if(write && (addr == I2C_CST))
		i2c_intr <= writedata[1];//Comme tous les bits de cst
	else if((state == LAST_WR) && busy_falling_edge)
		i2c_intr <= 1;
	else if ((state == LAST_RD) && (busy_falling_edge && cmpt_nbd_read==1))
	        i2c_intr <= 1;	
    end
assign intr = i2c_intr;

// Génération du flag d'erreur
always @ (posedge (clk))
    if (i2c_reset) 
        i2c_err<=0;
    else begin
        // A VOUS DE JOUER
	if(write && (addr==I2C_CST))
		i2c_err<=writedata[2];
	else if(( (state == WR) || (state == LAST_WR) ) && ( (busy_falling_edge)&&(ack_error) ) )
		i2c_err<= 1'b1;
    end


//Gestion des lectures AVALON :
// lecture des flags 
// lecture du registre i2c_nbd ,
// lecture d'une donnée de la FIFO de lecture
//
// de la fifo de lecture
always @ ( posedge clk ) begin
        // A VOUS DE JOUER
	InputFIFO_read <= 0;
     if(read) begin
           if (addr == I2C_CST)
	   	   readdata = {i2c_reset, i2c_spd, i2c_err, i2c_intr, i2c_busy};
	   else if (addr == I2C_NBD)
		   readdata <= cmpt_nbd_write;
	   else if (addr == I2C_FIFO)
		   InputFIFO_read <= 1;
         	   readdata <= data_write_in_OutputFIFO;
		  // readdata <= data_read_in_InputFIFO;
     end
end

//Gestion des écritures AVALON :
// ecriture dans le registre de contrôle
// envoi d'une donnée dans la FIFO d'écriture
always @ ( posedge clk )
if(i2c_reset) begin
        i2c_spd<=0;
        data_write_in_OutputFIFO<=0;
	OutputFIFO_write <= 0;
end else begin
	OutputFIFO_write <= 0;
	if(write) begin
	    if(addr==I2C_CST)
		i2c_spd = writedata[4:3];
	        i2c_intr = writedata[1];
            if(addr==I2C_FIFO) begin
	        OutputFIFO_write = 1;
                data_write_in_OutputFIFO = writedata[7:0];
            end	
    end	        
end

//  On génère directement le flag i2c_busy
assign i2c_busy = (state != WAIT) && !intr ;

// Si on veut sortir des signaux vers l'extérieur pour debugger avec 
// l'analyseur logique
assign debug = {14'b0, ena, busy} ;

endmodule
