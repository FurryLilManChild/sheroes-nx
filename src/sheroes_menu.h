/* sheroes_menu.h: the port's own menu (+ and - together) and its saved options.
 * See sheroes_menu.c. */
#ifndef SHEROES_MENU_H
#define SHEROES_MENU_H

/* Load sheroes_options.txt, apply it and install the pad filter and the
 * overlay. Call once at boot, before the kernel reads RECOMP_WIDESCREEN. */
void sheroes_menu_init(void);

#endif
