#define MAX_WINS 32
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

// Viewable struct for storing window-frame pair
// plus possibly some other stuff later
typedef struct Viewable Viewable;
struct Viewable {
    Window wndw;
    Window fram;
    int left;
    int rite;
    int topp;
    int botm;
};

// used for window positioning
int gen_suitable_random(int max, int limiter) {
    return rand() % (max - limiter);
}

// simple error handler called when something goes wrong
int handle_error(Display *dsp, XErrorEvent *err) {
    char err_text[1024];
    XGetErrorText(dsp, err->error_code, err_text, sizeof(err_text));
    puts("Encountered Error!");
    printf("Request:     %d\nError code:  %d\nError text:  %s\nResource ID: %d\n",
            err->request_code, err->error_code, err_text, err->resourceid);
    return 0;
}

// used to run external commands (eg dmenu) without stopping the wm
void start_external(char *toRun) {
    if (fork() == 0) {
        puts("This is the child speaking");
        system(toRun);
        exit(0);
    } else {
        puts("This is the parent speaking");
    }
}

// called in main loop, draws title string to the bottom left of a frame
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
}

// todo: delet this
int find_window_in_array(Window *winarray, Window query) {
    for (int i = 0; i < MAX_WINS; i++) {
        if (query == winarray[i]) {
            return i;
        }
    }
    return -1;
}

// attempts to get a title through FetchName, uses WMName otherwise
// generally called with draw_title_on_frame
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

// called when mapping window, used to add parent frame to show title, have border, etc
// gets title and stores it in the frame (not really used), but does not actually draw
// the title on the frame
Window add_frame_to_window(Display *dsp, Window root, Window toFrame,
        XWindowAttributes attrs, XFontStruct *font) {
    int ascent, descent;
    char *title;
    title = get_title_of_window(dsp, toFrame, title, &ascent, &descent, font);
    Window frame = XCreateSimpleWindow(
            dsp, root, attrs.x, attrs.y,
            attrs.width - 4, attrs.height - 4, 2, 0x7cafc2, 0x181818);

    XReparentWindow(dsp, toFrame, frame, 0, 0);
    XSelectInput(dsp, frame,
            SubstructureRedirectMask | SubstructureNotifyMask
            | PropertyChangeMask | EnterWindowMask | FocusChangeMask);
    XMoveResizeWindow(dsp, toFrame,
            0, 0,
            attrs.width - 4, attrs.height - 4 - (ascent + descent));

    XMapWindow(dsp, frame);
    XMapWindow(dsp, toFrame);

    //XStoreName(dsp, frame, title);
    return frame;
}

