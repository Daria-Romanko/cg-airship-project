#include <GL/glew.h>

#include <SFML/Graphics.hpp>
#include <iostream>

#include "game.h"

int main()
{
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.stencilBits = 8;
    settings.majorVersion = 3;
    settings.minorVersion = 3;

    sf::RenderWindow window(
        sf::VideoMode::getDesktopMode(),
        "Delivery Airship",
        sf::Style::Default,
        sf::State::Windowed,
        settings
    );

    if (!window.setActive(true))
        return -1;

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        std::cerr << "Failed to initialize GLEW\n";
        return -1;
    }

    Game game(window);
    if (!game.Initialize())
        return -1;

    game.Run();
}
