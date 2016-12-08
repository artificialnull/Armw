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

    XDrawString(dsp, frame, gc, 0, height - descent, title, strlen(title));
    printf("Title: %s\n", title);
}

void add_frame_to_window(Display *dsp, Window root, Window toFrame,
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
}

int main() {
    Display *dsp;
    Window root;
    XWindowAttributes attrs;
    XEvent e;
    Window subw;
    XFontStruct *font;
    int kcnt;
    time_t ltime;
    Window wndws[32] = {0};

    // initialize display and root window
    dsp = XOpenDisplay(0);
    root = DefaultRootWindow(dsp);
    XSelectInput(dsp, root,
            SubstructureRedirectMask | SubstructureNotifyMask |
            PropertyChangeMask | 0);

    // compute keycodes for necessary keys
    const int K_opave = XKeysymToKeycode(dsp, ' ');
    const int K_h     = XKeysymToKeycode(dsp, 'h');
    const int K_j     = XKeysymToKeycode(dsp, 'j');
    const int K_k     = XKeysymToKeycode(dsp, 'k');
    const int K_l     = XKeysymToKeycode(dsp, 'l');

    // add error handler and send request to server
    XSetErrorHandler(&handle_error);
    XSync(dsp, false);

    // load font for window titles
    font = XLoadQueryFont(dsp, "fixed");

    // grab mod+space for raising
    XGrabKey(dsp, K_opave,
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

    subw = None;
    kcnt = 2;
    while (true) {
        ltime = time(NULL);
        XNextEvent(dsp, &e);
        printf("Recv: event, type: %d\n", e.type);

        if (e.type == MapRequest) {
            // map window with frame and add to list
            printf("Mapping window: %d\n", e.xmaprequest.window);
            XGetWindowAttributes(dsp, e.xmaprequest.window,
                    &attrs);

            add_frame_to_window(dsp, root, e.xmaprequest.window, attrs, font);

            for (int i = 0; i < 32; i++) {
                if (wndws[i] == 0) {
                    wndws[i] = e.xmaprequest.window;
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
            for (int i = 0; i < 32; i++) {
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

            int Kp = e.xkey.keycode;
            if (Kp == K_opave)
                XRaiseWindow(dsp, subw);
            else if (Kp == K_h)
                XMoveResizeWindow(dsp, subw,
                        attrs.x - kcnt, attrs.y,
                        attrs.width, attrs.height);
            else if (Kp == K_j)
                XMoveResizeWindow(dsp, subw,
                        attrs.x, attrs.y + kcnt,
                        attrs.width, attrs.height);
            else if (Kp == K_k)
                XMoveResizeWindow(dsp, subw,
                        attrs.x, attrs.y - kcnt,
                        attrs.width, attrs.height);
            else if (Kp == K_l)
                XMoveResizeWindow(dsp, subw,
                        attrs.x + kcnt, attrs.y,
                        attrs.width, attrs.height);

        } else if (e.type == ButtonPress &&
                e.xbutton.subwindow != None) {
            // optional focus on alt+leftclick
            XGetWindowAttributes(dsp, e.xbutton.subwindow,
                    &attrs);
            subw = e.xbutton.window;
            printf("%dx%d @ %d,%d\n",
                    attrs.width, attrs.height, attrs.x, attrs.y);
        }
    }
    return 0;
}



