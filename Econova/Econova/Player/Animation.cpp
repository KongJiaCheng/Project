#include "Animation.h"

bool Animation::loadFromFolder(const std::string& folder, int frameCount, float fTime)
{
    sharedFrames = nullptr;
    frames.clear();
    frameTime = fTime;
    frames.reserve(frameCount);

    for (int i = 0; i < frameCount; ++i)
    {
        std::ostringstream ss;
        ss << folder << "/frame_" << std::setfill('0') << std::setw(3) << i << ".png";
        std::string path = resolvePath(ss.str());

        sf::Texture tex;
        if (!tex.loadFromFile(path))
        {
            std::cerr << "Animation: failed to load " << path << '\n';
            return false;
        }
        frames.push_back(std::move(tex));
    }

    current = 0;
    timer = 0.f;
    playing = false;
    return true;
}

void Animation::update(float dt)
{
    size_t count = sharedFrames ? sharedFrames->size() : frames.size();
    if (!playing || count == 0) return;

    timer += dt;
    if (timer >= frameTime)
    {
        timer -= frameTime;
        current = (current + 1) % static_cast<int>(count);
    }
}

const sf::Texture& Animation::getCurrentTexture() const
{
    const std::vector<sf::Texture>& targetFrames = sharedFrames ? *sharedFrames : frames;
    if (targetFrames.empty()) {
        static sf::Texture emptyTex;
        return emptyTex;
    }
    return targetFrames.at(std::max(0, std::min(current, (int)targetFrames.size() - 1)));
}

void Animation::stopAtFirst()
{
    playing = false;
    timer = 0.f;
    current = 0;
}

void AnimSet::load(const std::string& basePath, int frames) {
    south.loadFromFolder(basePath + "/South", frames);
    north.loadFromFolder(basePath + "/North", frames);
    east.loadFromFolder(basePath + "/East", frames);
    west.loadFromFolder(basePath + "/West", frames);
}

void AnimSet::update(float dt) {
    south.update(dt);
    north.update(dt);
    east.update(dt);
    west.update(dt);
}

void AnimSet::stop() {
    south.stopAtFirst();
    north.stopAtFirst();
    east.stopAtFirst();
    west.stopAtFirst();
}

void AnimSet::setPlaying(bool play) {
    south.playing = north.playing = east.playing = west.playing = play;
}

void AnimSet::setDirPlaying(int dir, bool play) {
    if (dir == 0) south.playing = play;
    else if (dir == 1) north.playing = play;
    else if (dir == 2) east.playing = play;
    else if (dir == 3) west.playing = play;
}

const sf::Texture& AnimSet::getTexture(int dir) {
    if (dir == 0) return south.getCurrentTexture();
    else if (dir == 1) return north.getCurrentTexture();
    else if (dir == 2) return east.getCurrentTexture();
    return west.getCurrentTexture();
}

void constructMaleAnimation(
    AnimSet& animWalk,
    AnimSet& animWalkWithSword,
    AnimSet& animWalkWithShield,
    AnimSet& animWalkWithShieldAndSword,
    AnimSet& animAttackWithSword,
    AnimSet& animAttackWithShieldAndSword,
    int frames
) {
    animWalk.load("Sprites/Male/Walk", frames);
    animWalkWithSword.load("Sprites/Male/WalkWithSword", frames);
    animWalkWithShield.load("Sprites/Male/WalkWithShield", frames);
    animWalkWithShieldAndSword.load("Sprites/Male/WalkWithShieldAndSword", frames);
    animAttackWithSword.load("Sprites/Male/AttackWithSword", frames);
    animAttackWithShieldAndSword.load("Sprites/Male/AttackWithSwordAndShield", frames);
}

void constructFemaleAnimation(
    AnimSet& animWalk,
    AnimSet& animWalkWithSword,
    AnimSet& animWalkWithShield,
    AnimSet& animWalkWithShieldAndSword,
    AnimSet& animAttackWithSword,
    AnimSet& animAttackWithShieldAndSword,
    int frames
) {
    animWalk.load("Sprites/Female/Walk", frames);
    animWalkWithSword.load("Sprites/Female/WalkWithSword", frames);
    animWalkWithShield.load("Sprites/Female/WalkWithShield", frames);
    animWalkWithShieldAndSword.load("Sprites/Female/WalkWithShieldAndSword", frames);
    animAttackWithSword.load("Sprites/Female/AttackWithSword", frames);
    animAttackWithShieldAndSword.load("Sprites/Female/AttackWithSwordAndShield", frames);
}
