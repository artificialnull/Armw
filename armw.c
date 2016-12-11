#define MAX_WINS 32
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    Window wndw;
    Window fram;
    int x;
    int y;
    int w;
    int h;
} Viewable;


int gen_suitable_random(int max, int limiter) {
    return rand() % (max - limiter);
}

int handle_error(Display *dsp, XErrorEvent *err) {
    char err_text[1024];
    XGetErrorText(dsp, err->error_code, err_text, sizeof(err_text));
    puts("Encountered Error!");
    printf("Request:     %d\nError code:  %d\nError text:  %s\nResource ID: %d\n",
            err->request_code, err->error_code, err_text, err->resourceid);
    return 0;
}

void start_external(char *toRun) {
    if (fork() == 0) {
        puts("This is the child speaking");
        system(toRun);
        exit(0);
    } else {
        puts("This is the parent speaking");
    }
}

void draw_title_on_frame(Display *dsp, Window frame, XFontStruct *font,
        char *title, int ascent, int descent) {
    XWindowAttributes frAttrs;
    XGetWindowAttributes(dsp, frame, &frAttrs);
    GC gc = XCreateGC(dsp, frame, 0, 0);

    XSetBackground(dsp, gc, 0x181818);
    XSetForeground(dsp, gc, 0x7cafc2);
    XSetFont(dsp, gc, font->fid);

    XClearArea(dsp, frame, 0, frAttrs.height - ascent - descent, frAttrs.width, ascent + descent, true);
    XDrawString(dsp, frame, gc, 2, frAttrs.height - descent, title, strlen(title));
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

bool array_has_blank(Window *winarray) {
    for (int i = 0; i < MAX_WINS; i++) {
        if (winarray[i] == 0) {
            return true;
        }
    }
    return false;
}

char *get_title_of_window(Display *dsp, Window titled, char *ttl,
        int *asc, int *desc, XFontStruct *font) {
    int direction, ascent, descent;
    XCharStruct overall;

    XFetchName(dsp, titled, &ttl);
    if (ttl == NULL) {
        XTextProperty textp_return;
        XGetWMName(dsp, titled, &textp_return);
        ttl = textp_return.value;
        if (ttl == NULL) {
            ttl = "Armw Window";
        }
    }

    XTextExtents(font, ttl, strlen(ttl), &direction, &ascent, &descent, &overall);
    *asc = ascent;
    *desc = descent;
    return ttl;
}


Window add_frame_to_window(Display *dsp, Window root, Window toFrame,
        XWindowAttributes attrs, XFontStruct *font) {
    puts("Adding frame to window");
    int ascent, descent;
    char *title;
    puts("Getting name of window");
    get_title_of_window(dsp, toFrame, title, &ascent, &descent, font);
    puts("Calculated text dimensions");
    Window frame = XCreateSimpleWindow(
            dsp, root, attrs.x, attrs.y,
            attrs.width, attrs.height + (ascent + descent), 2, 0x7cafc2, 0x181818);
    puts("Created frame window");

    XReparentWindow(dsp, toFrame, frame, 0, 0);
    XSelectInput(dsp, frame,
            SubstructureRedirectMask | SubstructureNotifyMask
            | PropertyChangeMask | EnterWindowMask | FocusChangeMask);
    XMapWindow(dsp, frame);
    XMapWindow(dsp, toFrame);

    XStoreName(dsp, frame, title);
    return frame;
}

int main() {
    srand(time(NULL));
    XWindowAttributes attrs;
    Window wndws[MAX_WINS] = {0};
    Window frams[MAX_WINS] = {0};

    // initialize display and root window
    Display *dsp = XOpenDisplay(0);
    Window root = DefaultRootWindow(dsp);

    int dispW, dispH, dispX, dispY, dispBW, dispZ;
    XGetGeometry(dsp, root, &root,
            &dispX, &dispY,
            &dispW, &dispH,
            &dispBW, &dispZ);
    printf("Display dimensions: %dx%d\n", dispW, dispH);

    XSelectInput(dsp, root,
            SubstructureRedirectMask | SubstructureNotifyMask |
            PropertyChangeMask | 0);


    Atom WM_PROTOCOLS     = XInternAtom(dsp, "WM_PROTOCOLS",     false);
    Atom WM_DELETE_WINDOW = XInternAtom(dsp, "WM_DELETE_WINDOW", false);

    // compute keycodes for necessary keys
    const int K_opabe = XKeysymToKeycode(dsp, ' ');
    const int K_h     = XKeysymToKeycode(dsp, 'h');
    const int K_j     = XKeysymToKeycode(dsp, 'j');
    const int K_k     = XKeysymToKeycode(dsp, 'k');
    const int K_l     = XKeysymToKeycode(dsp, 'l');
    const int K_r     = XKeysymToKeycode(dsp, 'r');
    const int K_q     = XKeysymToKeycode(dsp, 'q');
    const int K_d     = XKeysymToKeycode(dsp, 'd');

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

    // grab mod+q for closing windows
    XGrabKey(dsp, K_q,
            Mod1Mask, root, true,
            GrabModeAsync, GrabModeAsync);

    // grad mod+d for dmenu
    XGrabKey(dsp, K_d,
            Mod1Mask, root, true,
            GrabModeAsync, GrabModeAsync);

    Window subw = None;
    int kcnt = 2;
    bool resizing = false;
    unsigned long ltime = time(NULL);
    int count = 0;

    XEvent e;
    while (true) {
        if (XPending(dsp) > 0) {
            XNextEvent(dsp, &e);
        } else {
            puts("No events, titling windows in spare time");
            bool didAnything = false;
            for (int i = 0; i < MAX_WINS; i++) {
                Window wndw = wndws[i];
                if (wndw != 0) {
                    int ascent, descent;
                    char *title;
                    title = get_title_of_window(dsp, wndw, title, &ascent, &descent, font);
                    assert(title != NULL);
                    XGetWindowAttributes(dsp, frams[i],
                            &attrs);
                    draw_title_on_frame(dsp, frams[i],
                            font, title, ascent, descent);
                    didAnything = true;
                }
            }
            while (XPending(dsp) == 0 && count < (didAnything ? 40 : 80)) {
                struct timespec tosleep;
                tosleep.tv_sec = 0;
                tosleep.tv_nsec = 25000000;
                struct timespec remsleep;
                nanosleep(&tosleep, &remsleep);
                count++;
            }
            count = 0;
            continue;
        }

        printf("Recv: event, type: %d \n", e.type);

        if (e.type == MapRequest) {
            // map window with frame and add to list

            puts("Attempting to map window");
            if (!array_has_blank(wndws)) {
                printf("Cannot map window: %d, destroying!\n", e.xmaprequest.window);
                XDestroyWindow(dsp, e.xmaprequest.window);

                continue;
            }
            XGetWindowAttributes(dsp, e.xmaprequest.window,
                    &attrs);
            puts("Got attributes");
            printf("%dx%d\n", attrs.width, attrs.height);
            if (attrs.width == dispW) {
                attrs.width -= 10;
            }
            if (attrs.height == dispH) {
                attrs.height -= 10;
            }
            attrs.x = gen_suitable_random(dispW, attrs.width);
            attrs.y = gen_suitable_random(dispH, attrs.height);
            puts("Generated suitable position");

            Window frame = add_frame_to_window(dsp, root, e.xmaprequest.window, attrs, font);
            puts("Added frame to window");

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
            /* if (e.xexpose.window != root) {
                int ascent, descent, direction;
                XCharStruct overall;
                char *title;
                int fsch;
                if (find_window_in_array(wndws, e.xproperty.window) == -1) {
                    fsch = find_window_in_array(frams, e.xproperty.window);
                }
                Window wndw = wndws[fsch];

                XFetchName(dsp, wndw, &title);
                //title = "meems";
                XTextExtents(font, title, strlen(title), &direction, &ascent, &descent, &overall);
                XGetWindowAttributes(dsp, e.xexpose.window,
                        &attrs);
                draw_title_on_frame(dsp, e.xexpose.window,
                        font, title, ascent, descent);
            } */
        } else if (e.type == PropertyNotify) {
            /*
            XGetWindowAttributes(dsp, e.xproperty.window,
                    &attrs);
            puts("PropertyChange!");
            int fsch;
            if (find_window_in_array(wndws, e.xproperty.window) == -1) {
                fsch = find_window_in_array(frams, e.xproperty.window);
            }
            Window wndw = wndws[fsch];
            printf("window: %d\n", wndw);
            int ascent, descent, direction;
            XCharStruct overall;
            char *title;
            XFetchName(dsp, wndw, &title);
            XTextExtents(font, title, strlen(title), &direction, &ascent, &descent, &overall);
            draw_title_on_frame(dsp, e.xexpose.window,
                    font, title, ascent, descent);
            */
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
            int fsch = find_window_in_array(frams, e.xcrossing.window);
            if (fsch != -1) {
                e.xcrossing.subwindow = wndws[fsch];
            }

            printf("Entered window: %d\n", e.xcrossing.subwindow);
            XGetWindowAttributes(dsp, e.xcrossing.subwindow,
                    &attrs);
            subw = e.xcrossing.window;
            XSetInputFocus(dsp, e.xcrossing.subwindow, RevertToPointerRoot, CurrentTime);
            printf("%dx%d @ %d,%d\n",
                    attrs.width, attrs.height, attrs.x, attrs.y);
        } else if (e.type == KeyPress && subw != None) {
            // handle various keyboard actions
            XGetWindowAttributes(dsp, subw, &attrs);

            if (time(NULL) - ltime < 2) { kcnt++; }
            else { kcnt = 2; }

            ltime = time(NULL);

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
            printf("kcnt: %d\n", kcnt);

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
            else if (Kp == K_d)
                start_external("dmenu_run");
            else if (Kp == K_q) {
                Atom *supported;
                int num_supported;
                XGetWMProtocols(dsp, wndw, &supported, &num_supported);
                bool found = false;
                for (int i = 0; i < num_supported; i++) {
                    if (supported[i] == WM_DELETE_WINDOW) {
                        printf("%d\n", supported[i]);
                        found = true;
                        puts("WM_DELETE_WINDOW present!");
                        break;
                    }
                }
                if (found) {
                    printf("Sending killMsg to window: %d\n", wndw);
                    XEvent killMsg;
                    killMsg.xclient.type = ClientMessage;
                    killMsg.xclient.message_type = WM_PROTOCOLS;
                    killMsg.xclient.window = wndw;
                    killMsg.xclient.format = 32;
                    killMsg.xclient.data.l[0] = WM_DELETE_WINDOW;
                    XSendEvent(dsp, wndw, false, 0, &killMsg);
                } else {
                    printf("Killing window: %d\n", wndw);
                    XKillClient(dsp, wndw);
                    printf("Killing frame: %d\n", fram);
                    XKillClient(dsp, fram);
                }
            }

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
