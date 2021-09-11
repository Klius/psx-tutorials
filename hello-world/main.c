#include <sys/types.h> //this provides typedefs needed by libgte.h and libgpu.h
#include <stdio.h> //not neccesary but whatevs
#include <psxetc.h> //Includes some functions that control the display
#include <psxgte.h> //GTE header. not really used but libgpu.h depends on it
#include <psxgpu.h> //GPU library header

//define environment pairs and buffer counter
DISPENV disp[2];
DRAWENV draw[2];
int db;
int bg_color[3] = {238,230,0};

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

}
void display(void){
	//WAit for gpu finish drawing & vblank
	DrawSync(0);
	VSync(0);
	
	//Flip buffer counter
	db = !db;

	//Apply environments
	PutDispEnv(&disp[db]);
	PutDrawEnv(&draw[db]);

	//enable display
	SetDispMask(1);
}

void change_background_color(){
	int i = 0;
	for (i=0;i<3;i++){
		bg_color[i] += 1;
		if (bg_color[i]>255){
			bg_color[i] = 0;
		}
	}
	
	//Specifies the clear color of the drawenv
	setRGB0(&draw[0],bg_color[0],bg_color[1],bg_color[2]);
	setRGB0(&draw[1],bg_color[0],bg_color[1],bg_color[2]);
}

int main()
{
	init();
	while(1){
		FntPrint(-1,"Epa confirmao que esta disponeibol");
		//Flush puts prints in the screen
		FntFlush(-1);
		change_background_color();
		display();
	}
	return 0;
}