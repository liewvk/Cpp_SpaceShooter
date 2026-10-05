#include <SFML/Graphics.hpp>

#include "Effects.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    constexpr float Width = 800.f;
    constexpr float Height = 600.f;
    constexpr float FixedStep = 1.f / 120.f;

    enum class State
    {
        Ready,
        Playing,
        Paused,
        Won,
        GameOver
    };

    enum class EnemyKind
    {
        Straight,
        Sine,
        Chase,
        Boss
    };

    struct Projectile
    {
        sf::RectangleShape shape;
        sf::Vector2f velocity;
        bool hostile = false;
        bool active = true;

        Projectile(sf::Vector2f position,
            sf::Vector2f speed,
            bool enemyShot)
            : velocity(speed), hostile(enemyShot)
        {
            shape.setSize({ 6.f, 18.f });
            shape.setOrigin({ 3.f, 9.f });
            shape.setPosition(position);
            shape.setFillColor(
                hostile ? sf::Color(255, 90, 90)
                : sf::Color(100, 240, 255));
        }
    };

    struct Pickup
    {
        sf::RectangleShape shape;
        bool active = true;

        explicit Pickup(sf::Vector2f position)
        {
            shape.setSize({ 20.f, 20.f });
            shape.setOrigin({ 10.f, 10.f });
            shape.setPosition(position);
            shape.setFillColor(sf::Color(80, 255, 140));
        }
    };

    struct Enemy
    {
        sf::Sprite sprite;
        EnemyKind kind;
        float anchorX;
        float age = 0.f;
        float fireTimer = 1.4f;
        float descentSpeed;
        int hp;
        bool active = true;

        Enemy(const sf::Texture& texture,
            EnemyKind enemyKind,
            sf::Vector2f position,
            int health,
            float speed)
            : sprite(texture),
            kind(enemyKind),
            anchorX(position.x),
            descentSpeed(speed),
            hp(health)
        {
            sprite.setOrigin({ 12.f, 12.f });
            sprite.setPosition(position);

            if (kind == EnemyKind::Boss)
            {
                sprite.setScale({ 4.f, 4.f });
                sprite.setColor(sf::Color(255, 120, 220));
            }
            else
            {
                sprite.setScale({ 2.f, 2.f });

                if (kind == EnemyKind::Sine)
                    sprite.setColor(sf::Color(255, 210, 100));
                else if (kind == EnemyKind::Chase)
                    sprite.setColor(sf::Color(150, 120, 255));
            }
        }
    };

    struct Game
    {
        Effects effects;

        sf::Sprite player;
        const sf::Texture& enemyTexture;

        std::vector<Projectile> projectiles;
        std::vector<Enemy> enemies;
        std::vector<Pickup> pickups;

        State state = State::Ready;

        int score = 0;
        int hull = 3;
        int weapon = 0;
        int wave = 1;
        int spawned = 0;
        int destroyedRegular = 0;

        float fireCooldown = 0.f;
        float protection = 0.f;
        float spawnTimer = 0.f;
        float intermissionTimer = 0.f;

        bool betweenWaves = false;

        Game(const sf::Texture& playerTexture,
            const sf::Texture& enemyShipTexture)
            : player(playerTexture),
            enemyTexture(enemyShipTexture)
        {
            player.setOrigin({ 12.f, 12.f });
            player.setScale({ 2.f, 2.f });
            player.setPosition({ 400.f, 540.f });
        }
    };

    sf::Image makeShipImage(sf::Color colour, bool enemy)
    {
        sf::Image image({ 24u, 24u }, sf::Color::Transparent);

        for (unsigned int y = 2; y < 22; ++y)
        {
            const int halfWidth =
                static_cast<int>(y) / 2;

            for (unsigned int x = 0; x < 24; ++x)
            {
                const int distance =
                    std::abs(static_cast<int>(x) - 12);

                if (distance <= halfWidth)
                    image.setPixel({ x, y }, colour);
            }
        }

        for (unsigned int y = 9; y < 17; ++y)
        {
            for (unsigned int x = 10; x < 14; ++x)
                image.setPixel({ x, y }, sf::Color::White);
        }

        if (enemy)
            image.flipVertically();

        return image;
    }

    void resetGame(Game& game)
    {
        game.effects.reset();

        game.projectiles.clear();
        game.enemies.clear();
        game.pickups.clear();

        game.player.setPosition({ 400.f, 540.f });
        game.player.setColor(sf::Color::White);

        game.state = State::Ready;
        game.score = 0;
        game.hull = 3;
        game.weapon = 0;
        game.wave = 1;
        game.spawned = 0;
        game.destroyedRegular = 0;

        game.fireCooldown = 0.f;
        game.protection = 0.f;
        game.spawnTimer = 0.f;
        game.intermissionTimer = 0.f;
        game.betweenWaves = false;
    }

    void firePlayer(Game& game)
    {
        const sf::Vector2f muzzle =
            game.player.getPosition() + sf::Vector2f{ 0.f, -30.f };

        if (game.weapon == 0)
        {
            game.projectiles.emplace_back(
                muzzle, sf::Vector2f{ 0.f, -650.f }, false);
        }
        else if (game.weapon == 1)
        {
            game.projectiles.emplace_back(
                muzzle + sf::Vector2f{ -9.f, 0.f },
                sf::Vector2f{ 0.f, -650.f },
                false);

            game.projectiles.emplace_back(
                muzzle + sf::Vector2f{ 9.f, 0.f },
                sf::Vector2f{ 0.f, -650.f },
                false);
        }
        else
        {
            game.projectiles.emplace_back(
                muzzle, sf::Vector2f{ -180.f, -620.f }, false);

            game.projectiles.emplace_back(
                muzzle, sf::Vector2f{ 0.f, -650.f }, false);

            game.projectiles.emplace_back(
                muzzle, sf::Vector2f{ 180.f, -620.f }, false);
        }

        game.effects.shoot();
    }

    void damagePlayer(Game& game)
    {
        if (game.protection > 0.f || game.hull <= 0)
            return;

        game.effects.damage(game.player.getPosition());

        --game.hull;
        game.weapon = std::max(0, game.weapon - 1);
        game.protection = 1.f;

        if (game.hull <= 0)
            game.state = State::GameOver;
    }

    int plannedEnemies(const Game& game)
    {
        return game.wave == 4 ? 1 : 6;
    }

    void updateSpawning(Game& game, float dt)
    {
        if (game.betweenWaves)
        {
            game.intermissionTimer =
                std::max(0.f, game.intermissionTimer - dt);

            if (game.intermissionTimer > 0.f)
                return;

            ++game.wave;
            game.spawned = 0;
            game.spawnTimer = 0.f;
            game.betweenWaves = false;
            game.projectiles.clear();
        }

        if (game.spawned >= plannedEnemies(game))
            return;

        game.spawnTimer -= dt;

        if (game.spawnTimer > 0.f)
            return;

        if (game.wave == 4)
        {
            game.enemies.emplace_back(
                game.enemyTexture,
                EnemyKind::Boss,
                sf::Vector2f{ 400.f, 80.f },
                12,
                0.f);
        }
        else
        {
            const int pattern = game.spawned % 3;

            EnemyKind kind = EnemyKind::Straight;

            if (pattern == 1)
                kind = EnemyKind::Sine;
            else if (pattern == 2)
                kind = EnemyKind::Chase;

            const float x =
                140.f + static_cast<float>(pattern) * 260.f;

            game.enemies.emplace_back(
                game.enemyTexture,
                kind,
                sf::Vector2f{ x, -30.f },
                game.wave == 1 ? 1 : 2,
                55.f + static_cast<float>(game.wave) * 10.f);
        }

        ++game.spawned;
        game.spawnTimer = 0.65f;
    }

    void updateEnemies(Game& game, float dt)
    {
        for (auto& enemy : game.enemies)
        {
            if (!enemy.active)
                continue;

            enemy.age += dt;
            enemy.fireTimer -= dt;

            auto position = enemy.sprite.getPosition();

            if (enemy.kind == EnemyKind::Boss)
            {
                position.x =
                    400.f + std::sin(enemy.age * 1.5f) * 250.f;
                position.y = 80.f;
            }
            else
            {
                position.y += enemy.descentSpeed * dt;

                if (enemy.kind == EnemyKind::Sine)
                {
                    position.x =
                        enemy.anchorX
                        + std::sin(enemy.age * 2.f) * 70.f;
                }
                else if (enemy.kind == EnemyKind::Chase)
                {
                    const float difference =
                        game.player.getPosition().x - position.x;

                    position.x += std::clamp(
                        difference, -90.f * dt, 90.f * dt);
                }

                position.x =
                    std::clamp(position.x, 24.f, Width - 24.f);
            }

            enemy.sprite.setPosition(position);

            if (position.y <= 0.f ||
                position.y >= 300.f ||
                enemy.fireTimer > 0.f)
            {
                continue;
            }

            if (enemy.kind == EnemyKind::Boss)
            {
                const auto muzzle =
                    position + sf::Vector2f{ 0.f, 57.f };

                game.projectiles.emplace_back(
                    muzzle, sf::Vector2f{ -120.f, 240.f }, true);

                game.projectiles.emplace_back(
                    muzzle, sf::Vector2f{ 0.f, 260.f }, true);

                game.projectiles.emplace_back(
                    muzzle, sf::Vector2f{ 120.f, 240.f }, true);

                enemy.fireTimer = 0.9f;
            }
            else
            {
                game.projectiles.emplace_back(
                    position + sf::Vector2f{ 0.f, 33.f },
                    sf::Vector2f{ 0.f, 240.f },
                    true);

                enemy.fireTimer = 1.8f;
            }
        }
    }

    void updateProjectiles(Game& game, float dt)
    {
        for (auto& projectile : game.projectiles)
        {
            if (!projectile.active)
                continue;

            projectile.shape.move(projectile.velocity * dt);

            const auto bounds = projectile.shape.getGlobalBounds();

            if (bounds.position.y + bounds.size.y < 0.f ||
                bounds.position.y > Height ||
                bounds.position.x + bounds.size.x < 0.f ||
                bounds.position.x > Width)
            {
                projectile.active = false;
            }
        }
    }

    void checkHits(Game& game)
    {
        for (auto& projectile : game.projectiles)
        {
            if (!projectile.active || projectile.hostile)
                continue;

            for (auto& enemy : game.enemies)
            {
                if (!enemy.active)
                    continue;

                if (!projectile.shape.getGlobalBounds()
                    .findIntersection(enemy.sprite.getGlobalBounds()))
                {
                    continue;
                }

                projectile.active = false;
                --enemy.hp;

                if (enemy.hp <= 0)
                {
                    const bool boss =
                        enemy.kind == EnemyKind::Boss;

                    game.effects.explode(
                        enemy.sprite.getPosition(), boss);

                    enemy.active = false;
                    game.score += boss ? 1000 : 100;

                    if (!boss)
                    {
                        ++game.destroyedRegular;

                        if (game.destroyedRegular % 3 == 0)
                        {
                            game.pickups.emplace_back(
                                enemy.sprite.getPosition());
                        }
                    }
                }

                break;
            }
        }

        for (auto& projectile : game.projectiles)
        {
            if (game.state != State::Playing)
                break;

            if (!projectile.active || !projectile.hostile)
                continue;

            if (projectile.shape.getGlobalBounds()
                .findIntersection(game.player.getGlobalBounds()))
            {
                projectile.active = false;
                damagePlayer(game);
            }
        }

        for (auto& enemy : game.enemies)
        {
            if (game.state != State::Playing)
                break;

            if (!enemy.active)
                continue;

            const auto bounds = enemy.sprite.getGlobalBounds();

            const bool touchingPlayer =
                bounds.findIntersection(
                    game.player.getGlobalBounds()).has_value();

            const bool escaped =
                bounds.position.y > Height;

            if (touchingPlayer || escaped)
            {
                enemy.active = false;
                damagePlayer(game);
            }
        }
    }

    void updatePickups(Game& game, float dt)
    {
        for (auto& pickup : game.pickups)
        {
            if (!pickup.active)
                continue;

            pickup.shape.move({ 0.f, 140.f * dt });

            if (pickup.shape.getGlobalBounds()
                .findIntersection(game.player.getGlobalBounds()))
            {
                game.effects.pickup(pickup.shape.getPosition());

                game.weapon = std::min(2, game.weapon + 1);
                pickup.active = false;
            }
            else if (pickup.shape.getGlobalBounds().position.y > Height)
            {
                pickup.active = false;
            }
        }
    }

    void removeInactive(Game& game)
    {
        game.projectiles.erase(
            std::remove_if(
                game.projectiles.begin(),
                game.projectiles.end(),
                [](const Projectile& projectile)
                {
                    return !projectile.active;
                }),
            game.projectiles.end());

        game.enemies.erase(
            std::remove_if(
                game.enemies.begin(),
                game.enemies.end(),
                [](const Enemy& enemy)
                {
                    return !enemy.active;
                }),
            game.enemies.end());

        game.pickups.erase(
            std::remove_if(
                game.pickups.begin(),
                game.pickups.end(),
                [](const Pickup& pickup)
                {
                    return !pickup.active;
                }),
            game.pickups.end());
    }

    void updateGame(Game& game, float dt)
    {
        game.protection =
            std::max(0.f, game.protection - dt);

        game.fireCooldown =
            std::max(0.f, game.fireCooldown - dt);

        sf::Vector2f direction{ 0.f, 0.f };

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
            direction.x -= 1.f;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
            direction.x += 1.f;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
            direction.y -= 1.f;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
            direction.y += 1.f;

        const float length =
            std::sqrt(direction.x * direction.x
                + direction.y * direction.y);

        if (length > 0.f)
            direction /= length;

        game.player.move(direction * 360.f * dt);

        auto position = game.player.getPosition();

        position.x = std::clamp(position.x, 24.f, Width - 24.f);
        position.y = std::clamp(position.y, 330.f, Height - 24.f);

        game.player.setPosition(position);

        updateSpawning(game, dt);

        if (!game.betweenWaves &&
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) &&
            game.fireCooldown <= 0.f)
        {
            firePlayer(game);
            game.fireCooldown = 0.18f;
        }

        updateEnemies(game, dt);
        updateProjectiles(game, dt);
        checkHits(game);

        if (game.state == State::Playing)
            updatePickups(game, dt);

        removeInactive(game);

        if (game.state == State::Playing &&
            !game.betweenWaves &&
            game.spawned == plannedEnemies(game) &&
            game.enemies.empty())
        {
            game.projectiles.clear();

            if (game.wave == 4)
            {
                game.state = State::Won;
            }
            else
            {
                game.betweenWaves = true;
                game.intermissionTimer = 2.f;
            }
        }
    }

    void updateTitle(sf::RenderWindow& window, const Game& game)
    {
        std::string title =
            "SpaceShooter | Score: " + std::to_string(game.score)
            + " | Hull: " + std::to_string(game.hull)
            + " | Wave: " + std::to_string(game.wave) + "/4"
            + " | Weapon: " + std::to_string(game.weapon + 1);

        for (const auto& enemy : game.enemies)
        {
            if (enemy.active && enemy.kind == EnemyKind::Boss)
            {
                title += " | Boss HP: " + std::to_string(enemy.hp);
                break;
            }
        }

        switch (game.state)
        {
        case State::Ready:
            title += " | Press Enter to start";
            break;

        case State::Playing:
            if (game.betweenWaves)
                title += " | Next wave approaching";
            else
                title += " | Arrows: move | Space: fire | P: pause";
            break;

        case State::Paused:
            title += " | PAUSED - Press P to resume";
            break;

        case State::Won:
            title += " | VICTORY! Enter: reset";
            break;

        case State::GameOver:
            title += " | GAME OVER - Enter: reset";
            break;
        }

        window.setTitle(title);
    }

    void drawGame(sf::RenderWindow& window, Game& game)
    {
        window.clear(sf::Color(5, 8, 20));

        game.effects.drawBackground(window);

        for (const auto& projectile : game.projectiles)
        {
            if (projectile.active)
                window.draw(projectile.shape);
        }

        for (const auto& enemy : game.enemies)
        {
            if (enemy.active)
                window.draw(enemy.sprite);
        }

        for (const auto& pickup : game.pickups)
        {
            if (pickup.active)
                window.draw(pickup.shape);
        }

        sf::Color playerColour = sf::Color::White;

        if (game.protection > 0.f &&
            static_cast<int>(game.protection * 12.f) % 2 == 0)
        {
            playerColour.a = 90;
        }

        game.player.setColor(playerColour);
        window.draw(game.player);

        game.effects.drawParticles(window);

        if (game.state != State::Playing)
        {
            sf::RectangleShape overlay({ Width, Height });
            overlay.setFillColor(sf::Color(0, 0, 0, 90));
            window.draw(overlay);
        }

        window.display();
    }
} // namespace

