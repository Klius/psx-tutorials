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
char *SortSprite(int x, int y, u_int *ot, char *pri, SPRITE *sprite) {

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

	//TIM
	//This can be defined locally, if you don't need the TIM coordinates
	
	//extern u_int tim_my_image[];
	//Load the TIM
	//LoadTexture((u_int*)tim_my_image, &my_image);
	/*

	//Copy the TIM coordinates
	tim_prect = *my_image.prect;
	tim_crect = *my_image.crect;
	tim_mode = my_image.mode;

	//Calculate U.V offset for TIMS that are not page aligned
	tim_uoffs = (tim_prect.x%64)<<(2-(tim_mode&0x3));
	tim_voffs = (tim_prect.y&0xff);


    // set tpage of lone texture as initial tpage
    draw[0].tpage = getTPage( tim_mode&0x3, 0, tim_prect.x, tim_prect.y );
    draw[1].tpage = getTPage( tim_mode&0x3, 0, tim_prect.x, tim_prect.y );
	*/
	// apply initial drawing environment
    //PutDrawEnv(&draw[!db]);
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
void tiles(TILE* tile, int i){
	setTile(tile); //Initialize the primitive(very important)
	move(tile,i);
	setWH(tile,32,32);//Set size
	setRGB0(tile,colors[i][0],colors[i][1],colors[i][2]);//set color Blue 
	//Add primitive to the ordering table
	addPrim(ot[db],tile);
	
}
void draw_sprite(SPRT* sprt)
{
 	setSprt(sprt);                  // Initialize the primitive (very important)
    setXY0(sprt, 48, 48);           // Position the sprite at (48,48)
    setWH(sprt, 64, 64);            // Set size to 64x64 pixels
    setUV0(sprt,                    // Set UV coordinates
            tim_uoffs, 
            tim_voffs);
    setClut(sprt,                   // Set CLUT coordinates to sprite
            tim_crect.x,
            tim_crect.y);
    setRGB0(sprt,                   // Set primitive color
            128, 128, 128);
    addPrim(ot[db], sprt);          // Sort primitive to OT
}
int main()
{
    TILE *tile;  //pointer for TILE

	SPRITE sprite;
	TIM_IMAGE my_image;
		
	extern u_int tim_my_image[];
	init();
	
	//Load the TIM
	LoadTexture((u_int*)tim_my_image, &my_image);

	


	GetSprite(&my_image, &sprite);
    while(1){
		FntPrint(-1,"Illo es una PS1");
		

        ClearOTagR(ot[db],OTLEN); //Clear ordering table
		
		
		// char *SortSprite(int x, int y, u_int *ot, char *pri, SPRITE *sprite) 
		nextpri = SortSprite(32, 32, ot[db], nextpri, &sprite); 


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