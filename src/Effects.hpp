#pragma once
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

class Effects
{
public:
    enum class Cue
    {
        Shoot,
        Explosion,
        Damage,
        Pickup
    };

    Effects()
    {
        // Spread stars across the initial screen.
        for (int i = 0; i < 90; ++i)
        {
            stars.push_back({
                {random(0.f, 800.f), random(0.f, 600.f)},
                random(35.f, 150.f)
                });
        }

        shootReady = makeSound(
            shootBuffer, 0.09f, 1200.f, 450.f, false);

        explosionReady = makeSound(
            explosionBuffer, 0.30f, 120.f, 35.f, true);

        damageReady = makeSound(
            damageBuffer, 0.20f, 250.f, 80.f, true);

        pickupReady = makeSound(
            pickupBuffer, 0.25f, 500.f, 1300.f, false);
    }

    Effects(const Effects&) = delete;
    Effects& operator=(const Effects&) = delete;

    void update(float dt)
    {
        for (auto& star : stars)
        {
            star.position.y += star.speed * dt;

            if (star.position.y >= 600.f)
            {
                star.position.y -= 600.f;
                star.position.x = random(0.f, 800.f);
            }
        }

        for (auto& particle : particles)
        {
            particle.position += particle.velocity * dt;
            particle.life -= dt;

            // Gradually slow the particle.
            particle.velocity *= std::max(0.f, 1.f - 2.f * dt);
        }

        particles.erase(
            std::remove_if(
                particles.begin(),
                particles.end(),
                [](const Particle& particle)
                {
                    return particle.life <= 0.f;
                }),
            particles.end());

        voices.erase(
            std::remove_if(
                voices.begin(),
                voices.end(),
                [](const std::unique_ptr<sf::Sound>& voice)
                {
                    return voice->getStatus()
                        == sf::SoundSource::Status::Stopped;
                }),
            voices.end());
    }

    void drawBackground(sf::RenderTarget& target) const
    {
        sf::RectangleShape dot;

        for (const auto& star : stars)
        {
            const float size = star.speed > 100.f ? 2.f : 1.f;
            const auto brightness =
                static_cast<std::uint8_t>(100.f + star.speed);

            dot.setSize({ size, size });
            dot.setPosition(star.position);
            dot.setFillColor(
                sf::Color(brightness, brightness, brightness));

            target.draw(dot);
        }
    }

    void drawParticles(sf::RenderTarget& target) const
    {
        sf::CircleShape dot;

        for (const auto& particle : particles)
        {
            const float fraction = std::clamp(
                particle.life / particle.totalLife, 0.f, 1.f);

            const float radius =
                particle.radius * (0.4f + 0.6f * fraction);

            auto colour = particle.colour;
            colour.a = static_cast<std::uint8_t>(
                255.f * fraction);

            dot.setRadius(radius);
            dot.setOrigin({ radius, radius });
            dot.setPosition(particle.position);
            dot.setFillColor(colour);

            target.draw(dot);
        }
    }

    void shoot()
    {
        play(Cue::Shoot);
    }

    void explode(sf::Vector2f position, bool boss = false)
    {
        burst(
            position,
            boss ? 90 : 30,
            boss ? 330.f : 220.f,
            sf::Color(255, 170, 40));

        play(Cue::Explosion);
    }

    void damage(sf::Vector2f position)
    {
        burst(position, 24, 180.f, sf::Color(255, 70, 70));
        play(Cue::Damage);
    }

    void pickup(sf::Vector2f position)
    {
        burst(position, 20, 140.f, sf::Color(80, 255, 140));
        play(Cue::Pickup);
    }

    void setPaused(bool paused)
    {
        if (paused == audioPaused)
            return;

        audioPaused = paused;

        for (auto& voice : voices)
        {
            if (paused)
            {
                if (voice->getStatus()
                    == sf::SoundSource::Status::Playing)
                {
                    voice->pause();
                }
            }
            else
            {
                if (voice->getStatus()
                    == sf::SoundSource::Status::Paused)
                {
                    voice->play();
                }
            }
        }
    }