int main()
{
    sf::RenderWindow window(
        sf::VideoMode({ 800u, 600u }),
        "SpaceShooter",
        sf::Style::Titlebar | sf::Style::Close);

    window.setVerticalSyncEnabled(true);
    window.setKeyRepeatEnabled(false);

    sf::Texture playerTexture;
    sf::Texture enemyTexture;

    if (!playerTexture.loadFromImage(
        makeShipImage(sf::Color(80, 210, 255), false)) ||
        !enemyTexture.loadFromImage(
            makeShipImage(sf::Color(255, 110, 90), true)))
    {
        std::cerr << "Unable to create ship textures.\n";
        return 1;
    }

    Game game(playerTexture, enemyTexture);

    sf::Clock clock;
    float accumulator = 0.f;
    bool timingReset = false;

    while (window.isOpen())
    {
        timingReset = false;

        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
                continue;
            }

            if (event->is<sf::Event::FocusLost>() &&
                game.state == State::Playing)
            {
                game.state = State::Paused;
                timingReset = true;
            }

            if (const auto* key =
                event->getIf<sf::Event::KeyPressed>())
            {
                if (key->code == sf::Keyboard::Key::Escape)
                {
                    window.close();
                }
                else if (key->code == sf::Keyboard::Key::Enter)
                {
                    if (game.state == State::Ready)
                    {
                        game.state = State::Playing;
                        timingReset = true;
                    }
                    else if (game.state == State::Won ||
                        game.state == State::GameOver)
                    {
                        resetGame(game);
                        timingReset = true;
                    }
                }
                else if (key->code == sf::Keyboard::Key::P)
                {
                    if (game.state == State::Playing)
                    {
                        game.state = State::Paused;
                        timingReset = true;
                    }
                    else if (game.state == State::Paused &&
                        window.hasFocus())
                    {
                        game.state = State::Playing;
                        timingReset = true;
                    }
                }
            }
        }

        if (!window.isOpen())
            break;

        game.effects.setPaused(game.state == State::Paused);

        float elapsed =
            std::min(clock.restart().asSeconds(), 0.1f);

        if (timingReset)
        {
            elapsed = 0.f;
            accumulator = 0.f;
        }

        if (game.state == State::Paused)
        {
            accumulator = 0.f;
        }
        else
        {
            accumulator += elapsed;

            while (accumulator >= FixedStep)
            {
                if (game.state == State::Playing)
                    updateGame(game, FixedStep);

                // Continue explosions on the result screens.
                game.effects.update(FixedStep);

                accumulator -= FixedStep;
            }
        }

        updateTitle(window, game);
        drawGame(window, game);
    }

    return 0;
}