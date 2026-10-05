#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace Config {
constexpr unsigned int width = 800, height = 600;
constexpr float fieldWidth = 800.f, fieldHeight = 600.f;
constexpr float shipHalfSize = 24.f, playerMinimumY = 330.f;
constexpr float playerSpeed = 360.f, enemySpeed = 65.f;
constexpr float projectileSpeed = 650.f, firingInterval = 0.18f;
constexpr float fixedStep = 1.f / 120.f, maximumFrameTime = 0.1f;
constexpr int enemyPoints = 100;
}

enum class State { Ready, Playing, Paused, Won, GameOver };
struct Projectile {
    sf::RectangleShape shape;
    sf::Vector2f velocity{0.f, -Config::projectileSpeed};
    bool active = true;
};
struct Enemy {
    sf::Sprite sprite;
    bool active = true;
    Enemy(const sf::Texture& texture, sf::Vector2f position) : sprite(texture) {
        sprite.setOrigin({12.f, 12.f});
        sprite.setScale({2.f, 2.f});
        sprite.setPosition(position);
    }
};

sf::Image makeShipImage(sf::Color color, bool downward) {
    sf::Image image({24u, 24u}, sf::Color::Transparent);
    for (unsigned int y = 2; y <= 21; ++y) {
        const int halfWidth = 1 + (static_cast<int>(y) - 2) / 2;
        for (unsigned int x = 2; x <= 21; ++x) {
            if (std::abs(static_cast<int>(x) - 12) <= halfWidth)
                image.setPixel({x, y}, color);
        }
    }
    for (unsigned int y = 10; y <= 15; ++y)
        for (unsigned int x = 11; x <= 13; ++x)
            image.setPixel({x, y}, sf::Color(225, 245, 255));
    if (downward) image.flipVertically();
    return image;
}

sf::Vector2f readMovementDirection() {
    sf::Vector2f direction{0.f, 0.f};
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) direction.x -= 1.f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) direction.x += 1.f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)) direction.y -= 1.f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) direction.y += 1.f;
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length > 0.f) { direction.x /= length; direction.y /= length; }
    return direction;
}

void movePlayer(sf::Sprite& player, sf::Vector2f direction, float deltaTime) {
    auto position = player.getPosition();
    position.x += direction.x * Config::playerSpeed * deltaTime;
    position.y += direction.y * Config::playerSpeed * deltaTime;
    position.x = std::clamp(position.x, Config::shipHalfSize, Config::fieldWidth - Config::shipHalfSize);
    position.y = std::clamp(position.y, Config::playerMinimumY, Config::fieldHeight - Config::shipHalfSize);
    player.setPosition(position);
}

void fireProjectile(const sf::Sprite& player, std::vector<Projectile>& projectiles) {
    Projectile projectile;
    projectile.shape.setSize({6.f, 18.f});
    projectile.shape.setOrigin({3.f, 9.f});
    projectile.shape.setFillColor(sf::Color(255, 230, 90));
    const auto position = player.getPosition();
    projectile.shape.setPosition({position.x, position.y - Config::shipHalfSize - 9.f});
    projectiles.push_back(projectile);
}

void createEncounter(std::vector<Enemy>& enemies, const sf::Texture& texture) {
    enemies.clear();
    enemies.reserve(6);
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < 3; ++column)
            enemies.emplace_back(texture, sf::Vector2f{180.f + column * 220.f, 40.f - row * 140.f});
}

void updateProjectiles(std::vector<Projectile>& projectiles, float deltaTime) {
    for (auto& projectile : projectiles) {
        if (!projectile.active) continue;
        projectile.shape.move({projectile.velocity.x * deltaTime, projectile.velocity.y * deltaTime});
        const auto bounds = projectile.shape.getGlobalBounds();
        if (bounds.position.y + bounds.size.y < 0.f) projectile.active = false;
    }
}

void checkProjectileHits(std::vector<Projectile>& projectiles, std::vector<Enemy>& enemies, int& score) {
    for (auto& projectile : projectiles) {
        if (!projectile.active) continue;
        for (auto& enemy : enemies) {
            if (!enemy.active) continue;
            if (projectile.shape.getGlobalBounds().findIntersection(enemy.sprite.getGlobalBounds()).has_value()) {
                projectile.active = false;
                enemy.active = false;
                score += Config::enemyPoints;
                break;
            }
        }
    }
}

void removeInactive(std::vector<Projectile>& projectiles, std::vector<Enemy>& enemies) {
    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(),
        [](const Projectile& projectile) { return !projectile.active; }), projectiles.end());
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
        [](const Enemy& enemy) { return !enemy.active; }), enemies.end());
}

