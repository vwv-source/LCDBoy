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

SDL_Window* tileMapWindow;
SDL_Renderer* tileMapRenderer;
SDL_Surface* tileMapWindowSurface;
SDL_Texture* tileMapFinalizedFrame;
SDL_FRect tileMapscreenbox = {0,0, 256*2, 256*2};

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Surface* windowSurface;
SDL_Texture* finalizedFrame;
SDL_FRect screenbox = {0,0, GAMEBOY_WIDTH*SCALE, GAMEBOY_HEIGHT*SCALE};
uint8_t * bgMap = NULL;

void DisplayInit(){
    SDL_Init(SDL_INIT_VIDEO);
    
	tileMapWindow = SDL_CreateWindow("Background Tile Map", 256*2, 256*2, 0);
    tileMapRenderer = SDL_CreateRenderer(tileMapWindow, NULL);
	tileMapWindowSurface = SDL_CreateSurface(256*2,256*2,SDL_GetPixelFormatForMasks(32,0,0,0,0));
	tileMapFinalizedFrame = SDL_CreateTexture(tileMapRenderer, SDL_GetPixelFormatForMasks(32,0,0,0,0), SDL_TEXTUREACCESS_STREAMING, 256,256);
	SDL_SetTextureScaleMode(tileMapFinalizedFrame, SDL_SCALEMODE_NEAREST);
    
	window = SDL_CreateWindow("LCDBoy", GAMEBOY_WIDTH*SCALE, GAMEBOY_HEIGHT*SCALE, 0);
    renderer = SDL_CreateRenderer(window, NULL);
	windowSurface = SDL_CreateSurface(GAMEBOY_WIDTH,GAMEBOY_HEIGHT,SDL_GetPixelFormatForMasks(32,0,0,0,0));
	finalizedFrame = SDL_CreateTexture(renderer, SDL_GetPixelFormatForMasks(32,0,0,0,0), SDL_TEXTUREACCESS_STREAMING, 160,144);
	SDL_SetTextureScaleMode(finalizedFrame, SDL_SCALEMODE_NEAREST);

	bgMap = malloc(sizeof(uint8_t)*256*256);
}

void DrawTile(int x, int y, uint8_t id, int sprite){
    unsigned char* pixels = (unsigned char*)tileMapWindowSurface->pixels;
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
            if(y+j >= tileMapWindowSurface->h || x+p >= tileMapWindowSurface->w || x+p < 0 || y+j < 0)
          		continue;

			//sprites can have transparency
			if(!color && !sprite){
				pixels[4*((y+j) * tileMapWindowSurface->w + (x+p)) + 1 ] = colors[0];
				bgMap[(y+j) * 256 + (x+p)] = colors[0];
			}
			else if(color){
				pixels[4*((y+j) * tileMapWindowSurface->w + (x+p)) + 1 ] = colors[color];
				bgMap[(y+j) * 256 + (x+p)] = colors[color];
			}
        }
    }
}

void RenderTiles(){
	for(int x = 0; x < 32; x++){
		int index = 0x9800u + x * 32;
		for(int i = 0; i < 32; i++){
			//why did I not consider if it was starting at $9C00??
			DrawTile(i*8, x*8, memory[index + i], 0);
		}
	}
}

void RenderSprites(){
	if((memory[LCDC] >> 1u) & 1u && !((memory[LCDC] >> 0x2u) & 0x1u)){
		for(uint8_t i = 0; i < 40; i++){
			uint8_t spriteY = memory[0xFE00u + i*4];
			uint8_t spriteX = memory[0xFE00u + i*4 + 1];
			uint8_t spriteP = memory[0xFE00u + i*4 + 2];

			DrawTile(spriteX-8, spriteY-16, spriteP, 1);
		}
	}
}

void DrawScanline(){
	int xOffset = memory[SCX];
	int yOffset = memory[SCY];
	int crntScan = memory[LCDC_Y];
    unsigned char* pixels = (unsigned char*)windowSurface->pixels;
		
	uint8_t bgY = (yOffset + crntScan) & 0xFF;
	for(int x = 0; x < GAMEBOY_WIDTH; x++){
		uint8_t bgX = (xOffset + x) & 0xFF;
		pixels[4*(crntScan * windowSurface -> w + x) + 1 ] = bgMap[bgY * 256 + bgX];
	}
}

void DisplayCycle(){
	if(memory[LCDC_Y] == 153u){
	 	memory[LCDC_Y] = 0;
		RenderTiles();
		RenderSprites();
		SDL_UpdateTexture(finalizedFrame, NULL, windowSurface->pixels, windowSurface->pitch);
		SDL_RenderTexture(renderer, finalizedFrame, NULL, &screenbox);
		SDL_RenderPresent(renderer);

		SDL_UpdateTexture(tileMapFinalizedFrame, NULL, tileMapWindowSurface->pixels, tileMapWindowSurface->pitch);
		SDL_RenderTexture(tileMapRenderer, tileMapFinalizedFrame, NULL, &tileMapscreenbox);
		SDL_RenderPresent(tileMapRenderer);
	}else{
		//check for IME and IE
		if(memory[LCDC_Y] == 144u && ime == 1 && (memory[IE] & 0x1u) > 0){
			memory[IF] |= 0x1;
		}

		if((memory[STAT] & 0x20u)){
			if(ime)
				memory[IF] |= 0x2u;
			memory[STAT] |= 0x2u;
		}

		if(memory[LCDC_Y] < 144u){
			DrawScanline();
		}
		memory[LCDC_Y]++;
	}
}
