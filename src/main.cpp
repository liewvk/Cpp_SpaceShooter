#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace Config
{
    constexpr unsigned int width = 800;
    constexpr unsigned int height = 600;

    constexpr float fieldWidth = 800.f;
    constexpr float fieldHeight = 600.f;

    constexpr float playerSpeed = 360.f;
    constexpr float playerHalfSize = 24.f;
    constexpr float playerMinimumY = 330.f;

    constexpr float firingInterval = 0.18f;
    constexpr float spawnInterval = 0.65f;
    constexpr float waveDelay = 2.f;
    constexpr float protectionTime = 1.f;

    constexpr float fixedStep = 1.f / 120.f;
    constexpr float maximumFrameTime = 0.1f;
}

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
};

struct Pickup
{
    sf::RectangleShape shape;
    bool active = true;
};

struct Enemy
{
    sf::Sprite sprite;
    EnemyKind kind;

    float anchorX = 400.f;
    float age = 0.f;
    float fireTimer = 1.4f;
    float descentSpeed = 65.f;

    int health = 1;
    bool active = true;

    Enemy(
        const sf::Texture& texture,
        EnemyKind type,
        sf::Vector2f position,
        int startingHealth,
        float speed
    )
        : sprite(texture),
        kind(type),
        anchorX(position.x),
        descentSpeed(speed),
        health(startingHealth)
    {
        sprite.setOrigin({ 12.f, 12.f });

        const float scale =
            kind == EnemyKind::Boss ? 4.f : 2.f;

        sprite.setScale({ scale, scale });
        sprite.setPosition(position);
    }
};

struct Game
{
    sf::Sprite player;

    std::vector<Projectile> projectiles;
    std::vector<Enemy> enemies;
    std::vector<Pickup> pickups;

    State state = State::Ready;

    int score = 0;
    int hull = 3;
    int weapon = 0;
    int destroyedRegularEnemies = 0;

    int wave = 1;
    int spawned = 0;

    float fireCooldown = 0.f;
    float protection = 0.f;
    float spawnTimer = 0.f;
    float intermissionTimer = 0.f;

    bool betweenWaves = false;

    explicit Game(const sf::Texture& texture)
        : player(texture)
    {
        player.setOrigin({ 12.f, 12.f });
        player.setScale({ 2.f, 2.f });
    }
};

sf::Image makeShipImage(sf::Color color, bool downward)
{
    sf::Image image({ 24u, 24u }, sf::Color::Transparent);

    for (unsigned int y = 2; y <= 21; ++y)
    {
        const int halfWidth =
            1 + (static_cast<int>(y) - 2) / 2;

        for (unsigned int x = 2; x <= 21; ++x)
        {
            if (
                std::abs(static_cast<int>(x) - 12)
                <= halfWidth
                )
            {
                image.setPixel({ x, y }, color);
            }
        }
    }

    for (unsigned int y = 10; y <= 15; ++y)
    {
        for (unsigned int x = 11; x <= 13; ++x)
        {
            image.setPixel(
                { x, y }, sf::Color(225, 245, 255)
            );
        }
    }

    if (downward)
    {
        image.flipVertically();
    }

    return image;
}

void resetGame(Game& game)
{
    game.player.setPosition({ 400.f, 540.f });
    game.player.setColor(sf::Color::White);

    game.projectiles.clear();
    game.enemies.clear();
    game.pickups.clear();

    game.score = 0;
    game.hull = 3;
    game.weapon = 0;
    game.destroyedRegularEnemies = 0;

    game.wave = 1;
    game.spawned = 0;

    game.fireCooldown = 0.f;
    game.protection = 0.f;
    game.spawnTimer = 0.f;
    game.intermissionTimer = 0.f;
    game.betweenWaves = false;

    game.state = State::Ready;
}

sf::Vector2f readDirection()
{
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
        + direction.y * direction.y
    );

    if (length > 0.f)
    {
        direction.x /= length;
        direction.y /= length;
    }

    return direction;
}

