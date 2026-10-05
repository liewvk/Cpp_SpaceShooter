#include <SFML/Graphics.hpp>

#include "Effects.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    constexpr float Width = 800.f;
    constexpr float Height = 600.f;
    constexpr float FixedStep = 1.f / 120.f;
    constexpr int MaximumScore = 2800;

    enum class State
    {
        Menu,
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

    const std::array<std::string, 3> DifficultyNames{
        "Easy", "Normal", "Hard"
    };

    const std::array<int, 3> StartingHull{ 5, 3, 2 };
    const std::array<float, 3> EnemyPace{ 0.8f, 1.f, 1.25f };

    std::filesystem::path scoreFilePath()
    {
        std::filesystem::path base;

#ifdef _WIN32
        char* folder = nullptr;
        std::size_t length = 0;

        if (_dupenv_s(&folder, &length, "LOCALAPPDATA") == 0
            && folder != nullptr)
        {
            base = folder;
        }

        std::free(folder);
#endif

        if (base.empty())
        {
            std::error_code error;
            base = std::filesystem::current_path(error);

            if (error)
                base = ".";
        }

        return base / "VBTutor" / "SpaceShooter" / "highscores.txt";
    }

    std::array<int, 3> loadScores(
        const std::filesystem::path& path)
    {
        std::array<int, 3> scores{ 0, 0, 0 };
        std::array<int, 3> loaded{};

        std::ifstream input(path);

        if (input >> loaded[0] >> loaded[1] >> loaded[2])
        {
            const bool valid = std::all_of(
                loaded.begin(),
                loaded.end(),
                [](int score)
                {
                    return score >= 0 && score <= MaximumScore;
                });

            if (valid)
                scores = loaded;
        }

        return scores;
    }

    bool saveScores(
        const std::filesystem::path& path,
        const std::array<int, 3>& scores)
    {
        std::error_code error;
        std::filesystem::create_directories(
            path.parent_path(), error);

        if (error)
            return false;

        std::ofstream output(path, std::ios::trunc);

        if (!output)
            return false;

        for (int score : scores)
            output << score << '\n';

        output.flush();
        return static_cast<bool>(output);
    }

    struct Projectile
    {
        sf::RectangleShape shape;
        sf::Vector2f velocity;
        bool hostile;
        bool active = true;

        Projectile(
            sf::Vector2f position,
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

        Enemy(
            const sf::Texture& texture,
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

        State state = State::Menu;
        int difficulty = 1;

        std::filesystem::path scorePath = scoreFilePath();
        std::array<int, 3> bestScores{ 0, 0, 0 };

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
        bool saveFailed = false;

        Game(
            const sf::Texture& playerTexture,
            const sf::Texture& enemyShipTexture)
            : player(playerTexture),
            enemyTexture(enemyShipTexture)
        {
            player.setOrigin({ 12.f, 12.f });
            player.setScale({ 2.f, 2.f });
            player.setPosition({ 400.f, 540.f });

            bestScores = loadScores(scorePath);
        }
    };

    sf::Image makeShipImage(sf::Color colour, bool enemy)
    {
        sf::Image image({ 24u, 24u }, sf::Color::Transparent);

        for (unsigned int y = 2; y < 22; ++y)
        {
            const int halfWidth = static_cast<int>(y) / 2;

            for (unsigned int x = 0; x < 24; ++x)
            {
                if (std::abs(static_cast<int>(x) - 12)
                    <= halfWidth)
                {
                    image.setPixel({ x, y }, colour);
                }
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

    void clearRun(Game& game)
    {
        game.effects.reset();
        game.projectiles.clear();
        game.enemies.clear();
        game.pickups.clear();

        game.player.setPosition({ 400.f, 540.f });
        game.player.setColor(sf::Color::White);

        game.score = 0;
        game.hull = StartingHull[game.difficulty];
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

    void startGame(Game& game)
    {
        clearRun(game);
        game.state = State::Playing;
    }

    void returnToMenu(Game& game)
    {
        clearRun(game);
        game.state = State::Menu;
    }

    void finishGame(Game& game, State result)
    {
        if (game.state != State::Playing)
            return;

        game.state = result;

        int& best = game.bestScores[game.difficulty];

        if (game.score > best)
        {
            best = game.score;
            game.saveFailed =
                !saveScores(game.scorePath, game.bestScores);
        }
    }

    void firePlayer(Game& game)
    {
        const auto muzzle =
            game.player.getPosition() + sf::Vector2f{ 0.f, -30.f };

        if (game.weapon == 0)
        {
            game.projectiles.emplace_back(
                muzzle, sf::Vector2f{ 0.f, -650.f }, false);
        }
        else if (game.weapon == 1)
        {
            for (float offset : {-9.f, 9.f})
            {
                game.projectiles.emplace_back(
                    muzzle + sf::Vector2f{ offset, 0.f },
                    sf::Vector2f{ 0.f, -650.f },
                    false);
            }
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
            finishGame(game, State::GameOver);
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

            const EnemyKind kind =
                pattern == 0 ? EnemyKind::Straight
                : pattern == 1 ? EnemyKind::Sine
                : EnemyKind::Chase;

            game.enemies.emplace_back(
                game.enemyTexture,
                kind,
                sf::Vector2f{
                    140.f + static_cast<float>(pattern) * 260.f,
                    -30.f },
                    game.wave == 1 ? 1 : 2,
                    (55.f + static_cast<float>(game.wave) * 10.f)
                    * EnemyPace[game.difficulty]);
        }

        ++game.spawned;
        game.spawnTimer = 0.65f;
    }

    void updateEnemies(Game& game, float dt)
    {
        const float pace = EnemyPace[game.difficulty];

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
                    400.f + std::sin(enemy.age * 1.5f * pace) * 250.f;
                position.y = 80.f;
            }
            else
            {
                position.y += enemy.descentSpeed * dt;

                if (enemy.kind == EnemyKind::Sine)
                {
                    position.x = enemy.anchorX
                        + std::sin(enemy.age * 2.f * pace) * 70.f;
                }
                else if (enemy.kind == EnemyKind::Chase)
                {
                    const float difference =
                        game.player.getPosition().x - position.x;

                    position.x += std::clamp(
                        difference,
                        -90.f * pace * dt,
                        90.f * pace * dt);
                }

                position.x =
                    std::clamp(position.x, 24.f, Width - 24.f);
            }

            enemy.sprite.setPosition(position);

            if (position.y <= 0.f || position.y >= 300.f
                || enemy.fireTimer > 0.f)
            {
                continue;
            }

            if (enemy.kind == EnemyKind::Boss)
            {
                const auto muzzle =
                    position + sf::Vector2f{ 0.f, 57.f };

                game.projectiles.emplace_back(
                    muzzle,
                    sf::Vector2f{ -120.f, 240.f } * pace,
                    true);

                game.projectiles.emplace_back(
                    muzzle,
                    sf::Vector2f{ 0.f, 260.f } * pace,
                    true);

                game.projectiles.emplace_back(
                    muzzle,
                    sf::Vector2f{ 120.f, 240.f } * pace,
                    true);

                enemy.fireTimer = 0.9f / pace;
            }
            else
            {
                game.projectiles.emplace_back(
                    position + sf::Vector2f{ 0.f, 33.f },
                    sf::Vector2f{ 0.f, 240.f } * pace,
                    true);

                enemy.fireTimer = 1.8f / pace;
            }
        }
    }

    void updateProjectiles(Game& game, float dt)
    {
        for (auto& shot : game.projectiles)
        {
            if (!shot.active)
                continue;

            shot.shape.move(shot.velocity * dt);

            const auto bounds = shot.shape.getGlobalBounds();

            if (bounds.position.y + bounds.size.y < 0.f
                || bounds.position.y > Height
                || bounds.position.x + bounds.size.x < 0.f
                || bounds.position.x > Width)
            {
                shot.active = false;
            }
        }
    }

    void checkHits(Game& game)
    {
        for (auto& shot : game.projectiles)
        {
            if (!shot.active || shot.hostile)
                continue;

            for (auto& enemy : game.enemies)
            {
                if (!enemy.active)
                    continue;

                if (!shot.shape.getGlobalBounds().findIntersection(
                    enemy.sprite.getGlobalBounds()))
                {
                    continue;
                }

                shot.active = false;
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

        for (auto& shot : game.projectiles)
        {
            if (game.state != State::Playing)
                break;

            if (!shot.active || !shot.hostile)
                continue;

            if (shot.shape.getGlobalBounds().findIntersection(
                game.player.getGlobalBounds()))
            {
                shot.active = false;
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

            if (bounds.findIntersection(
                game.player.getGlobalBounds())
                || bounds.position.y > Height)
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

            if (pickup.shape.getGlobalBounds().findIntersection(
                game.player.getGlobalBounds()))
            {
                game.effects.pickup(pickup.shape.getPosition());
                game.weapon = std::min(2, game.weapon + 1);
                pickup.active = false;
            }
            else if (pickup.shape.getGlobalBounds().position.y
                 > Height)
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
                [](const Projectile& shot)
                {
                    return !shot.active;
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

        const float length = std::sqrt(
            direction.x * direction.x
            + direction.y * direction.y);

        if (length > 0.f)
            direction /= length;

        game.player.move(direction * 360.f * dt);

        auto position = game.player.getPosition();
        position.x = std::clamp(position.x, 24.f, Width - 24.f);
        position.y = std::clamp(position.y, 330.f, Height - 24.f);
        game.player.setPosition(position);

        updateSpawning(game, dt);

        if (!game.betweenWaves
            && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space)
            && game.fireCooldown <= 0.f)
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

        if (game.state == State::Playing
            && !game.betweenWaves
            && game.spawned == plannedEnemies(game)
            && game.enemies.empty())
        {
            game.projectiles.clear();

            if (game.wave == 4)
            {
                finishGame(game, State::Won);
            }
            else
            {
                game.betweenWaves = true;
                game.intermissionTimer = 2.f;
            }
        }
    }

    void drawText(
        sf::RenderTarget& target,
        const sf::Font& font,
        const std::string& message,
        unsigned int size,
        float x,
        float y,
        sf::Color colour = sf::Color::White,
        bool centred = false)
    {
        sf::Text text(font);
        text.setString(message);
        text.setCharacterSize(size);
        text.setFillColor(colour);

        if (centred)
        {
            const auto bounds = text.getLocalBounds();
            text.setOrigin({
                bounds.position.x + bounds.size.x / 2.f,
                bounds.position.y + bounds.size.y / 2.f });
        }

        text.setPosition({ x, y });
        target.draw(text);
    }

    void drawPanel(sf::RenderTarget& target)
    {
        sf::RectangleShape panel({ Width, Height });
        panel.setFillColor(sf::Color(0, 0, 0, 185));
        target.draw(panel);
    }

    void drawInterface(
        sf::RenderWindow& window,
        const sf::Font& font,
        const Game& game)
    {
        const auto cyan = sf::Color(100, 230, 255);

        if (game.state == State::Menu)
        {
            drawPanel(window);

            drawText(window, font, "SPACE SHOOTER",
                48, 400.f, 105.f, cyan, true);

            drawText(window, font, "Select difficulty",
                24, 400.f, 185.f, sf::Color::White, true);

            for (int i = 0; i < 3; ++i)
            {
                const bool selected = i == game.difficulty;

                const std::string label =
                    (selected ? "> " : "  ")
                    + std::to_string(i + 1) + ": "
                    + DifficultyNames[i]
                    + "   Best: " + std::to_string(game.bestScores[i]);

                drawText(
                    window, font, label, 24,
                    400.f, 230.f + static_cast<float>(i) * 42.f,
                    selected ? cyan : sf::Color(180, 180, 190),
                    true);
            }

            drawText(window, font, "Enter: Start",
                28, 400.f, 390.f, sf::Color::White, true);

            drawText(window, font, "Arrows: Move    Space: Fire",
                20, 400.f, 445.f, sf::Color::White, true);

            drawText(window, font, "P: Pause    M: Menu    Escape: Exit",
                20, 400.f, 480.f, sf::Color::White, true);
        }
        else
        {
            sf::RectangleShape hud({ Width, 42.f });
            hud.setFillColor(sf::Color(0, 0, 0, 190));
            window.draw(hud);

            drawText(
                window, font,
                "Score: " + std::to_string(game.score),
                19, 12.f, 8.f);

            drawText(
                window, font,
                "Hull: " + std::to_string(game.hull),
                19, 185.f, 8.f);

            drawText(
                window, font,
                "Weapon: " + std::to_string(game.weapon + 1),
                19, 290.f, 8.f);

            drawText(
                window, font,
                "Wave: " + std::to_string(game.wave) + "/4",
                19, 445.f, 8.f);

            drawText(
                window, font,
                "Best: "
                + std::to_string(game.bestScores[game.difficulty]),
                19, 610.f, 8.f);

            for (const auto& enemy : game.enemies)
            {
                if (!enemy.active || enemy.kind != EnemyKind::Boss)
                    continue;

                sf::RectangleShape background({ 300.f, 12.f });
                background.setPosition({ 250.f, 48.f });
                background.setFillColor(sf::Color(60, 30, 50));
                window.draw(background);

                sf::RectangleShape health({
                    300.f * std::clamp(
                        static_cast<float>(enemy.hp) / 12.f,
                        0.f, 1.f),
                    12.f });

                health.setPosition({ 250.f, 48.f });
                health.setFillColor(sf::Color(255, 90, 180));
                window.draw(health);
                break;
            }

            if (game.state == State::Playing && game.betweenWaves)
            {
                drawText(
                    window, font, "NEXT WAVE APPROACHING",
                    28, 400.f, 260.f, cyan, true);
            }

            if (game.state == State::Paused)
            {
                drawPanel(window);

                drawText(window, font, "PAUSED",
                    44, 400.f, 240.f, cyan, true);

                drawText(window, font, "P: Resume    M: Menu",
                    24, 400.f, 310.f, sf::Color::White, true);
            }

            if (game.state == State::Won
                || game.state == State::GameOver)
            {
                drawPanel(window);

                drawText(
                    window, font,
                    game.state == State::Won
                    ? "MISSION COMPLETE"
                    : "GAME OVER",
                    42, 400.f, 210.f, cyan, true);

                drawText(
                    window, font,
                    "Score: " + std::to_string(game.score),
                    28, 400.f, 285.f, sf::Color::White, true);

                drawText(
                    window, font,
                    DifficultyNames[game.difficulty]
                    + " best: "
                    + std::to_string(
                        game.bestScores[game.difficulty]),
                    24, 400.f, 330.f, sf::Color::White, true);

                drawText(
                    window, font, "Enter: Play again    M: Menu",
                    22, 400.f, 405.f, sf::Color::White, true);
            }
        }

        if (game.saveFailed)
        {
            drawText(
                window, font, "High score could not be saved.",
                18, 400.f, 565.f,
                sf::Color(255, 160, 100), true);
        }
    }

    void drawGame(
        sf::RenderWindow& window,
        const sf::Font& font,
        Game& game)
    {
        window.clear(sf::Color(5, 8, 20));
        game.effects.drawBackground(window);

        if (game.state != State::Menu)
        {
            for (const auto& shot : game.projectiles)
            {
                if (shot.active)
                    window.draw(shot.shape);
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

            sf::Color colour = sf::Color::White;

            if (game.protection > 0.f
                && static_cast<int>(game.protection * 12.f) % 2 == 0)
            {
                colour.a = 90;
            }

            game.player.setColor(colour);
            window.draw(game.player);
        }

        game.effects.drawParticles(window);
        drawInterface(window, font, game);
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

    const auto fontPath =
        std::filesystem::path(GAME_ASSET_DIR)
        / "fonts" / "welcome.ttf";

    sf::Font font;

    if (!font.openFromFile(fontPath))
    {
        std::cerr
            << "Unable to load the font.\n"
            << "Expected location: " << fontPath << '\n'
            << "Copy a real font file into this location.\n";
        return 1;
    }

    sf::Texture playerTexture;
    sf::Texture enemyTexture;

    if (!playerTexture.loadFromImage(
        makeShipImage(sf::Color(80, 210, 255), false))
        || !enemyTexture.loadFromImage(
            makeShipImage(sf::Color(255, 110, 90), true)))
    {
        std::cerr << "Unable to create ship textures.\n";
        return 1;
    }

    Game game(playerTexture, enemyTexture);

    sf::Clock clock;
    float accumulator = 0.f;

    while (window.isOpen())
    {
        bool resetTiming = false;

        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
                continue;
            }

            if (event->is<sf::Event::FocusLost>()
                && game.state == State::Playing)
            {
                game.state = State::Paused;
                resetTiming = true;
            }

            const auto* key =
                event->getIf<sf::Event::KeyPressed>();

            if (!key)
                continue;

            if (key->code == sf::Keyboard::Key::Escape)
            {
                window.close();
            }
            else if (key->code == sf::Keyboard::Key::M)
            {
                returnToMenu(game);
                resetTiming = true;
            }
            else if (game.state == State::Menu)
            {
                if (key->code == sf::Keyboard::Key::Num1)
                    game.difficulty = 0;
                else if (key->code == sf::Keyboard::Key::Num2)
                    game.difficulty = 1;
                else if (key->code == sf::Keyboard::Key::Num3)
                    game.difficulty = 2;
                else if (key->code == sf::Keyboard::Key::Enter)
                {
                    startGame(game);
                    resetTiming = true;
                }
            }
            else if (key->code == sf::Keyboard::Key::Enter
                && (game.state == State::Won
                    || game.state == State::GameOver))
            {
                startGame(game);
                resetTiming = true;
            }
            else if (key->code == sf::Keyboard::Key::P)
            {
                if (game.state == State::Playing)
                {
                    game.state = State::Paused;
                    resetTiming = true;
                }
                else if (game.state == State::Paused
                    && window.hasFocus())
                {
                    game.state = State::Playing;
                    resetTiming = true;
                }
            }
        }

        if (!window.isOpen())
            break;

        game.effects.setPaused(game.state == State::Paused);

        float elapsed =
            std::min(clock.restart().asSeconds(), 0.1f);

        if (resetTiming)
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

                game.effects.update(FixedStep);
                accumulator -= FixedStep;
            }
        }

        drawGame(window, font, game);
    }

    return 0;
}
