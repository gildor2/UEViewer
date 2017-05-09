SDL2main.lib is not needed.

SDL2 has got rid of SDL_main function. The overrided function just calls SDL_SetMainReady(), which
does nothing than just sets some internal 'SDL_MainIsReady' to 'SDL_TRUE', i.e. no initialization is
needed at all. However, SDL 2.0.5 still overrides 'main' function in headers unless SDL_MAIN_HANDLED
is defined.

SDL 2.0.5 has bug in console_main command line processing: when application got '-path="A B C"' parameter,
it will be returned to main() as '-path="A', 'B', 'C"' i.e. 3 arguments. SDL 2.0.3 doesn't have this bug.