void addProjectile(
    Game& game,
    sf::Vector2f position,
    sf::Vector2f velocity,
    bool hostile
)
{
    Projectile projectile;

    projectile.shape.setSize({ 6.f, 18.f });
    projectile.shape.setOrigin({ 3.f, 9.f });

    projectile.shape.setFillColor(
        hostile
        ? sf::Color(255, 100, 110)
        : sf::Color(255, 230, 90)
    );

    projectile.shape.setPosition(position);
    projectile.velocity = velocity;
    projectile.hostile = hostile;

    game.projectiles.push_back(projectile);
}

void firePlayer(Game& game)
{
    const auto ship = game.player.getPosition();
    const float y = ship.y - 33.f;

    if (game.weapon == 0)
    {
        addProjectile(
            game, { ship.x, y }, { 0.f, -650.f }, false
        );
    }
    else if (game.weapon == 1)
    {
        addProjectile(
            game, { ship.x - 9.f, y }, { 0.f, -650.f }, false
        );

        addProjectile(
            game, { ship.x + 9.f, y }, { 0.f, -650.f }, false
        );
    }
    else
    {
        addProjectile(
            game, { ship.x, y }, { -180.f, -620.f }, false
        );

        addProjectile(
            game, { ship.x, y }, { 0.f, -650.f }, false
        );

        addProjectile(
            game, { ship.x, y }, { 180.f, -620.f }, false
        );
    }
}

void damagePlayer(Game& game)
{
    if (game.protection > 0.f || game.hull <= 0)
    {
        return;
    }

    --game.hull;
    game.weapon = std::max(0, game.weapon - 1);
    game.protection = Config::protectionTime;

    if (game.hull <= 0)
    {
        game.state = State::GameOver;
    }
}

void spawnPickup(Game& game, sf::Vector2f position)
{
    Pickup pickup;

    pickup.shape.setSize({ 20.f, 20.f });
    pickup.shape.setOrigin({ 10.f, 10.f });
    pickup.shape.setFillColor(sf::Color(100, 235, 140));
    pickup.shape.setPosition(position);

    game.pickups.push_back(pickup);
}

void updateSpawning(
    Game& game,
    const sf::Texture& enemyTexture,
    float deltaTime
)
{
    if (game.betweenWaves)
    {
        game.intermissionTimer =
            std::max(0.f, game.intermissionTimer - deltaTime);

        if (game.intermissionTimer > 0.f)
        {
            return;
        }

        ++game.wave;
        game.spawned = 0;
        game.spawnTimer = 0.f;
        game.betweenWaves = false;

        game.projectiles.clear();
    }

    const int planned = game.wave == 4 ? 1 : 6;

    if (game.spawned >= planned)
    {
        return;
    }

    game.spawnTimer -= deltaTime;

    if (game.spawnTimer > 0.f)
    {
        return;
    }

    if (game.wave == 4)
    {
        game.enemies.emplace_back(
            enemyTexture,
            EnemyKind::Boss,
            sf::Vector2f{ 400.f, 80.f },
            12,
            0.f
        );
    }
    else
    {
        EnemyKind kind = EnemyKind::Straight;

        if (game.spawned % 3 == 1)
            kind = EnemyKind::Sine;
        else if (game.spawned % 3 == 2)
            kind = EnemyKind::Chase;

        const float x =
            140.f + (game.spawned % 3) * 260.f;

        game.enemies.emplace_back(
            enemyTexture,
            kind,
            sf::Vector2f{ x, -30.f },
            game.wave == 1 ? 1 : 2,
            55.f + game.wave * 10.f
        );
    }

    ++game.spawned;
    game.spawnTimer = Config::spawnInterval;
}

