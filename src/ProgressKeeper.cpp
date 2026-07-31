#include <Geode/Geode.hpp>
#include <Geode/loader/Dirs.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/ui/Button.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/OverlayManager.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace progress_keeper {

enum class SaveUnit {
    Seconds,
    Minutes,
    Hours
};

class BackupTicker;
class FloatingSaveButton;

BackupTicker* backupTicker = nullptr;
FloatingSaveButton* floatingButton = nullptr;
bool levelIsOpen = false;

std::int64_t currentTime() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::int64_t intervalInSeconds(int value, SaveUnit unit) {
    auto factor = std::int64_t {1};
    if (unit == SaveUnit::Minutes) factor = 60;
    if (unit == SaveUnit::Hours) factor = 3600;
    return static_cast<std::int64_t>(value) * factor;
}

std::string unitName(SaveUnit unit, int value) {
    if (unit == SaveUnit::Seconds) return value == 1 ? "second" : "seconds";
    if (unit == SaveUnit::Minutes) return value == 1 ? "minute" : "minutes";
    return value == 1 ? "hour" : "hours";
}

bool isProgressFile(std::filesystem::path const& path) {
    auto name = path.filename().string();
    return name.starts_with("CCGameManager") || name.starts_with("CCLocalLevels");
}

void removeOldBackups(std::filesystem::path const& root) {
    std::error_code error;
    std::vector<std::filesystem::path> folders;

    for (std::filesystem::directory_iterator it(root, error), end; it != end && !error; it.increment(error)) {
        if (!it->is_directory(error)) continue;
        auto name = it->path().filename().string();
        if (name.starts_with("backup-")) folders.push_back(it->path());
    }

    std::sort(folders.begin(), folders.end(), std::greater<>());
    for (std::size_t index = 20; index < folders.size(); ++index) {
        std::filesystem::remove_all(folders[index], error);
        error.clear();
    }
}

bool createLocalBackup() {
    auto source = dirs::getSaveDir();
    auto root = Mod::get()->getSaveDir() / "backups";
    auto stamp = currentTime();
    std::error_code error;

    std::filesystem::create_directories(root, error);
    if (error) return false;

    auto destination = root / fmt::format("backup-{}", stamp);
    for (int suffix = 1; std::filesystem::exists(destination, error) && suffix < 100; ++suffix) {
        error.clear();
        destination = root / fmt::format("backup-{}-{}", stamp, suffix);
    }

    auto pending = root / fmt::format("pending-{}", stamp);
    std::filesystem::remove_all(pending, error);
    error.clear();
    std::filesystem::create_directories(pending, error);
    if (error) return false;

    int copied = 0;
    for (std::filesystem::directory_iterator it(source, error), end; it != end && !error; it.increment(error)) {
        if (!it->is_regular_file(error) || !isProgressFile(it->path())) continue;
        error.clear();
        std::filesystem::copy_file(
            it->path(),
            pending / it->path().filename(),
            std::filesystem::copy_options::overwrite_existing,
            error
        );
        if (error) break;
        ++copied;
    }

    if (error || copied == 0) {
        std::filesystem::remove_all(pending, error);
        return false;
    }

    std::filesystem::rename(pending, destination, error);
    if (error) {
        std::filesystem::remove_all(pending, error);
        return false;
    }

    removeOldBackups(root);
    return true;
}

class BackupTicker final : public CCNode {
public:
    static BackupTicker* create() {
        auto node = new BackupTicker();
        if (node && node->init()) {
            node->autorelease();
            return node;
        }
        CC_SAFE_DELETE(node);
        return nullptr;
    }

    bool init() override {
        if (!CCNode::init()) return false;

        auto rawUnit = std::clamp(Mod::get()->getSavedValue<int>("schedule-unit", 1), 0, 2);
        m_unit = static_cast<SaveUnit>(rawUnit);
        m_value = std::max(1, Mod::get()->getSavedValue<int>("schedule-value", 5));
        m_enabled = Mod::get()->getSavedValue<bool>("schedule-enabled", false);
        m_lastBackup = Mod::get()->getSavedValue<std::int64_t>("last-backup", currentTime());
        scheduleUpdate();
        return true;
    }

    void update(float delta) override {
        m_checkDelay += delta;
        if (m_checkDelay < .5f) return;
        m_checkDelay = 0.f;

        if (!m_enabled || m_saving || levelIsOpen) return;
        auto now = currentTime();
        if (m_lastBackup > now) {
            m_lastBackup = now;
            saveState();
            return;
        }

        if (now - m_lastBackup >= intervalInSeconds(m_value, m_unit)) {
            startBackup(true);
        }
    }

