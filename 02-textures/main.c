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
int speed_x[3] = {-1,-1,2};
int speed_y[3] = {-1,1,-2};
int colors[3][3] = {{255,1,1},{1,1,255},{40,1,128}};

//define ordering table
u_int ot[2][OTLEN];
char primbuff[2][32768];//primitive buffer
char *nextpri;//next primitive pointer

//keep a copy of TIM coordinates for later
int tim_mode;
RECT tim_prect, tim_crect;
int tim_uoffs,tim_voffs;

typedef struct _SPRITE {
    u_short tpage;  // Tpage value
    u_short clut;   // CLUT value
    u_char  u,v;    // UV offset (useful for non page aligned TIMs)
    u_char  w,h;    // Size (primitives can only draw 256x256 anyway)
    CVECTOR col;
} SPRITE;

// Sets parameters to a MYSPRITE using coordinates from TIM_INFO

void GetSprite(TIM_IMAGE *tim, SPRITE *sprite) {
    // Get tpage value
    sprite->tpage = getTPage(tim->mode&0x3, 0, 
        tim->prect->x, tim->prect->y);
        
    // Get CLUT value
    if( tim->mode & 0x8 ) {
        sprite->clut = getClut(tim->crect->x, tim->crect->y);
    }
    
    // Set sprite size
    sprite->w = tim->prect->w<<(2-tim->mode&0x3);
    sprite->h = tim->prect->h;
    
    // Set UV offset
    sprite->u = (tim->prect->x&0x3f)<<(2-tim->mode&0x3);
    sprite->v = tim->prect->y&0xff;
    
    // Set neutral color
    sprite->col.r = 128;
    sprite->col.g = 128;
    sprite->col.b = 128;
}
//sorts the sprite into the ordering table
char *SortSprite(int x, int y,int colors[], u_int *ot, char *pri, SPRITE *sprite) {

    SPRT *sprt;
    DR_TPAGE *tpage;
    
    sprt = (SPRT*)pri;                  // initialize the sprite
    setSprt(sprt);

    setXY0(sprt, x, y);                 // Set position
    setWH(sprt, sprite->w, sprite->h);  // Set size
    setUV0(sprt, sprite->u, sprite->v); // Set UV coordinate of sprite
    setRGB0(sprt,                       // Set the color
        sprite->col.r, 
        sprite->col.g, 
        sprite->col.b);
    sprt->clut = sprite->clut;          // Set the CLUT value
    
    addPrim(ot, sprt);                  // Sort the primitive and advance
    pri += sizeof(SPRT);

    tpage = (DR_TPAGE*)pri;             // Sort the texture page value
    setDrawTPage(tpage, 0, 1, sprite->tpage);
    addPrim(ot, tpage);

    return pri+sizeof(DR_TPAGE);        // Return new primitive pointer
                                        // (set to nextpri)

}

void LoadTexture(u_int *tim, TIM_IMAGE *tparam){

	//Read TIM information 
	GetTimInfo(tim, tparam);

	//Upload pixel data to framebuffer
	LoadImage(tparam->prect, (u_int*)tparam->paddr);
	DrawSync(0);

	// Upload CLUT to framebuffer if present
	if( tparam->mode & 0x8){
		LoadImage(tparam->crect,(u_int*)tparam->caddr);
		DrawSync(0);
	}
}
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

void move_sprite(u_char w,u_char h,int i){
	x[i] += speed_x[i];
	y[i] += speed_y[i];
	if (y[i] < 0 ){
		y[i] = 0;
		speed_y[i] *= -1;
	}
	else if (y[i]+h > 240 ){
		y[i] = 240 - h;
		speed_y[i] *= -1;
	}
	if (x[i] < 0 ){
		x[i] = 1;
		speed_x[i] *= -1;
	}
	else if (x[i]+w > 320 ){
		x[i] = 320 - w;
		speed_x[i] *= -1;
	} 
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
void tiles(TILE* tile, int i){
	setTile(tile); //Initialize the primitive(very important)
	move(tile,i);
	setWH(tile,32,32);//Set size
	setRGB0(tile,colors[i][0],colors[i][1],colors[i][2]);//set color Blue 
	//Add primitive to the ordering table
	addPrim(ot[db],tile);
	
}
int Random(int max)
{
	srand(GetRCnt(0)); // initialise the random seed generator
	int r = rand()%max;
	r = rand()%max;
	r = rand()%max;
	return r; // return a random number based off of max
}
void setColors(SPRITE *sprite){
/*	for(int j=0;j<3;j++){
		colors[it][j]++;
	}*/
	sprite->col.r = Random(255);
    sprite->col.g = Random(255);
    sprite->col.b = Random(255);
}
void changeColors(SPRITE *sprite){
	int r = 1+sprite->col.r;
	int g = 1+sprite->col.g;
	int b = 1+sprite->col.b; 
	sprite->col.r=r;
    sprite->col.g=g;
    sprite->col.b=b;
}
void changeSize(SPRITE *sprite){
	sprite->w = Random(64);
	sprite->h = Random(64);
}
int main()
{
    TILE *tile;  //pointer for TILE

	//SPRITE sprite;
	TIM_IMAGE my_image;
		
	extern u_int tim_my_image[];
	init();
	
	//Load the TIM
	LoadTexture((u_int*)tim_my_image, &my_image);

	

	SPRITE sprites[3];
	for (int i = 0;i<3;i++){
		SPRITE sprite;
		GetSprite(&my_image, &sprite);
		if ( i % 2 == 0){
			setColors(&sprite);
		}
		else{
			changeSize(&sprite);
		}
		sprites[i] = sprite;
	}
	
    while(1){
		FntPrint(-1,"Illo es una PS1");
		

        ClearOTagR(ot[db],OTLEN); //Clear ordering table
		
		
		// char *SortSprite(int x, int y, u_int *ot, char *pri, SPRITE *sprite)
		int j = 0;
		for (j=0;j<3;j++){
			SPRITE sprite;
			sprite = sprites[j];
			if ( j % 2 == 0){
				changeColors(&sprite);
			}
			else{
				//changeSize(&sprite);
			}
			nextpri = SortSprite(x[j], y[j], colors[j], ot[db], nextpri, &sprite); 
			move_sprite(sprite.w,sprite.h,j);
			
		}
		
		int i = 0;
		for (i=0;i<3;i++){
			tile = (TILE*)nextpri;
			tiles(tile,i);
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