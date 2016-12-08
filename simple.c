#define MAX_WINS 32
#include <X11/Xlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <string.h>


int handle_error(Display *dsp, XErrorEvent *err) {
    puts("Encountered Error!");
    return 0;
}

void draw_title_on_frame(Display *dsp, Window frame, XFontStruct *font,
        char *title, int descent, int height) {
    GC gc = XCreateGC(dsp, frame, 0, 0);

    XSetBackground(dsp, gc, 0x181818);
    XSetForeground(dsp, gc, 0x7cafc2);
    XSetFont(dsp, gc, font->fid);

    XDrawString(dsp, frame, gc, 2, height - descent, title, strlen(title));
    printf("Title: %s\n", title);
}

int find_window_in_array(Window *winarray, Window query) {
    for (int i = 0; i < MAX_WINS; i++) {
        if (query == winarray[i]) {
            return i;
        }
    }
    return -1;
}

Window add_frame_to_window(Display *dsp, Window root, Window toFrame,
        XWindowAttributes attrs, XFontStruct *font) {
    int ascent, descent, direction;
    XCharStruct overall;
    char *title;
    XFetchName(dsp, toFrame, &title);
    XTextExtents(font, title, strlen(title), &direction, &ascent, &descent, &overall);
    Window frame = XCreateSimpleWindow(
            dsp, root, attrs.x, attrs.y,
            attrs.width, attrs.height + (ascent + descent), 2, 0x7cafc2, 0x181818);

    XReparentWindow(dsp, toFrame, frame, 0, 0);
    XSelectInput(dsp, frame,
            SubstructureRedirectMask | SubstructureNotifyMask
            | PropertyChangeMask | EnterWindowMask | ExposureMask);
    XMapWindow(dsp, frame);
    XMapWindow(dsp, toFrame);
    draw_title_on_frame(dsp, frame, font, title, descent, attrs.height + ascent + descent);
    XStoreName(dsp, frame, title);
    return frame;
}

