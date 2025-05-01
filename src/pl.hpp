/*
   pl.hpp
     - Pilot Light core APIs
*/

#ifndef PL_HPP
#define PL_HPP

#include "pl.h"

namespace PilotLight
{
    class Window
    {
        static const plWindowI* sptApi;

        public:

        plWindow* ptWindow = nullptr;

        static void set_api(plApiRegistryI* ptApiRegistry)
        {
            sptApi = pl_get_api_latest(ptApiRegistry, plWindowI);
        }

        Window(plWindowDesc tDesc)
        {
            sptApi->create(tDesc, &ptWindow);
        };

        ~Window()
        {
            if(ptWindow)
            {
                sptApi->destroy(ptWindow);
                ptWindow = nullptr;
            }
        };

        void show()
        {
            sptApi->show(ptWindow);
        }


    };

    const plWindowI* Window::sptApi = nullptr;

};

#endif