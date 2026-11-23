#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <random>
#include <algorithm>
#include "KeyMap.hpp"
#include "InputEngine.hpp"

enum class CatCoatTheme : int {
    ClassicBlack = 0,
    Siamese = 1,
    Shorthair = 2,
    OrangeWhite = 3,
    SapphireTwinTail = 4
};

enum class KeyboardTheme : int {
    ClassicMilk = 0,
    MidnightHacker = 1,
    RetroCream = 2
};

enum class EyeColorTheme : int {
    NeonChartreuse = 0,
    SiameseBlue = 1,
    WarmAmber = 2,
    IceCyan = 3,
    SoftAmber = 4,
    ReferenceHazel = 5
};

struct CatSettings {
    bool showKeyBadges = true;
    bool footEasterEgg = true;
    bool petInteraction = true;
    bool notificationReactions = false;
    bool fullClickThrough = false;
    bool alwaysOnTop = true;
    float scale = 1.0f;
    CatCoatTheme coatTheme = CatCoatTheme::SapphireTwinTail;
    KeyboardTheme keyboardTheme = KeyboardTheme::ClassicMilk;
    EyeColorTheme eyeTheme = EyeColorTheme::SiameseBlue;
    int posX = -99999;
    int posY = -99999;

    void loadFromRegistry() {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\KuroCat", 0, KEY_READ, &hKey) != ERROR_SUCCESS) return;
        auto readDword = [&](const wchar_t* name, DWORD defVal) -> DWORD {
            DWORD val = defVal, sz = sizeof(val), type = 0;
            if (RegQueryValueExW(hKey, name, nullptr, &type, (LPBYTE)&val, &sz) == ERROR_SUCCESS
                && type == REG_DWORD && sz == sizeof(val)) return val;
            return defVal;
        };
        showKeyBadges    = readDword(L"ShowKeyBadges", 1) != 0;
        footEasterEgg    = readDword(L"FootEasterEgg", 1) != 0;
        petInteraction   = readDword(L"PetInteraction", 1) != 0;
        notificationReactions = readDword(L"NotificationReactions", 0) == 1;
        fullClickThrough = readDword(L"FullClickThrough", 0) != 0;
        alwaysOnTop      = readDword(L"AlwaysOnTop", 1) != 0;
        scale            = std::clamp((float)readDword(L"ScalePct", 100) / 100.0f, 0.5f, 2.0f);
        // The custom edition keeps its appearance independent of the upstream app.
        coatTheme        = (CatCoatTheme)std::clamp((int)readDword(L"TwinTailCoatTheme", (DWORD)CatCoatTheme::SapphireTwinTail), 0, 4);
        keyboardTheme    = (KeyboardTheme)std::clamp((int)readDword(L"KeyboardTheme", 0), 0, 2);
        eyeTheme         = (EyeColorTheme)std::clamp((int)readDword(L"TwinTailEyeTheme", (DWORD)EyeColorTheme::SiameseBlue), 0, 5);
        DWORD hasPos     = readDword(L"HasPos", 0);
        if (hasPos) {
            posX = (int)readDword(L"PosX", 0);
            posY = (int)readDword(L"PosY", 0);
        }
        RegCloseKey(hKey);
    }

    void saveToRegistry() const {
        HKEY hKey = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\KuroCat", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) return;
        auto writeDword = [&](const wchar_t* name, DWORD val) {
            RegSetValueExW(hKey, name, 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        };
        writeDword(L"ShowKeyBadges", showKeyBadges ? 1 : 0);
        writeDword(L"FootEasterEgg", footEasterEgg ? 1 : 0);
        writeDword(L"PetInteraction", petInteraction ? 1 : 0);
        writeDword(L"NotificationReactions", notificationReactions ? 1 : 0);
        writeDword(L"FullClickThrough", fullClickThrough ? 1 : 0);
        writeDword(L"AlwaysOnTop", alwaysOnTop ? 1 : 0);
        writeDword(L"ScalePct", (DWORD)(scale * 100.0f + 0.5f));
        writeDword(L"TwinTailCoatTheme", (DWORD)coatTheme);
        writeDword(L"KeyboardTheme", (DWORD)keyboardTheme);
        writeDword(L"TwinTailEyeTheme", (DWORD)eyeTheme);
        if (posX != -99999 && posY != -99999) {
            writeDword(L"HasPos", 1);
            writeDword(L"PosX", (DWORD)posX);
            writeDword(L"PosY", (DWORD)posY);
        }
        RegCloseKey(hKey);
    }
};

struct Spring1D {
    float value = 0.0f;
    float velocity = 0.0f;
    float target = 0.0f;