// contains variable decls
int main() {
    srand(time(NULL)); // seed the rng for window positioning
    XWindowAttributes attrs;
    Viewable vwbls[MAX_WINS]; // create array of Viewables for organization
    // and set all viewables to 0
    for (int i = 0; i < MAX_WINS; i++) {
        vwbls[i].wndw = 0;
        vwbls[i].fram = 0;
        vwbls[i].left = -1;
        vwbls[i].rite = -1;
        vwbls[i].topp = -1;
        vwbls[i].botm = -1;
    }

    // initialize display and root window
    Display *dsp = XOpenDisplay(0);
    Window root = DefaultRootWindow(dsp);

    // get display geometry for random calculations
    int dispW, dispH, dispX, dispY, dispBW, dispZ;
    XGetGeometry(dsp, root, &root,
            &dispX, &dispY,
            &dispW, &dispH,
            &dispBW, &dispZ);
    printf("Display dimensions: %dx%d\n", dispW, dispH);

    // set input masks for the root window so we can get events
    XSelectInput(dsp, root,
            SubstructureRedirectMask | SubstructureNotifyMask |
            PropertyChangeMask | 0);

    XSetInputFocus(dsp, root, RevertToPointerRoot, CurrentTime);

    // intern some atoms so we can change and compare them later
    Atom WM_PROTOCOLS     = XInternAtom(dsp, "WM_PROTOCOLS", false);
    Atom WM_DELETE_WINDOW = XInternAtom(dsp, "WM_DELETE_WINDOW", false);
    Atom WM_SUPP_CHECK    = XInternAtom(dsp, "_NET_SUPPORTING_WM_CHECK", false);
    Atom WM_NAME          = XInternAtom(dsp, "_NET_WM_NAME", false);
    Atom UTF8_STR         = XInternAtom(dsp, "UTF8_STRING", false);
    XChangeProperty(dsp, root, WM_SUPP_CHECK, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&root,  1);
    XChangeProperty(dsp, root, WM_NAME,       UTF8_STR,  8,  PropModeReplace, (unsigned char *)"Armw", 5);

    // compute keycodes for necessary keys
    const int K_opabe = XKeysymToKeycode(dsp, ' ');
    const int K_h     = XKeysymToKeycode(dsp, 'h');
    const int K_j     = XKeysymToKeycode(dsp, 'j');
    const int K_k     = XKeysymToKeycode(dsp, 'k');
    const int K_l     = XKeysymToKeycode(dsp, 'l');
    const int K_r     = XKeysymToKeycode(dsp, 'r');
    const int K_q     = XKeysymToKeycode(dsp, 'q');
    const int K_d     = XKeysymToKeycode(dsp, 'd');
    const int K_e     = XKeysymToKeycode(dsp, 'e');
    const int K_b     = XKeysymToKeycode(dsp, 'b');
    const int K_v     = XKeysymToKeycode(dsp, 'v');

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

    // grab buttons for switching focus
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


    // grab buttons for moving
    XGrabKey(dsp, K_h,
            Mod1Mask | ShiftMask, root, true,
            GrabModeAsync, GrabModeAsync);
    XGrabKey(dsp, K_j,
            Mod1Mask | ShiftMask, root, true,
            GrabModeAsync, GrabModeAsync);
    XGrabKey(dsp, K_k,
            Mod1Mask | ShiftMask, root, true,
            GrabModeAsync, GrabModeAsync);
    XGrabKey(dsp, K_l,
            Mod1Mask | ShiftMask, root, true,
            GrabModeAsync, GrabModeAsync);

    // grab mod+mouse for focus
    XGrabButton(dsp, 1, Mod1Mask, root, true,
            ButtonPressMask, GrabModeAsync,
            GrabModeAsync, None, None);

    // grab mod+shift+q for closing windows
    XGrabKey(dsp, K_q,
            Mod1Mask | ShiftMask, root, true,
            GrabModeAsync, GrabModeAsync);

    // mod+shift+e for closing wm
    XGrabKey(dsp, K_e,
            Mod1Mask | ShiftMask, root, true,
            GrabModeAsync, GrabModeAsync);

    // grab mod+d for dmenu
    XGrabKey(dsp, K_d,
            Mod1Mask, root, true,
            GrabModeAsync, GrabModeAsync);

    // grab keys for changing tiling style
    XGrabKey(dsp, K_b,
            Mod1Mask, root, true,
            GrabModeAsync, GrabModeAsync);
    XGrabKey(dsp, K_v,
            Mod1Mask, root, true,
            GrabModeAsync, GrabModeAsync);


    // final variable declarations
    int subw = -1;
    int kcnt = 2;
    int filled = 0;
    bool resizing = false;
    unsigned long ltime = time(NULL);
    int count = 0;
    bool tilingVertically = false;
    XEvent e;

    // main loop, contains event checking and processing
    while (true) {
        if (XPending(dsp) > 0) {
            XNextEvent(dsp, &e); // get the next event if there is one
            // attempt to avoid blocking
        } else {
            // title all frames in every Viewable
            bool didAnything = false;
            for (int i = 0; i < MAX_WINS; i++) {
                Viewable vwbl = vwbls[i];
                if (vwbl.wndw != 0) {
                    int ascent, descent;
                    char *title;
                    title = get_title_of_window(dsp, vwbl.wndw, title, &ascent, &descent, font);
                    assert(title != NULL);
                    XGetWindowAttributes(dsp, vwbl.fram,
                            &attrs);
                    draw_title_on_frame(dsp, vwbl.fram,
                            font, title, ascent, descent);
                    didAnything = true;
                }
            }

            // semi-sleep for a while if there are no events
            // this goes on for longer if no windows were actually titled
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

        //printf("Recv: event, type: %d \n", e.type);
        if (e.type == MapRequest) {
            // map window with frame and add the ids to an available Viewable
            puts("Attempting to map window");
            XGetGeometry(dsp, root, &root,
                    &dispX, &dispY,
                    &dispW, &dispH,
                    &dispBW, &dispZ);


            // if there is space in the Viewable list, then we can map the window
            // otherwise, reject it (possibly will cause a crash but idk)
            bool foundBlank = false;
            for (int i = 0; i < MAX_WINS; i++) {
                if (vwbls[i].wndw == 0) {
                    foundBlank = true;
                    break;
                }
            }
            if (!foundBlank) {
                printf("Cannot map window: %d, destroying!\n", e.xmaprequest.window);
                XDestroyWindow(dsp, e.xmaprequest.window);

                continue;
            }

            // get and set attributes needed for random positioning
            XGetWindowAttributes(dsp, e.xmaprequest.window,
                    &attrs);

            printf("There are currently %d windows filled\n", filled);
            if (filled == 0) {
                // fill whole screen if nothing else is there
                puts("Setting attrs to fullscreen");
                printf("%dx%d\n", dispW, dispH);
                attrs.width = dispW;
                attrs.height = dispH;
                attrs.x = 0;
                attrs.y = 0;
            } else {
                XWindowAttributes priorAttrs, pwndwAttrs;
                XGetWindowAttributes(dsp, vwbls[subw].fram, &priorAttrs);
                XGetWindowAttributes(dsp, vwbls[subw].wndw, &pwndwAttrs);
                int ascent, descent, direction;
                XCharStruct overall;
                XTextExtents(font, "Ag", strlen("Ag"), &direction, &ascent, &descent, &overall);

               if (tilingVertically) {
                    XMoveResizeWindow(dsp, vwbls[subw].fram,
                            priorAttrs.x, priorAttrs.y,
                            priorAttrs.width, priorAttrs.height / 2);
                    // we get the attributes for the window and the frame
                    // then we resize the frame to make room for the new Viewable

                    XMoveResizeWindow(dsp, vwbls[subw].wndw,
                            pwndwAttrs.x, pwndwAttrs.y,
                            pwndwAttrs.width, (priorAttrs.height / 2) - (ascent + descent));
                    // and, after calculating the font dims (urggg) we resize the window as well

                    priorAttrs.width += 4;
                    priorAttrs.height += 4;
                    // adjusting for 2px borders

                    printf("%dx%d @ %d,%d\n", priorAttrs.width, priorAttrs.height, priorAttrs.x, priorAttrs.y);
                    attrs.width = priorAttrs.width;
                    attrs.x = priorAttrs.x;
                    attrs.y = priorAttrs.y + (priorAttrs.height / 2);
                    attrs.height = priorAttrs.height / 2;
                    // we set the new window to take up the space which was left over when we resized the old one
                    // essentially, we split the old window in two and filled in the new half
                } else {
                    // same thing for horizontal, but font stuff isnt needed here
                    XMoveResizeWindow(dsp, vwbls[subw].fram,
                            priorAttrs.x, priorAttrs.y,
                            priorAttrs.width / 2, priorAttrs.height);
                    XMoveResizeWindow(dsp, vwbls[subw].wndw,
                            pwndwAttrs.x, pwndwAttrs.y,
                            (priorAttrs.width / 2), pwndwAttrs.height);
                    priorAttrs.width += 4;
                    priorAttrs.height += 4;

                    attrs.width = priorAttrs.width / 2;
                    attrs.x = priorAttrs.x + (priorAttrs.width / 2);
                    attrs.y = priorAttrs.y;
                    attrs.height = priorAttrs.height;
                }
            }

            printf("Requesting %dx%d @ %d,%d\n", attrs.width, attrs.height, attrs.x, attrs.y);
            // actually add the frame here (function includes the mapping of both window and frame
            Window frame = add_frame_to_window(dsp, root, e.xmaprequest.window, attrs, font);
            if (filled == 0) {
                XSetInputFocus(dsp, e.xmaprequest.window, RevertToPointerRoot, CurrentTime);
                subw = -1;
            }

            // finally, store the ids in an empty viewable
            printf("Mapping window: %d/frame: %d\n", e.xmaprequest.window, frame);
            for (int i = 0; i < MAX_WINS; i++) {
                if (vwbls[i].wndw == 0) {
                    vwbls[i].wndw = e.xmaprequest.window;
                    vwbls[i].fram = frame;
                    if (subw != -1) {
                        puts("This isn't the first window, so we can set some properties");
                        if (tilingVertically) {
                            vwbls[i].topp = subw;
                            vwbls[i].left = vwbls[subw].left;
                            vwbls[i].rite = vwbls[subw].rite;
                            printf("Set topp on [%d] window to: %d\n", i, vwbls[subw].wndw);
                            vwbls[subw].botm = i;
                            printf("Set botm on [%d] window to: %d\n", subw, vwbls[i].wndw);
                        } else {
                            vwbls[i].left = subw;
                            vwbls[i].topp = vwbls[subw].topp;
                            vwbls[i].botm = vwbls[subw].botm;
                            vwbls[subw].rite = i;
                        }
                    } else {
                        puts("This is the first window, so we can't set any properties");
                        subw = i;
                    }
                    filled++;
                    break;
                }
            }
            puts("Finished mapping Viewable");
        } else if (e.type == Expose) {
        } else if (e.type == PropertyNotify) {
        } else if (e.type == DestroyNotify) {
            puts("Destroying a window");
            // destroy empty frames and remove window from list
            for (int i = 0; i < MAX_WINS; i++) {
                if (vwbls[i].wndw == e.xdestroywindow.window) {
                    // find the appropriate Viewable, unparent the window from the frame,
                    // kill the frame, and reset the Viewable
                    Window focused = vwbls[subw].wndw;
                    int toFocus;
                    printf("Focused: %d vs ToDelete: %d\n", focused, vwbls[i].wndw);
                    if (focused == vwbls[i].wndw) {
                        puts("Refocusing, dont want to delete focused window");
                        if (vwbls[i].topp != -1) {
                            toFocus = vwbls[i].topp;
                            puts("Focusing on top");
                        } else if (vwbls[i].botm != -1) {
                            toFocus = vwbls[i].botm;
                            puts("Focusing on bottom");
                        } else if (vwbls[i].left != -1) {
                            toFocus = vwbls[i].left;
                            puts("Focusing on left");
                        } else if (vwbls[i].rite != -1) {
                            toFocus = vwbls[i].rite;
                            puts("Focusing on right");
                        } else {
                            puts("Focusing on root");
                            toFocus = root;
                        }
                        if (toFocus != root) {
                            printf("Refocusing on window: %d\n", vwbls[toFocus].wndw);
                            XSetInputFocus(dsp, vwbls[toFocus].wndw, RevertToPointerRoot, CurrentTime);
                            subw = toFocus;
                        } else {
                            XSetInputFocus(dsp, toFocus, RevertToPointerRoot, CurrentTime);
                            subw = -1;
                        }
                    }

                    printf("Destroyed window: %d -> frame: %d\n", vwbls[i].wndw,
                            vwbls[i].fram);
                    XUnmapWindow(dsp, vwbls[i].fram);
                    XReparentWindow(dsp, vwbls[i].wndw, root, 0, 0);
                    XDestroyWindow(dsp, vwbls[i].fram);
                    vwbls[i].wndw = 0;
                    vwbls[i].fram = 0;
                    if (vwbls[i].botm != -1 && vwbls[vwbls[i].botm].topp == i) {
                        vwbls[vwbls[i].botm].topp = vwbls[i].topp;
                    }
                    if (vwbls[i].topp != -1 && vwbls[vwbls[i].topp].botm == i) {
                        vwbls[vwbls[i].topp].botm = vwbls[i].botm;
                    }
                    if (vwbls[i].left != -1 && vwbls[vwbls[i].left].rite == i) {
                        vwbls[vwbls[i].left].rite = vwbls[i].rite;
                    }
                    if (vwbls[i].rite != -1 && vwbls[vwbls[i].rite].left == i) {
                        vwbls[vwbls[i].rite].left = vwbls[i].left;
                    }
                    vwbls[i].botm = -1;
                    vwbls[i].topp = -1;
                    vwbls[i].left = -1;
                    vwbls[i].rite = -1;
                    filled--;
                    printf("There are now %d windows\n", filled);
                    break;
                }
            }
        } else if (e.type == EnterNotify) {
            /*
            // change focus based on location of mouse
            for (int i = 0; i < MAX_WINS; i++) {
                if (vwbls[i].wndw == e.xcrossing.subwindow ||
                        vwbls[i].fram == e.xcrossing.window) {
                    // match the event subwindow with a Viewable and save the Viewable's
                    // frame and window info
                    e.xcrossing.subwindow = vwbls[i].wndw;
                    subw = i;
                    break;
                }
            }

            printf("Entered window: %d\n", vwbls[subw].wndw);
            XGetWindowAttributes(dsp, vwbls[subw].wndw,
                    &attrs);
            // explicitly change focus to the desired window
            XSetInputFocus(dsp, vwbls[subw].wndw, RevertToPointerRoot, CurrentTime);
            printf("%dx%d @ %d,%d\n",
                    attrs.width, attrs.height, attrs.x, attrs.y);
                    */
        } else if (e.type == KeyPress && e.xkey.keycode == K_e) {
            // kill the wm with a cheerful message
            puts("Gonna go die now, seeya!");
            exit(0);
        } else if (e.type == KeyPress) {
            puts("Handling keypresses...");
            // handle various keyboard actions
            if (subw != -1) {
                XGetWindowAttributes(dsp, vwbls[subw].wndw, &attrs);
            }

            // set the movement scale based on how long its been since the last keyboard event
            // may change this in the future for consistency
            if (time(NULL) - ltime < 2) { kcnt++; }
            else { kcnt = 2; }
            ltime = time(NULL);

            // get attributes about the current focused window
            Window wndw;
            Window fram;
            XWindowAttributes wndwAttrs;
            XWindowAttributes framAttrs;

            if (subw != -1) {
                wndw = vwbls[subw].wndw;
                fram = vwbls[subw].fram;
                printf("Got keypress from window: %d/frame: %d\n", wndw, fram);
                XGetWindowAttributes(dsp, wndw, &wndwAttrs);
                XGetWindowAttributes(dsp, fram, &framAttrs);
            } else {
                puts("Got keypress from root");
            }

            printf("%d\n", e.xkey.state);

            // long if-else chain to act on the window
            int Kp = e.xkey.keycode;
            if (Kp == K_opabe) {
                // put window on top if so desired
                XRaiseWindow(dsp, vwbls[subw].fram);
            } else if (Kp == K_h) {
                // handle left actions (resizing and movement)
                if (resizing) {
                    if (wndwAttrs.width > kcnt) {
                        XMoveResizeWindow(dsp, wndw,
                                wndwAttrs.x, wndwAttrs.y,
                                wndwAttrs.width - kcnt, wndwAttrs.height);
                        XMoveResizeWindow(dsp, fram,
                                framAttrs.x, framAttrs.y,
                                framAttrs.width - kcnt, framAttrs.height);
                    }
                } else {
                    if (e.xkey.state == 9) {
                        XMoveResizeWindow(dsp, vwbls[subw].fram,
                                framAttrs.x - kcnt, framAttrs.y,
                                framAttrs.width, framAttrs.height);
                    } else if (e.xkey.state == 8 && vwbls[subw].left != -1) {
                        XSetInputFocus(dsp, vwbls[vwbls[subw].left].wndw, RevertToPointerRoot, CurrentTime);
                        subw = vwbls[subw].left;
                    }
                }
            } else if (Kp == K_j) {
                // handle down actions
                if (resizing) {
                    XMoveResizeWindow(dsp, wndw,
                            wndwAttrs.x, wndwAttrs.y,
                            wndwAttrs.width, wndwAttrs.height + kcnt);
                    XMoveResizeWindow(dsp, fram,
                            framAttrs.x, framAttrs.y,
                            framAttrs.width, framAttrs.height + kcnt);
                } else {
                    if (e.xkey.state == 9) {
                        XMoveResizeWindow(dsp, vwbls[subw].fram,
                                framAttrs.x, framAttrs.y + kcnt,
                                framAttrs.width, framAttrs.height);
                    } else if (e.xkey.state == 8 && vwbls[subw].botm != -1) {
                        XSetInputFocus(dsp, vwbls[vwbls[subw].botm].wndw, RevertToPointerRoot, CurrentTime);
                        printf("Focus changed to window: %d\n", vwbls[vwbls[subw].botm].wndw);
                        subw = vwbls[subw].botm;
                    }
                }
            } else if (Kp == K_k) {
                // handle up actions
                if (resizing) {
                    if (wndwAttrs.height > kcnt) {
                        XMoveResizeWindow(dsp, wndw,
                                wndwAttrs.x, wndwAttrs.y,
                                wndwAttrs.width, wndwAttrs.height - kcnt);
                        XMoveResizeWindow(dsp, fram,
                                framAttrs.x, framAttrs.y,
                                framAttrs.width, framAttrs.height - kcnt);
                    }
                } else {
                    if (e.xkey.state == 9) {
                        XMoveResizeWindow(dsp, vwbls[subw].fram,
                                framAttrs.x, framAttrs.y - kcnt,
                                framAttrs.width, framAttrs.height);
                    } else if (e.xkey.state == 8 && vwbls[subw].topp != -1) {
                        XSetInputFocus(dsp, vwbls[vwbls[subw].topp].wndw, RevertToPointerRoot, CurrentTime);
                        printf("Focus changed to window: %d\n", vwbls[vwbls[subw].topp].wndw);
                        subw = vwbls[subw].topp;
                    }
                }
            } else if (Kp == K_l) {
                // handle right actions
                if (resizing) {
                    XMoveResizeWindow(dsp, wndw,
                            wndwAttrs.x, wndwAttrs.y,
                            wndwAttrs.width + kcnt, wndwAttrs.height);
                    XMoveResizeWindow(dsp, fram,
                            framAttrs.x, framAttrs.y,
                            framAttrs.width + kcnt, framAttrs.height);
                } else {
                    if (e.xkey.state == 9) {
                        XMoveResizeWindow(dsp, vwbls[subw].fram,
                                framAttrs.x + kcnt, framAttrs.y,
                                framAttrs.width, framAttrs.height);
                    } else if (e.xkey.state == 8 && vwbls[subw].rite != -1) {
                        XSetInputFocus(dsp, vwbls[vwbls[subw].rite].wndw, RevertToPointerRoot, CurrentTime);
                        subw = vwbls[subw].rite;
                    }
                }
            } else if (Kp == K_r) {
                // toggle resize mode
                resizing = !resizing;
            } else if (Kp == K_d) {
                // start dmenu
                start_external("dmenu_run");
            } else if (Kp == K_b) {
                tilingVertically = false;
            } else if (Kp == K_v) {
                tilingVertically = true;
            } else if (Kp == K_q) {
                // kill the window in the best way possible

                Atom *supported;
                int num_supported;

                // get the available window protocols and see if it supports
                // elegant killing
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
                    // elegantly kill a window with support
                    printf("Sending killMsg to window: %d\n", wndw);
                    XEvent killMsg;
                    killMsg.xclient.type = ClientMessage;
                    killMsg.xclient.message_type = WM_PROTOCOLS;
                    killMsg.xclient.window = wndw;
                    killMsg.xclient.format = 32;
                    killMsg.xclient.data.l[0] = WM_DELETE_WINDOW;
                    XSendEvent(dsp, wndw, false, 0, &killMsg);
                } else {
                    // just kill the client with more simple windows
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
            vwbls[subw].wndw = e.xbutton.subwindow;
            vwbls[subw].fram = e.xbutton.window;
            printf("%dx%d @ %d,%d\n",
                    attrs.width, attrs.height, attrs.x, attrs.y);
        }
    }
    return 0;
}
