BINARY_NAME = imhp
RELEASE_NAME = $(BINARY_NAME)-$(shell git describe --tags)

SDL2_CFLAGS = $(shell sdl2-config --cflags)
SDL2_LIBS = $(shell sdl2-config --libs)

CC ?= gcc

$(BINARY_NAME): main.c
	$(CC) -o $@ -lm $(SDL2_CFLAGS) $(SDL2_LIBS) -lSDL2_mixer -lSDL2_image -lSDL2_ttf $< -Wl,-rpath='$${ORIGIN}/lib'

$(RELEASE_NAME).tar.gz: $(BINARY_NAME)
	rm -rf $@
	tar --dereference \
		--transform 's|^|$(RELEASE_NAME)/|' \
		--transform 's|$(BINARY_NAME)$$|bin/$(BINARY_NAME)|' \
		--transform 's|usr|bin|' \
		--transform 's|pkg/||' \
		-czf $(RELEASE_NAME).tar.gz \
			$(wildcard assets/*.ttf) \
			$(wildcard assets/*.png) \
			$(wildcard assets/*.wav) \
			/usr/lib/libSDL2-2.0.so.0 \
			/usr/lib/libSDL2_image-2.0.so.0 \
			/usr/lib/libSDL2_mixer-2.0.so.0 \
			/usr/lib/libSDL2_ttf-2.0.so.0 \
			$(BINARY_NAME) \
			pkg/start \
			pkg/README

tar: $(RELEASE_NAME).tar.gz

index.html index.wasm index.data index.js: main.c
	emcc $< \
		-s USE_SDL=2 \
		-s USE_SDL_IMAGE=2 \
		-s USE_SDL_MIXER=2 \
		-s USE_SDL_TTF=2 \
		-s SDL2_IMAGE_FORMATS='["png"]' \
		-o index.html --preload-file assets --shell-file shell.html

web: index.html index.wasm index.data index.js

clean:
	rm -f $(BINARY_NAME)
	rm -f $(BINARY_NAME)-*.tar.gz
	rm -f index.html index.wasm index.js index.data

.PHONY: clean tar web