    explicit Spring1D(float v = 0.0f) : value(v), velocity(0.0f), target(v) {}

    void step(float dt, float stiffness = 260.0f, float damping = 22.0f) {
        float force = (target - value) * stiffness;
        float damp = velocity * damping;
        velocity += (force - damp) * dt;
        value += velocity * dt;
    }
};

struct Spring2D {
    Spring1D x;
    Spring1D y;

    explicit Spring2D(Pt p = {0.0f, 0.0f}) : x(p.x), y(p.y) {}
    Pt point() const { return { x.value, y.value }; }
    void setTarget(Pt p) { x.target = p.x; y.target = p.y; }
    void step(float dt, float stiffness = 340.0f, float damping = 25.0f) {
        x.step(dt, stiffness, damping);
        y.step(dt, stiffness, damping);
    }
};

struct FloatingKeycap {
    std::wstring label;
    Pt position;
    Pt velocity;
    float age = 0.0f;
    float lifetime = 1.25f;
    float rotation = 0.0f;
};

struct PurrSparkle {
    Pt position;
    Pt velocity;
    float age = 0.0f;
    float lifetime = 0.9f;
    bool isSleepZ = false;
};

struct CatPose {
    static constexpr float CanvasWidth  = 360.0f;
    static constexpr float CanvasHeight = 320.0f;
    static constexpr Pt SeatAnchor      = { 184.0f, 226.0f };
    static constexpr Pt LeftPawRest     = { 164.0f, 235.0f };
    static constexpr Pt RightPawRest    = { 204.0f, 235.0f };
    static constexpr Pt HindFootRest    = { 214.0f, 234.0f };
    static constexpr Pt MouseBaseCenter = { 78.0f, 258.0f };

    Spring2D leftPaw{LeftPawRest};
    Spring2D rightPaw{RightPawRest};
    Spring2D hindFoot{HindFootRest};
    Spring1D hindFootActive{0.0f};

    Spring1D bodyShiftX{0.0f};
    Spring1D bodyTilt{0.0f};
    Spring1D bodySquash{1.0f};
    Spring1D headTilt{0.0f};
    Spring1D headYaw{0.0f};
    Spring1D headPitch{0.0f};
    Spring1D headOffsetY{0.0f};
    Spring1D earFold{0.0f};
    Spring1D earTwitch{0.0f};

    Spring2D eyeGaze{{0.0f, 0.0f}};
    Spring1D pupilScale{1.0f};
    Spring1D huntIntensity{0.0f};

    bool leftPawOnMouse = false;
    bool leftPawTyping = false;
    bool rightPawTyping = false;

    float time = 0.0f;
    float tailPhase = 0.0f;

    float petHeadIntensity = 0.0f;
    float petBodyIntensity = 0.0f;
    float sleepiness = 0.0f;
    float jumpHeight = 0.0f;
    float tailLiftDegrees = 0.0f;
    float alertPupils = 0.0f;
    float sleepZOpacity = 1.0f;

    float effectiveEyeClosure() const {
        float petClose = std::max(petHeadIntensity, petBodyIntensity * 0.62f);
        float sleepClose = sleepiness * 0.92f;
        return std::min(1.0f, std::max(blinkProgress_, std::max(petClose, sleepClose)));
    }

    bool isHappyClosedEyes() const { return petHeadIntensity > 0.45f; }
    bool isBlissfulSquint() const { return petBodyIntensity > 0.35f && !isHappyClosedEyes(); }

protected:
    float blinkProgress_ = 0.0f;
};

enum class StartleMode { None, Idle, Sleep, Typing, Mouse };

// Only the displayed joints are blended. The simulation, input and key effects
// continue to update even while the paws briefly hover over the keyboard.
class StartleReaction {
public:
    StartleMode mode() const { return mode_; }
    const CatPose& displayPose(const CatPose& live) const {
        return mode_ == StartleMode::None ? live : pose_;
    }

