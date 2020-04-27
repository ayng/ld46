BINARY_NAME = imhp
RELEASE_NAME = $(BINARY_NAME)-$(shell git describe --tags)

SDL2_CFLAGS = $(shell sdl2-config --cflags)
SDL2_LIBS = $(shell sdl2-config --libs)

CC ?= gcc

$(BINARY_NAME): main.c
	$(CC) -o $@ $< -lm $(SDL2_CFLAGS) $(SDL2_LIBS) -lSDL2_mixer -lSDL2_image -Wl,-rpath='$${ORIGIN}/lib'

linux: $(BINARY_NAME)

$(RELEASE_NAME)-linux-x86_64.tar.gz: $(BINARY_NAME)
	rm -rf $@
	tar --dereference \
		--transform 's|^|$(RELEASE_NAME)/|' \
		--transform 's|$(BINARY_NAME)$$|bin/$(BINARY_NAME)|' \
		--transform 's|usr|bin|' \
		--transform 's|pkg/||' \
		-czf $@ \
			$(wildcard assets/*.png) \
			$(wildcard assets/*.wav) \
			/usr/lib/libSDL2-2.0.so.0 \
			/usr/lib/libSDL2_image-2.0.so.0 \
			/usr/lib/libSDL2_mixer-2.0.so.0 \
			$(BINARY_NAME) \
			pkg/start \
			pkg/README

linuxtar: $(RELEASE_NAME)-linux-x86_64.tar.gz

index.html index.wasm index.data index.js: main.c
	emcc $< \
		-s USE_SDL=2 \
		-s USE_SDL_IMAGE=2 \
		-s USE_SDL_MIXER=2 \
		-s SDL2_IMAGE_FORMATS='["png"]' \
		-o index.html --preload-file assets --shell-file shell.html

web: index.html index.wasm index.data index.js

$(RELEASE_NAME)-web.zip: index.html index.wasm index.data index.js
	zip $@ $^

webzip: $(RELEASE_NAME)-web.zip

$(BINARY_NAME).exe: main.c
	x86_64-w64-mingw32-gcc -o $@ $< -lm $(shell x86_64-w64-mingw32-sdl2-config --cflags) $(shell x86_64-w64-mingw32-sdl2-config --libs) -lSDL2_mixer -lSDL2_image

win: $(BINARY_NAME).exe

$(RELEASE_NAME)-windows-x86_64.zip: $(BINARY_NAME).exe
	rm -rf $(RELEASE_NAME)
	mkdir -p $(RELEASE_NAME)/assets
	ln -s ../$< $(RELEASE_NAME)/$(BINARY_NAME).exe
	ln -s ../w64pkg/README.txt                        $(RELEASE_NAME)/README.txt
	ln -s /usr/x86_64-w64-mingw32/bin/SDL2.dll        $(RELEASE_NAME)/SDL2.dll
	ln -s /usr/x86_64-w64-mingw32/bin/SDL2_mixer.dll  $(RELEASE_NAME)/SDL2_mixer.dll
	ln -s /usr/x86_64-w64-mingw32/bin/SDL2_image.dll  $(RELEASE_NAME)/SDL2_image.dll
	ln -s /usr/x86_64-w64-mingw32/bin/libpng16-16.dll $(RELEASE_NAME)/libpng16-16.dll
	ln -s /usr/x86_64-w64-mingw32/bin/zlib1.dll       $(RELEASE_NAME)/zlib1.dll
	cp -r assets/*.png assets/*.wav                   $(RELEASE_NAME)/assets/
	zip -r $@ $(RELEASE_NAME)
	rm -rf $(RELEASE_NAME)

winzip: $(RELEASE_NAME)-windows-x86_64.zip

clean:
	rm -f $(BINARY_NAME)
	rm -f $(BINARY_NAME).exe
	rm -f $(BINARY_NAME)-*-web.zip
	rm -f $(BINARY_NAME)-*-linux-x86_64.tar.gz
	rm -f $(BINARY_NAME)-*-windows-x86_64.zip
	rm -f index.html index.wasm index.js index.data

.PHONY: clean linux linuxtar web webzip win winzip
