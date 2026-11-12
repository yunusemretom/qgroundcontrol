/****************************************************************************
 *
 * (c) 2009-2026 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

// SDL2's SDL_IsTablet() (src/SDL.c) calls SDL_IsIPad() on iOS, but SDL_IsIPad()
// lives in SDL's UIKit video backend (src/video/uikit/SDL_uikitvideo.m). QGC builds
// SDL for joysticks only, with SDL_VIDEO OFF, so the symbol was missing and the iOS
// app failed to link. Provide it here with the same behaviour as SDL's version.

#import <UIKit/UIKit.h>

#include <SDL_stdinc.h>

extern "C" SDL_bool SDL_IsIPad(void)
{
    return ([UIDevice currentDevice].userInterfaceIdiom == UIUserInterfaceIdiomPad) ? SDL_TRUE : SDL_FALSE;
}