void updateEnemies(Game& game, float deltaTime)
{
    for (auto& enemy : game.enemies)
    {
        if (!enemy.active)
        {
            continue;
        }

        enemy.age += deltaTime;
        enemy.fireTimer -= deltaTime;

        auto position = enemy.sprite.getPosition();

        if (enemy.kind == EnemyKind::Boss)
        {
            position.x =
                400.f + std::sin(enemy.age * 1.5f) * 250.f;

            position.y = 80.f;
        }
        else
        {
            position.y += enemy.descentSpeed * deltaTime;

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

                const float maximumMove = 90.f * deltaTime;

                position.x += std::clamp(
                    difference, -maximumMove, maximumMove
                );
            }

            position.x = std::clamp(
                position.x, 24.f, 776.f
            );
        }

        enemy.sprite.setPosition(position);

        if (
            enemy.fireTimer <= 0.f
            && position.y > 0.f
            && position.y < 300.f
            )
        {
            const float muzzleY =
                position.y
                + (enemy.kind == EnemyKind::Boss ? 57.f : 33.f);

            if (enemy.kind == EnemyKind::Boss)
            {
                addProjectile(
                    game, { position.x, muzzleY },
                    { -120.f, 240.f }, true
                );

                addProjectile(
                    game, { position.x, muzzleY },
                    { 0.f, 260.f }, true
                );

                addProjectile(
                    game, { position.x, muzzleY },
                    { 120.f, 240.f }, true
                );

                enemy.fireTimer = 0.9f;
            }
            else
            {
                addProjectile(
                    game, { position.x, muzzleY },
                    { 0.f, 240.f }, true
                );

                enemy.fireTimer = 1.8f;
            }
        }
    }
}

void updateProjectiles(Game& game, float deltaTime)
{
    for (auto& projectile : game.projectiles)
    {
        projectile.shape.move({
            projectile.velocity.x * deltaTime,
            projectile.velocity.y * deltaTime
            });

        const auto bounds =
            projectile.shape.getGlobalBounds();

        if (
            bounds.position.y + bounds.size.y < 0.f
            || bounds.position.y > Config::fieldHeight
            || bounds.position.x + bounds.size.x < 0.f
            || bounds.position.x > Config::fieldWidth
            )
        {
            projectile.active = false;
        }
    }
}

void checkHits(Game& game)
{
    // Player shots damage enemies.
    for (auto& projectile : game.projectiles)
    {
        if (!projectile.active || projectile.hostile)
        {
            continue;
        }

        for (auto& enemy : game.enemies)
        {
            if (!enemy.active)
            {
                continue;
            }

            if (
                projectile.shape.getGlobalBounds()
                .findIntersection(
                    enemy.sprite.getGlobalBounds()
                ).has_value()
                )
            {
                projectile.active = false;
                --enemy.health;

                if (enemy.health <= 0)
                {
                    enemy.active = false;

                    game.score +=
                        enemy.kind == EnemyKind::Boss
                        ? 1000 : 100;

                    if (enemy.kind != EnemyKind::Boss)
                    {
                        ++game.destroyedRegularEnemies;

                        if (
                            game.destroyedRegularEnemies % 3 == 0
                            )
                        {
                            spawnPickup(
                                game,
                                enemy.sprite.getPosition()
                            );
                        }
                    }
                }

                break;
            }
        }
    }

    // Hostile shots are consumed even during protection.
    for (auto& projectile : game.projectiles)
    {
        if (!projectile.active || !projectile.hostile)
        {
            continue;
        }

        if (
            projectile.shape.getGlobalBounds()
            .findIntersection(
                game.player.getGlobalBounds()
            ).has_value()
            )
        {
            projectile.active = false;
            damagePlayer(game);
        }
    }

    // Surviving ships may collide or escape.
    for (auto& enemy : game.enemies)
    {
        if (!enemy.active)
        {
            continue;
        }

        const auto bounds =
            enemy.sprite.getGlobalBounds();

        const bool escaped =
            bounds.position.y > Config::fieldHeight;

        const bool contact =
            bounds.findIntersection(
                game.player.getGlobalBounds()
            ).has_value();

        if (escaped || contact)
        {
            enemy.active = false;
            damagePlayer(game);
        }
    }
}