    bool startBackup(bool automatic) {
        if (m_saving || levelIsOpen) return false;
        auto app = AppDelegate::get();
        if (!app) return false;

        m_saving = true;
        m_automatic = automatic;
        app->trySaveGame(true);

        auto wait = CCDelayTime::create(.35f);
        auto finish = CCCallFunc::create(this, callfunc_selector(BackupTicker::finishBackup));
        runAction(CCSequence::create(wait, finish, nullptr));

        if (!automatic) {
            Notification::create("Saving progress...", NotificationIcon::Loading, 1.f)->show();
        }
        return true;
    }

    void setSchedule(int value, SaveUnit unit) {
        m_value = value;
        m_unit = unit;
        m_enabled = true;
        m_lastBackup = currentTime();
        saveState();
    }

    void stopSchedule() {
        m_enabled = false;
        saveState();
    }

    bool isEnabled() const {
        return m_enabled;
    }

    int value() const {
        return m_value;
    }

    SaveUnit unit() const {
        return m_unit;
    }

private:
    bool m_enabled = false;
    bool m_saving = false;
    bool m_automatic = false;
    int m_value = 5;
    SaveUnit m_unit = SaveUnit::Minutes;
    std::int64_t m_lastBackup = 0;
    float m_checkDelay = 0.f;

    void finishBackup() {
        auto success = createLocalBackup();
        m_lastBackup = currentTime();
        saveState();
        m_saving = false;

        if (success) {
            Notification::create(
                m_automatic ? "Automatic backup saved" : "Backup saved",
                NotificationIcon::Success,
                1.6f
            )->show();
        }
        else {
            Notification::create("Backup could not be created", NotificationIcon::Error, 2.5f)->show();
        }
    }

    void saveState() {
        Mod::get()->setSavedValue("schedule-enabled", m_enabled);
        Mod::get()->setSavedValue("schedule-value", m_value);
        Mod::get()->setSavedValue("schedule-unit", static_cast<int>(m_unit));
        Mod::get()->setSavedValue("last-backup", m_lastBackup);
        (void) Mod::get()->saveData();
    }
};

void setFloatingVisible(bool visible);

class SavePopup final : public Popup {
public:
    static SavePopup* create() {
        auto popup = new SavePopup();
        if (popup && popup->init()) {
            popup->autorelease();
            return popup;
        }
        CC_SAFE_DELETE(popup);
        return nullptr;
    }

protected:
    TextInput* m_input = nullptr;
    CCLabelBMFont* m_status = nullptr;
    ButtonSprite* m_scheduleSprite = nullptr;
    CCMenuItemSpriteExtra* m_scheduleButton = nullptr;
    CCMenuItemSpriteExtra* m_stopButton = nullptr;
    std::array<CCMenuItemToggler*, 3> m_units {};
    SaveUnit m_unit = SaveUnit::Minutes;

