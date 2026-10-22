#ifndef FLER_MENU_KEY_HPP
#define FLER_MENU_KEY_HPP

/* Global hotkey that shows/hides the menu. Grabs the key on the X server so it
   works while the game holds the focus (the game itself runs as an X client
   under XWayland). On a pure Wayland desktop with a native game the grab is
   simply inert and the tray icon takes over. */
namespace MenuKey {

  /* (re)grabs the given X11 keysym, releasing the previous one. */
  void start(int keysym);

  /* true once per press of the grabbed key. */
  bool consume_toggle();
}

#endif