void updatePickups(Game& game, float deltaTime)
{
    for (auto& pickup : game.pickups)
    {
        pickup.shape.move({ 0.f, 140.f * deltaTime });

        if (
            pickup.shape.getGlobalBounds()
            .findIntersection(
                game.player.getGlobalBounds()
            ).has_value()
            )
        {
            game.weapon = std::min(2, game.weapon + 1);
            pickup.active = false;
        }
        else if (
            pickup.shape.getGlobalBounds().position.y
            > Config::fieldHeight
            )
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
            [](const Projectile& item)
            {
                return !item.active;
            }
        ),
        game.projectiles.end()
    );

    game.enemies.erase(
        std::remove_if(
            game.enemies.begin(),
            game.enemies.end(),
            [](const Enemy& item)
            {
                return !item.active;
            }
        ),
        game.enemies.end()
    );

    game.pickups.erase(
        std::remove_if(
            game.pickups.begin(),
            game.pickups.end(),
            [](const Pickup& item)
            {
                return !item.active;
            }
        ),
        game.pickups.end()
    );
}

void updateGame(
    Game& game,
    const sf::Texture& enemyTexture,
    sf::Vector2f direction,
    bool firing,
    float deltaTime
)
{
    game.protection =
        std::max(0.f, game.protection - deltaTime);

    game.fireCooldown =
        std::max(0.f, game.fireCooldown - deltaTime);

    auto position = game.player.getPosition();

    position.x +=
        direction.x * Config::playerSpeed * deltaTime;

    position.y +=
        direction.y * Config::playerSpeed * deltaTime;

    position.x = std::clamp(
        position.x,
        Config::playerHalfSize,
        Config::fieldWidth - Config::playerHalfSize
    );

    position.y = std::clamp(
        position.y,
        Config::playerMinimumY,
        Config::fieldHeight - Config::playerHalfSize
    );

    game.player.setPosition(position);

    if (firing && game.fireCooldown <= 0.f)
    {
        firePlayer(game);
        game.fireCooldown = Config::firingInterval;
    }

    updateSpawning(game, enemyTexture, deltaTime);
    updateEnemies(game, deltaTime);
    updateProjectiles(game, deltaTime);
    checkHits(game);

    if (game.state == State::Playing)
    {
        updatePickups(game, deltaTime);
    }

    removeInactive(game);

    const int planned = game.wave == 4 ? 1 : 6;

    if (
        game.state == State::Playing
        && !game.betweenWaves
        && game.spawned == planned
        && game.enemies.empty()
        )
    {
        game.projectiles.clear();

        if (game.wave == 4)
        {
            game.state = State::Won;
        }
        else
        {
            game.betweenWaves = true;
            game.intermissionTimer = Config::waveDelay;
        }
    }
}

std::string buildTitle(const Game& game)
{
    std::string title =
        "Space Shooter | Score: " + std::to_string(game.score)
        + " | Hull: " + std::to_string(game.hull)
        + " | Weapon: " + std::to_string(game.weapon + 1);

    title += game.wave == 4
        ? " | BOSS"
        : " | Wave: " + std::to_string(game.wave);

    for (const auto& enemy : game.enemies)
    {
        if (enemy.kind == EnemyKind::Boss && enemy.active)
        {
            title += " | Boss HP: "
                + std::to_string(enemy.health);
        }
    }

    switch (game.state)
    {
    case State::Ready:
        title += " | Enter: Start";
        break;

    case State::Playing:
        title += game.betweenWaves
            ? " | Next wave approaching"
            : " | Arrows: Move | Space: Fire | P: Pause";
        break;

    case State::Paused:
        title += " | PAUSED | P: Resume";
        break;

    case State::Won:
        title += " | VICTORY | Enter: Reset";
        break;

    case State::GameOver:
        title += " | GAME OVER | Enter: Reset";
        break;
    }

    return title + " | Esc: Exit";
}

