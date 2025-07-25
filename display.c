//currently just "boilerplate" code since the way I'm rendering is
//completely different from how the gb does it (scanlines)

#include <stdlib.h>
#include <stdint.h>

//todo
//switch from SDL2 to SDL3 as the compat layer (SDL2->SDL3)
//is extremely slow and the SDL2 packages don't exist anymore
#include <SDL2/SDL.h>

extern uint8_t memory[0x10000];
extern int c;
extern int ime;
extern uint16_t sp;
extern uint16_t pc;

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Surface* windowSurface;

void DisplayInit(){
    SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow("LCDBoy", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 160*3, 144*3, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, 0);
}

void DrawTile(int x, int y, uint8_t id, int sprite){
    for(int j = 0; j < 8; j++){
        uint8_t byte1, byte2 = 0;
        //checking bg tile map display select
        if(((memory[0xFF40] >> 0x4u) & 0x1) == 1 || sprite){
			byte1 = memory[0x8000 + 16*id + j*2];
			byte2 = memory[0x8000 + 16*id + j*2 + 1];
        }else{
			byte1 = memory[0x9000 + 16*id + j*2];
			byte2 = memory[0x9000 + 16*id + j*2 + 1];
			//since this uses unsigned numbers
			if(id > 127){
				byte1 = memory[0x8800 + 16*(id-128) + j*2];
				byte2 = memory[0x8800 + 16*(id-128) + j*2 + 1];
			}
        }

        for(int p = 0; p < 8; p++){
            uint8_t color =  ((byte2 >> 7-p) & 0x1u) << 1u | ((byte1 >> 7-p) & 0x1u);

            unsigned char* pixels = (unsigned char*)windowSurface->pixels;
            
            //bounds check since something seems to be horribly wrong with these sums
            if(y+j >= windowSurface->h || x+p >= windowSurface->w || x+p < 0 || y+j < 0)
          		continue;

            switch(color){
				//I genuinely have no clue how I came up with this indexing pattern
                case 0:
                    if(!sprite)
                    	pixels[4*((y+j) * windowSurface -> w + (x+p)) + 1 ] = 255;
                    break;
                case 1:
                    pixels[4*((y+j) * windowSurface -> w + (x+p)) + 1 ] = 200;
                    break;
                case 2:
                    pixels[4*((y+j) * windowSurface -> w + (x+p)) + 1 ] = 125;
                    break;
                case 3:
                    pixels[4*((y+j) * windowSurface -> w + (x+p)) + 1 ] = 75;
                    break;
            }
        }
    }
}

void RenderTiles(){
	for(int x = 0; x < 32; x++){
		for(int i = 0; i < 32; i++){
			//why did I not consider if it was starting at $9C00??
			DrawTile(i*8, x*8, memory[0x9800u + x * 32 + i], 0);
		}
	}
}

void RenderSprites(){
	if((memory[0xFF40u] >> 1u) & 1u && !((memory[0xFF40u] >> 0x2u) & 0x1u)){
		for(uint8_t i = 0; i < 40; i++){
			uint8_t spriteY = memory[0xFE00u + i*4];
			uint8_t spriteX = memory[0xFE00u + i*4 + 1];
			uint8_t spriteP = memory[0xFE00u + i*4 + 2];

			DrawTile(spriteX-8, spriteY-16, spriteP, 1);
		}
	}
}

void DisplayCycle(){
	if(memory[0xFF44] == 153u){
		memory[0xFF44] = 0;
		windowSurface = SDL_CreateRGBSurface(0,160,144,32,0,0,0,0);
		RenderTiles();
		RenderSprites();
		SDL_Rect screenbox = {0,0, 160*3, 144*3};
		SDL_Texture* finalizedFrame = SDL_CreateTextureFromSurface(renderer, windowSurface);
		SDL_RenderCopy(renderer, finalizedFrame, NULL, &screenbox);
		SDL_RenderPresent(renderer);

	}else{
		//check for IME and IE
		if(memory[0xFF44] == 144u && ime == 1 && (memory[0xFFFF] & 0x1u) > 0){
			//prevent all interrupts
			ime = 0;
			sp--;
			memory[sp] = (pc & 0xFF00u) >> 8u;
			sp--;
			memory[sp] = (pc & 0x00FFu);
			pc = 0x0040u;
		}
		memory[0xFF44]++;
	}
}
