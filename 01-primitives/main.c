#include <sys/types.h> //this provides typedefs needed by libgte.h and libgpu.h
#include <stdio.h> //not neccesary but whatevs
#include <psxetc.h> //Includes some functions that control the display
#include <psxgte.h> //GTE header. not really used but libgpu.h depends on it
#include <psxgpu.h> //GPU library header

#define OTLEN 8 //ordering table length

//define environment pairs and buffer counter
DISPENV disp[2];
DRAWENV draw[2];
int db;

int bg_color[3] = {238,230,0};

int	x[3] = {320/2,24,256};
int	y[3] = {240/2,32,200};
int speed_x[3] = {-1,-1,1};
int speed_y[3] = {-1,1,-2};
int colors[3][3] = {{255,0,0},{0,0,255},{0,0,0}};

//define ordering table
u_long ot[2][OTLEN];
char primbuff[2][32768];//primitive buffer
char *nextpri;//next primitive pointer


void init(void){
	ResetGraph(0);
	//Configure the pair of DISPENVS for 320x240 NTSC
	SetDefDispEnv(&disp[0],0,0,320,240);
	SetDefDispEnv(&disp[1],0,240,320,240);
	//Configures the pair of drawenvs for the dispenvs
	SetDefDrawEnv(&draw[0],0,240,320,240);
	SetDefDrawEnv(&draw[1],0,0,320,240);
	//Specifies the clear color of the drawenv
	setRGB0(&draw[0],bg_color[0],bg_color[1],bg_color[2]);
	setRGB0(&draw[1],bg_color[0],bg_color[1],bg_color[2]);
	//enable backround clear
	draw[0].isbg = 1;
	draw[1].isbg = 1;
	//Apply environments
	PutDispEnv(&disp[0]);
	PutDrawEnv(&draw[0]);
	//make sure db starts as 0
	db = 0; 

	//Load the internal font texture
	FntLoad(960, 0);
	//create the text stream
	FntOpen(0, 8, 320, 224, 0, 100);

    //Set initial primitive pointer address
    nextpri = primbuff[0];
}
void display(void){
	//WAit for gpu finish drawing & vblank
	DrawSync(0);
	VSync(0);
	
	//Apply environments
	PutDispEnv(&disp[db]);
	PutDrawEnv(&draw[db]);

	//enable display
	SetDispMask(1);

    //Draw ordering table
    DrawOTag(ot[db]+OTLEN-1);

    //Flip buffer counter
	db = !db;
    nextpri = primbuff[db]; //Reset next primitive pointer
}
void move(TILE* tile,int i){
	x[i] += speed_x[i];
	y[i] += speed_y[i];
	if (y[i] < 0 ){
		y[i] = 0;
		speed_y[i] *= -1;
	}
	else if (y[i]+32 > 240 ){
		y[i] = 240 - 32;
		speed_y[i] *= -1;
	}
	if (x[i] < 0 ){
		x[i] = 1;
		speed_x[i] *= -1;
	}
	else if (x[i]+32 > 320 ){
		x[i] = 320 - 32;
		speed_x[i] *= -1;
	} 
	setXY0(tile,x[i],y[i]);
}
int main()
{
    TILE *tile;  //primitive pointer

	init();
	
    while(1){
		FntPrint(-1,"Illo es una PS1");
		

        ClearOTagR(ot[db],OTLEN); //Clear ordering table
		int i = 0;
		for (i=0;i<3;i++){
			tile = (TILE*)nextpri;
			setTile(tile); //Initialize the primitive(very important)
			move(tile,i);
			setWH(tile,32,32);//Set size
			setRGB0(tile,colors[i][0],colors[i][1],colors[i][2]);//set color Blue 
			//Add primitive to the ordering table
			addPrim(ot[db],tile);
			//Advance te next primitive pointer
			nextpri += sizeof(TILE);
		}
        //Flush puts prints in the screen
		FntFlush(-1);

        //Updates display
        display();
	}
	
    return 0;
}
/*
TODO A few things you may want to experiment with yourself for further learning:

    - Play around with the values specified in setXY0(), setRGB0() and setWH() to change the position, color and size of the sprite respectively.
    - Try drawing more sprites by repeating the primitive creation process. Make sure the nextpri and tile pointers have been advanced before creating a new primitive.
    - You can advance the primitive pointer with tile++;, and set the updated address to nextpri by converting types back (nextpri = (char*)tile;)
    - Try making the yellow square bounce around the screen, by defining two variables for (x,y) coordinates and two more for the direction flags, and write some logic that makes the (x,y) coordinates move and bounce around the screen.
*/ 