int runGame()
{
    sf::RenderWindow window(
        sf::VideoMode({ Config::width, Config::height }),
        "Space Shooter",
        sf::Style::Titlebar | sf::Style::Close
    );

    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    sf::Texture playerTexture;
    sf::Texture enemyTexture;

    if (
        !playerTexture.loadFromImage(
            makeShipImage(sf::Color(70, 190, 240), false)
        )
        || !enemyTexture.loadFromImage(
            makeShipImage(sf::Color(235, 95, 100), true)
        )
        )
    {
        std::cerr << "Cannot create ship textures.\n";
        return 1;
    }

    Game game(playerTexture);
    resetGame(game);

    sf::RectangleShape overlay(
        { Config::fieldWidth, Config::fieldHeight }
    );
    overlay.setFillColor(sf::Color(0, 0, 0, 110));

    float accumulator = 0.f;
    sf::Clock clock;
    std::string previousTitle;

    while (window.isOpen())
    {
        float frameTime = std::min(
            clock.restart().asSeconds(),
            Config::maximumFrameTime
        );

        bool stateChanged = false;

        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (
                event->is<sf::Event::FocusLost>()
                && game.state == State::Playing
                )
            {
                game.state = State::Paused;
                stateChanged = true;
            }

            const auto* key =
                event->getIf<sf::Event::KeyPressed>();

            if (!key)
            {
                continue;
            }

            const auto code = key->code;

            if (code == sf::Keyboard::Key::Escape)
            {
                window.close();
                continue;
            }

            if (!window.hasFocus())
            {
                continue;
            }

            if (
                code == sf::Keyboard::Key::Enter
                && (game.state == State::Won
                    || game.state == State::GameOver)
                )
            {
                resetGame(game);
                stateChanged = true;
            }
            else if (
                code == sf::Keyboard::Key::Enter
                && game.state == State::Ready
                )
            {
                game.state = State::Playing;
                stateChanged = true;
            }
            else if (
                code == sf::Keyboard::Key::P
                && (game.state == State::Playing
                    || game.state == State::Paused)
                )
            {
                game.state =
                    game.state == State::Playing
                    ? State::Paused
                    : State::Playing;

                stateChanged = true;
            }
        }

        if (!window.isOpen())
        {
            break;
        }

        if (stateChanged)
        {
            accumulator = 0.f;
            frameTime = 0.f;
            static_cast<void>(clock.restart());
        }

        if (
            game.state == State::Playing
            && window.hasFocus()
            )
        {
            accumulator += frameTime;

            const auto direction = readDirection();

            const bool firing =
                sf::Keyboard::isKeyPressed(
                    sf::Keyboard::Key::Space
                );

            while (
                accumulator >= Config::fixedStep
                && game.state == State::Playing
                )
            {
                accumulator -= Config::fixedStep;

                updateGame(
                    game,
                    enemyTexture,
                    direction,
                    firing,
                    Config::fixedStep
                );
            }

            if (game.state != State::Playing)
            {
                accumulator = 0.f;
            }
        }
        else
        {
            accumulator = 0.f;
        }

        const bool flash =
            game.protection > 0.f
            && static_cast<int>(game.protection * 12.f) % 2 == 0;

        game.player.setColor(
            flash
            ? sf::Color(255, 255, 255, 90)
            : sf::Color::White
        );

        const auto title = buildTitle(game);

        if (title != previousTitle)
        {
            window.setTitle(title);
            previousTitle = title;
        }

        window.clear(sf::Color(12, 18, 35));

        for (const auto& projectile : game.projectiles)
            window.draw(projectile.shape);

        for (const auto& enemy : game.enemies)
            window.draw(enemy.sprite);

        for (const auto& pickup : game.pickups)
            window.draw(pickup.shape);

        window.draw(game.player);

        if (game.state != State::Playing)
            window.draw(overlay);

        window.display();
    }

    return 0;
}

int main()
{
    try
    {
        return runGame();
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "Space Shooter could not continue:\n"
            << error.what() << '\n';

        return 1;
    }
}
