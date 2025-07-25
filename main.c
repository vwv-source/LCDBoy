#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> //switch to manually allocated stuff for fun
#include <unistd.h>
#include <SDL2/SDL.h>

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
    int size;

    FILE* fptr;
    fptr = fopen(filename, "rb");

    fseek(fptr, 0L, SEEK_END);
    size = ftell(fptr);
    fseek(fptr, 0L, SEEK_SET);

    unsigned char buffer[size];

    fread(buffer, sizeof(buffer), 1, fptr);

    fclose(fptr);

    for(int i = 0; i<0x8000u; i++){
        memory[0x0000+i] = buffer[i];
    }
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

    case SDLK_a: P15buffer = 0x2Eu; break; 
    case SDLK_d: P15buffer = 0x2Du; break; 
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

    DisplayInit();
    readROM(filename);

    int onlyonce = 0;
    memory[0xFF00u] = 0x0Fu;
    while(1){
		while (SDL_PollEvent(&sdl_e)) {
			switch(sdl_e.type){
				case SDL_QUIT:
					exit(0);
				
				//Is it fine to ignore the out ports? (p14 & p15)
				case SDL_KEYDOWN:
					GB_HandleInput(sdl_e.key.keysym.sym);
					break;

				case SDL_KEYUP:
					P14buffer = 0x1Fu;
					P15buffer = 0x2Fu;
					memory[0xFF00u] = 0x0Fu;
					break;

			}
		}
	    GB_Loop();
        //What the fuck was this?
	    //if(memory[0xFF50] == 0x1u && onlyonce == 0){
	    //    readROM(argv[1]);
	    //    onlyonce = 1;
	    //}
    }
    return 0;
}
