//currently just "boilerplate" code since the way I'm rendering is
//completely different from how the gb does it (scanlines)

#include <stdlib.h>
#include <stdio.h>

#include <SDL3/SDL.h> //SDL is probably the biggest bottleneck in this project

#include "instructions.h"
#include "display.h"
#include "interrupts.h"

extern uint8_t * memory;
extern int c;
extern int ime;
extern uint16_t sp;
extern uint16_t pc;

typedef struct {
	int flipX;
	int flipY;
	int paletteNum;
} spriteData;

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Surface* windowSurface;
SDL_Texture* finalizedFrame;
SDL_FRect screenbox = {0,0, GAMEBOY_WIDTH*SCALE, GAMEBOY_HEIGHT*SCALE};
uint8_t * bgMap = NULL;

void DisplayInit(){
    SDL_Init(SDL_INIT_VIDEO);

	window = SDL_CreateWindow("LCDBoy", GAMEBOY_WIDTH*SCALE, GAMEBOY_HEIGHT*SCALE, 0);
    renderer = SDL_CreateRenderer(window, NULL);
	windowSurface = SDL_CreateSurface(GAMEBOY_WIDTH,GAMEBOY_HEIGHT,SDL_GetPixelFormatForMasks(32,0,0,0,0));
	finalizedFrame = SDL_CreateTexture(renderer, SDL_GetPixelFormatForMasks(32,0,0,0,0), SDL_TEXTUREACCESS_STREAMING, 160,144);
	SDL_SetTextureScaleMode(finalizedFrame, SDL_SCALEMODE_NEAREST);

	bgMap = calloc(256*256,sizeof(uint8_t));
}

void DrawTile(int x, int y, uint8_t id, spriteData * sprite){
	int index8000 = 0x8000 + 16*id;
	int index9000 = 0x9000 + 16*id;
    for(int j = 0; j < 8; j++){
        uint8_t byte1, byte2 = 0;
        //checking bg tile map display select
        if(((memory[0xFF40] >> 0x4u) & 0x1) == 1 || sprite){
			byte1 = memory[index8000 + j*2];
			byte2 = memory[index8000 + j*2 + 1];
        }else{
			byte1 = memory[index9000 + j*2];
			byte2 = memory[index9000 + j*2 + 1];
			//since this uses unsigned numbers
			if(id > 127){
				byte1 = memory[0x8800 + 16*(id-128) + j*2];
				byte2 = memory[0x8800 + 16*(id-128) + j*2 + 1];
			}
        }

        for(int p = 0; p < 8; p++){
            uint8_t color =  ((byte2 >> 7-p) & 0x1u) << 1u | ((byte1 >> 7-p) & 0x1u);
			int colors[4] = {255, 200, 125, 75};
            
            //bounds check since something seems to be horribly wrong with these sums
            if(y+j >= 256 || x+p >= 256 || x+p < 0 || y+j < 0)
          		continue;

			//sprites can have transparency
			if(!color && !sprite){
				bgMap[(y+j) * 256 + (x+p)] = colors[0];
			}else if(color){
				bgMap[(y+j) * 256 + (x+p)] = colors[color];
			}
        }
    }
}


void RenderTiles(){
	int initialIndex = (memory[LCDC] & 0x8u) ? 0x9C00 : 0x9800;

	for(int y = 0; y < 32; y++){
		int index = initialIndex + y * 32;
		//if(((y <= (memory[LCDC_Y]+memory[SCY])) && ((y*8) >= (memory[LCDC_Y]+memory[SCY])))){
			for(int x = 0; x < 32; x++){
				//if(((x*8 >= memory[SCX]) && (x*8 <= memory[SCX]+160))){
					DrawTile(x*8, y*8, memory[index + x], NULL);
				//}
			}
		//}
	}
}


void RenderSprites(){
	if((memory[LCDC] >> 1u) & 1u && !((memory[LCDC] >> 0x2u) & 0x1u)){
		for(uint8_t i = 0; i < 40; i++){
			spriteData sprite;
			uint8_t spriteY = memory[0xFE00u + i*4];
			uint8_t spriteX = memory[0xFE00u + i*4 + 1];
			uint8_t spriteP = memory[0xFE00u + i*4 + 2];
			uint8_t spriteF = memory[0xFE00u + i*4 + 3];

			//TODO
			//make this actually do something
			if(spriteF & 0x20) sprite.flipX = 1;

			DrawTile(memory[SCX]+spriteX-8, memory[SCY]+spriteY-16, spriteP, &sprite);
		}
	}
}

void RenderWindow(){
	if(!(memory[LCDC] & 0x20u)) return;
	int initialIndex = (memory[LCDC] & 0x40u) ? 0x9C00 : 0x9800;

	for(int y = 0; y < 32; y++){
		int index = initialIndex + y * 32;
		for(int x = 0; x < 32; x++){
			DrawTile(memory[WX]-7+memory[SCX]+x*8, memory[WY]+memory[SCY]+y*8, memory[index + x], NULL);
		}
	}
}

void DrawScanline(){
	int xOffset = memory[SCX];
	int yOffset = memory[SCY];
	int crntScan = memory[LCDC_Y];
	SDL_LockSurface(windowSurface);
    unsigned char* pixels = (unsigned char*)windowSurface->pixels;
		
	uint8_t bgY = (yOffset + crntScan) & 0xFFu;
	for(int x = 0; x < GAMEBOY_WIDTH; x++){
		uint8_t bgX = (xOffset + x) & 0xFFu;
		pixels[4*(crntScan * windowSurface -> w + x) + 1 ] = bgMap[bgY * 256 + bgX];
	}
	SDL_UnlockSurface(windowSurface);
}

void DisplayCycle(){
	if(memory[LCDC_Y] == 153u){
	 	memory[LCDC_Y] = 0;
		SDL_UpdateTexture(finalizedFrame, NULL, windowSurface->pixels, windowSurface->pitch);
		SDL_RenderTexture(renderer, finalizedFrame, NULL, &screenbox);
		SDL_RenderPresent(renderer);
	}else{
		//VBLANK
		if(memory[LCDC_Y] == 144u && interruptEnabled(INTERRUPT_VBLANK)){
			memory[IF] |= 0x1;
		}
		
		if((memory[STAT] & 0x20u)){
			if(ime)
				memory[IF] |= 0x2u;
			memory[STAT] |= 0x2u;
		}
		
		if(memory[LCDC_Y] < 144u){
			//framerate abolisher(s)
			//should probably only compute pixels thatre needed 
			//at each scanline

			//you can make this run faster by moving the Render functions
			//into the (memory[LCDC_Y] == 153u) condition, but you'll
			//experience more graphical glitches
			RenderTiles();
			RenderSprites();
			RenderWindow();
			DrawScanline();
		}
		memory[LCDC_Y]++;
	}
}