    StartleMode begin(const CatPose& live, const InputEngine& input, double now) {
        if (mode_ != StartleMode::None) return StartleMode::None;
        double lastMouse = std::max(input.lastMouseMoveTime, input.lastMouseClickTime);
        auto recent = [now](double stamp, double window) {
            return stamp > 0.0 && now - stamp < window;
        };
        bool key = !input.activeKeys.empty() || recent(input.lastKeyActivityTime, 0.65);
        bool mouse = buttonHeld(input) || recent(lastMouse, 0.75);
        if (key && (!mouse || input.lastKeyActivityTime >= lastMouse)) mode_ = StartleMode::Typing;
        else if (mouse) mode_ = StartleMode::Mouse;
        else if (live.sleepiness > 0.45f) mode_ = StartleMode::Sleep;
        else mode_ = StartleMode::Idle;

        elapsed_ = 0.f;
        pose_ = live;
        // Look away from the current cursor briefly, whichever side it was on.
        glanceSide_ = live.headYaw.value > 0.15f ? -1.f : 1.f;
        if (mode_ == StartleMode::Typing) {
            leftHover_ = hoverTarget(live.leftPaw, live.leftPawTyping, PhysicalKey::J, -3.f, 29.f);
            rightHover_ = hoverTarget(live.rightPaw, live.rightPawTyping, PhysicalKey::D, 3.f, 26.f);
        }
        return mode_;
    }

    void advance(float dt, const CatPose& live, const InputEngine& input) {
        if (mode_ == StartleMode::None) return;
        elapsed_ += dt;
        // No keyboard history, particle list, or runtime input is copied.
        pose_ = live;
        switch (mode_) {
        case StartleMode::Idle: {
            float perk = pulse(0.f,.15f,.44f,1.21f);
            float dip = pulse(0.f,.06f,.07f,.15f);
            float bounce = pulse(.06f,.22f,.23f,.45f);
            float land = pulse(.31f,.45f,.47f,.69f);
            look(perk,.32f,-.38f,-.06f,-3.f);
            pose_.headOffsetY.value += 3.f*(dip+land);
            pose_.bodySquash.value += -.07f*dip+.035f*bounce-.06f*land;
            pose_.jumpHeight = 13.f*bounce;
            pose_.tailLiftDegrees = 11.f*pulse(.04f,.24f,.35f,.90f);
            pose_.alertPupils = .32f*perk;
            finishAfter(1.21f);
            break;
        }
        case StartleMode::Sleep: {
            float wake = pulse(.095f,.28f,1.f,2.58f);
            float open = pulse(.095f,.22f,1.f,2.47f);
            look(wake,.32f,-.42f,-.055f,-4.f);
            pose_.headOffsetY.value -= 6.f*pulse(.12f,.30f,.31f,.55f);
            toward(pose_.bodySquash.value,1.02f,wake);
            pose_.sleepiness *= 1.f-open;
            pose_.alertPupils = .97f*open;
            pose_.tailLiftDegrees = 4.f*wake;
            pose_.sleepZOpacity = 1.f-pulse(.085f,.28f,2.12f,2.67f);
            finishAfter(2.67f);
            break;
        }
        case StartleMode::Typing: {
            float pause = pulse(0.f,.14f,.74f,1.01f);
            float lift = pulse(.025f,.24f,.64f,1.05f);
            look(lift,.28f,-.52f,.025f,-4.f);
            if (!(live.leftPawOnMouse && buttonHeld(input))) {
                toward(pose_.leftPaw, leftHover_, pause);
            }
            toward(pose_.rightPaw,rightHover_,pause);
            pose_.tailLiftDegrees = 5.f*lift;
            pose_.alertPupils = .41f*lift;
            finishAfter(1.05f);
            break;
        }
        case StartleMode::Mouse: {
            float glance = pulse(.035f,.31f,.75f,1.47f);
            float eyes = pulse(0.f,.16f,.67f,1.17f);
            float tuck = pulse(.045f,.19f,.50f,.97f);
            look(glance,.91f*glanceSide_,-.43f,.10f*glanceSide_,-3.2f);
            pose_.eyeGaze = live.eyeGaze;
            toward(pose_.eyeGaze,{.95f*glanceSide_,-.5f},eyes);
            // The paw on the mouse stays in contact. A paw assigned a real key
            // keeps tracking that key even in a mixed mouse/keyboard gesture.
            if (!live.rightPawTyping) {
                pose_.rightPaw.x.value += 6.f*tuck;
                pose_.rightPaw.y.value -= 20.f*tuck;
            }
            pose_.tailLiftDegrees = 4.5f*glance;
            pose_.alertPupils = .33f*eyes;
            finishAfter(1.47f);
            break;
        }
        case StartleMode::None: break;
        }
    }

private:
    StartleMode mode_ = StartleMode::None;
    CatPose pose_;
    Pt leftHover_{}, rightHover_{};
    float elapsed_ = 0.f;
    float glanceSide_ = 1.f;

