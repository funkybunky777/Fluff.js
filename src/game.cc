/*
  
  Fluff.js
  Copyright (c) 2010 Ivo Wetzel.
  
  All rights reserved.
  
  Fluff.js is free software: you can redistribute it and/or
  modify it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Fluff.js is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along with
  Fluff.js. If not, see <http://www.gnu.org/licenses/>.
  
*/

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <stdio.h>
#include "graphics.h"
#include "game.h"
#include "input.h"
#include "socket.h"
#include "util.h"
#include "fluff.h"

#include <iostream>
using namespace v8;
using namespace std;


// Game Runtime ----------------------------------------------------------------
// -----------------------------------------------------------------------------
void GameCreate(int width, int height, bool full, bool vsync, int fsaa) {
    if (gameWindow.IsOpened()) {
        gameWindow.Close();
    }
    
    sf::WindowSettings settings;
    settings.DepthBits = 8;
    settings.StencilBits = 8;
    settings.AntialiasingLevel = fsaa;
    
    unsigned long style = sf::Style::Close | (full ? sf::Style::Fullscreen : 0);
    gameWindow.Create(sf::VideoMode(width, height, 32), "", style, settings);
    
    gameWindow.EnableKeyRepeat(false);
    gameWindow.UseVerticalSync(vsync);
    gameWindow.SetFramerateLimit(gameFPS);
    gameWindow.PreserveOpenGLStates(gameUseFonts);
    GameInitGL();
    gameRunning = true;
}

void GameSetCaption(const char *caption) {
    // TODO modify SFML to support this -.-"
}

void GameSetSize(int width, int height) {
    gameWindow.SetSize(width, height);
}

void GameInitGL(void) {
    if (gameRunning) {
        glPopMatrix();
    }
    glPushMatrix();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, gameWindow.GetWidth(), gameWindow.GetHeight(), 0, 0, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    
    glClearColor(gameBackColorR, gameBackColorG, gameBackColorB, gameBackColorA);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glEnable(GL_MULTISAMPLE);
    glDisable(GL_BLEND);
    glEnable(GL_BLEND);
    GameSetBlendMode();
    glColor4f(gameColorR, gameColorG, gameColorB, gameColorA);
}

void GameSetBlendMode(void) {
    switch (gameBlendMode) {
        case 0:
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
        
        case 1:
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            break;
        
        default:
            break;
    }
}

void GameResetFonts(void) {
    gameUseFonts = false;
    gameFont = "";
    for(unsigned int i = 0; i < gameFontData.size(); i++) {
        sf::Font *font = gameFontData.at(i);
        string *str = gameFontNames.at(i);
        delete font;
        delete str;
    }
    gameFontData.clear();
    gameFontNames.clear();
}

bool GameLoadFont(string fontName, int size) {
    for(unsigned int i = 0; i < gameFontNames.size(); i++) {
        if (gameFontNames.at(i)->compare(fontName) == 0) {
            gameFont = string(fontName);
            return true;
        }
    }
    
    sf::Font *font = new sf::Font();
    if (font->LoadFromFile(fontName, size)) {
        gameFontData.push_back(font);
        gameFontNames.push_back(new string(fontName));
        gameFont = string(fontName);
        return true;
    
    } else {
        delete font;
        return false;
    }
}

bool GameDrawText(const char *text, string fontName, int x, int y, int size) {
    if (!gameUseFonts) {
        gameUseFonts = true;
        gameWindow.PreserveOpenGLStates(gameUseFonts);
    }
    sf::Font *font = NULL;
    for(unsigned int i = 0; i < gameFontNames.size(); i++) {
        if (gameFontNames.at(i)->compare(fontName) == 0) {
            font = gameFontData.at(i);
            break;
        }
    }
    if (font != NULL) {
        sf::String textData(text, *font, size);
        textData.Move(x, y);
        textData.SetColor(sf::Color(gameColorR * 255, gameColorG * 255,
                                    gameColorB * 255, gameColorA * 255));
        
        if (gameFontRotation != 0) {
            textData.Rotate(gameFontRotation);
        }
        if (gameFontScaleX != 1 || gameFontScaleY != 1) {
            textData.Scale(gameFontScaleX, gameFontScaleY);
        }
        gameWindow.Draw(textData);
        return true;
        
    } else {
        return false;
    }
}