std::string buildTitle(State state, int score, std::size_t remainingEnemies) {
    std::string title = "Space Shooter | Score: " + std::to_string(score)
        + " | Enemies: " + std::to_string(remainingEnemies);
    switch (state) {
    case State::Ready: title += " | Enter: Start"; break;
    case State::Playing: title += " | Arrows: Move | Hold Space: Fire | P: Pause"; break;
    case State::Paused: title += " | PAUSED | P: Resume"; break;
    case State::Won: title += " | ENCOUNTER COMPLETE | Enter: Reset"; break;
    case State::GameOver: title += " | GAME OVER | Enter: Reset"; break;
    }
    return title + " | Esc: Exit";
}

int runGame() {
    sf::RenderWindow window(sf::VideoMode({Config::width, Config::height}), "Space Shooter",
        sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    // Shared textures must outlive their sprites.
    sf::Texture playerTexture, enemyTexture;
    const auto playerImage = makeShipImage(sf::Color(70, 190, 240), false);
    const auto enemyImage = makeShipImage(sf::Color(235, 95, 100), true);
    if (!playerTexture.loadFromImage(playerImage) || !enemyTexture.loadFromImage(enemyImage)) {
        std::cerr << "Cannot create ship textures.\n";
        return 1;
    }
    sf::Sprite player(playerTexture);
    player.setOrigin({12.f, 12.f});
    player.setScale({2.f, 2.f});
    std::vector<Projectile> projectiles;
    std::vector<Enemy> enemies;
    int score = 0;
    float firingCooldown = 0.f, accumulator = 0.f;
    State state = State::Ready;
    const auto resetEncounter = [&]() {
        player.setPosition({400.f, 540.f});
        projectiles.clear();
        createEncounter(enemies, enemyTexture);
        score = 0;
        firingCooldown = 0.f;
        accumulator = 0.f;
        state = State::Ready;
    };
    resetEncounter();
    sf::RectangleShape overlay({Config::fieldWidth, Config::fieldHeight});
    overlay.setFillColor(sf::Color(0, 0, 0, 100));
    sf::Clock clock;
    std::string previousTitle;

    while (window.isOpen()) {
        float frameTime = std::min(clock.restart().asSeconds(), Config::maximumFrameTime);
        bool stateChanged = false;
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            if (event->is<sf::Event::FocusLost>() && state == State::Playing) {
                state = State::Paused;
                stateChanged = true;
            }
            const auto* key = event->getIf<sf::Event::KeyPressed>();
            if (!key) continue;
            const auto code = key->code;
            if (code == sf::Keyboard::Key::Escape) { window.close(); continue; }
            if (!window.hasFocus()) continue;
            if (code == sf::Keyboard::Key::Enter && (state == State::Won || state == State::GameOver)) {
                resetEncounter(); stateChanged = true;
            } else if (code == sf::Keyboard::Key::Enter && state == State::Ready) {
                state = State::Playing; stateChanged = true;
            } else if (code == sf::Keyboard::Key::P && (state == State::Playing || state == State::Paused)) {
                state = state == State::Playing ? State::Paused : State::Playing;
                stateChanged = true;
            }
        }
        if (!window.isOpen()) break;
        if (stateChanged) {
            accumulator = 0.f;
            frameTime = 0.f;
            static_cast<void>(clock.restart());
        }
        if (state == State::Playing && window.hasFocus()) {
            accumulator += frameTime;
            const auto direction = readMovementDirection();
            const bool firing = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
            while (accumulator >= Config::fixedStep && state == State::Playing) {
                accumulator -= Config::fixedStep;
                movePlayer(player, direction, Config::fixedStep);
                firingCooldown = std::max(0.f, firingCooldown - Config::fixedStep);
                if (firing && firingCooldown <= 0.f) {
                    fireProjectile(player, projectiles);
                    firingCooldown = Config::firingInterval;
                }
                for (auto& enemy : enemies)
                    if (enemy.active) enemy.sprite.move({0.f, Config::enemySpeed * Config::fixedStep});
                updateProjectiles(projectiles, Config::fixedStep);
                checkProjectileHits(projectiles, enemies, score);
                for (const auto& enemy : enemies) {
                    if (!enemy.active) continue;
                    const auto bounds = enemy.sprite.getGlobalBounds();
                    if (bounds.position.y > Config::fieldHeight || bounds.findIntersection(player.getGlobalBounds()).has_value()) {
                        state = State::GameOver;
                        break;
                    }
                }
                removeInactive(projectiles, enemies);
                if (state == State::Playing && enemies.empty()) state = State::Won;
                if (state != State::Playing) accumulator = 0.f;
            }
        } else accumulator = 0.f;
        const auto title = buildTitle(state, score, enemies.size());
        if (title != previousTitle) { window.setTitle(title); previousTitle = title; }
        window.clear(sf::Color(12, 18, 35));
        for (const auto& projectile : projectiles) window.draw(projectile.shape);
        for (const auto& enemy : enemies) window.draw(enemy.sprite);
        window.draw(player);
        if (state != State::Playing) window.draw(overlay);
        window.display();
    }
    return 0;
}

int main() {
    try { return runGame(); }
    catch (const std::exception& error) {
        std::cerr << "Space Shooter could not continue:\n" << error.what() << '\n';
        return 1;
    }
}
