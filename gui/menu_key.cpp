#include "menu_key.hpp"

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <atomic>
#include <thread>
#include <unistd.h>

namespace MenuKey {

  namespace {
    std::atomic<int> g_generation{0};
    std::atomic<bool> g_toggle{false};

    /* another instance may already hold the key — that must not kill us, and
       the overlay spams errors of its own, so report every kind only once */
    int swallow_errors(Display *, XErrorEvent *event) {
      static unsigned char seen[256] = {0};
      const int code = event->error_code;
      if (code >= 0 && code < 256 && !seen[code]) {
        seen[code] = 1;
        char text[128];
        XGetErrorText(event->display, code, text, sizeof(text));
        fprintf(stderr, "Fler: X11 error: %s\n", text);
      }
      return 0;
    }
  }

  void start(int keysym) {
    const int generation = ++g_generation;

    std::thread([generation, keysym]() {
      Display *display = XOpenDisplay(NULL);
      if (!display) return;

      XSetErrorHandler(swallow_errors);

      const Window root = DefaultRootWindow(display);
      const KeyCode code = XKeysymToKeycode(display, (KeySym)keysym);
      if (code == 0) {
        XCloseDisplay(display);
        return;
      }

      XGrabKey(display, code, 0, root, True, GrabModeAsync, GrabModeAsync);
      XSync(display, False);
      XSync(display, False);

      for (;;) {
        if (g_generation.load() != generation) break;

        XEvent event;
        while (XCheckMaskEvent(display, KeyPressMask, &event)) {
          if (event.type == KeyPress) g_toggle = true;
        }

        usleep(15000);
      }

      XUngrabKey(display, code, 0, root);
      XCloseDisplay(display);
    }).detach();
  }

  bool consume_toggle() { return g_toggle.exchange(false); }
}
