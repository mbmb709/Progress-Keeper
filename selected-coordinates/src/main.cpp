#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace {
constexpr float kGridSize = 30.f;

// The Geometry Dash object position is the CENTER / anchor, in level units.
// One editor grid cell is 30 GD units, but grid indices are not the same as X/Y.
class SelectedCoordinatesHUD final : public CCNode {
    EditorUI* m_editorUI = nullptr;
    CCLabelBMFont* m_world = nullptr;
    CCLabelBMFont* m_grid = nullptr;
    CCLabelBMFont* m_cell = nullptr;
    std::string m_lastWorld;
    std::string m_lastGrid;
    std::string m_lastCell;

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
        m_world = makeLabel("world-coordinates", x, screen.height - 19.f, .40f);
        m_grid = makeLabel("grid-coordinates", x, screen.height - 31.f, .32f);
        m_cell = makeLabel("cell-coordinates", x, screen.height - 42.f, .28f);
        if (!m_world || !m_grid || !m_cell) return false;

        addChild(m_world);
        addChild(m_grid);
        addChild(m_cell);
        scheduleUpdate();
        return true;
    }

    void update(float) override {
        if (!m_editorUI) return;

        // A lone selected object may be in m_selectedObject or m_selectedObjects.
        // Some editor states keep the SAME pointer in both. Count UNIQUE objects.
        std::vector<GameObject*> selected;
        if (auto* obj = m_editorUI->m_selectedObject) selected.push_back(obj);
        if (auto* arr = m_editorUI->m_selectedObjects) {
            for (auto* obj : CCArrayExt<GameObject*>(arr)) {
                if (obj && std::find(selected.begin(), selected.end(), obj) == selected.end()) {
                    selected.push_back(obj);
                }
            }
        }

        if (selected.empty()) {
            setLabel(m_world, "", m_lastWorld);
            setLabel(m_grid, "", m_lastGrid);
            setLabel(m_cell, "", m_lastCell);
            return;
        }

        if (selected.size() != 1) {
            setLabel(m_world, fmt::format("{} objects selected", selected.size()), m_lastWorld);
            setLabel(m_grid, "Select one object for X / Y", m_lastGrid);
            setLabel(m_cell, "", m_lastCell);
            return;
        }

        // This is the saved coordinate system for Geometry Dash level objects,
        // not a position converted from the camera / editor screen.
        CCPoint const p = selected.front()->getPosition();
        float const bx = p.x / kGridSize;
        float const by = p.y / kGridSize;

        // A CELL is numbered 1-based, left-to-right and bottom-to-top.
        // A regular 30x30 block with its center at X=15,Y=15 is in cell 1,1.
        int const cellX = static_cast<int>(std::floor(bx)) + 1;
        int const cellY = static_cast<int>(std::floor(by)) + 1;

        setLabel(m_world, fmt::format("GD X: {:.2f}    Y: {:.2f}", p.x, p.y), m_lastWorld);
        setLabel(m_grid, fmt::format("Grid X: {:.3f}    Y: {:.3f} blocks", bx, by), m_lastGrid);
        setLabel(m_cell, fmt::format("Cell X: {}    Y: {}", cellX, cellY), m_lastCell);
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
