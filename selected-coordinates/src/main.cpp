#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace {
constexpr float kGridSize = 30.f;
constexpr float kCameraOffsetScale = 3.f; // GD world units / 3 = Camera Offset values

class SelectedCoordinatesHUD final : public CCNode {
    EditorUI* m_editorUI = nullptr;
    CCLabelBMFont* m_worldLabel = nullptr;
    CCLabelBMFont* m_gridLabel = nullptr;
    CCLabelBMFont* m_cellLabel = nullptr;
    CCLabelBMFont* m_cameraLabel = nullptr;
    std::string m_lastWorld;
    std::string m_lastGrid;
    std::string m_lastCell;
    std::string m_lastCamera;
    bool m_hasReference = false;
    CCPoint m_reference = {0.f, 0.f};

    std::vector<GameObject*> selection() const {
        std::vector<GameObject*> objects;
        if (!m_editorUI) return objects;

        if (auto* obj = m_editorUI->m_selectedObject) {
            objects.push_back(obj);
        }
        if (auto* arr = m_editorUI->m_selectedObjects) {
            for (auto* obj : CCArrayExt<GameObject*>(arr)) {
                if (obj && std::find(objects.begin(), objects.end(), obj) == objects.end()) {
                    objects.push_back(obj);
                }
            }
        }
        return objects;
    }

    static CCLabelBMFont* makeLabel(char const* id, float x, float y, float scale) {
        auto* label = CCLabelBMFont::create("", "bigFont.fnt");
        if (!label) return nullptr;
        label->setID(id);
        label->setScale(scale);
        label->setAnchorPoint({.5f, .5f});
        label->setPosition({x, y});
        label->setVisible(false);
        return label;
    }

    static void setLabel(CCLabelBMFont* label, std::string const& text, std::string& old) {
        if (!label) return;
        label->setVisible(!text.empty());
        if (old != text) {
            old = text;
            label->setString(text.c_str());
        }
    }

    void onSetReference(CCObject*) {
        auto const selected = selection();
        if (selected.size() != 1) {
            Notification::create("Select ONE reference object", NotificationIcon::Warning, 2.f)->show();
            return;
        }
        m_reference = selected.front()->getPosition();
        m_hasReference = true;
        Notification::create("Reference saved. Select destination.", NotificationIcon::Success, 2.f)->show();
    }

    void onClearReference(CCObject*) {
        m_hasReference = false;
        Notification::create("Camera reference cleared", NotificationIcon::Info, 1.5f)->show();
    }

public:
    static SelectedCoordinatesHUD* create(EditorUI* ui) {
        auto* hud = new SelectedCoordinatesHUD();
        if (hud && hud->initHUD(ui)) {
            hud->autorelease();
            return hud;
        }
        CC_SAFE_DELETE(hud);
        return nullptr;
    }

    bool initHUD(EditorUI* ui) {
        if (!CCNode::init() || !ui) return false;
        m_editorUI = ui;
        setID("coordinate-hud"_spr);

        auto const screen = CCDirector::sharedDirector()->getWinSize();
        auto const x = screen.width / 2.f;
        m_worldLabel  = makeLabel("gd-coordinates", x, screen.height - 17.f, .36f);
        m_gridLabel   = makeLabel("grid-coordinates", x, screen.height - 29.f, .30f);
        m_cellLabel   = makeLabel("cell-coordinates", x, screen.height - 40.f, .28f);
        m_cameraLabel = makeLabel("camera-offset-values", x, screen.height - 52.f, .33f);
        if (!m_worldLabel || !m_gridLabel || !m_cellLabel || !m_cameraLabel) return false;

        addChild(m_worldLabel);
        addChild(m_gridLabel);
        addChild(m_cellLabel);
        addChild(m_cameraLabel);

        auto* menu = CCMenu::create();
        if (!menu) return false;
        menu->setPosition({0.f, 0.f});
        menu->setID("coordinate-controls"_spr);

        auto* setSprite = ButtonSprite::create(
            "Set Ref", 80, true, "goldFont.fnt", "GJ_button_01.png", 0.f, .63f
        );
        auto* clearSprite = ButtonSprite::create(
            "Clear Ref", 80, true, "goldFont.fnt", "GJ_button_04.png", 0.f, .63f
        );
        auto* setButton = CCMenuItemSpriteExtra::create(
            setSprite, this, menu_selector(SelectedCoordinatesHUD::onSetReference)
        );
        auto* clearButton = CCMenuItemSpriteExtra::create(
            clearSprite, this, menu_selector(SelectedCoordinatesHUD::onClearReference)
        );
        setButton->setPosition({55.f, screen.height - 18.f});
        clearButton->setPosition({55.f, screen.height - 49.f});
        setButton->setID("set-reference-button"_spr);
        clearButton->setID("clear-reference-button"_spr);
        menu->addChild(setButton);
        menu->addChild(clearButton);
        addChild(menu, 5);

        scheduleUpdate();
        return true;
    }

    void update(float) override {
        auto const selected = selection();
        if (selected.empty()) {
            setLabel(m_worldLabel, "", m_lastWorld);
            setLabel(m_gridLabel, "", m_lastGrid);
            setLabel(m_cellLabel, "", m_lastCell);
            setLabel(m_cameraLabel, m_hasReference ? "Reference saved - select destination" : "", m_lastCamera);
            return;
        }

        if (selected.size() != 1) {
            setLabel(m_worldLabel, fmt::format("{} objects selected", selected.size()), m_lastWorld);
            setLabel(m_gridLabel, "", m_lastGrid);
            setLabel(m_cellLabel, "", m_lastCell);
            setLabel(m_cameraLabel, "Select one object", m_lastCamera);
            return;
        }

        // World coordinates: actual object anchor/center in the Geometry Dash level.
        CCPoint const p = selected.front()->getPosition();
        float const bx = p.x / kGridSize;
        float const by = p.y / kGridSize;
        int const cellX = static_cast<int>(std::floor(bx)) + 1;
        int const cellY = static_cast<int>(std::floor(by)) + 1;

        setLabel(m_worldLabel, fmt::format("GD X: {:.2f}   Y: {:.2f}", p.x, p.y), m_lastWorld);
        setLabel(m_gridLabel, fmt::format("Grid X: {:.3f}  Y: {:.3f}", bx, by), m_lastGrid);
        setLabel(m_cellLabel, fmt::format("Cell X: {}  Y: {}", cellX, cellY), m_lastCell);

        if (m_hasReference) {
            // Camera Offset is relative to the DEFAULT camera center at activation.
            // Mark an object placed at that center as the reference; then select a
            // destination at the desired center and enter this delta into the trigger.
            float const offsetX = (p.x - m_reference.x) / kCameraOffsetScale;
            float const offsetY = (p.y - m_reference.y) / kCameraOffsetScale;
            setLabel(
                m_cameraLabel,
                fmt::format("Camera OFFSET X: {:.2f}  Y: {:.2f}", offsetX, offsetY),
                m_lastCamera
            );
        } else {
            setLabel(m_cameraLabel, "Camera OFFSET: first Set Ref", m_lastCamera);
        }
    }
};
}

class $modify(SelectedCoordinatesEditorUI, EditorUI) {
    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;
        if (auto* hud = SelectedCoordinatesHUD::create(this)) {
            addChild(hud, 99999);
        }
        return true;
    }
};