    static bool buttonHeld(const InputEngine& input) {
        return input.leftMouseDown || input.rightMouseDown || input.otherMouseDown;
    }
    static Pt hoverTarget(const Spring2D& paw, bool typing, PhysicalKey fallback, float dx, float lift) {
        Pt key = typing ? Pt{paw.x.target,paw.y.target} : KeyboardLayout::canvasPointForKey(fallback);
        return {key.x+dx, key.y-lift};
    }
    static void toward(float& value, float target, float weight) { value += (target-value)*weight; }
    static void toward(Spring2D& joint, Pt target, float weight) {
        toward(joint.x.value,target.x,weight);
        toward(joint.y.value,target.y,weight);
    }
    void look(float weight, float yaw, float pitch, float tilt, float height) {
        toward(pose_.headYaw.value,yaw,weight);
        toward(pose_.headPitch.value,pitch,weight);
        toward(pose_.headTilt.value,tilt,weight);
        toward(pose_.headOffsetY.value,height,weight);
        toward(pose_.earFold.value,-.26f,weight);
        toward(pose_.eyeGaze,{yaw,pitch},weight);
    }
    float ramp(float start, float end) const {
        float v = std::clamp((elapsed_-start)/(end-start),0.f,1.f);
        return v*v*(3.f-2.f*v);
    }
    float pulse(float start, float peak, float release, float end) const {
        return ramp(start,peak)*(1.f-ramp(release,end));
    }
    void finishAfter(float duration) {
        if (elapsed_ >= duration) mode_ = StartleMode::None;
    }
};

class CatPhysics : public CatPose {
public:
    Spring2D mouseOffset{{0.0f, 0.0f}};
    Spring1D mousePressLeft{0.0f};
    Spring1D mousePressRight{0.0f};
    std::unordered_map<PhysicalKey, float> keyGlow;
    std::vector<FloatingKeycap> floatingKeycaps;
    std::vector<PurrSparkle> sparkles;
    int totalPetSessions = 0;

    StartleMode startle(const InputEngine& input) {
        return reaction_.begin(*this, input, nowSeconds());
    }
    StartleMode startleMode() const { return reaction_.mode(); }
    const CatPose& displayPose() const { return reaction_.displayPose(*this); }

    void triggerKeyDown(PhysicalKey key, const CatSettings& settings) {
        double now = nowSeconds();
        PhysicalKey vk = visualKeyOf(key);
        keyGlow[vk] = 1.0f;

        Pt pt = KeyboardLayout::canvasPointForKey(vk);
        hasLastTypedPoint_ = true;
        lastTypedPoint_ = pt;

        if (prefersViewerLeftPaw(key)) {
            recentLeftKey_ = vk;
            recentLeftTime_ = now;
            leftPaw.y.velocity -= 180.0f;
        } else {
            recentRightKey_ = vk;
            recentRightTime_ = now;
            rightPaw.y.velocity -= 180.0f;
        }

        bodySquash.velocity -= 1.3f;
        earTwitch.velocity += (prefersViewerLeftPaw(key) ? -3.8f : 3.8f);
        headTilt.velocity += (prefersViewerLeftPaw(key) ? -1.4f : 1.4f);
        pupilScale.target = 1.16f;
        sleepiness = 0.0f;

        if (settings.showKeyBadges) {
            spawnFloatingKeycap(keyBadgeLabel(key), pt, now);
        }
    }

    void triggerMouseClick(int button, bool isDown) {
        sleepiness = 0.0f;
        if (isDown) {
            if (button == 0) {
                mousePressLeft.target = 1.0f;
                leftPaw.y.velocity += 130.0f;
            } else {
                mousePressRight.target = 1.0f;
                leftPaw.y.velocity += 110.0f;
            }
            pupilScale.target = 1.12f;
            earTwitch.velocity -= 2.2f;
        } else {
            if (button == 0) mousePressLeft.target = 0.0f;
            else mousePressRight.target = 0.0f;
        }
    }

    void registerPetStroke(Pt canvasPoint, float delta, bool isHead, const CatSettings& settings) {
        if (!settings.petInteraction) return;
        double now = nowSeconds();
        lastPetTime_ = now;
        sleepiness = 0.0f;

        float energy = std::min(delta / 26.0f, 0.35f);
        if (isHead) {
            petHeadIntensity = std::min(1.0f, petHeadIntensity + energy + 0.08f);
            petBodyIntensity = std::max(petBodyIntensity, petHeadIntensity * 0.6f);
        } else {
            petBodyIntensity = std::min(1.0f, petBodyIntensity + energy + 0.07f);
        }

        petStrokeAccum_ += delta;
        if (!isCurrentlyPetting_ && petStrokeAccum_ > 45.0f) {
            isCurrentlyPetting_ = true;
            totalPetSessions++;
        }

        float dx = (canvasPoint.x - SeatAnchor.x) / 60.0f;
        headTilt.target = std::clamp(dx * 0.16f, -0.18f, 0.18f);
        headOffsetY.velocity += 18.0f;
    }

