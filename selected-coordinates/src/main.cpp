#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>

using namespace geode::prelude;

// The HUD belongs to EditorUI, so it disappears automatically on editor exit.
// Checking selection on update also catches dragging, move buttons and undo/redo.
class SelectedCoordinatesHUD final : public CCNode {
    EditorUI* m_editorUI = nullptr;
    CCLabelBMFont* m_label = nullptr;
    std::string m_previousText;

public:
    static SelectedCoordinatesHUD* create(EditorUI* editorUI) {
        auto* hud = new SelectedCoordinatesHUD();
        if (hud && hud->initHUD(editorUI)) {
            hud->autorelease();
            return hud;
        }
        CC_SAFE_DELETE(hud);
        return nullptr;
    }

    bool initHUD(EditorUI* editorUI) {
        if (!CCNode::init() || !editorUI) return false;
        m_editorUI = editorUI;
        setID("coordinate-hud"_spr);

        m_label = CCLabelBMFont::create("", "bigFont.fnt");
        if (!m_label) return false;
        m_label->setScale(0.42f);
        m_label->setAnchorPoint({ 0.5f, 0.5f });
        auto const screen = CCDirector::sharedDirector()->getWinSize();
        m_label->setPosition({ screen.width / 2.f, screen.height - 19.f });
        m_label->setVisible(false);
        m_label->setID("coordinate-label"_spr);
        addChild(m_label);
        scheduleUpdate();
        return true;
    }

    void update(float) override {
        if (!m_editorUI || !m_label) return;

        GameObject* selected = m_editorUI->m_selectedObject;
        auto* multi = m_editorUI->m_selectedObjects;
        unsigned int count = multi ? multi->count() : 0;

        // In some selection states the single object lives only in the array.
        if (!selected && count == 1) {
            selected = static_cast<GameObject*>(multi->objectAtIndex(0));
        }

        std::string display;
        if (selected && count <= 1) {
            auto const p = selected->getPosition();
            display = fmt::format("X: {:.2f}   Y: {:.2f}", p.x, p.y);
        }
        else if (count > 1) {
            display = fmt::format("{} objects selected", count);
        }

        if (display.empty()) {
            m_label->setVisible(false);
        }
        else {
            m_label->setVisible(true);
            if (display != m_previousText) {
                m_previousText = display;
                m_label->setString(display.c_str());
            }
        }
    }
};

class $modify(SelectedCoordinatesEditorUI, EditorUI) {
    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;

        if (auto* hud = SelectedCoordinatesHUD::create(this)) {
            addChild(hud, 99999);
        }
        return true;
    }
};
