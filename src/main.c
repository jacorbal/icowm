// main.c
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include "config.h"
#include "window.h"

int main(void) {
    // Inicializar la configuración
    config_td *config = config_init();
    if (!config) {
        return EXIT_FAILURE; // Manejar el error de inicialización
    }

    // Cargar la configuración
    config_load(config);

    printf("THM: %s\n", config->base.theme);
    printf("THM: %s\n", config->theme.name);
    printf("VAR: %s\n", config->theme.icon.font);

    // Inicializar la conexión al servidor X
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "No se pudo abrir la pantalla\n");
        config_destroy(config);
        // TODO: Asegúrate de liberar la configuración en caso de error
        return EXIT_FAILURE;
    }

    // Inicializar la ventana con la configuración cargada
    window_td *window = window_init(display, 800, 600, 100, 100,
            &(config->theme));
    if (!window) {
        XCloseDisplay(display);
        config_destroy(config);
        return EXIT_FAILURE;
        // TODO: Manejar el error de inicialización de la ventana
    }

    // Mostrar la ventana
    window_show(window);

    // Esperar que el usuario cierre la ventana o realice otra acción
    getchar(); // Puedes reemplazarlo por un mejor manejo de eventos de ventana
    
    // Limpiar recursos
    window_destroy(window);
    XCloseDisplay(display);
    config_destroy(config);

    return EXIT_SUCCESS;
}