    void step(float rawDt, const RECT& windowRect, InputEngine& engine, const CatSettings& settings) {
        float dt = std::min(rawDt, 1.0f / 25.0f);
        time += dt;
        engine.pollMouse(dt);

        double now = nowSeconds();
        const auto& activeKeys = engine.activeKeys;
        const auto& orderedKeys = engine.pressedOrder;

        // 1. Petting decay & sparkles
        if (now - lastPetTime_ > 0.28) {
            petHeadIntensity = std::max(0.0f, petHeadIntensity - dt * 1.4f);
            petBodyIntensity = std::max(0.0f, petBodyIntensity - dt * 1.2f);
            if (petHeadIntensity < 0.05f && petBodyIntensity < 0.05f) {
                isCurrentlyPetting_ = false;
                petStrokeAccum_ = 0.0f;
            }
        } else if (petHeadIntensity > 0.55f || petBodyIntensity > 0.55f) {
            petSparkleCooldown_ -= dt;
            if (petSparkleCooldown_ <= 0.0f) {
                petSparkleCooldown_ = 0.42f;
                sparkles.push_back({
                    { SeatAnchor.x + randRange(-44.0f, 44.0f), 88.0f + randRange(-14.0f, 18.0f) },
                    { randRange(-10.0f, 10.0f), -24.0f },
                    0.0f, 0.9f, false
                });
            }
        }

        // 2. Sleepy idle detection
        double lastAny = std::max(engine.lastKeyActivityTime, std::max(engine.lastMouseMoveTime, lastPetTime_));
        double idleSec = now - lastAny;
        if (activeKeys.empty() && idleSec > 45.0) {
            float targetSleep = std::min(1.0f, (float)((idleSec - 45.0) / 35.0));
            sleepiness += (targetSleep - sleepiness) * dt * 1.5f;
            if (sleepiness > 0.8f) {
                sleepZCooldown_ -= dt;
                if (sleepZCooldown_ <= 0.0f) {
                    sleepZCooldown_ = 2.6f;
                    sparkles.push_back({ { SeatAnchor.x + 42.0f, 100.0f }, { 12.0f, -18.0f }, 0.0f, 1.1f, true });
                }
            }
        } else {
            sleepiness = std::max(0.0f, sleepiness - dt * 5.0f);
        }

        // 2b. Fast Mouse Movement -> Hunting Mode (big round boba pupils + low crouch!)
        float mouseSpeed = std::hypot(engine.mouseVelocity.x, engine.mouseVelocity.y);
        if (mouseSpeed > 460.0f && petHeadIntensity < 0.25f && petBodyIntensity < 0.25f) {
            float boost = std::min(1.0f, (mouseSpeed - 460.0f) / 1100.0f);
            huntIntensity.target = std::min(1.0f, huntIntensity.target + (0.45f + boost * 1.15f) * dt);
            sleepiness = 0.0f;
        } else {
            huntIntensity.target = std::max(0.0f, huntIntensity.target - dt * 0.65f);
        }
        huntIntensity.step(dt, 85.0f, 16.0f);
        float hunt = std::clamp(huntIntensity.value, 0.0f, 1.0f);

        // 3. Limb assignment (Left Paw, Right Paw, 3-Key Hind Foot!)
        constexpr double tapHoldWindow = 0.14;
        bool mouseRecent = (now - engine.lastMouseMoveTime < 2.2) || engine.leftMouseDown || engine.rightMouseDown;
        bool mouseActivelyUsed = (now - engine.lastMouseMoveTime < 0.75) || engine.leftMouseDown || engine.rightMouseDown;

        std::vector<PhysicalKey> leftCands, rightCands;
        for (PhysicalKey k : orderedKeys) {
            if (prefersViewerLeftPaw(k)) leftCands.push_back(k);
            else rightCands.push_back(k);
        }

        PhysicalKey leftKey = leftCands.empty() ? PhysicalKey::None : leftCands.back();
        PhysicalKey rightKey = rightCands.empty() ? PhysicalKey::None : rightCands.back();
        PhysicalKey footKey = PhysicalKey::None;

        if (leftKey == PhysicalKey::None && rightCands.size() >= 2 && !mouseActivelyUsed) {
            auto sorted = rightCands;
            std::sort(sorted.begin(), sorted.end(), [](PhysicalKey a, PhysicalKey b) {
                return KeyboardLayout::canvasPointForKey(a).x < KeyboardLayout::canvasPointForKey(b).x;
            });
            leftKey = sorted.front();
            rightKey = sorted.back();
        } else if (rightKey == PhysicalKey::None && leftCands.size() >= 2) {
            auto sorted = leftCands;
            std::sort(sorted.begin(), sorted.end(), [](PhysicalKey a, PhysicalKey b) {
                return KeyboardLayout::canvasPointForKey(a).x < KeyboardLayout::canvasPointForKey(b).x;
            });
            leftKey = sorted.front();
            rightKey = sorted.back();
        }

        if (settings.footEasterEgg) {
            if (orderedKeys.size() >= 3) {
                for (PhysicalKey k : orderedKeys) {
                    if (k != leftKey && k != rightKey) { footKey = k; break; }
                }
            } else if (mouseActivelyUsed && leftKey == PhysicalKey::None && rightCands.size() >= 2) {
                for (PhysicalKey k : rightCands) {
                    if (k != rightKey) { footKey = k; break; }
                }
            }
        }

        if (leftKey == PhysicalKey::None && recentLeftKey_ != PhysicalKey::None && now - recentLeftTime_ < tapHoldWindow) {
            leftKey = recentLeftKey_;
        }
        if (rightKey == PhysicalKey::None && recentRightKey_ != PhysicalKey::None && now - recentRightTime_ < tapHoldWindow) {
            rightKey = recentRightKey_;
        }

        // Subtle mini-mouse movement
        mouseOffset.setTarget({
            std::clamp(engine.mouseVelocity.x * 0.0045f, -5.5f, 5.5f),
            std::clamp(engine.mouseVelocity.y * 0.0045f, -4.5f, 4.5f)
        });
        mouseOffset.step(dt, 120.0f, 16.0f);
        mousePressLeft.step(dt, 400.0f, 24.0f);
        mousePressRight.step(dt, 400.0f, 24.0f);

        Pt liveMouse = { MouseBaseCenter.x + mouseOffset.point().x, MouseBaseCenter.y + mouseOffset.point().y };

        if (leftKey != PhysicalKey::None) {
            leftPaw.setTarget(KeyboardLayout::canvasPointForKey(leftKey));
            leftPawTyping = true;
            leftPawOnMouse = false;
        } else if (mouseRecent && sleepiness < 0.3f) {
            leftPaw.setTarget({ liveMouse.x + 2.0f, liveMouse.y - 3.0f });
            leftPawTyping = false;
            leftPawOnMouse = true;
        } else {
            leftPaw.setTarget(LeftPawRest);
            leftPawTyping = false;
            leftPawOnMouse = false;
        }

        if (rightKey != PhysicalKey::None) {
            rightPaw.setTarget(KeyboardLayout::canvasPointForKey(rightKey));
            rightPawTyping = true;
        } else {
            rightPaw.setTarget(RightPawRest);
            rightPawTyping = false;
        }

        if (footKey != PhysicalKey::None) {
            hindFoot.setTarget(KeyboardLayout::canvasPointForKey(footKey));
            hindFootActive.target = 1.0f;
        } else {
            hindFoot.setTarget(HindFootRest);
            hindFootActive.target = 0.0f;
        }

        leftPaw.step(dt, 360.0f, 24.0f);
        rightPaw.step(dt, 360.0f, 24.0f);
        hindFoot.step(dt, 320.0f, 22.0f);
        hindFootActive.step(dt, 260.0f, 22.0f);

        // 4. Body lean, squash & ear motion
        float targetShiftX = 0.0f, targetBodyTilt = 0.0f;
        if (leftPawOnMouse && !rightPawTyping) {
            targetShiftX = -10.0f;
            targetBodyTilt = -0.075f;
        } else if (leftPawOnMouse && rightPawTyping) {
            targetShiftX = -3.0f;
            targetBodyTilt = (rightPaw.point().x - 196.0f) * 0.0008f;
        } else if (leftPawTyping || rightPawTyping) {
            float avgX = ((leftPawTyping ? leftPaw.point().x : 184.0f) + (rightPawTyping ? rightPaw.point().x : 184.0f)) * 0.5f;
            targetShiftX = (avgX - 184.0f) * 0.14f;
            targetBodyTilt = (avgX - 184.0f) * 0.0011f;
        }
        bodyShiftX.target = std::clamp(targetShiftX, -16.0f, 16.0f);
        bodyTilt.target = std::clamp(targetBodyTilt, -0.12f, 0.12f);
        bodySquash.target = 1.0f - hunt * 0.11f;
        headOffsetY.target = (leftPawTyping || rightPawTyping) ? 3.5f : (sleepiness * 5.0f + hunt * 10.5f);
        earFold.target = std::max(petHeadIntensity * 0.7f, petBodyIntensity * 0.45f) + sleepiness * 0.25f - hunt * 0.18f;
        earTwitch.target = 0.0f;

        bodyShiftX.step(dt, 140.0f, 18.0f);
        bodyTilt.step(dt, 140.0f, 18.0f);
        bodySquash.step(dt, 280.0f, 18.0f);
        headOffsetY.step(dt, 180.0f, 19.0f);
        earFold.step(dt, 120.0f, 16.0f);
        earTwitch.step(dt, 320.0f, 16.0f);

        // 5. 2.5D Head Turning (Yaw + Pitch) & Eye Tracking (Windows screen Y is top-down!)
        bool typingRecently = !activeKeys.empty() || (now - engine.lastKeyActivityTime < 0.65);
        bool mouseMovedRecently = (now - engine.lastMouseMoveTime < 1.2);
        Pt eyeOrigin = { SeatAnchor.x + bodyShiftX.value, 124.0f + headOffsetY.value };

        float winW = std::max(1.0f, (float)(windowRect.right - windowRect.left));
        float winScale = std::max(0.2f, winW / CanvasWidth);
        float headScreenX = (float)windowRect.left + eyeOrigin.x * winScale;
        float headScreenY = (float)windowRect.top  + eyeOrigin.y * winScale;

        float curDx = ((float)engine.lastCursorPos.x - headScreenX) / 290.0f;
        float curDy = ((float)engine.lastCursorPos.y - headScreenY) / 260.0f;
        float curMag = std::hypot(curDx, curDy);
        float clampedCur = std::min(1.0f, curMag);
        float cursorNX = curMag > 0.001f ? (curDx / curMag) * clampedCur : 0.0f;
        float cursorNY = curMag > 0.001f ? (curDy / curMag) * clampedCur : 0.0f;

        float lookX = cursorNX, lookY = cursorNY;
        if (typingRecently && !mouseMovedRecently && hasLastTypedPoint_) {
            lookX = std::clamp((lastTypedPoint_.x - eyeOrigin.x) / 95.0f, -0.85f, 0.85f);
            lookY = std::clamp((lastTypedPoint_.y - eyeOrigin.y) / 125.0f, -0.3f, 0.92f);
        } else if (typingRecently && mouseMovedRecently && hasLastTypedPoint_) {
            float kdx = std::clamp((lastTypedPoint_.x - eyeOrigin.x) / 95.0f, -0.85f, 0.85f);
            lookX = cursorNX * 0.72f + kdx * 0.28f;
            lookY = cursorNY * 0.72f + 0.25f;
        }

        if (sleepiness > 0.45f) {
            headYaw.target = 0.0f;
            headPitch.target = 0.45f * sleepiness;
        } else {
            headYaw.target = std::clamp(lookX, -1.0f, 1.0f);
            headPitch.target = std::clamp(lookY, -1.0f, 1.0f);
        }
        float headStiff = 88.0f + hunt * 75.0f;
        float headDamp  = 14.5f + hunt * 4.5f;
        headYaw.step(dt, headStiff, headDamp);
        headPitch.step(dt, headStiff, headDamp);

        if (now - lastPetTime_ > 0.28) {
            headTilt.target = bodyTilt.target * 0.45f + headYaw.value * (0.11f + hunt * 0.05f) - headYaw.value * headPitch.value * 0.04f;
        }
        headTilt.step(dt, 130.0f, 16.0f);

        eyeGaze.setTarget({ lookX * 0.88f, lookY * 0.85f });
        eyeGaze.step(dt, 210.0f + hunt * 80.0f, 21.0f);

        if (hindFootActive.value > 0.3f) {
            pupilScale.target = 1.24f;
        } else if (now - engine.lastKeyActivityTime > 0.45 && now - engine.lastMouseClickTime > 0.45) {
            pupilScale.target = 1.0f;
        }
        pupilScale.step(dt, 150.0f, 18.0f);

        // 6. Blinking (no blinking during locked-on hunting!)
        if (isBlinking_) {
            blinkTimer_ += dt;
            constexpr float blinkDur = 0.14f;
            if (blinkTimer_ >= blinkDur) {
                isBlinking_ = false;
                blinkProgress_ = 0.0f;
                nextBlinkTime_ = time + randRange(2.6f, 5.6f);
            } else {
                blinkProgress_ = std::sin((blinkTimer_ / blinkDur) * 3.14159265f);
            }
        } else if (time >= nextBlinkTime_ && sleepiness < 0.5f && petHeadIntensity < 0.3f && hunt < 0.35f) {
            isBlinking_ = true;
            blinkTimer_ = 0.0f;
        }

        // 7. Tail wag speed
        float kpmBoost = std::min(2.4f, (float)engine.currentKPM() / 160.0f);
        float petBoost = std::max(petHeadIntensity, petBodyIntensity) * 1.4f;
        float tailSpeed = 1.85f + kpmBoost + petBoost + hunt * 2.8f - sleepiness * 1.1f;
        tailPhase += dt * tailSpeed;

        // 8. Keycap glow decay
        std::unordered_set<PhysicalKey> visualActive;
        for (PhysicalKey k : activeKeys) visualActive.insert(visualKeyOf(k));
        for (auto it = keyGlow.begin(); it != keyGlow.end(); ) {
            if (visualActive.count(it->first)) {
                it->second = 1.0f;
                ++it;
            } else {
                it->second -= dt * 4.5f;
                if (it->second <= 0.01f) it = keyGlow.erase(it);
                else ++it;
            }
        }
        for (PhysicalKey vk : visualActive) keyGlow[vk] = 1.0f;

        // 9. Floating keycaps
        for (int i = (int)floatingKeycaps.size() - 1; i >= 0; --i) {
            auto& b = floatingKeycaps[i];
            b.age += dt;
            b.position.x += b.velocity.x * dt;
            b.position.y += b.velocity.y * dt;
            b.velocity.x *= (1.0f - dt * 2.4f);
            b.velocity.y *= (1.0f - dt * 1.6f);
            if (b.age >= b.lifetime) floatingKeycaps.erase(floatingKeycaps.begin() + i);
        }

        // 10. Sparkles / Sleep Z
        for (int i = (int)sparkles.size() - 1; i >= 0; --i) {
            auto& sp = sparkles[i];
            sp.age += dt;
            sp.position.x += sp.velocity.x * dt;
            sp.position.y += sp.velocity.y * dt;
            if (sp.age >= sp.lifetime) sparkles.erase(sparkles.begin() + i);
        }
        reaction_.advance(dt, *this, engine);
    }

private:
    StartleReaction reaction_;
    float nextBlinkTime_ = 3.2f;
    bool isBlinking_ = false;
    float blinkTimer_ = 0.0f;