int main() {
    XWindowAttributes attrs;
    Window wndws[MAX_WINS] = {0};
    Window frams[MAX_WINS] = {0};

    // initialize display and root window
    Display *dsp = XOpenDisplay(0);
    Window root = DefaultRootWindow(dsp);
    XSelectInput(dsp, root,
            SubstructureRedirectMask | SubstructureNotifyMask |
            PropertyChangeMask | 0);

    // compute keycodes for necessary keys
    const int K_opabe = XKeysymToKeycode(dsp, ' ');
    const int K_h     = XKeysymToKeycode(dsp, 'h');
    const int K_j     = XKeysymToKeycode(dsp, 'j');
    const int K_k     = XKeysymToKeycode(dsp, 'k');
    const int K_l     = XKeysymToKeycode(dsp, 'l');
    const int K_r     = XKeysymToKeycode(dsp, 'r');

    // add error handler and send request to server
    XSetErrorHandler(&handle_error);
    XSync(dsp, false);

    // load font for window titles
    XFontStruct *font = XLoadQueryFont(dsp, "fixed");

    // grab mod+space for raising
    XGrabKey(dsp, K_opabe,
            Mod1Mask, root, true,
            GrabModeAsync, GrabModeAsync);

    // grab mod+r for resize toggle
    XGrabKey(dsp, K_r,
            Mod1Mask, root, true,
            GrabModeAsync, GrabModeAsync);

    // grab buttons for moving
    XGrabKey(dsp, K_h,
            Mod1Mask, root, true,
            GrabModeAsync, GrabModeAsync);
    XGrabKey(dsp, K_j,
            Mod1Mask, root, true,
            GrabModeAsync, GrabModeAsync);
    XGrabKey(dsp, K_k,
            Mod1Mask, root, true,
            GrabModeAsync, GrabModeAsync);
    XGrabKey(dsp, K_l,
            Mod1Mask, root, true,
            GrabModeAsync, GrabModeAsync);

    // grab mod+mouse for focus
    XGrabButton(dsp, 1, Mod1Mask, root, true,
            ButtonPressMask, GrabModeAsync,
            GrabModeAsync, None, None);

    Window subw = None;
    int kcnt = 2;
    bool resizing = false;
    time_t ltime = time(NULL);
    for (XEvent e;; XNextEvent(dsp, &e)) {
        printf("Recv: event, type: %d\n", e.type);

        if (e.type == MapRequest) {
            // map window with frame and add to list
            XGetWindowAttributes(dsp, e.xmaprequest.window,
                    &attrs);

            Window frame = add_frame_to_window(dsp, root, e.xmaprequest.window, attrs, font);

            printf("Mapping window: %d/frame: %d\n", e.xmaprequest.window, frame);
            for (int i = 0; i < MAX_WINS; i++) {
                if (wndws[i] == 0) {
                    wndws[i] = e.xmaprequest.window;
                    frams[i] = frame;
                    break;
                }
            }
        } else if (e.type == Expose) {
            // redraw frame elements if window was overlapped
            if (e.xexpose.window != root) {
                int ascent, descent, direction;
                XCharStruct overall;
                char *title;
                XFetchName(dsp, e.xexpose.window, &title);
                //title = "meems";
                XTextExtents(font, title, strlen(title), &direction, &ascent, &descent, &overall);
                XGetWindowAttributes(dsp, e.xexpose.window,
                        &attrs);
                draw_title_on_frame(dsp, e.xexpose.window,
                        font, title, descent, attrs.height);
            }
        } else if (e.type == DestroyNotify) {
            // destroy empty frames and remove window from list
            for (int i = 0; i < MAX_WINS; i++) {
                if (wndws[i] == e.xdestroywindow.window) {
                    printf("Destroyed window: %d -> frame: %d\n", e.xdestroywindow.window,
                            e.xdestroywindow.event);
                    XUnmapWindow(dsp, e.xdestroywindow.event);
                    XReparentWindow(dsp, e.xdestroywindow.window, root, 0, 0);
                    XDestroyWindow(dsp, e.xdestroywindow.event);
                    wndws[i] = 0;
                    break;
                }
            }
        } else if (e.type == EnterNotify) {
            // change focus based on location of mouse
            printf("Entered window: %d\n", e.xcrossing.subwindow);
            XGetWindowAttributes(dsp, e.xcrossing.subwindow,
                    &attrs);
            subw = e.xcrossing.window;
            printf("%dx%d @ %d,%d\n",
                    attrs.width, attrs.height, attrs.x, attrs.y);
        } else if (e.type == KeyPress && subw != None) {
            // handle various keyboard actions
            XGetWindowAttributes(dsp, subw, &attrs);

            if (difftime(time(NULL), ltime) < 0.25) kcnt++;
            else kcnt = 2;

            Window wndw;
            Window fram;
            int wsch = find_window_in_array(wndws, subw);
            int fsch = find_window_in_array(frams, subw);
            printf("wsch: %d/fsch: %d\n", wsch, fsch);
            if (wsch != -1) {
                wndw = wndws[wsch];
                fram = frams[wsch];
            } else if (fsch != -1) {
                fram = frams[fsch];
                wndw = wndws[fsch];
            } else {
                puts("BAD THINGS ARE HAPPENING!");
            }
            printf("Got keypress from window: %d/frame: %d\n", wndw, fram);
            XWindowAttributes wndwAttrs;
            XWindowAttributes framAttrs;
            XGetWindowAttributes(dsp, wndw, &wndwAttrs);
            XGetWindowAttributes(dsp, fram, &framAttrs);

            int Kp = e.xkey.keycode;
            if (Kp == K_opabe)
                XRaiseWindow(dsp, subw);
            else if (Kp == K_h)
                if (resizing) {
                    XMoveResizeWindow(dsp, wndw,
                            wndwAttrs.x, wndwAttrs.y,
                            wndwAttrs.width - kcnt, wndwAttrs.height);
                    XMoveResizeWindow(dsp, fram,
                            framAttrs.x, framAttrs.y,
                            framAttrs.width - kcnt, framAttrs.height);
                }
                else
                    XMoveResizeWindow(dsp, subw,
                            attrs.x - kcnt, attrs.y,
                            attrs.width, attrs.height);
            else if (Kp == K_j)
                if (resizing) {
                    XMoveResizeWindow(dsp, wndw,
                            wndwAttrs.x, wndwAttrs.y,
                            wndwAttrs.width, wndwAttrs.height + kcnt);
                    XMoveResizeWindow(dsp, fram,
                            framAttrs.x, framAttrs.y,
                            framAttrs.width, framAttrs.height + kcnt);
                }
                else
                    XMoveResizeWindow(dsp, subw,
                            attrs.x, attrs.y + kcnt,
                            attrs.width, attrs.height);
            else if (Kp == K_k)
                if (resizing) {
                    XMoveResizeWindow(dsp, wndw,
                            wndwAttrs.x, wndwAttrs.y,
                            wndwAttrs.width, wndwAttrs.height - kcnt);
                    XMoveResizeWindow(dsp, fram,
                            framAttrs.x, framAttrs.y,
                            framAttrs.width, framAttrs.height - kcnt);
                }
                else
                    XMoveResizeWindow(dsp, subw,
                            attrs.x, attrs.y - kcnt,
                            attrs.width, attrs.height);
            else if (Kp == K_l)
                if (resizing) {
                    XMoveResizeWindow(dsp, wndw,
                            wndwAttrs.x, wndwAttrs.y,
                            wndwAttrs.width + kcnt, wndwAttrs.height);
                    XMoveResizeWindow(dsp, fram,
                            framAttrs.x, framAttrs.y,
                            framAttrs.width + kcnt, framAttrs.height);
                }
                else
                    XMoveResizeWindow(dsp, subw,
                            attrs.x + kcnt, attrs.y,
                            attrs.width, attrs.height);
            else if (Kp == K_r)
                resizing = !resizing;

        } else if (e.type == ButtonPress &&
                e.xbutton.subwindow != None) {
            // optional focus on alt+leftclick
            XGetWindowAttributes(dsp, e.xbutton.subwindow,
                    &attrs);
            subw = e.xbutton.window;
            printf("%dx%d @ %d,%d\n",
                    attrs.width, attrs.height, attrs.x, attrs.y);
        }
        ltime = time(NULL);
    }
    return 0;
}