    bool init() override {
        if (!backupTicker || !Popup::init(360.f, 250.f)) return false;

        setID("save-popup"_spr);
        setTitle("Progress Keeper", "goldFont.fnt", .75f, 20.f);
        m_unit = backupTicker->unit();

        auto saveSprite = ButtonSprite::create(
            "Save Now", 125, true, "goldFont.fnt", "GJ_button_01.png", 0.f, .75f
        );
        auto saveButton = CCMenuItemSpriteExtra::create(
            saveSprite, this, menu_selector(SavePopup::onSaveNow)
        );
        saveButton->setPosition({180.f, 174.f});
        saveButton->setID("save-now-button"_spr);
        m_buttonMenu->addChild(saveButton);

        auto heading = CCLabelBMFont::create("Schedule saving", "bigFont.fnt");
        heading->setScale(.52f);
        heading->setPosition({180.f, 139.f});
        heading->setID("schedule-heading"_spr);
        m_mainLayer->addChild(heading);

        m_input = TextInput::create(105.f, "Interval");
        m_input->setCommonFilter(CommonFilter::Uint);
        m_input->setMaxCharCount(6);
        m_input->setString(fmt::format("{}", backupTicker->value()));
        m_input->setPosition({93.f, 104.f});
        m_input->setID("interval-input"_spr);
        m_input->setCallback([this](std::string const&) {
            refreshStatus();
        });
        m_mainLayer->addChild(m_input);

        std::array<char const*, 3> names {"S", "M", "H"};
        std::array<float, 3> positions {175.f, 230.f, 285.f};
        for (int index = 0; index < 3; ++index) {
            auto off = ButtonSprite::create(
                names[index], 42, true, "goldFont.fnt", "GJ_button_04.png", 0.f, .72f
            );
            auto on = ButtonSprite::create(
                names[index], 42, true, "goldFont.fnt", "GJ_button_01.png", 0.f, .72f
            );
            auto toggle = CCMenuItemToggler::create(
                off, on, this, menu_selector(SavePopup::onUnit)
            );
            toggle->setPosition({positions[index], 104.f});
            toggle->setTag(index);
            toggle->setID(fmt::format("unit-{}", names[index]));
            m_buttonMenu->addChild(toggle);
            m_units[index] = toggle;
        }

        m_status = CCLabelBMFont::create("", "chatFont.fnt");
        m_status->setScale(.58f);
        m_status->setPosition({180.f, 75.f});
        m_status->setID("schedule-status"_spr);
        m_mainLayer->addChild(m_status);

        m_scheduleSprite = ButtonSprite::create(
            "Schedule", 105, true, "goldFont.fnt", "GJ_button_01.png", 0.f, .68f
        );
        m_scheduleButton = CCMenuItemSpriteExtra::create(
            m_scheduleSprite, this, menu_selector(SavePopup::onSchedule)
        );
        m_scheduleButton->setPosition({180.f, 39.f});
        m_scheduleButton->setID("schedule-button"_spr);
        m_buttonMenu->addChild(m_scheduleButton);

        auto stopSprite = ButtonSprite::create(
            "Stop", 78, true, "goldFont.fnt", "GJ_button_06.png", 0.f, .68f
        );
        m_stopButton = CCMenuItemSpriteExtra::create(
            stopSprite, this, menu_selector(SavePopup::onStop)
        );
        m_stopButton->setPosition({245.f, 39.f});
        m_stopButton->setID("stop-button"_spr);
        m_buttonMenu->addChild(m_stopButton);

        refreshUnits();
        refreshStatus();
        return true;
    }

    void onClose(CCObject* sender) override {
        Popup::onClose(sender);
        setFloatingVisible(!levelIsOpen);
    }

    void onSaveNow(CCObject*) {
        if (!backupTicker->startBackup(false)) {
            showError("A save is already running");
        }
    }

    void onUnit(CCObject* sender) {
        auto selected = static_cast<CCMenuItemToggler*>(sender);
        m_unit = static_cast<SaveUnit>(std::clamp(selected->getTag(), 0, 2));
        refreshUnits();
        refreshStatus();
    }

    void onSchedule(CCObject*) {
        auto parsed = numFromString<int>(m_input->getString());
        if (parsed.isErr() || parsed.unwrap() < 1) {
            showError("Enter a positive interval");
            return;
        }

        auto value = parsed.unwrap();
        if (intervalInSeconds(value, m_unit) < 10) {
            showError("The minimum interval is 10 seconds");
            return;
        }

        backupTicker->setSchedule(value, m_unit);
        refreshStatus();
        Notification::create("Schedule saved", NotificationIcon::Success, 1.5f)->show();
    }

    void onStop(CCObject*) {
        backupTicker->stopSchedule();
        refreshStatus();
        Notification::create("Scheduled saving stopped", NotificationIcon::Info, 1.5f)->show();
    }

    void refreshUnits() {
        for (int index = 0; index < 3; ++index) {
            m_units[index]->toggle(index == static_cast<int>(m_unit));
        }
    }

    void refreshStatus() {
        if (!m_status) return;

        if (backupTicker->isEnabled()) {
            auto value = backupTicker->value();
            m_status->setString(fmt::format(
                "Active: every {} {}", value, unitName(backupTicker->unit(), value)
            ).c_str());
            m_status->setColor({155, 255, 155});
            m_scheduleSprite->setString("Update");
            m_scheduleButton->setPositionX(125.f);
            m_stopButton->setVisible(true);
            m_stopButton->setEnabled(true);
        }
        else {
            m_status->setString("No schedule active");
            m_status->setColor({220, 220, 220});
            m_scheduleSprite->setString("Schedule");
            m_scheduleButton->setPositionX(180.f);
            m_stopButton->setVisible(false);
            m_stopButton->setEnabled(false);
        }
    }

    void showError(char const* message) {
        m_status->setString(message);
        m_status->setColor({255, 120, 120});
    }
};