    void reset()
    {
        particles.clear();

        for (auto& voice : voices)
            voice->stop();

        voices.clear();
        audioPaused = false;
    }

private:
    static constexpr float Pi = 3.14159265359f;

    struct Star
    {
        sf::Vector2f position;
        float speed;
    };

    struct Particle
    {
        sf::Vector2f position;
        sf::Vector2f velocity;
        float life;
        float totalLife;
        float radius;
        sf::Color colour;
    };

    float random(float minimum, float maximum)
    {
        return std::uniform_real_distribution<float>(
            minimum, maximum)(generator);
    }

    bool makeSound(
        sf::SoundBuffer& buffer,
        float duration,
        float startFrequency,
        float endFrequency,
        bool noisy)
    {
        constexpr unsigned int sampleRate = 44100;

        const auto count = static_cast<std::size_t>(
            duration * static_cast<float>(sampleRate));

        std::vector<std::int16_t> samples(count);
        float phase = 0.f;

        for (std::size_t i = 0; i < count; ++i)
        {
            const float progress =
                static_cast<float>(i)
                / static_cast<float>(count);

            const float frequency =
                startFrequency
                + (endFrequency - startFrequency) * progress;

            phase += 2.f * Pi * frequency
                / static_cast<float>(sampleRate);

            float wave = std::sin(phase);

            if (noisy)
                wave = 0.35f * wave + 0.65f * random(-1.f, 1.f);

            // A short attack prevents an abrupt start.
            const float attack = std::min(1.f, progress / 0.03f);
            const float decay = (1.f - progress) * (1.f - progress);

            const float sample = std::clamp(
                wave * attack * decay * 0.45f, -1.f, 1.f);

            samples[i] = static_cast<std::int16_t>(
                sample * 32767.f);
        }

        return buffer.loadFromSamples(
            samples.data(),
            samples.size(),
            1,
            sampleRate,
            { sf::SoundChannel::Mono });
    }

    void burst(
        sf::Vector2f position,
        int count,
        float maximumSpeed,
        sf::Color colour)
    {
        for (int i = 0; i < count; ++i)
        {
            const float angle = random(0.f, 2.f * Pi);
            const float speed = random(40.f, maximumSpeed);
            const float lifetime = random(0.25f, 0.65f);

            particles.push_back({
                position,
                {std::cos(angle) * speed,
                 std::sin(angle) * speed},
                lifetime,
                lifetime,
                random(2.f, 5.f),
                colour
                });
        }
    }

    void play(Cue cue)
    {
        if (audioPaused)
            return;

        sf::SoundBuffer* buffer = nullptr;
        bool ready = false;
        float volume = 50.f;

        switch (cue)
        {
        case Cue::Shoot:
            buffer = &shootBuffer;
            ready = shootReady;
            volume = 25.f;
            break;

        case Cue::Explosion:
            buffer = &explosionBuffer;
            ready = explosionReady;
            volume = 60.f;
            break;

        case Cue::Damage:
            buffer = &damageBuffer;
            ready = damageReady;
            volume = 55.f;
            break;

        case Cue::Pickup:
            buffer = &pickupBuffer;
            ready = pickupReady;
            volume = 45.f;
            break;
        }

        if (!ready)
            return;

        // Keep the number of simultaneous sounds bounded.
        if (voices.size() >= 24)
        {
            voices.front()->stop();
            voices.erase(voices.begin());
        }

        auto voice = std::make_unique<sf::Sound>(*buffer);
        voice->setVolume(volume);
        voice->play();

        voices.push_back(std::move(voice));
    }

    std::mt19937 generator{ std::random_device{}() };

    std::vector<Star> stars;
    std::vector<Particle> particles;

    // Buffers are declared before voices, so they are
    // destroyed after the sounds that use them.
    sf::SoundBuffer shootBuffer;
    sf::SoundBuffer explosionBuffer;
    sf::SoundBuffer damageBuffer;
    sf::SoundBuffer pickupBuffer;

    std::vector<std::unique_ptr<sf::Sound>> voices;

    bool shootReady = false;
    bool explosionReady = false;
    bool damageReady = false;
    bool pickupReady = false;
    bool audioPaused = false;
};