    float petStrokeAccum_ = 0.0f;
    double lastPetTime_ = 0.0;
    bool isCurrentlyPetting_ = false;
    float petSparkleCooldown_ = 0.0f;
    float sleepZCooldown_ = 0.0f;

    PhysicalKey recentLeftKey_ = PhysicalKey::None;
    double recentLeftTime_ = 0.0;
    PhysicalKey recentRightKey_ = PhysicalKey::None;
    double recentRightTime_ = 0.0;
    bool hasLastTypedPoint_ = false;
    Pt lastTypedPoint_ = { 184.0f, 260.0f };

    int badgeCascade_ = 0;
    double lastBadgeTime_ = 0.0;

    static float randRange(float a, float b) {
        static thread_local std::mt19937 rng{ std::random_device{}() };
        std::uniform_real_distribution<float> dist(a, b);
        return dist(rng);
    }

    void spawnFloatingKeycap(const std::wstring& label, Pt pt, double now) {
        if (label.empty()) return;
        if (now - lastBadgeTime_ < 0.38) badgeCascade_ = (badgeCascade_ + 1) % 6;
        else badgeCascade_ = 0;
        lastBadgeTime_ = now;

        static const float spread[6] = { -10.0f, 14.0f, -22.0f, 26.0f, 2.0f, 36.0f };
        float dx = spread[badgeCascade_] + randRange(-3.0f, 3.0f);
        float startX = std::clamp(pt.x + dx * 0.45f, 55.0f, 315.0f);
        float startY = pt.y - 26.0f - (float)(badgeCascade_ % 2) * 8.0f;

        floatingKeycaps.push_back({
            label,
            { startX, startY },
            { dx * 0.65f, -36.0f - randRange(0.0f, 14.0f) },
            0.0f, 1.25f, randRange(-0.07f, 0.07f)
        });
        if (floatingKeycaps.size() > 16) {
            floatingKeycaps.erase(floatingKeycaps.begin(), floatingKeycaps.begin() + (floatingKeycaps.size() - 16));
        }
    }
};
