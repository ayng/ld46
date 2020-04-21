#!/bin/sh

emcc ../main.c -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s USE_SDL_MIXER=1 -s USE_SDL_TTF=2 -s SDL2_IMAGE_FORMATS='["png"]' -o index.html --preload-file ../assets --use-preload-plugins

