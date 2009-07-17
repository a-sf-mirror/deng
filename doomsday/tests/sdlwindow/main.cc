#include <de/CommandLine>
#include <iostream>
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_syswm.h>

using std::cout;
using std::endl;
using namespace de;

int main(int argc, char* argv[])
{
    CommandLine args(argc, argv);

    cout << "Initializing...\n";    

    SDL_Init(SDL_INIT_VIDEO);
    SDL_EnableUNICODE(SDL_ENABLE);
    SDL_SetVideoMode(640, 480, 0, SDL_OPENGL | SDL_RESIZABLE);

    int w = 640;
    int h = 480;

    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

    int counter = 0;

    bool quitNow = false;
    while(!quitNow)
    {
        SDL_Event ev;
        if(!SDL_PollEvent(&ev))
        {
            glClearColor(++counter/255.f, counter/255.f, counter/255.f, 1);
            if(counter > 255) counter = 0;
            glClear(GL_COLOR_BUFFER_BIT);
            SDL_GL_SwapBuffers();
            SDL_Delay(5);
            continue;
        }

        switch(ev.type)
        {
        case SDL_QUIT:
            quitNow = true;
            break;

        case SDL_ACTIVEEVENT:
            cout << "Active:";
            cout << (ev.active.state & SDL_APPMOUSEFOCUS? " mousefocus" : "") <<
                (ev.active.state & SDL_APPINPUTFOCUS? " inputfocus" : "") <<
                (ev.active.state & SDL_APPACTIVE? " app" : "") << ", gain:" << int(ev.active.gain) << endl;
            break;

        case SDL_KEYDOWN:
            cout << "Keydown: scancode:" << int(ev.key.keysym.scancode) << 
                ", unicode:" << int(ev.key.keysym.unicode) << ", sym:" << int(ev.key.keysym.sym) <<
                ", mod:" << int(ev.key.keysym.mod) << endl;
            break;

        case SDL_MOUSEMOTION:
            break;

        case SDL_SYSWMEVENT:
            if(args.has("--syswm"))
            {
#ifdef WIN32
                cout << "System event: hwnd:" << (void*)(ev.syswm.msg->hwnd) << ", msg:0x" << std::hex
                    << ev.syswm.msg->msg << std::dec << ", w:" << ev.syswm.msg->wParam << ", l:" << 
                    ev.syswm.msg->lParam << endl;
#endif
            }
            break;

        case SDL_VIDEORESIZE:
            cout << "Window resize event: " << ev.resize.w << " x " << ev.resize.h << endl;
            w = ev.resize.w;
            h = ev.resize.h;
            break;

        default:
            cout << "Got event with type " << int(ev.type) << endl;
            break;
        }
    }

    SDL_Quit();
    return 0;
}