void GameLoop(void) {
    HandleScope scope;
    Handle<Number> delta;
    Handle<Value> args[1];
    
    while (gameRunning) {
        GameEvents();
        GameSockets();
        
        delta = Number::New(gameWindow.GetFrameTime());
        args[0] = delta;
        callFunction("onUpdate", args, 1);
        
        callFunction("onDraw", NULL, 0);
        gameWindow.Display();
        fflush(stdout);
    }
    for(unsigned int i = 0; i < sockets.size(); i++) {
        sockets[i]->close();
    }
    callFunction("onExit", NULL, 0);
    gameWindow.Close();
}


void GameSockets(void) {
    for(unsigned int i = 0; i < sockets.size(); i++) {
        Socket *socket = sockets.at(i);
        if (socket->status > 0 && socket->status != 3) {
            socket->handle();
        }
    }
}

void GameEvents(void) {
    updateInput();
    
    sf::Event event;
    while (gameWindow.GetEvent(event)) {
        switch (event.Type) {
            
            // Keyboard
            case sf::Event::KeyPressed:
                if (event.Key.Code == sf::Key::Escape) {
                    gameRunning = false;
                
                } else if (event.Key.Code == sf::Key::F5) {
                    gameRunning = false;
                    gameReload = true;
                }
                gameKeys[event.Key.Code] = 1;
                
                if (event.Key.Code == sf::Key::LControl
                    || event.Key.Code == sf::Key::RControl) {
                    
                    gameKeyControl = true;
                }
                if (event.Key.Code == sf::Key::LShift
                    || event.Key.Code == sf::Key::RShift) {
                    
                    gameKeyShift = true;
                }
                if (event.Key.Code == sf::Key::LAlt
                    || event.Key.Code == sf::Key::RAlt) {
                    
                    gameKeyAlt = true;
                }
                break;
            
            case sf::Event::KeyReleased:
                gameKeys[event.Key.Code] = 0;
                
                if (event.Key.Code == sf::Key::LControl
                    || event.Key.Code == sf::Key::RControl) {
                    
                    gameKeyControl = false;
                }
                if (event.Key.Code == sf::Key::LShift
                    || event.Key.Code == sf::Key::RShift) {
                    
                    gameKeyShift = false;
                }
                if (event.Key.Code == sf::Key::LAlt
                    || event.Key.Code == sf::Key::RAlt) {
                    
                    gameKeyAlt = false;
                }
                break;
            
            // Window
            case sf::Event::Closed:
                gameRunning = false;
                break;
            
            case sf::Event::GainedFocus:
                gameFocus = true;
                callSimpleEvent("onFocus");
                break;
            
            case sf::Event::LostFocus:
                gameFocus = false;
                if (gameButtonReset) {
                    resetButtons(false);
                }
                if (gameKeyReset) {
                    resetKeys(false);
                }
                callSimpleEvent("onBlur");
                break;
            
            // Mouse
            case sf::Event::MouseWheelMoved:
                gameMouseScroll = event.MouseWheel.Delta;
                break;
            
            case sf::Event::MouseMoved:
                gameMouseX = event.MouseMove.X;
                gameMouseY = event.MouseMove.Y;
                break;
            
            case sf::Event::MouseEntered:
                gameMouse = true;
                callSimpleEvent("onEnter");
                break;
            
            case sf::Event::MouseLeft:
                gameMouse = false;
                if (gameButtonReset) {
                    resetButtons(false);
                }
                callSimpleEvent("onLeave");
                break;
                
            case sf::Event::MouseButtonPressed:
                gameButtons[event.MouseButton.Button] = 1;
                break;
            
            case sf::Event::MouseButtonReleased:
                gameButtons[event.MouseButton.Button] = 0;
                break;
            
            default:
                break;
        }
    }
}

Handle<Value> GameExit(const Arguments& args) {
    gameRunning = false;
    return Undefined();
}

