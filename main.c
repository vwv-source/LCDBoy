#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> //switch to manually allocated stuff for fun
#include <unistd.h>
#include <SDL3/SDL.h>

#include "instructions.h"
#include "display.h"

uint16_t pc = 0x0100u; //program counter
uint16_t sp = 0xFFFEu; //stack counter
uint8_t memory[0x10000];
uint16_t opcode;

SDL_Event sdl_e;

char* filename;
void GB_ReadRangeIntoMemory(uint8_t bank_number) {
    FILE* fptr = fopen(filename, "rb");

    uint32_t file_offset = 0x4000 * bank_number;
    fseek(fptr, file_offset, SEEK_SET);
    fread(&memory[0x4000], 1, 0x4000, fptr);

    fclose(fptr);
}

void readROM(char* filename){
    FILE* fptr = fopen(filename, "rb");

	if(!fptr){
		printf("Invalid file.\n");
		exit(0);
	}

    fseek(fptr, 0L, SEEK_SET);
    fread(memory, 1, 0x8000u, fptr);
    fclose(fptr);
}

//cursed keypad emulation
uint8_t P14buffer = 0x1Fu;
uint8_t P15buffer = 0x2Fu;
void GB_HandleInput(int keyenum){
  switch(keyenum){
    case SDLK_RIGHT: P14buffer = 0x1Eu; break; 
    case SDLK_LEFT: P14buffer = 0x1Du; break; 
    case SDLK_UP: P14buffer = 0x1Bu; break; 
    case SDLK_DOWN: P14buffer = 0x17u; break; 

    case SDLK_A: P15buffer = 0x2Eu; break; 
    case SDLK_D: P15buffer = 0x2Du; break; 
    case SDLK_LSHIFT: P15buffer = 0x2Bu; break; 
    case SDLK_SPACE: P15buffer = 0x27u; break; 
  }
}

int main(int argc, char* argv[]){

    if(argc < 2){
        printf("You need to specify a ROM to use :P\n");
        return 0;
    }
    filename = argv[1];

    readROM(filename);
    DisplayInit();

    memory[0xFF00u] = 0x0Fu;

	uint64_t lastPoll = SDL_GetTicks();
    uint64_t pollInterval = 50;
    while(1){
		//I'm doing this crap because SDL_PollEvent is EXTREMELY slow for
		//some odd reason, maybe a bug with SDL3?
		uint64_t now = SDL_GetTicks();
        if (now - lastPoll >= pollInterval) {
            lastPoll = now;
			while(SDL_PollEvent(&sdl_e)){
				switch(sdl_e.type){
					case SDL_EVENT_QUIT:
						exit(0);
					
					//Is it fine to ignore the out ports? (p14 & p15)
					case SDL_EVENT_KEY_DOWN:
						GB_HandleInput(sdl_e.key.key);
						break;

					case SDL_EVENT_KEY_UP:
						P14buffer = 0x1Fu;
						P15buffer = 0x2Fu;
						memory[0xFF00u] = 0x0Fu;
						break;

				}
			}
		}
	    GB_Loop();
    }
    return 0;
}
