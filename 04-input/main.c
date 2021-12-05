#include <sys/types.h> //this provides typedefs needed by libgte.h and libgpu.h
#include <stdio.h> //not neccesary but whatevs
#include <psxetc.h> //Includes some functions that control the display
#include <psxgte.h> //GTE header. not really used but libgpu.h depends on it
#include <psxgpu.h> //GPU library header
#include <psxpad.h> //PAD defines
#define OTLEN 8 //ordering table length

// pad buffer arrays
u_char padbuff[2][34];

//define environment pairs and buffer counter
DISPENV disp[2];
DRAWENV draw[2];
int db;

int bg_color[3] = {255,128,128};//{238,230,0};

int	x = 0;
int	y = 0;


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
	
	//init pads
	InitPAD( padbuff[0], 34, padbuff[1], 34 );
	//initialise buffer before starting controller polling
	padbuff[0][0] = padbuff[0][1] = 0xff;
	padbuff[1][0] = padbuff[1][1] = 0xff;
	//Start polling controller
	StartPAD();
	//avoid vsync timeout
	ChangeClearPAD(1);
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



int main()
{
	PADTYPE *pad;
	pad = (PADTYPE*)padbuff[0];
    TILE *tile;  //pointer for TILE

	//SPRITE sprite;
	TIM_IMAGE my_image;
	TIM_IMAGE player;
		
	extern u_int tim_my_image[];
	extern u_int tim_player[];
	init();
	
	//Load the TIM
	LoadTexture((u_int*)tim_player, &player);
	LoadTexture((u_int*)tim_my_image, &my_image);
	SPRITE sprite;

	GetSprite(&player, &sprite);
	
    while(1){
		 // Print player coordinates
        FntPrint( -1, "POS_X=%d \n", x );
        FntPrint( -1, "POS_Y=%d \n", y );
		

        ClearOTagR(ot[db],OTLEN); //Clear ordering table
		
		
		// char *SortSprite(int x, int y, u_int *ot, char *pri, SPRITE *sprite)

		nextpri = SortSprite(x, y, ot[db], nextpri, &sprite); 
		if( pad->stat == 0 )
        {
            // Only parse when a digital pad, 
            // dual-analog and dual-shock is connected
            if( ( pad->type == 0x4 ) || 
                ( pad->type == 0x5 ) || 
                ( pad->type == 0x7 ) )
            {
                if( !(pad->btn&PAD_UP) )            // test UP
                {
                    y--;
                }
                else if( !(pad->btn&PAD_DOWN) )       // test DOWN
                {
                    y++;
                }
                if( !(pad->btn&PAD_LEFT) )          // test LEFT
                {
                    x--;
                }
                else if( !(pad->btn&PAD_RIGHT) )    // test RIGHT
                {
                    x++;
                }
            }
        }
			
	
        //Flush puts prints in the screen
		FntFlush(-1);

        //Updates display
        display();
	}
	
    return 0;
}