class FloatingSaveButton final : public Button {
public:
    static FloatingSaveButton* create() {
        auto button = new FloatingSaveButton();
        if (button && button->init()) {
            button->autorelease();
            return button;
        }
        CC_SAFE_DELETE(button);
        return nullptr;
    }

    void placeFromSavedPosition() {
        auto size = CCDirector::get()->getWinSize();
        auto x = Mod::get()->getSavedValue<float>("button-x", .91f) * size.width;
        auto y = Mod::get()->getSavedValue<float>("button-y", .76f) * size.height;
        setPosition(clampedPosition({x, y}));
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent* event) override {
        if (!Button::ccTouchBegan(touch, event) || !getParent()) return false;
        m_touchStart = getParent()->convertToNodeSpace(touch->getLocation());
        m_buttonStart = getPosition();
        m_dragging = false;
        return true;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent* event) override {
        if (!getParent()) return;
        auto current = getParent()->convertToNodeSpace(touch->getLocation());
        auto movement = current - m_touchStart;
        if (movement.x * movement.x + movement.y * movement.y > 25.f) m_dragging = true;

        if (m_dragging) {
            setPosition(clampedPosition(m_buttonStart + movement));
        }
        else {
            Button::ccTouchMoved(touch, event);
        }
    }

    void ccTouchEnded(CCTouch* touch, CCEvent* event) override {
        if (!m_dragging) {
            Button::ccTouchEnded(touch, event);
            return;
        }

        savePosition();
        Button::ccTouchCancelled(touch, event);
        m_dragging = false;
    }

    void ccTouchCancelled(CCTouch* touch, CCEvent* event) override {
        Button::ccTouchCancelled(touch, event);
        m_dragging = false;
    }

private:
    CCPoint m_touchStart;
    CCPoint m_buttonStart;
    bool m_dragging = false;

    bool init() override {
        auto label = CCLabelBMFont::create("S", "bigFont.fnt");
        label->setScale(.68f);
        auto circle = CircleButtonSprite::create(
            label, CircleBaseColor::DarkAqua, CircleBaseSize::Medium
        );
        if (!circle || !Button::initWithNode(circle, [](Button*) {
            setFloatingVisible(false);
            if (auto popup = SavePopup::create()) {
                popup->show();
            }
            else {
                setFloatingVisible(true);
            }
        })) {
            return false;
        }

        setID("floating-save-button"_spr);
        setTouchPriority(cocos2d::kCCMenuHandlerPriority - 20);
        setTouchMultiplier(1.15f);
        setScaleMultiplier(1.1f);
        return true;
    }

    CCPoint clampedPosition(CCPoint position) {
        auto size = CCDirector::get()->getWinSize();
        auto scaled = getScaledContentSize();
        auto halfWidth = scaled.width * .5f;
        auto halfHeight = scaled.height * .5f;
        position.x = std::clamp(position.x, halfWidth + 5.f, size.width - halfWidth - 5.f);
        position.y = std::clamp(position.y, halfHeight + 5.f, size.height - halfHeight - 5.f);
        return position;
    }

    void savePosition() {
        auto size = CCDirector::get()->getWinSize();
        if (size.width <= 0.f || size.height <= 0.f) return;
        Mod::get()->setSavedValue("button-x", getPositionX() / size.width);
        Mod::get()->setSavedValue("button-y", getPositionY() / size.height);
        (void) Mod::get()->saveData();
    }
};

void setFloatingVisible(bool visible) {
    if (!floatingButton) return;
    floatingButton->setVisible(visible);
    floatingButton->setEnabled(visible);
}

void ensureOverlay() {
    auto overlay = OverlayManager::get();
    if (!backupTicker) {
        backupTicker = BackupTicker::create();
        if (backupTicker) overlay->addChild(backupTicker, -10);
    }

    if (!floatingButton) {
        floatingButton = FloatingSaveButton::create();
        if (floatingButton) {
            overlay->addChild(floatingButton, 100);
            floatingButton->placeFromSavedPosition();
        }
    }

    setFloatingVisible(!levelIsOpen);
}

struct $modify(ProgressKeeperMenu, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        ensureOverlay();
        return true;
    }
};

struct $modify(ProgressKeeperPlay, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        levelIsOpen = true;
        setFloatingVisible(false);
        return true;
    }

    void onExit() {
        PlayLayer::onExit();
        levelIsOpen = false;
        setFloatingVisible(true);
    }
};

}
